from pathlib import Path
import os
import subprocess
import sys

from report_metrics import Metrics, MetricsConfig


REPORT_DIR = Path("output/backtest")
ASSETS_DIR = Path("output/backtest/_assets")
EQUITY_PATH = ASSETS_DIR / "equity.csv"
ORDERS_PATH = ASSETS_DIR / "orders.csv"
REPORT_MD_PATH = REPORT_DIR / "report.md"
REPORT_PDF_PATH = REPORT_DIR / "report.pdf"


def _format_value(value: float | int | str) -> str:
    if isinstance(value, float):
        return f"{value:.6g}"

    return str(value)


def write_markdown_report(report: dict[str, float | int | str]) -> None:
    REPORT_DIR.mkdir(parents=True, exist_ok=True)

    lines = [
        "# Backtest Report",
        "",
        "![Equity Curve](_assets/equity_curve.png)",
        "",
        "## Metrics",
        "",
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

    subprocess.run(
        [
            "pandoc",
            REPORT_MD_PATH.name,
            "-o",
            REPORT_PDF_PATH.name,
            "--pdf-engine=weasyprint",
        ],
        cwd=REPORT_DIR,
        env=env,
        check=True,
    )


def main() -> None:
    metrics = Metrics(
        EQUITY_PATH,
        ORDERS_PATH,
        MetricsConfig(),
        ASSETS_DIR,
    )
    report = metrics.run()
    write_markdown_report(report)
    write_pdf_report()



if __name__ == "__main__":
    main()
