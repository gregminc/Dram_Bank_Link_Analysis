import argparse
import csv
import html
import os
import subprocess
from pathlib import Path

from generate_addresses import generate_trace


DEFAULT_WINDOWS_NS = (1.0, 2.0, 4.0, 8.0, 10.0, 20.0)
DEFAULT_GROUP_SIZES = (1, 2, 4, 8)
SOURCE_FILES = (
    "main.cpp",
    "trace_reader.cpp",
    "address_mapper.cpp",
    "statistics.cpp",
)


def parse_number_list(text: str, conversion):
    values = tuple(conversion(value.strip()) for value in text.split(","))
    if not values or any(value <= 0 for value in values):
        raise argparse.ArgumentTypeError("Values must be positive")
    return values


def value_label(value: float) -> str:
    return f"{value:g}".replace(".", "p")


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as source:
        return list(csv.DictReader(source))


def write_csv(path: Path, rows: list[dict], fieldnames: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.DictWriter(destination, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def build_analyzer(source_directory: Path, executable: Path) -> None:
    compiler = os.environ.get("CXX", "clang++")
    command = [
        compiler,
        "-std=c++20",
        "-O2",
        "-Wall",
        "-Wextra",
        "-pedantic",
        *(str(source_directory / filename) for filename in SOURCE_FILES),
        "-o",
        str(executable),
    ]
    print("Building trace analyzer...")
    subprocess.run(command, check=True)


def run_experiment(
    executable: Path,
    trace_path: Path,
    window_ns: float,
    group_size: int,
    output_directory: Path,
) -> None:
    command = [
        str(executable),
        str(trace_path),
        "--window-ns",
        str(window_ns),
        "--banks-per-link-group",
        str(group_size),
        "--output-dir",
        str(output_directory),
    ]
    subprocess.run(command, check=True)


def summarize_run(
    trace_name: str,
    window_ns: float,
    group_size: int,
    run_directory: Path,
) -> tuple[dict, list[dict], list[dict], list[dict]]:
    banks = read_csv(run_directory / "bank_statistics.csv")
    histogram = read_csv(run_directory / "active_bank_histogram.csv")
    groups = read_csv(run_directory / "group_statistics.csv")

    nonempty_windows = sum(int(row["window_count"]) for row in histogram)
    active_bank_windows = sum(
        int(row["active_banks"]) * int(row["window_count"])
        for row in histogram
    )
    mean_active_banks = (
        active_bank_windows / nonempty_windows if nonempty_windows else 0.0
    )
    maximum_active_banks = max(
        (
            int(row["active_banks"])
            for row in histogram
            if int(row["window_count"]) > 0
        ),
        default=0,
    )

    total_requests = sum(int(row["total_requests"]) for row in groups)
    active_group_windows = sum(int(row["active_windows"]) for row in groups)
    contended_group_windows = sum(
        int(row["contended_windows"]) for row in groups
    )
    aggregate_contention_fraction = (
        contended_group_windows / active_group_windows
        if active_group_windows
        else 0.0
    )
    maximum_group_requests = max(
        (int(row["max_requests_in_window"]) for row in groups),
        default=0,
    )

    summary = {
        "trace": trace_name,
        "window_ns": window_ns,
        "banks_per_link_group": group_size,
        "num_link_groups": len(groups),
        "total_requests": total_requests,
        "nonempty_windows": nonempty_windows,
        "mean_active_banks": mean_active_banks,
        "maximum_active_banks": maximum_active_banks,
        "aggregate_contention_fraction": aggregate_contention_fraction,
        "maximum_group_requests_in_window": maximum_group_requests,
    }

    common = {
        "trace": trace_name,
        "window_ns": window_ns,
        "banks_per_link_group": group_size,
    }
    aggregate_groups = [{**common, **row} for row in groups]
    aggregate_histogram = [{**common, **row} for row in histogram]
    aggregate_banks = [{**common, **row} for row in banks]
    return summary, aggregate_groups, aggregate_histogram, aggregate_banks


def heat_color(value: float, minimum: float, maximum: float) -> str:
    ratio = 0.5 if maximum == minimum else (value - minimum) / (maximum - minimum)
    red = round(245 - 40 * (1 - ratio))
    green = round(245 - 160 * ratio)
    blue = round(250 - 170 * ratio)
    return f"#{red:02x}{green:02x}{blue:02x}"


def write_heatmap_svg(
    summaries: list[dict],
    windows: tuple[float, ...],
    group_sizes: tuple[int, ...],
    metric: str,
    title: str,
    output_path: Path,
    number_format: str,
) -> None:
    lookup = {
        (float(row["window_ns"]), int(row["banks_per_link_group"])):
            float(row[metric])
        for row in summaries
    }
    values = list(lookup.values())
    minimum, maximum = min(values), max(values)

    cell_width, cell_height = 135, 65
    left, top, right, bottom = 145, 95, 35, 80
    width = left + len(group_sizes) * cell_width + right
    height = top + len(windows) * cell_height + bottom

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
        f'height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width / 2}" y="35" text-anchor="middle" '
        f'font-family="sans-serif" font-size="22" font-weight="bold">'
        f'{html.escape(title)}</text>',
        f'<text x="{width / 2}" y="{height - 18}" text-anchor="middle" '
        f'font-family="sans-serif" font-size="16">Banks per link group</text>',
        f'<text x="22" y="{height / 2}" text-anchor="middle" '
        f'transform="rotate(-90 22 {height / 2})" font-family="sans-serif" '
        f'font-size="16">Window size (ns)</text>',
    ]

    for column, group_size in enumerate(group_sizes):
        x = left + column * cell_width + cell_width / 2
        svg.append(
            f'<text x="{x}" y="{top - 15}" text-anchor="middle" '
            f'font-family="sans-serif" font-size="15">{group_size}</text>'
        )

    for row_index, window in enumerate(windows):
        y = top + row_index * cell_height
        svg.append(
            f'<text x="{left - 18}" y="{y + cell_height / 2 + 5}" '
            f'text-anchor="end" font-family="sans-serif" font-size="15">'
            f'{window:g}</text>'
        )
        for column, group_size in enumerate(group_sizes):
            value = lookup[(float(window), int(group_size))]
            x = left + column * cell_width
            color = heat_color(value, minimum, maximum)
            label = format(value, number_format)
            svg.extend([
                f'<rect x="{x}" y="{y}" width="{cell_width}" '
                f'height="{cell_height}" fill="{color}" stroke="#555"/>',
                f'<text x="{x + cell_width / 2}" '
                f'y="{y + cell_height / 2 + 5}" text-anchor="middle" '
                f'font-family="sans-serif" font-size="15">{label}</text>',
            ])

    svg.append("</svg>")
    output_path.write_text("\n".join(svg), encoding="utf-8")


def write_line_plot_svg(
    summaries: list[dict],
    windows: tuple[float, ...],
    group_sizes: tuple[int, ...],
    output_path: Path,
) -> None:
    lookup = {
        (float(row["window_ns"]), int(row["banks_per_link_group"])):
            float(row["aggregate_contention_fraction"])
        for row in summaries
    }
    width, height = 900, 580
    left, top, right, bottom = 90, 70, 190, 80
    plot_width = width - left - right
    plot_height = height - top - bottom
    colors = ("#2455a4", "#d1495b", "#2a9d5b", "#8b5fbf", "#e17c05", "#008c95")

    def x_position(index: int) -> float:
        return left + index * plot_width / max(1, len(group_sizes) - 1)

    def y_position(value: float) -> float:
        return top + (1.0 - value) * plot_height

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
        f'height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width / 2}" y="35" text-anchor="middle" '
        f'font-family="sans-serif" font-size="22" font-weight="bold">'
        'Contention versus link-group size</text>',
    ]

    for tick in range(0, 11, 2):
        value = tick / 10
        y = y_position(value)
        svg.extend([
            f'<line x1="{left}" y1="{y}" x2="{left + plot_width}" y2="{y}" '
            'stroke="#ddd"/>',
            f'<text x="{left - 12}" y="{y + 5}" text-anchor="end" '
            f'font-family="sans-serif" font-size="13">{value:.1f}</text>',
        ])

    for index, group_size in enumerate(group_sizes):
        x = x_position(index)
        svg.append(
            f'<text x="{x}" y="{top + plot_height + 27}" text-anchor="middle" '
            f'font-family="sans-serif" font-size="14">{group_size}</text>'
        )

    svg.extend([
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top + plot_height}" '
        'stroke="black"/>',
        f'<line x1="{left}" y1="{top + plot_height}" '
        f'x2="{left + plot_width}" y2="{top + plot_height}" stroke="black"/>',
        f'<text x="{left + plot_width / 2}" y="{height - 20}" '
        'text-anchor="middle" font-family="sans-serif" font-size="16">'
        'Banks per link group</text>',
        f'<text x="20" y="{top + plot_height / 2}" text-anchor="middle" '
        f'transform="rotate(-90 20 {top + plot_height / 2})" '
        'font-family="sans-serif" font-size="16">Aggregate contention fraction</text>',
    ])

    for line_index, window in enumerate(windows):
        color = colors[line_index % len(colors)]
        points = [
            (
                x_position(index),
                y_position(lookup[(float(window), int(group_size))]),
            )
            for index, group_size in enumerate(group_sizes)
        ]
        point_text = " ".join(f"{x},{y}" for x, y in points)
        svg.append(
            f'<polyline points="{point_text}" fill="none" stroke="{color}" '
            'stroke-width="2.5"/>'
        )
        for x, y in points:
            svg.append(f'<circle cx="{x}" cy="{y}" r="4" fill="{color}"/>')

        legend_y = top + 22 * line_index
        svg.extend([
            f'<line x1="{left + plot_width + 25}" y1="{legend_y}" '
            f'x2="{left + plot_width + 55}" y2="{legend_y}" '
            f'stroke="{color}" stroke-width="3"/>',
            f'<text x="{left + plot_width + 65}" y="{legend_y + 5}" '
            f'font-family="sans-serif" font-size="13">{window:g} ns</text>',
        ])

    svg.append("</svg>")
    output_path.write_text("\n".join(svg), encoding="utf-8")


