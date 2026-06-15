from pathlib import Path
import os
import subprocess
import sys
from time import perf_counter

from report.report_metrics import Metrics, MetricsConfig


REPORT_DIR = Path("output/backtest")
ASSETS_DIR = Path("output/backtest/_assets")
EQUITY_PATH = ASSETS_DIR / "equity.csv"
ORDERS_PATH = ASSETS_DIR / "orders.csv"
METADATA_PATH = ASSETS_DIR / "metadata.csv"
REPORT_MD_PATH = REPORT_DIR / "report.md"
REPORT_PDF_PATH = REPORT_DIR / "report.pdf"
REPORT_CSS_PATH = Path("py/src/report/report_style.css")


def _format_value(value: float | int | str) -> str:
    if isinstance(value, float):
        return f"{value:.6g}"

    return str(value)


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


def write_markdown_report(
    report: dict[str, float | int | str],
    metadata: dict[str, str],
) -> None:
    REPORT_DIR.mkdir(parents=True, exist_ok=True)

    lines = [
        "# Backtest Report",
        "",
        "## Metadata",
        "| Field | Value |",
        "|---|---|",
        f"| Strategy | {metadata.get('strat_name', '')} |",
        f"| Data | {metadata.get('data', '')} |",
        "",
        "![Equity Curve](_assets/equity_curve.png)",
        "",
        "## Metrics",
        "| Metric | Value |",
        "|---|---:|",
    ]

    for key, value in report.items():
        if key.endswith("_plot"):
            continue
        lines.append(f"| `{key}` | {_format_value(value)} |")

    REPORT_MD_PATH.write_text("\n".join(lines), encoding="utf-8")


def write_pdf_report() -> None:
    REPORT_DIR.mkdir(parents=True, exist_ok=True)

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


def main() -> None:
    print("\ngenerating report...")
    start = perf_counter()

    metrics = Metrics(
        EQUITY_PATH,
        ORDERS_PATH,
        MetricsConfig(),
        ASSETS_DIR,
    )
    report = metrics.run()
    metadata = read_metadata()
    write_markdown_report(report, metadata)
    write_pdf_report()

    runtime_ms = (perf_counter() - start) * 1000
    print(f"report runtime: {runtime_ms:.0f}ms")


if __name__ == "__main__":
    main()
