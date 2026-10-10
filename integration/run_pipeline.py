#!/usr/bin/env python3

from __future__ import annotations

import argparse
import configparser
import csv
import json
import shlex
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DRAMSIM3_ROOT = PROJECT_ROOT.parent / "DRAMsim3"
DEFAULT_WORKLOAD = PROJECT_ROOT / "integration" / "workloads" / "smoke.trace"
DEFAULT_RESULTS_ROOT = PROJECT_ROOT / "results" / "integration"


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate a DRAMsim3 READ-issue trace and sweep the downstream "
            "finite-bandwidth bank-link model."
        )
    )
    parser.add_argument(
        "--dramsim3-root", type=Path, default=DEFAULT_DRAMSIM3_ROOT
    )
    parser.add_argument(
        "--config",
        type=Path,
        help=(
            "DRAMsim3 INI file; defaults to "
            "configs/LPDDR4_8Gb_x16_2400.ini under --dramsim3-root"
        ),
    )
    parser.add_argument("--workload", type=Path, default=DEFAULT_WORKLOAD)
    parser.add_argument(
        "--cycles",
        type=int,
        default=1_300_000,
        help=(
            "DRAM cycles to run; the default passes the supplied config's "
            "first statistics epoch"
        ),
    )
    parser.add_argument("--run-name", default="smoke_lpddr4_2400")
    parser.add_argument("--results-root", type=Path, default=DEFAULT_RESULTS_ROOT)
    parser.add_argument("--banks-per-link", default="1,2,4,8")
    parser.add_argument("--link-width-bits", default="8,16,32")
    parser.add_argument("--transfers-per-ns", default="4.266")
    parser.add_argument("--fifo-depth", default="1,2,4,8")
    parser.add_argument("--transfer-bits", type=int, default=256)
    parser.add_argument(
        "--no-build",
        action="store_true",
        help="Reuse existing DRAMsim3 and bank-link executables.",
    )
    return parser.parse_args()


def run_command(
    command: list[str],
    *,
    cwd: Path,
    log_path: Path,
) -> subprocess.CompletedProcess[str]:
    printable = shlex.join(command)
    print(f"+ {printable}")
    completed = subprocess.run(
        command,
        cwd=cwd,
        text=True,
        capture_output=True,
    )
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(
        f"$ {printable}\n\n[stdout]\n{completed.stdout}"
        f"\n[stderr]\n{completed.stderr}",
        encoding="utf-8",
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"Command failed with exit code {completed.returncode}; "
            f"see {log_path}"
        )
    return completed


def build_dramsim3(dramsim3_root: Path, log_directory: Path) -> Path:
    # Use a dedicated directory so an unrelated or relocated DRAMsim3 build
    # cache cannot contaminate this integration build.
    build_directory = dramsim3_root / "build" / "bank_ready_trace"
    configure = [
        "cmake",
        "-S",
        str(dramsim3_root),
        "-B",
        str(build_directory),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        "-DBANK_READY_TRACE=ON",
    ]
    run_command(
        configure,
        cwd=dramsim3_root,
        log_path=log_directory / "configure.log",
    )
    run_command(
        [
            "cmake",
            "--build",
            str(build_directory),
            "--target",
            "dramsim3main",
            "--config",
            "Release",
        ],
        cwd=dramsim3_root,
        log_path=log_directory / "build.log",
    )
    executable = build_directory / "dramsim3main"
    if not executable.is_file():
        executable = build_directory / "Release" / "dramsim3main"
    if not executable.is_file():
        raise FileNotFoundError("DRAMsim3 build did not produce dramsim3main")
    return executable