def main() -> None:
    source_directory = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(
        description="Build the analyzer and sweep window and link-group sizes"
    )
    parser.add_argument(
        "--trace", type=Path, default=source_directory / "random_trace.txt"
    )
    parser.add_argument(
        "--results-dir", type=Path, default=source_directory / "results"
    )
    parser.add_argument(
        "--windows",
        default=",".join(f"{value:g}" for value in DEFAULT_WINDOWS_NS),
        help="Comma-separated window sizes in ns",
    )
    parser.add_argument(
        "--group-sizes",
        default=",".join(str(value) for value in DEFAULT_GROUP_SIZES),
        help="Comma-separated banks-per-link-group values",
    )
    parser.add_argument("--skip-build", action="store_true")
    arguments = parser.parse_args()

    windows = parse_number_list(arguments.windows, float)
    group_sizes = parse_number_list(arguments.group_sizes, int)
    invalid_groups = [size for size in group_sizes if size > 8 or 8 % size != 0]
    if invalid_groups:
        parser.error("Group sizes must divide the eight banks in each rank")

    trace_path = arguments.trace.resolve()
    generate_trace(trace_path)

    executable = source_directory / "trace_analyzer"
    if not arguments.skip_build:
        build_analyzer(source_directory, executable)
    elif not executable.exists():
        parser.error("--skip-build was used, but trace_analyzer does not exist")

    experiment_root = arguments.results_dir.resolve() / trace_path.stem
    plots_directory = experiment_root / "plots"
    plots_directory.mkdir(parents=True, exist_ok=True)

    summaries: list[dict] = []
    all_groups: list[dict] = []
    all_histograms: list[dict] = []
    all_banks: list[dict] = []

    for window_ns in windows:
        for group_size in group_sizes:
            run_directory = (
                experiment_root
                / f"window_{value_label(window_ns)}ns"
                / f"group_{group_size}"
            )
            run_directory.mkdir(parents=True, exist_ok=True)
            print(f"Running window={window_ns:g} ns, group={group_size}")
            run_experiment(
                executable,
                trace_path,
                window_ns,
                group_size,
                run_directory,
            )
            summary, groups, histogram, banks = summarize_run(
                trace_path.name,
                window_ns,
                group_size,
                run_directory,
            )
            summaries.append(summary)
            all_groups.extend(groups)
            all_histograms.extend(histogram)
            all_banks.extend(banks)

    summary_fields = [
        "trace",
        "window_ns",
        "banks_per_link_group",
        "num_link_groups",
        "total_requests",
        "nonempty_windows",
        "mean_active_banks",
        "maximum_active_banks",
        "aggregate_contention_fraction",
        "maximum_group_requests_in_window",
    ]
    common_fields = ["trace", "window_ns", "banks_per_link_group"]
    write_csv(experiment_root / "summary.csv", summaries, summary_fields)
    write_csv(
        experiment_root / "all_group_statistics.csv",
        all_groups,
        common_fields + [
            "group",
            "total_requests",
            "active_windows",
            "contended_windows",
            "contention_fraction",
            "max_requests_in_window",
        ],
    )
    write_csv(
        experiment_root / "all_active_bank_histograms.csv",
        all_histograms,
        common_fields + ["active_banks", "window_count"],
    )
    write_csv(
        experiment_root / "all_bank_statistics.csv",
        all_banks,
        common_fields + ["bank", "reads", "writes", "total", "fraction"],
    )

    write_heatmap_svg(
        summaries,
        windows,
        group_sizes,
        "aggregate_contention_fraction",
        "Aggregate contention fraction",
        plots_directory / "contention_heatmap.svg",
        ".3f",
    )
    write_heatmap_svg(
        summaries,
        windows,
        group_sizes,
        "maximum_group_requests_in_window",
        "Maximum requests in a group/window",
        plots_directory / "maximum_group_requests_heatmap.svg",
        ".0f",
    )
    write_heatmap_svg(
        summaries,
        windows,
        group_sizes,
        "mean_active_banks",
        "Mean active banks per nonempty window",
        plots_directory / "mean_active_banks_heatmap.svg",
        ".2f",
    )
    write_line_plot_svg(
        summaries,
        windows,
        group_sizes,
        plots_directory / "contention_vs_group_size.svg",
    )

    print(f"Sweep complete: {experiment_root}")


if __name__ == "__main__":
    main()
