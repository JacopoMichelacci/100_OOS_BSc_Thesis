from datetime import datetime
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
    print("\ngenerating synthetic comparison report...")
    start = perf_counter()

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    ASSETS_DIR.mkdir(parents=True, exist_ok=True)

    metadata = read_metadata()
    plot_path = plot_price_paths()

    write_markdown_report(metadata, plot_path)
    write_pdf_report()
    cleanup_markdown_report()

    runtime_ms = (perf_counter() - start) * 1000
    print(f"\nsynthetic comparison report runtime: {runtime_ms:.0f}ms")


REPORT_DIR = Path("output/synthetic_compare")
ASSETS_DIR = REPORT_DIR / "_assets"
REAL_PATH = ASSETS_DIR / "real_path.csv"
METADATA_PATH = ASSETS_DIR / "metadata.csv"
REPORT_MD_PATH = REPORT_DIR / "synthetic_compare_report.md"
REPORT_PDF_PATH = REPORT_DIR / "synthetic_compare_report.pdf"
REPORT_CSS_PATH = Path("py/src/report/report_style.css")

SYNTH_FILES = [
    ("GBM", ASSETS_DIR / "synthetic_paths_gbm.csv"),
    ("GBM Rolling Vol", ASSETS_DIR / "synthetic_paths_gbm_rolling_vol.csv"),
    ("GBM Rolling Vol Threshold", ASSETS_DIR / "synthetic_paths_gbm_rolling_vol_threshold.csv"),
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


def ts_to_dt(values: list[int]) -> list[datetime]:
    return [datetime.fromtimestamp(value / 1000.0) for value in values]


def plot_price_paths() -> Path:
    out_path = ASSETS_DIR / "synthetic_price_paths.png"

    real = pl.read_csv(REAL_PATH)
    real_x = ts_to_dt(real["ts"].to_list())
    real_close = real["close"].to_list()

    fig, axes = plt.subplots(len(SYNTH_FILES), 1, figsize=(20, 30), sharex=True)

    for ax, (title, synth_path) in zip(axes, SYNTH_FILES):
        synth = pl.read_csv(synth_path)

        for path_id in synth["path_id"].unique().sort().to_list():
            path = synth.filter(pl.col("path_id") == path_id)
            ax.plot(
                ts_to_dt(path["ts"].to_list()),
                path["close"].to_list(),
                color="gray",
                alpha=0.22,
                lw=0.6,
            )

        ax.plot(
            real_x,
            real_close,
            color="red",
            alpha=0.8,
            linewidth=0.8,
            label="Real",
        )

        ax.set_title(title)
        ax.set_ylabel("Close")
        ax.grid(True, alpha=0.3)
        ax.legend()

    fig.autofmt_xdate()
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)

    return out_path


def write_markdown_report(metadata: dict[str, str], plot_path: Path) -> None:
    lines = [
        "# Synthetic Market Comparison",
        "",
        (
            f"Data: {metadata.get('data', '')} | "
            f"Iterations: {metadata.get('iterations', '')} | "
            f"Vol window: {metadata.get('vol_window', '')} | "
            f"Threshold multiplier: {metadata.get('threshold_mult', '')} | "
            f"Seed: {metadata.get('seed', '')}"
        ),
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
