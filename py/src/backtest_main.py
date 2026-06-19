from pathlib import Path
import os
import subprocess
import sys
from time import perf_counter

from report.report_metrics import (
    DRAWDOWN_PLOT_MODE,
    EquityCurvePlotConfig,
    Metrics,
    MetricsConfig,
    PLOT_MODE,
    PlotConfig,
    ReportItem,
)

############################################################################################################

def main() -> None:
    print("\ngenerating report...")
    start = perf_counter()

    metadata = read_metadata()
    cost_bps = float(resolve_cost_bps(metadata))
    metrics = Metrics(
        EQUITY_PATH,
        ORDERS_PATH,
        MetricsConfig(
            plot_equity_curve_cfg=EquityCurvePlotConfig(
                base=PlotConfig(mode=PLOT_MODE.DEFAULT),
                dd_mode=DRAWDOWN_PLOT_MODE.ALL,
            ),
            plot_equity_ret_distr_cfg=PlotConfig(
                importance=2,
                mode=PLOT_MODE.DEFAULT,
            ),
            plot_trade_ret_distr_cfg=PlotConfig(
                importance=2,
                mode=PLOT_MODE.DEFAULT,
            ),
        ),
        ASSETS_DIR,
        cost_bps=cost_bps,
    )
    report = metrics.run()
    metadata["resample"] = metrics.config.resample.name
    write_markdown_report(report, metadata)
    write_pdf_report()
    cleanup_markdown_report()

    runtime_ms = (perf_counter() - start) * 1000
    print(f"\nreport runtime: {runtime_ms:.0f}ms")

############################################################################################################

REPORT_DIR = Path("output/backtest")
ASSETS_DIR = Path("output/backtest/_assets")
EQUITY_PATH = ASSETS_DIR / "equity.csv"
ORDERS_PATH = ASSETS_DIR / "orders.csv"
METADATA_PATH = ASSETS_DIR / "metadata.csv"
REPORT_MD_PATH = REPORT_DIR / "report.md"
REPORT_PDF_PATH = REPORT_DIR / "report.pdf"
REPORT_CSS_PATH = Path("py/src/report/report_style.css")

CURRENCY_SYMBOLS = {
    "USD": "$",
    "EUR": "€",
    "GBP": "£",
    "JPY": "¥",
}

PCT_METRICS = {
    "tot_ret_pct",
    "cagr",
    "mean_yearly_ret_pct",
    "max_dd_pct",
    "win_rate_pct",
}
NOTIONAL_METRICS = {
    "mean_yearly_ret_not",
    "net_profit",
    "avg_trade",
    "max_dd_not",
    "cost_notional",
}

METRIC_LABELS = {
    "tot_ret_pct": "Total Return",
    "cagr": "CAGR",
    "mean_yearly_ret_pct": "Mean Annual Return",
    "mean_yearly_ret_not": "Mean Annual Profit",
    "n_trades": "Completed Trades",
    "net_profit": "Net Profit",
    "avg_trade": "Average Trade",
    "sharpe": "Sharpe Ratio",
    "max_dd_not": "Maximum Drawdown",
    "max_dd_pct": "Maximum Drawdown",
    "cost_notional": "Transaction Costs",
    "win_rate_pct": "Win Rate",
    "return_skewness": "Return Skewness",
    "return_kurtosis": "Return Kurtosis",
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


def _format_metric(item: ReportItem, currency_symbol: str) -> tuple[str, str]:
    label = METRIC_LABELS.get(item.name, item.name.replace("_", " ").title())

    if isinstance(item.value, str):
        return label, item.value

    if item.name in PCT_METRICS:
        return label, f"{_format_value(item.value)}%"

    if item.name in NOTIONAL_METRICS:
        return label, _format_notional_value(item.value, currency_symbol)

    return label, _format_value(item.value)


def resolve_currency(metadata: dict[str, str]) -> str:
    currency = metadata.get("currency", "").strip()
    if currency:
        return currency

    print("metadata currency missing; defaulting report currency to $")
    return "$"


def currency_symbol(currency: str) -> str:
    return CURRENCY_SYMBOLS.get(currency.upper(), currency)


def resolve_cost_bps(metadata: dict[str, str]) -> str:
    cost_bps = metadata.get("cost_bps", "").strip()
    if cost_bps:
        return cost_bps

    print("metadata cost_bps missing; defaulting report cost_bps to 0")
    return "0"


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
    currency_symbol_ = currency_symbol(currency)
    cost_bps = resolve_cost_bps(metadata)

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
        f"| Currency | {currency} ({_markdown_currency(currency_symbol_)}) |",
        f"| Cost per trade | {cost_bps} bps |",
        f"| Resample | {metadata.get('resample', '')} |",
        "",
        "</div>",
        "",
    ]

    importance_levels = sorted(
        {item.importance for item in report},
    )
    metrics_started = False
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
            lines.extend(["", f"![]({plot_path})"])

        if metrics:
            if not metrics_started:
                lines.extend(["", '<div class="metrics-page-break"></div>'])
                metrics_started = True
            lines.extend(["", f"## {section_title}", "", "| Metric | Value |", "|---|---:|"])
            for item in metrics:
                metric_name, metric_value = _format_metric(item, currency_symbol_)
                lines.append(f"| {metric_name} | {metric_value} |")

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


def cleanup_markdown_report() -> None:
    REPORT_MD_PATH.unlink(missing_ok=True)




if __name__ == "__main__":
    main()
