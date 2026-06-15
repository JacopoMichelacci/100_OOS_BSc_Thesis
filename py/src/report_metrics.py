from pathlib import Path
from datetime import datetime
from dataclasses import dataclass

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import polars as pl


@dataclass(frozen=True)
class MetricsConfig:
    """Controls which metrics/assets are produced by Metrics.run()."""

    mean_returns: bool = True
    avg_trade: bool = True
    sharpe: bool = True
    max_drawdown: bool = True
    skewness: bool = True
    kurtosis: bool = True
    equity_curve_plot: bool = True
    plot_equity_curve_cfg: str = "all"
    periods_per_year_override: float = -1.0
    trading_days_inferred: float = 252.0



class Metrics:
    """
    Computes backtest metrics and plot assets from C++ backtester CSV outputs.

    The class owns cleaned in-memory copies of the equity curve and order log.
    Derived series such as returns and drawdowns are computed once and reused by
    metric methods. Calling run() executes the enabled metrics from MetricsConfig
    and returns a flat report dictionary suitable for printing or later export.
    """

    def __init__(
        self,
        equity_path: Path,
        orders_path: Path,
        config: MetricsConfig,
        assets_dir: Path,
    ) -> None:
        self.equity_path = equity_path
        self.orders_path = orders_path
        self.config = config
        self.assets_dir = assets_dir

        self.equity = pl.read_csv(self.equity_path)
        self.orders = pl.read_csv(self.orders_path)
        self._clean_equity()
        self._add_equity_columns()
        self.periods_per_year = self._resolve_periods_per_year()

        self.report: dict[str, float | int | str] = {}

    def _clean_equity(self) -> None:
        """Drop invalid rows from the in-memory equity curve only."""
        self.equity = self.equity.filter(
            pl.col("ts").is_not_null()
            & pl.col("equity").is_not_null()
            & pl.col("equity").is_not_nan()
        )

    def _add_equity_columns(self) -> None:
        """Add reusable return and drawdown columns to the equity curve."""
        # Return columns are kept on self.equity so every metric uses one definition.
        self.equity = self.equity.with_columns(
            pl.col("equity").pct_change().alias("eq_ret"),
            pl.col("equity").diff().alias("eq_ret_not"),
        )

        # Drawdown columns are also stored once and reused by metrics and plots.
        self.equity = self.equity.with_columns(
            pl.col("equity").cum_max().alias("equity_peak")
        ).with_columns(
            (pl.col("equity") - pl.col("equity_peak")).alias("dd_not"),
            (pl.col("equity") / pl.col("equity_peak") - 1.0).alias("dd_pct"),
        )

    def _resolve_periods_per_year(self) -> float:
        """Use an explicit annualization override when provided, otherwise infer it."""
        if self.config.periods_per_year_override > 0:
            return self.config.periods_per_year_override

        return self._infer_periods_per_year()

    def _infer_periods_per_year(self) -> float:
        """Infer sampling frequency from the median timestamp gap."""
        if self.equity.height < 2:
            raise ValueError("cannot infer periods_per_year: equity curve has fewer than 2 rows")

        ts_diffs = self.equity["ts"].diff().drop_nulls().drop_nans()
        if ts_diffs.is_empty():
            raise ValueError("cannot infer periods_per_year: timestamp differences are empty")

        median_diff_ms = ts_diffs.median()
        if median_diff_ms is None:
            raise ValueError("cannot infer periods_per_year: median timestamp difference is null")
        
        if median_diff_ms <= 0:
            raise ValueError(
                f"cannot infer periods_per_year: non-positive median timestamp difference {median_diff_ms}"
            )

        year_ms = self.config.trading_days_inferred * 24 * 60 * 60 * 1000
        return year_ms / median_diff_ms

    def run(self) -> dict[str, float | int | str]:
        """Run enabled metrics and return the collected report values."""
        self.mean_returns()
        self.avg_trade()
        self.sharpe()
        self.max_drawdown()
        self.skewness()
        self.kurtosis()
        self.plot_equity_curve()

        return self.report

    def mean_returns(self) -> None:
        """Add average percentage and notional equity returns to the report."""
        if not self.config.mean_returns:
            return

        self.report["mean_return_pct"] = self.equity["eq_ret"].drop_nulls().drop_nans().mean() * 100.0
        self.report["mean_return_notional"] = self.equity["eq_ret_not"].drop_nulls().drop_nans().mean()

    def avg_trade(self) -> None:
        """Add net profit, trade count, and average profit per filled order."""
        if not self.config.avg_trade:
            return

        n_trades = self._filled_order_count()
        net_profit = self._net_profit()

        self.report["n_trades"] = n_trades
        self.report["net_profit"] = net_profit
        self.report["avg_trade"] = net_profit / n_trades if n_trades else 0.0

    def _filled_order_count(self) -> int:
        """Count filled orders from the order log."""
        if self.orders.is_empty() or "status" not in self.orders.columns:
            return 0

        return self.orders.filter(pl.col("status") == "filled").height

    def _net_profit(self) -> float:
        """Compute final equity minus initial equity."""
        if self.equity.is_empty():
            return 0.0

        return float(self.equity["equity"][-1] - self.equity["equity"][0])

    def sharpe(self) -> None:
        """Add annualized Sharpe based on inferred or overridden periods per year."""
        if not self.config.sharpe:
            return

        eq_ret = self.equity["eq_ret"].drop_nulls().drop_nans()
        mean_return = eq_ret.mean()
        std_return = eq_ret.std()
        if std_return is None or std_return == 0.0:
            self.report["sharpe"] = 0.0
            return

        self.report["sharpe"] = (
            mean_return / std_return * self.periods_per_year ** 0.5
        )

    def max_drawdown(self) -> None:
        """Add max drawdown in notional and percentage terms."""
        if not self.config.max_drawdown:
            return

        self.report["max_dd_not"] = self.equity["dd_not"].min()
        self.report["max_dd_pct"] = self.equity["dd_pct"].min() * 100.0

    def skewness(self) -> None:
        """Add skewness of percentage returns."""
        if not self.config.skewness:
            return

        value = self.equity["eq_ret"].drop_nulls().drop_nans().skew()
        self.report["return_skewness"] = 0.0 if value is None else value

    def kurtosis(self) -> None:
        """Add excess kurtosis of percentage returns."""
        if not self.config.kurtosis:
            return

        value = self.equity["eq_ret"].drop_nulls().drop_nans().kurtosis()
        self.report["return_kurtosis"] = 0.0 if value is None else value

    def plot_equity_curve(self, tick_count: int = 8) -> None:
        """Save an equity curve plot using the equity timestamps as the x-axis."""
        if not self.config.equity_curve_plot:
            return

        dd_cfg = self._validate_equity_curve_dd_cfg(self.config.plot_equity_curve_cfg)

        out_path = self.assets_dir / "equity_curve.png"
        timestamps = [
            datetime.fromtimestamp(ts / 1000.0)
            for ts in self.equity["ts"].to_list()
        ]

        plot_rows = 1
        if dd_cfg in {"ddnot", "ddpct"}:
            plot_rows = 2
        elif dd_cfg == "all":
            plot_rows = 3

        fig, axes = plt.subplots(
            plot_rows,
            1,
            figsize=(10, 3.2 * plot_rows),
            sharex=True,
        )
        if plot_rows == 1:
            axes = [axes]

        equity_ax = axes[0]
        equity_ax.plot(timestamps, self.equity["equity"].to_list(), linewidth=1.2)
        equity_ax.set_title("Equity Curve")
        equity_ax.set_ylabel("Equity")
        equity_ax.grid(True, alpha=0.3)

        row = 1
        if dd_cfg in {"ddnot", "all"}:
            self._plot_drawdown_axis(
                axes[row],
                timestamps,
                self.equity["dd_not"].to_list(),
                "Drawdown Notional",
                "Drawdown",
            )
            row += 1

        if dd_cfg in {"ddpct", "all"}:
            self._plot_drawdown_axis(
                axes[row],
                timestamps,
                self.equity["dd_pct"].to_list(),
                "Drawdown %",
                "Drawdown %",
            )

        bottom_ax = axes[-1]
        self._set_date_ticks(bottom_ax, timestamps, tick_count)
        bottom_ax.set_xlim(timestamps[0], timestamps[-1])
        bottom_ax.set_xlabel("Date")
        fig.tight_layout()
        fig.savefig(out_path, dpi=150)
        plt.close(fig)

        self.report["equity_curve_plot"] = str(out_path)

    def _validate_equity_curve_dd_cfg(self, dd_cfg: str) -> str:
        """Validate drawdown subplot mode for the equity curve plot."""
        valid = {"clean", "ddpct", "ddnot", "all"}
        if dd_cfg not in valid:
            raise ValueError(
                f"invalid equity_curve_dd_cfg {dd_cfg!r}; expected one of {sorted(valid)}"
            )

        return dd_cfg

    def _plot_drawdown_axis(
        self,
        ax: plt.Axes,
        timestamps: list[datetime],
        drawdown: list[float],
        title: str,
        ylabel: str,
    ) -> None:
        """Plot a red drawdown line and fill the area up to zero."""
        ax.plot(timestamps, drawdown, color="red", linewidth=1.0)
        ax.fill_between(timestamps, drawdown, 0, color="red", alpha=0.2)
        ax.axhline(0.0, color="black", linewidth=0.8, alpha=0.6)
        ax.set_title(title)
        ax.set_ylabel(ylabel)
        ax.grid(True, alpha=0.3)

    def _set_date_ticks(self, ax: plt.Axes, timestamps: list[datetime], xtick_count: int) -> None:
        """Show a fixed number of readable date ticks for the plotted date span."""
        if not timestamps:
            return

        xtick_count = max(1, min(xtick_count, len(timestamps)))
        if xtick_count == 1:
            tick_indices = [0]
        else:
            tick_indices = [
                round(i * (len(timestamps) - 1) / (xtick_count - 1))
                for i in range(xtick_count)
            ]

        tick_dates = [timestamps[i] for i in tick_indices]
        date_span = timestamps[-1] - timestamps[0]

        if date_span.days >= 365 * xtick_count:
            date_format = "%Y"
        elif date_span.days >= 30 * xtick_count:
            date_format = "%b %Y"
        elif date_span.days >= xtick_count:
            date_format = "%b %d"
        else:
            date_format = "%d %Hh"

        ax.set_xticks(tick_dates)
        ax.set_xticklabels([dt.strftime(date_format) for dt in tick_dates])
