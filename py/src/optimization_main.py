from pathlib import Path
import os
import subprocess
import sys
from time import perf_counter

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import polars as pl


def main() -> None:
    print("\ngenerating optimization report...")
    start = perf_counter()

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    ASSETS_DIR.mkdir(parents=True, exist_ok=True)

    results = pl.read_csv(RESULTS_PATH)
    metadata = read_metadata()

    optimized_param_1 = metadata.get("optimized_param_1", metadata.get("optimized_param", "threshold"))
    optimized_param_2 = metadata.get("optimized_param_2", "none")

    if optimized_param_2 and optimized_param_2 != "none":
        plot_paths = plot_metric_heatmaps(results, optimized_param_1, optimized_param_2)
    else:
        plot_paths = plot_metric_lines(results, optimized_param_1)

    write_markdown_report(metadata, plot_paths)
    write_pdf_report()
    cleanup_markdown_report()

    runtime_ms = (perf_counter() - start) * 1000
    print(f"\noptimization report runtime: {runtime_ms:.0f}ms")


REPORT_DIR = Path("output/optimization")
ASSETS_DIR = REPORT_DIR / "_assets"
RESULTS_PATH = ASSETS_DIR / "optimization_results.csv"
METADATA_PATH = ASSETS_DIR / "metadata.csv"
REPORT_MD_PATH = REPORT_DIR / "opt_report.md"
REPORT_PDF_PATH = REPORT_DIR / "opt_report.pdf"
REPORT_CSS_PATH = Path("py/src/report/report_style.css")


METRICS = [
    ("net_profit", "Net Profit", "forestgreen"),
    ("avg_trade", "Average Trade", "deepskyblue"),
    ("sharpe", "Sharpe", "purple"),
    ("max_dd", "Max Drawdown", "indianred"),
    ("n_trades_per_year", "Trades / Year", "dimgray"),
]

HEATMAP_METRICS = [
    ("net_profit", "Net Profit"),
    ("avg_trade", "Average Trade"),
    ("sharpe", "Sharpe"),
    ("max_dd", "Max Drawdown (%)"),
    ("max_dd_not", "Max Drawdown Notional"),
    ("n_trades_per_year", "Trades / Year"),
]


def read_metadata() -> dict[str, str]:
    if not METADATA_PATH.exists():
        return {}

    metadata: dict[str, str] = {}
    for line in METADATA_PATH.read_text(encoding="utf-8").splitlines()[1:]:
        if not line:
            continue
        key, value = line.split(",", maxsplit=1)
        metadata[key] = value

    return metadata


def plot_metric_lines(results: pl.DataFrame, optimized_param: str) -> list[Path]:
    line_data = (
        results
        .sort(optimized_param)
    )

    x_values = line_data[optimized_param].to_list()
    plot_paths = []

    for metric, label, color in METRICS:
        out_path = ASSETS_DIR / f"optimization_{metric}.png"
        fig, ax = plt.subplots(1, 1, figsize=(9, 2.5))
        values = [float(value) for value in line_data[metric].to_list()]

        if metric == "max_dd" and "max_dd_not" in line_data.columns:
            not_values = [float(value) for value in line_data["max_dd_not"].to_list()]

            pct_line = ax.plot(
                x_values,
                values,
                marker="o",
                markersize=1,
                linewidth=2,
                color=color,
                label="Max Drawdown (%)",
            )
            ax.set_title("Max Drawdown")
            ax.set_xlabel(optimized_param)
            ax.set_ylabel("Max Drawdown (%)", color=color)
            ax.tick_params(axis="y", labelcolor=color)
            ax.grid(True, alpha=0.3)

            ax_not = ax.twinx()
            not_line = ax_not.plot(
                x_values,
                not_values,
                marker="o",
                markersize=1,
                linewidth=2,
                color="firebrick",
                linestyle="--",
                label="Max Drawdown Notional",
            )
            ax_not.set_ylabel("Max Drawdown Notional", color="firebrick")
            ax_not.tick_params(axis="y", labelcolor="firebrick")

            lines = pct_line + not_line
            labels = [line.get_label() for line in lines]
            ax.legend(lines, labels, loc="best")
        else:
            ax.plot(
                x_values,
                values,
                marker="o",
                markersize=1,
                linewidth=2,
                color=color,
                label=label,
            )
            ax.set_title(label)
            ax.set_xlabel(optimized_param)
            ax.set_ylabel(label, color=color)
            ax.tick_params(axis="y", labelcolor=color)
            ax.grid(True, alpha=0.3)
            ax.legend(loc="best")

        fig.tight_layout()
        fig.savefig(out_path, dpi=150)
        plt.close(fig)

        plot_paths.append(out_path)

    return plot_paths


