from pathlib import Path
import os
import subprocess
import sys
from time import perf_counter

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import polars as pl


def main() -> None:
    print("\ngenerating optimization report...")
    start = perf_counter()

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    ASSETS_DIR.mkdir(parents=True, exist_ok=True)

    results = pl.read_csv(RESULTS_PATH)
    metadata = read_metadata()

    plot_path = plot_metric_lines(results, metadata.get("optimized_param", "threshold"))

    write_markdown_report(metadata, plot_path)
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


def plot_metric_lines(results: pl.DataFrame, optimized_param: str) -> Path:
    out_path = ASSETS_DIR / "optimization_metric_lines.png"
    line_data = (
        results
        .sort(optimized_param)
    )

    fig, axes = plt.subplots(len(METRICS), 1, figsize=(9, 7.6), sharex=True)
    x_values = line_data[optimized_param].to_list()

    for metric_ax, (metric, label, color) in zip(axes, METRICS):
        values = [float(value) for value in line_data[metric].to_list()]
        metric_ax.plot(
            x_values,
            values,
            marker="o",
            markersize=1,
            linewidth=2,
            color=color,
            label=label,
        )
        metric_ax.set_ylabel(label, color=color)
        metric_ax.tick_params(axis="y", labelcolor=color)
        metric_ax.grid(True, alpha=0.3)
        metric_ax.legend(loc="best")

    axes[0].set_title("Optimization Metrics")
    axes[-1].set_xlabel(optimized_param)

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)

    return out_path


def write_markdown_report(metadata: dict[str, str], plot_path: Path) -> None:
    optimized_param = metadata.get("optimized_param", "")
    fast_len = metadata.get("fast_len", "")
    slow_len = metadata.get("slow_len", "")
    best_param = metadata.get(f"best_{optimized_param}", "")
    best_sharpe = metadata.get("best_sharpe", "")

    lines = [
        "# Optimization Report",
        "",
        "| Field | Value |",
        "|---|---|",
        f"| Strategy | {metadata.get('strategy', '')} |",
        f"| Data | {metadata.get('data', '')} |",
        f"| IS start date | {metadata.get('is_start_date', '')} |",
        f"| IS end date | {metadata.get('is_end_date', '')} |",
        f"| IS split | {metadata.get('is_split_pct', '')} |",
        f"| Optimized parameter | {optimized_param} |",
        f"| Fast length | {fast_len} |",
        f"| Slow length | {slow_len} |",
        f"| Objective | {metadata.get('objective', '')} |",
        f"| Best {optimized_param} | {best_param} |",
        f"| Sharpe | {best_sharpe} |",
        "",
        "## Metric Line Plot",
        "",
        f"![](_assets/{plot_path.name})",
    ]

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
