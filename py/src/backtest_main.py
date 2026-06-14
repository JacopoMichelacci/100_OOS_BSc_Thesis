from pathlib import Path

from report_metrics import Metrics, MetricsConfig


ASSETS_DIR = Path("output/backtest/_assets")
EQUITY_PATH = ASSETS_DIR / "equity.csv"
ORDERS_PATH = ASSETS_DIR / "orders.csv"


def main() -> None:
    metrics = Metrics(
        EQUITY_PATH,
        ORDERS_PATH,
        MetricsConfig(),
        ASSETS_DIR,
    )
    report = metrics.run()
    print(report)


if __name__ == "__main__":
    main()