def existing_dramsim3_executable(dramsim3_root: Path) -> Path:
    candidates = [
        dramsim3_root / "build" / "bank_ready_trace" / "dramsim3main",
        dramsim3_root
        / "build"
        / "bank_ready_trace"
        / "Release"
        / "dramsim3main",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("No existing DRAMsim3 executable was found")


def load_geometry(config_path: Path) -> dict[str, int | str]:
    parser = configparser.ConfigParser()
    if not parser.read(config_path):
        raise FileNotFoundError(f"Unable to read DRAMsim3 config: {config_path}")

    structure = parser["dram_structure"]
    system = parser["system"]
    geometry: dict[str, int | str] = {
        "protocol": structure.get("protocol", ""),
        "channels": system.getint("channels"),
        "bank_groups": structure.getint("bankgroups"),
        "banks_per_group": structure.getint("banks_per_group"),
        "rows": structure.getint("rows"),
        "columns": structure.getint("columns"),
        "device_width_bits": structure.getint("device_width"),
        "bus_width_bits": system.getint("bus_width"),
        "burst_length": structure.getint("BL"),
        "channel_size_mb": system.getint("channel_size"),
        "address_mapping": system.get("address_mapping"),
    }

    if geometry["address_mapping"] != "rochrababgco":
        raise ValueError(
            "The current bank-link mapper requires DRAMsim3 address mapping "
            "rochrababgco"
        )
    bus_width = int(geometry["bus_width_bits"])
    device_width = int(geometry["device_width_bits"])
    if bus_width % 8 != 0 or bus_width % device_width != 0:
        raise ValueError("DRAM bus width is incompatible with the device width")

    devices_per_rank = bus_width // device_width
    page_size_bytes = int(geometry["columns"]) * device_width // 8
    megabytes_per_bank = (
        page_size_bytes * (int(geometry["rows"]) // 1024) // 1024
    )
    banks_per_rank = (
        int(geometry["bank_groups"]) * int(geometry["banks_per_group"])
    )
    megabytes_per_rank = megabytes_per_bank * banks_per_rank * devices_per_rank
    channel_size = int(geometry["channel_size_mb"])
    ranks = 1 if megabytes_per_rank > channel_size else channel_size // megabytes_per_rank
    if ranks <= 0:
        raise ValueError("DRAMsim3 geometry produced no addressable ranks")

    geometry["ranks"] = ranks
    geometry["bus_width_bytes"] = bus_width // 8
    geometry["request_size_bytes"] = (
        int(geometry["bus_width_bytes"]) * int(geometry["burst_length"])
    )
    return geometry


def validate_bank_ready_trace(trace_path: Path) -> dict[str, float | int]:
    if not trace_path.is_file():
        raise FileNotFoundError(f"DRAMsim3 did not produce {trace_path}")

    count = 0
    first_time = 0.0
    last_time = 0.0
    previous_time = -1.0
    for line_number, raw_line in enumerate(
        trace_path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != 3:
            raise ValueError(f"Malformed bank-ready trace line {line_number}")
        timestamp = float(fields[0])
        if fields[1] != "Read":
            raise ValueError(f"Unexpected operation on trace line {line_number}")
        int(fields[2], 0)
        if timestamp < previous_time:
            raise ValueError("Bank-ready timestamps are not nondecreasing")
        if count == 0:
            first_time = timestamp
        previous_time = timestamp
        last_time = timestamp
        count += 1

    if count == 0:
        raise ValueError("The bank-ready trace contains no READ events")
    return {
        "records": count,
        "first_time_ns": first_time,
        "last_time_ns": last_time,
    }


def git_revision(repository: Path) -> str | None:
    completed = subprocess.run(
        ["git", "-C", str(repository), "rev-parse", "HEAD"],
        text=True,
        capture_output=True,
    )
    return completed.stdout.strip() if completed.returncode == 0 else None


def count_sweep_results(summary_path: Path) -> dict[str, int]:
    with summary_path.open(newline="", encoding="utf-8") as summary_file:
        rows = list(csv.DictReader(summary_file))
    return {
        "configurations": len(rows),
        "feasible": sum(row.get("feasible") == "true" for row in rows),
        "overflowed": sum(row.get("feasible") != "true" for row in rows),
    }


def main() -> int:
    arguments = parse_arguments()
    if arguments.cycles <= 0 or arguments.transfer_bits <= 0:
        raise ValueError("cycles and transfer bits must be positive")

    dramsim3_root = arguments.dramsim3_root.resolve()
    config_path = (
        arguments.config.resolve()
        if arguments.config is not None
        else dramsim3_root / "configs" / "LPDDR4_8Gb_x16_2400.ini"
    )
    workload = arguments.workload.resolve()
    if not dramsim3_root.is_dir():
        raise FileNotFoundError(f"DRAMsim3 root does not exist: {dramsim3_root}")
    if not config_path.is_file():
        raise FileNotFoundError(f"DRAMsim3 config does not exist: {config_path}")
    if not workload.is_file():
        raise FileNotFoundError(f"Workload does not exist: {workload}")

    geometry = load_geometry(config_path)
    if int(geometry["channels"]) != 1:
        raise ValueError(
            "The preliminary integration expects one DRAM channel; "
            "multi-channel trace merging is not implemented yet"
        )

    run_directory = arguments.results_root.resolve() / arguments.run_name
    dramsim_output = run_directory / "dramsim3"
    bank_link_output = run_directory / "bank_link"
    details_directory = bank_link_output / "details"
    dramsim_output.mkdir(parents=True, exist_ok=True)
    details_directory.mkdir(parents=True, exist_ok=True)

    if arguments.no_build:
        dramsim_executable = existing_dramsim3_executable(dramsim3_root)
    else:
        dramsim_executable = build_dramsim3(dramsim3_root, dramsim_output)

    dramsim_command = [
        str(dramsim_executable),
        str(config_path),
        "--cycles",
        str(arguments.cycles),
        "--trace",
        str(workload),
        "--output-dir",
        str(dramsim_output),
    ]
    run_command(
        dramsim_command,
        cwd=dramsim3_root,
        log_path=dramsim_output / "run.log",
    )

    bank_ready_trace = dramsim_output / "ch_0_bank_ready.trace"
    trace_summary = validate_bank_ready_trace(bank_ready_trace)

    sweep_script = (
        PROJECT_ROOT / "finite_bandwidth_link" / "scripts" / "run_sweep.py"
    )
    sweep_summary = bank_link_output / "sweep.csv"
    sweep_command = [
        sys.executable,
        str(sweep_script),
        "--trace",
        str(bank_ready_trace),
        "--output",
        str(sweep_summary),
        "--details-dir",
        str(details_directory),
        "--banks-per-link",
        arguments.banks_per_link,
        "--link-width-bits",
        arguments.link_width_bits,
        "--transfers-per-ns",
        arguments.transfers_per_ns,
        "--fifo-depth",
        arguments.fifo_depth,
        "--transfer-bits",
        str(arguments.transfer_bits),
        "--channels",
        str(geometry["channels"]),
        "--ranks",
        str(geometry["ranks"]),
        "--bank-groups",
        str(geometry["bank_groups"]),
        "--banks-per-group",
        str(geometry["banks_per_group"]),
        "--rows",
        str(geometry["rows"]),
        "--columns",
        str(geometry["columns"]),
        "--bus-width-bytes",
        str(geometry["bus_width_bytes"]),
        "--burst-length",
        str(geometry["burst_length"]),
    ]
    if arguments.no_build:
        sweep_command.append("--no-build")
    run_command(
        sweep_command,
        cwd=PROJECT_ROOT,
        log_path=bank_link_output / "run.log",
    )
    sweep_counts = count_sweep_results(sweep_summary)

    manifest = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "run_name": arguments.run_name,
        "semantics": (
            "Each READ issue produces one 256-bit normalized bank-ready event; "
            "fixed internal pipeline latency is omitted."
        ),
        "dramsim3": {
            "root": str(dramsim3_root),
            "revision": git_revision(dramsim3_root),
            "config": str(config_path),
            "workload": str(workload),
            "cycles": arguments.cycles,
            "command": dramsim_command,
        },
        "analysis_repository_revision": git_revision(PROJECT_ROOT),
        "geometry": geometry,
        "trace": {"path": str(bank_ready_trace), **trace_summary},
        "bank_link_sweep": {
            "command": sweep_command,
            "summary": str(sweep_summary),
            **sweep_counts,
        },
    }
    manifest_path = run_directory / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )

    print(
        f"Validated {trace_summary['records']} READ events and wrote "
        f"{sweep_counts['configurations']} sweep rows "
        f"({sweep_counts['feasible']} feasible, "
        f"{sweep_counts['overflowed']} overflowed)."
    )
    print(f"Results: {run_directory}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
