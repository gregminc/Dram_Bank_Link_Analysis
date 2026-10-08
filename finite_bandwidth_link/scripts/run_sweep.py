#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import io
import itertools
import math
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]


def comma_separated_ints(value: str) -> list[int]:
    values = [int(item.strip()) for item in value.split(",")]
    if not values or any(item <= 0 for item in values):
        raise argparse.ArgumentTypeError("values must be positive integers")
    return values


def comma_separated_floats(value: str) -> list[float]:
    values = [float(item.strip()) for item in value.split(",")]
    if not values or any(not math.isfinite(item) or item <= 0.0 for item in values):
        raise argparse.ArgumentTypeError("values must be positive numbers")
    return values


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Sweep bank grouping, link width/rate, and FIFO depth."
    )
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument(
        "--output",
        type=Path,
        default=PROJECT_ROOT / "results" / "sweep.csv",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=PROJECT_ROOT / "build",
    )
    parser.add_argument(
        "--banks-per-link",
        type=comma_separated_ints,
        default=comma_separated_ints("1,2,4,8"),
    )
    parser.add_argument(
        "--link-width-bits",
        type=comma_separated_ints,
        default=comma_separated_ints("8,16,32"),
    )
    parser.add_argument(
        "--transfers-per-ns",
        type=comma_separated_floats,
        default=comma_separated_floats("4.266"),
    )
    parser.add_argument(
        "--fifo-depth",
        type=comma_separated_ints,
        default=comma_separated_ints("1,2,4,8"),
    )
    parser.add_argument("--transfer-bits", type=int, default=256)
    parser.add_argument("--bank-groups", type=int, default=2)
    parser.add_argument("--banks-per-group", type=int, default=4)
    parser.add_argument(
        "--details-dir",
        type=Path,
        help="Optionally preserve per-link and per-bank CSVs for every run.",
    )
    parser.add_argument(
        "--no-build",
        action="store_true",
        help="Use the existing executable without invoking CMake.",
    )
    return parser.parse_args()


def build_simulator(build_dir: Path) -> Path:
    subprocess.run(
        [
            "cmake",
            "-S",
            str(PROJECT_ROOT),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(build_dir), "--config", "Release"],
        check=True,
    )

    executable = build_dir / "bank_link_sim"
    if not executable.exists():
        executable = build_dir / "Release" / "bank_link_sim"
    if not executable.exists():
        raise FileNotFoundError("CMake did not produce bank_link_sim")
    return executable


def run_configuration(
    executable: Path,
    trace: Path,
    banks_per_link: int,
    width_bits: int,
    transfers_per_ns: float,
    fifo_depth: int,
    transfer_bits: int,
    bank_groups: int,
    banks_per_group: int,
    details_dir: Path | None,
) -> dict[str, str]:
    command = [
        str(executable),
        "--trace",
        str(trace),
        "--banks-per-link",
        str(banks_per_link),
        "--link-width-bits",
        str(width_bits),
        "--transfers-per-ns",
        str(transfers_per_ns),
        "--fifo-depth",
        str(fifo_depth),
        "--transfer-bits",
        str(transfer_bits),
        "--bank-groups",
        str(bank_groups),
        "--banks-per-group",
        str(banks_per_group),
    ]

    if details_dir is not None:
        run_name = (
            f"bpl{banks_per_link}_w{width_bits}_r{transfers_per_ns:g}"
            f"_q{fifo_depth}"
        )
        command.extend(
            [
                "--link-output",
                str(details_dir / f"{run_name}_links.csv"),
                "--bank-output",
                str(details_dir / f"{run_name}_banks.csv"),
            ]
        )

    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode not in (0, 2):
        raise RuntimeError(
            f"Simulator failed with code {completed.returncode}:\n"
            f"{completed.stderr}"
        )

    rows = list(csv.DictReader(io.StringIO(completed.stdout)))
    if len(rows) != 1:
        raise RuntimeError("Simulator did not emit exactly one summary row")
    row = rows[0]
    row["process_return_code"] = str(completed.returncode)
    return row


def main() -> int:
    arguments = parse_arguments()
    trace = arguments.trace.resolve()
    if not trace.is_file():
        raise FileNotFoundError(f"Trace does not exist: {trace}")
    if arguments.transfer_bits <= 0:
        raise ValueError("--transfer-bits must be positive")
    if arguments.bank_groups <= 0 or arguments.banks_per_group <= 0:
        raise ValueError("bank counts must be positive")

    build_dir = arguments.build_dir.resolve()
    executable = build_dir / "bank_link_sim"
    if not arguments.no_build:
        executable = build_simulator(build_dir)
    elif not executable.exists():
        release_executable = build_dir / "Release" / "bank_link_sim"
        if release_executable.exists():
            executable = release_executable
        else:
            raise FileNotFoundError(f"Simulator executable not found: {executable}")

    output = arguments.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    details_dir = (
        arguments.details_dir.resolve()
        if arguments.details_dir is not None
        else None
    )
    if details_dir is not None:
        details_dir.mkdir(parents=True, exist_ok=True)

    results: list[dict[str, str]] = []
    combinations = itertools.product(
        arguments.banks_per_link,
        arguments.link_width_bits,
        arguments.transfers_per_ns,
        arguments.fifo_depth,
    )
    for banks_per_link, width_bits, rate, fifo_depth in combinations:
        results.append(
            run_configuration(
                executable=executable,
                trace=trace,
                banks_per_link=banks_per_link,
                width_bits=width_bits,
                transfers_per_ns=rate,
                fifo_depth=fifo_depth,
                transfer_bits=arguments.transfer_bits,
                bank_groups=arguments.bank_groups,
                banks_per_group=arguments.banks_per_group,
                details_dir=details_dir,
            )
        )

    if not results:
        raise RuntimeError("The sweep contained no configurations")
    with output.open("w", newline="") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)

    feasible = sum(row["feasible"] == "true" for row in results)
    print(
        f"Wrote {len(results)} configurations to {output} "
        f"({feasible} feasible, {len(results) - feasible} overflowed)."
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