def plot_metric_heatmaps(
    results: pl.DataFrame,
    optimized_param_1: str,
    optimized_param_2: str,
) -> list[Path]:
    line_data = results.sort([optimized_param_2, optimized_param_1])
    x_values = sorted(line_data[optimized_param_1].unique().to_list())
    y_values = sorted(line_data[optimized_param_2].unique().to_list())

    x_idx = {value: idx for idx, value in enumerate(x_values)}
    y_idx = {value: idx for idx, value in enumerate(y_values)}
    plot_paths = []

    for metric, label in HEATMAP_METRICS:
        if metric not in line_data.columns:
            continue

        out_path = ASSETS_DIR / f"optimization_{metric}.png"
        grid = np.full((len(y_values), len(x_values)), np.nan)

        for row in line_data.select([optimized_param_1, optimized_param_2, metric]).iter_rows(named=True):
            x = row[optimized_param_1]
            y = row[optimized_param_2]
            grid[y_idx[y], x_idx[x]] = float(row[metric])

        fig, ax = plt.subplots(1, 1, figsize=(9, 4.8))
        image = ax.imshow(
            grid,
            origin="lower",
            aspect="auto",
            cmap="RdYlGn",
        )

        ax.set_title(label)
        ax.set_xlabel(optimized_param_1)
        ax.set_ylabel(optimized_param_2)

        set_heatmap_ticks(ax, x_values, y_values)
        fig.colorbar(image, ax=ax, label=label)

        fig.tight_layout()
        fig.savefig(out_path, dpi=150)
        plt.close(fig)

        plot_paths.append(out_path)

    return plot_paths


def set_heatmap_ticks(ax: plt.Axes, x_values: list[float], y_values: list[float], max_ticks: int = 10) -> None:
    def tick_positions(values: list[float]) -> list[int]:
        if len(values) <= max_ticks:
            return list(range(len(values)))
        return sorted(set(
            round(i * (len(values) - 1) / (max_ticks - 1))
            for i in range(max_ticks)
        ))

    x_positions = tick_positions(x_values)
    y_positions = tick_positions(y_values)

    ax.set_xticks(x_positions)
    ax.set_xticklabels([format_tick(x_values[idx]) for idx in x_positions], rotation=45, ha="right")
    ax.set_yticks(y_positions)
    ax.set_yticklabels([format_tick(y_values[idx]) for idx in y_positions])


def format_tick(value: float) -> str:
    if float(value).is_integer():
        return str(int(value))
    return f"{value:.2f}".rstrip("0").rstrip(".")


def write_markdown_report(metadata: dict[str, str], plot_paths: list[Path]) -> None:
    optimized_param = metadata.get("optimized_param_1", metadata.get("optimized_param", ""))
    optimized_param_2 = metadata.get("optimized_param_2", "none")
    has_second_param = bool(optimized_param_2 and optimized_param_2 != "none")
    fast_len = metadata.get("fast_len", metadata.get("mac_fast_len", ""))
    slow_len = metadata.get("slow_len", metadata.get("mac_slow_len", ""))
    best_param = metadata.get("best_value_1", metadata.get(f"best_{optimized_param}", ""))
    best_param_2 = metadata.get("best_value_2", "")
    best_sharpe = metadata.get("best_sharpe", "")

    lines = [
        "# Optimization Report",
        "",
        "| Field | Value |",
        "|---|---|",
        f"| Strategy | {metadata.get('strategy', '')} |",
        f"| Data | {metadata.get('data', '')} |",
        f"| Data mode | {metadata.get('data_mode', '')} |",
        f"| IS start date | {metadata.get('is_start_date', '')} |",
        f"| IS end date | {metadata.get('is_end_date', '')} |",
        f"| IS optimization pct | {metadata.get('is_optimization_pct', '')} |",
        f"| OOS test pct | {metadata.get('oos_test_pct', '')} |",
        f"| Optimized parameter 1 | {optimized_param} |",
        f"| Fast length | {fast_len} |",
        f"| Slow length | {slow_len} |",
        f"| Objective | {metadata.get('objective', '')} |",
        f"| Best {optimized_param} | {best_param} |",
        f"| Sharpe | {best_sharpe} |",
        "",
        "## Metric Plots",
        "",
    ]

    if has_second_param:
        insert_idx = lines.index(f"| Fast length | {fast_len} |")
        lines.insert(insert_idx, f"| Optimized parameter 2 | {optimized_param_2} |")
        lines.insert(lines.index(f"| Sharpe | {best_sharpe} |"), f"| Best {optimized_param_2} | {best_param_2} |")

    for plot_path in plot_paths:
        lines.extend([
            f"![](_assets/{plot_path.name})",
            "",
        ])

    REPORT_MD_PATH.write_text("\n".join(lines), encoding="utf-8")


def write_pdf_report() -> None:
    env = os.environ.copy()
    venv_bin = Path(sys.executable).parent
    env["PATH"] = f"{venv_bin}{os.pathsep}{env.get('PATH', '')}"

    proc = subprocess.run(
        [
            "pandoc",
            REPORT_MD_PATH.name,
            "-o",
            REPORT_PDF_PATH.name,
            "--pdf-engine=weasyprint",
            "--css",
            str(REPORT_CSS_PATH.resolve()),
            "--quiet",
        ],
        cwd=REPORT_DIR,
        env=env,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip())


def cleanup_markdown_report() -> None:
    REPORT_MD_PATH.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
