from pathlib import Path
import os
import subprocess
import sys
from time import perf_counter

from report.report_metrics import DRAWDOWN_PLOT_MODE, Metrics, MetricsConfig, ReportItem


REPORT_DIR = Path("output/backtest")
ASSETS_DIR = Path("output/backtest/_assets")
EQUITY_PATH = ASSETS_DIR / "equity.csv"
ORDERS_PATH = ASSETS_DIR / "orders.csv"
METADATA_PATH = ASSETS_DIR / "metadata.csv"
REPORT_MD_PATH = REPORT_DIR / "report.md"
REPORT_PDF_PATH = REPORT_DIR / "report.pdf"
REPORT_CSS_PATH = Path("py/src/report/report_style.css")

PCT_METRICS = {
    "mean_return_pct",
    "max_dd_pct",
}
NOTIONAL_METRICS = {
    "mean_return_notional",
    "net_profit",
    "avg_trade",
    "max_dd_not",
}


def _format_value(value: float | int | str) -> str:
    if isinstance(value, float):
        return f"{value:.6g}"

    return str(value)


def _markdown_currency(currency: str) -> str:
    if currency == "$":
        return r"\$"

    return currency


def _format_notional_value(value: float | int | str, currency: str) -> str:
    formatted = _format_value(value)
    currency = _markdown_currency(currency)
    if formatted.startswith("-"):
        return f"-{currency}{formatted[1:]}"

    return f"{currency}{formatted}"


def _format_metric(item: ReportItem, currency: str) -> tuple[str, str]:
    if item.name in PCT_METRICS:
        return item.name, f"{_format_value(item.value)}%"

    if item.name in NOTIONAL_METRICS:
        return item.name, _format_notional_value(item.value, currency)

    return item.name, _format_value(item.value)


def resolve_currency(metadata: dict[str, str]) -> str:
    currency = metadata.get("currency", "").strip()
    if currency:
        return currency

    print("metadata currency missing; defaulting report currency to $")
    return "$"


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
    report: list[ReportItem],
    metadata: dict[str, str],
) -> None:
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    currency = resolve_currency(metadata)

    lines = [
        "# Backtest Report",
        "",
        "## Metadata",
        '<div class="metadata-table">',
        "",
        "| Field | Value |",
        "|---|---|",
        f"| Strategy | {metadata.get('strat_name', '')} |",
        f"| Data | {metadata.get('data', '')} |",
        f"| Currency | {_markdown_currency(currency)} |",
        "",
        "</div>",
        "",
    ]

    importance_levels = sorted(
        {item.importance for item in report},
    )
    for section_idx, importance in enumerate(importance_levels):
        plots = [
            item for item in report
            if item.importance == importance and item.kind == "plot"
        ]
        metrics = [
            item for item in report
            if item.importance == importance and item.kind == "metric"
        ]

        section_title = "Key Metrics" if section_idx == 0 else "Metrics"

        for item in plots:
            plot_path = Path(str(item.value))
            if plot_path.is_relative_to(REPORT_DIR):
                plot_path = plot_path.relative_to(REPORT_DIR)
            lines.extend(["", f"**{item.name}**", "", f"![]({plot_path})"])

        if metrics:
            lines.extend(["", f"## {section_title}", "", "| Metric | Value |", "|---|---:|"])
            for item in metrics:
                metric_name, metric_value = _format_metric(item, currency)
                lines.append(f"| `{metric_name}` | {metric_value} |")

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
        MetricsConfig(
            plot_equity_curve_cfg=(DRAWDOWN_PLOT_MODE.ALL, 12.0, 4.8, 1),
        ),
        ASSETS_DIR,
    )
    report = metrics.run()
    metadata = read_metadata()
    write_markdown_report(report, metadata)
    write_pdf_report()

    runtime_ms = (perf_counter() - start) * 1000
    print(f"\nreport runtime: {runtime_ms:.0f}ms")


if __name__ == "__main__":
    main()
