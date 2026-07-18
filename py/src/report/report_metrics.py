from pathlib import Path
from datetime import datetime
from dataclasses import dataclass, field
from enum import Enum
from typing import Literal

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import numpy as np
import polars as pl


class DRAWDOWN_PLOT_MODE(Enum):
    """Controls which drawdown panels are included under the equity curve."""

    CLEAN = "clean"
    DD_PCT = "dd_pct"
    DD_NOT = "dd_not"
    ALL = "all"


class RESAMPLE(Enum):
    """Controls the equity frequency used for resamplable metrics and plots."""

    NOCHANGE = "nochange"
    DAILY = "daily"


class PLOT_MODE(Enum):
    """Controls which trade side contributes to a plot."""

    SHORT = -1
    DEFAULT = 0
    LONG = 1
    ALL = 2


@dataclass(frozen=True)
class RegressionResult:
    """OLS coefficients produced for one configured factor group."""

    alpha_pct: float
    betas: tuple[tuple[str, float], ...]
    r_squared: float


@dataclass(frozen=True)
class ReportItem:
    """One layout-aware report entry produced by Metrics.run()."""

    name: str
    value: float | int | str | RegressionResult
    importance: int
    kind: Literal["metric", "plot", "regression"]




@dataclass(frozen=True)
class MetricConfig:
    """Config for one scalar metric."""

    enabled: bool = True
    decimals: int = 2
    importance: int = 1


@dataclass(frozen=True)
class PlotConfig:
    """Common config for plot assets."""

    enabled: bool = True
    figsize_x: float = 12.0
    figsize_y: float = 4.8
    importance: int = 1
    mode: PLOT_MODE = PLOT_MODE.DEFAULT

@dataclass(frozen=True)
class EquityCurvePlotConfig:
    """Config for the equity curve plot asset."""

    base: PlotConfig = field(default_factory=PlotConfig)
    dd_mode: DRAWDOWN_PLOT_MODE = DRAWDOWN_PLOT_MODE.ALL


@dataclass(frozen=True)
class RegressionConfig:
    """Configures univariate and multivariate factor regressions."""

    enabled: bool = True
    factors: tuple[tuple[str, ...], ...] = ()
    plot_correlation: bool = True
    decimals: int = 4
    importance: int = 3

    
@dataclass(frozen=True)
class MetricsConfig:
    """Controls which metrics/assets are produced by Metrics.run()."""

    resample: RESAMPLE = RESAMPLE.NOCHANGE
    tot_ret_pct: MetricConfig = MetricConfig(enabled=True, decimals=2, importance=1)
    cagr: MetricConfig = MetricConfig(enabled=True, decimals=2, importance=1)
    mean_yearly_ret_pct: MetricConfig = MetricConfig(enabled=False, decimals=2, importance=1)
    mean_yearly_ret_not: MetricConfig = MetricConfig(enabled=False, decimals=1, importance=1)
    avg_trade: MetricConfig = MetricConfig(enabled=True, decimals=1, importance=1)
    sharpe: MetricConfig = MetricConfig(enabled=True, decimals=2, importance=1)
    max_drawdown: MetricConfig = MetricConfig(enabled=True, decimals=1, importance=1)
    cost_notional: MetricConfig = MetricConfig(enabled=True, decimals=2, importance=2)
    win_rate_pct: MetricConfig = MetricConfig(enabled=True, decimals=2, importance=2)
    skewness: MetricConfig = MetricConfig(enabled=False, decimals=2, importance=2)
    kurtosis: MetricConfig = MetricConfig(enabled=False, decimals=2, importance=2)
    plot_equity_curve_cfg: EquityCurvePlotConfig = field(default_factory=EquityCurvePlotConfig)
    plot_equity_ret_distr_cfg: PlotConfig = PlotConfig(
        enabled=True,
        figsize_x=9.0,
        figsize_y=5.0,
        importance=2,
    )
    plot_trade_ret_distr_cfg: PlotConfig = PlotConfig(
        enabled=True,
        figsize_x=9.0,
        figsize_y=5.0,
        importance=2,
    )
    regression: RegressionConfig = field(default_factory=RegressionConfig)
    periods_per_year_override: float = -1.0



class Metrics:
    """
    Computes backtest metrics and plot assets from C++ backtester CSV outputs.

    Input timestamp contract:
        The ``ts`` column in the equity and order CSV files must contain Unix
        epoch timestamps in milliseconds.

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
        cost_bps: float = 0.0,
        internal_data_dir: Path = Path("data/_internal"),
    ) -> None:
        self.equity_path = equity_path
        self.orders_path = orders_path
        self.config = config
        self.assets_dir = assets_dir
        self.cost_bps = cost_bps
        self.internal_data_dir = internal_data_dir

        self.equity_raw = pl.read_csv(
            self.equity_path,
            schema_overrides={
                "ts": pl.Int64,
                "equity": pl.Float64,
            },
        )
        self.equity = self._prepare_equity(self.equity_raw)
        self.orders = pl.read_csv(self.orders_path)
        self._add_equity_columns()
        self.trades = self._build_trades_from_orders()
        self.periods_per_year = self._resolve_periods_per_year()

        self.report: list[ReportItem] = []
        self._equity_ret_skewness: float | None = None
        self._equity_ret_kurtosis: float | None = None

    def run(self) -> list[ReportItem]:
        """Run enabled metrics and return the collected report values."""
        self.report.clear()

        self.tot_ret_pct()
        self.cagr()
        self.mean_yearly_ret_pct()
        self.mean_yearly_ret_not()
        self.avg_trade()
        self.sharpe()
        self.max_drawdown()
        self.cost_notional()
        self.win_rate_pct()
        self.plot_equity_curve()
        self.plot_equity_ret_distr()
        self.skewness()
        self.kurtosis()
        self.plot_trade_ret_distr()
        self.regression()

        return self.report

    def regression(self) -> None:
        """Regress daily strategy returns on each configured factor group."""
        cfg = self.config.regression
        if not cfg.enabled or not cfg.factors:
            return

        factor_files: dict[str, Path] = {}
        for path in self.internal_data_dir.rglob("*.parquet"):
            if path.stem in factor_files:
                raise ValueError(f"duplicate internal-data ticker: {path.stem}")
            factor_files[path.stem] = path

        strategy_returns = (
            self._resample_equity_daily(self._clean_equity(self.equity_raw))
            .with_columns(
                pl.from_epoch("ts", time_unit="ms").dt.date().alias("date"),
                pl.col("equity").pct_change().alias("strategy_return"),
            )
            .select("date", "strategy_return")
            .drop_nulls()
        )

        factor_returns: dict[str, pl.DataFrame] = {}
        for ticker in dict.fromkeys(ticker for group in cfg.factors for ticker in group):
            path = factor_files.get(ticker)
            if path is None:
                raise ValueError(f"internal data not found for ticker: {ticker}")

            column = f"factor_{ticker}"
            factor_returns[ticker] = (
                pl.read_parquet(path)
                .sort("date")
                .with_columns(pl.col("close").pct_change().alias(column))
                .select("date", column)
                .drop_nulls()
            )

        for factors in cfg.factors:
            if not factors:
                raise ValueError("regression factor groups cannot be empty")
            if len(set(factors)) != len(factors):
                raise ValueError(
                    f"regression factor group contains duplicates: {' + '.join(factors)}"
                )

            regression_data = strategy_returns
            factor_columns: list[str] = []
            for ticker in factors:
                column = f"factor_{ticker}"
                factor_columns.append(column)
                regression_data = regression_data.join(
                    factor_returns[ticker],
                    on="date",
                    how="inner",
                )

            regression_data = regression_data.drop_nans()
            if regression_data.height <= len(factors) + 1:
                raise ValueError(
                    f"not enough aligned observations for regression: {' + '.join(factors)}"
                )

            y = regression_data["strategy_return"].to_numpy()
            factor_matrix = regression_data.select(factor_columns).to_numpy()
            design_matrix = np.column_stack((np.ones(y.size), factor_matrix))
            coefficients, _, _, _ = np.linalg.lstsq(design_matrix, y, rcond=None)
            fitted = design_matrix @ coefficients
            residual_sum = float(np.sum((y - fitted) ** 2))
            total_sum = float(np.sum((y - y.mean()) ** 2))
            r_squared = 1.0 - residual_sum / total_sum if total_sum > 0.0 else 0.0

            result = RegressionResult(
                alpha_pct=round(float(coefficients[0]) * 100.0, cfg.decimals),
                betas=tuple(
                    (ticker, round(float(beta), cfg.decimals))
                    for ticker, beta in zip(factors, coefficients[1:], strict=True)
                ),
                r_squared=round(r_squared, cfg.decimals),
            )
            self.report.append(
                ReportItem(
                    " + ".join(factors),
                    result,
                    cfg.importance,
                    "regression",
                )
            )

        if cfg.plot_correlation:
            correlation_data = strategy_returns.rename(
                {"strategy_return": "Strategy"}
            )
            labels = ["Strategy"]
            for ticker, returns in factor_returns.items():
                correlation_data = correlation_data.join(
                    returns.rename({f"factor_{ticker}": ticker}),
                    on="date",
                    how="inner",
                )
                labels.append(ticker)

            correlation_data = correlation_data.drop_nans()
            if correlation_data.height < 2:
                raise ValueError(
                    "not enough common observations for the regression correlation plot"
                )

            correlation = np.corrcoef(
                correlation_data.select(labels).to_numpy(),
                rowvar=False,
            )
            masked_correlation = np.ma.masked_where(
                np.triu(np.ones_like(correlation, dtype=bool), k=1),
                correlation,
            )

            out_path = self.assets_dir / "regression_correlation.png"
            figure_size = max(6.0, len(labels) * 1.1)
            fig, ax = plt.subplots(figsize=(figure_size, figure_size))
            image = ax.imshow(
                masked_correlation,
                cmap="coolwarm",
                vmin=-1.0,
                vmax=1.0,
            )
            ax.set_title("Strategy and Factor Return Correlations")
            ax.set_xticks(range(len(labels)), labels=labels, rotation=45, ha="right")
            ax.set_yticks(range(len(labels)), labels=labels)

            for row in range(len(labels)):
                for column in range(row + 1):
                    value = correlation[row, column]
                    text_color = "white" if abs(value) >= 0.55 else "black"
                    ax.text(
                        column,
                        row,
                        f"{value:.2f}",
                        ha="center",
                        va="center",
                        color=text_color,
                        fontsize=8,
                    )

            fig.colorbar(image, ax=ax, fraction=0.046, pad=0.04)
            fig.tight_layout()
            fig.savefig(out_path, dpi=150)
            plt.close(fig)
            self.report.append(
                ReportItem(
                    "regression_correlation",
                    str(out_path),
                    cfg.importance,
                    "plot",
                )
            )

    def _prepare_equity(self, equity: pl.DataFrame) -> pl.DataFrame:
        """Clean equity with epoch-millisecond timestamps and apply resampling."""
        equity = self._clean_equity(equity)

        if self.config.resample == RESAMPLE.NOCHANGE:
            return equity

        if self.config.resample == RESAMPLE.DAILY:
            return self._resample_equity_daily(equity)

        raise ValueError(f"unknown equity resample mode: {self.config.resample}")

    def _clean_equity(self, equity: pl.DataFrame) -> pl.DataFrame:
        """Drop invalid rows from an equity curve dataframe."""
        return equity.filter(
            pl.col("ts").is_not_null()
            & pl.col("equity").is_not_null()
            & pl.col("equity").is_not_nan()
        )

    def _resample_equity_daily(self, equity: pl.DataFrame) -> pl.DataFrame:
        """Keep the last equity point for each calendar date."""
        return (
            equity
            .sort("ts")
            .with_columns(
                pl.from_epoch("ts", time_unit="ms").dt.date().alias("_date")
            )
            .group_by("_date", maintain_order=True)
            .agg(
                pl.col("ts").last().alias("ts"),
                pl.col("equity").last().alias("equity"),
            )
            .drop("_date")
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

        if self.equity.height < 2:
            raise ValueError("cannot infer periods_per_year: equity curve has fewer than 2 rows")

        first_ts = self.equity["ts"][0]
        last_ts = self.equity["ts"][-1]
        elapsed_ms = last_ts - first_ts
        if elapsed_ms == 0:
            raise ValueError(
                "cannot infer periods_per_year: equity curve has zero elapsed time"
            )

        milliseconds_per_year = 365.25 * 24 * 60 * 60 * 1000
        elapsed_years = elapsed_ms / milliseconds_per_year
        return (self.equity.height - 1) / elapsed_years

    def _round_metric(self, value: float | int | None, decimals: int) -> float:
        """Round a scalar metric while normalizing missing values to zero."""
        if value is None:
            return 0.0

        return round(float(value), decimals)

    def _build_trades_from_orders(self) -> pl.DataFrame:
        """Reconstruct completed round-trip trades from filled order events."""
        columns = ["trade_id", "entry_ts", "exit_ts", "side", "qty", "entry_price", "exit_price", "pnl", "pnl_pct"]
        if self.orders.is_empty():
            return pl.DataFrame(schema={name: pl.Float64 for name in columns})

        required = {"id", "ts", "signal", "qty", "price", "status"}
        if not required.issubset(set(self.orders.columns)):
            return pl.DataFrame(schema={name: pl.Float64 for name in columns})

        filled_orders = (
            self.orders
            .filter(pl.col("status") == "filled")
            .with_columns(
                pl.when(pl.col("signal").is_in(["bbuy", "long", "cover"]))
                .then(pl.col("qty"))
                .when(pl.col("signal").is_in(["bsell", "short", "sell"]))
                .then(-pl.col("qty"))
                .otherwise(0.0)
                .alias("signed_qty")
            )
            .filter(pl.col("signed_qty") != 0.0)
            .sort(["ts", "id"])
        )

        if "pid" in filled_orders.columns:
            entries = {
                int(row["id"]): row
                for row in (
                    filled_orders
                    .filter(pl.col("pid") == -1)
                    .iter_rows(named=True)
                )
            }
            exits = filled_orders.filter(pl.col("pid") != -1)

            trades: list[dict[str, float | int | str]] = []
            next_trade_id = 1

            for exit_row in exits.iter_rows(named=True):
                entry = entries.get(int(exit_row["pid"]))
                if entry is None:
                    continue

                side = "long" if entry["signal"] in ["bbuy", "long"] else "short"
                qty = float(exit_row["qty"])
                entry_price = float(entry["price"])
                exit_price = float(exit_row["price"])
                pnl = (
                    (exit_price - entry_price) * qty
                    if side == "long"
                    else (entry_price - exit_price) * qty
                )
                pnl_pct = pnl / (entry_price * qty) * 100.0 if entry_price else 0.0

                trades.append(
                    {
                        "trade_id": next_trade_id,
                        "entry_ts": int(entry["ts"]),
                        "exit_ts": int(exit_row["ts"]),
                        "side": side,
                        "qty": qty,
                        "entry_price": entry_price,
                        "exit_price": exit_price,
                        "pnl": pnl,
                        "pnl_pct": pnl_pct,
                    }
                )
                next_trade_id += 1

            return pl.DataFrame(trades)

        trades: list[dict[str, float | int | str]] = []
        open_lots: list[dict[str, float | int]] = []
        next_trade_id = 1

        for ts_raw, signed_qty_raw, price_raw in (
            filled_orders
            .select("ts", "signed_qty", "price")
            .iter_rows()
        ):
            ts = int(ts_raw)
            signed_qty = float(signed_qty_raw)
            price = float(price_raw)

            remaining_qty = signed_qty

            while open_lots and open_lots[0]["qty"] * remaining_qty < 0.0:
                lot = open_lots[0]
                close_qty = min(abs(float(lot["qty"])), abs(remaining_qty))
                side = "long" if lot["qty"] > 0 else "short"
                entry_price = float(lot["entry_price"])
                pnl = (
                    (price - entry_price) * close_qty
                    if side == "long"
                    else (entry_price - price) * close_qty
                )
                pnl_pct = pnl / (entry_price * close_qty) * 100.0 if entry_price else 0.0

                trades.append(
                    {
                        "trade_id": next_trade_id,
                        "entry_ts": int(lot["entry_ts"]),
                        "exit_ts": ts,
                        "side": side,
                        "qty": close_qty,
                        "entry_price": entry_price,
                        "exit_price": price,
                        "pnl": pnl,
                        "pnl_pct": pnl_pct,
                    }
                )
                next_trade_id += 1

                if abs(float(lot["qty"])) == close_qty:
                    remaining_qty += float(lot["qty"])
                    open_lots.pop(0)
                else:
                    lot["qty"] = float(lot["qty"]) + close_qty * (-1.0 if lot["qty"] > 0 else 1.0)
                    remaining_qty = 0.0

            if remaining_qty != 0.0:
                open_lots.append(
                {
                    "entry_ts": ts,
                    "qty": remaining_qty,
                    "entry_price": price,
                }
            )

        return pl.DataFrame(trades)

    def tot_ret_pct(self) -> None:
        """Add total percentage return from first to final equity."""
        cfg = self.config.tot_ret_pct
        if not cfg.enabled:
            return

        if self.equity.is_empty() or self.equity["equity"][0] == 0:
            value = 0.0
        else:
            value = (self.equity["equity"][-1] / self.equity["equity"][0] - 1.0) * 100.0

        self.report.append(
            ReportItem(
                "tot_ret_pct",
                self._round_metric(value, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def cagr(self) -> None:
        """Add compound annual growth rate using elapsed calendar time."""
        cfg = self.config.cagr
        if not cfg.enabled:
            return

        initial_equity = self.equity["equity"][0]
        final_equity = self.equity["equity"][-1]
        elapsed_years = (self.equity["ts"][-1] - self.equity["ts"][0]) / (
            365.25 * 24 * 60 * 60 * 1000
        )

        if initial_equity <= 0 or final_equity <= 0 or elapsed_years <= 0:
            value: float | str = "N/A"
        else:
            value = (
                (final_equity / initial_equity) ** (1.0 / elapsed_years) - 1.0
            ) * 100.0

        self.report.append(
            ReportItem(
                "cagr",
                value if isinstance(value, str) else self._round_metric(value, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def mean_yearly_ret_pct(self) -> None:
        """Add annualized average percentage return."""
        cfg = self.config.mean_yearly_ret_pct
        if not cfg.enabled:
            return

        value = self.equity["eq_ret"].drop_nulls().drop_nans().mean() * self.periods_per_year * 100.0
        self.report.append(
            ReportItem(
                "mean_yearly_ret_pct",
                self._round_metric(value, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def mean_yearly_ret_not(self) -> None:
        """Add annualized average notional return."""
        cfg = self.config.mean_yearly_ret_not
        if not cfg.enabled:
            return

        value = self.equity["eq_ret_not"].drop_nulls().drop_nans().mean() * self.periods_per_year
        self.report.append(
            ReportItem(
                "mean_yearly_ret_not",
                self._round_metric(value, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def avg_trade(self) -> None:
        """Add net profit, completed trade count, and average profit per trade."""
        cfg = self.config.avg_trade
        if not cfg.enabled:
            return

        n_trades = self.trades.height
        net_profit = self._net_profit()

        self.report.append(ReportItem("n_trades", n_trades, cfg.importance, "metric"))
        self.report.append(
            ReportItem(
                "net_profit",
                self._round_metric(net_profit, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )
        self.report.append(
            ReportItem(
                "avg_trade",
                self._round_metric(net_profit / n_trades if n_trades else 0.0, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def _net_profit(self) -> float:
        """Compute final equity minus initial equity."""
        if self.equity.is_empty():
            return 0.0

        return float(self.equity["equity"][-1] - self.equity["equity"][0])

    def sharpe(self) -> None:
        """Add annualized Sharpe based on inferred or overridden periods per year."""
        cfg = self.config.sharpe
        if not cfg.enabled:
            return

        eq_ret = self.equity["eq_ret"].drop_nulls().drop_nans()
        mean_return = eq_ret.mean()
        std_return = eq_ret.std()
        if std_return is None or std_return == 0.0:
            self.report.append(ReportItem("sharpe", 0.0, cfg.importance, "metric"))
            return

        self.report.append(
            ReportItem(
                "sharpe",
                self._round_metric(
                    mean_return / std_return * self.periods_per_year ** 0.5,
                    cfg.decimals,
                ),
                cfg.importance,
                "metric",
            )
        )

    def max_drawdown(self) -> None:
        """Add max drawdown in notional and percentage terms."""
        cfg = self.config.max_drawdown
        if not cfg.enabled:
            return

        self.report.append(
            ReportItem(
                "max_dd_not",
                self._round_metric(self.equity["dd_not"].min(), cfg.decimals),
                cfg.importance,
                "metric",
            )
        )
        self.report.append(
            ReportItem(
                "max_dd_pct",
                self._round_metric(self.equity["dd_pct"].min() * 100.0, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def cost_notional(self) -> None:
        """Add total transaction costs across all filled orders."""
        cfg = self.config.cost_notional
        if not cfg.enabled:
            return

        if self.orders.is_empty():
            value = 0.0
        else:
            traded_notional = (
                self.orders
                .filter(pl.col("status") == "filled")
                .select((pl.col("price") * pl.col("qty")).sum())
                .item()
            )
            value = float(traded_notional or 0.0) * self.cost_bps / 10_000.0

        self.report.append(
            ReportItem(
                "cost_notional",
                self._round_metric(value, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def win_rate_pct(self) -> None:
        """Add the percentage of completed round trips with positive gross PnL."""
        cfg = self.config.win_rate_pct
        if not cfg.enabled:
            return

        n_trades = self.trades.height
        n_wins = self.trades.filter(pl.col("pnl") > 0.0).height if n_trades else 0
        value = n_wins / n_trades * 100.0 if n_trades else 0.0

        self.report.append(
            ReportItem(
                "win_rate_pct",
                self._round_metric(value, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def skewness(self) -> None:
        """Add skewness of percentage returns."""
        cfg = self.config.skewness
        if not cfg.enabled:
            return

        if self._equity_ret_skewness is None:
            self._equity_ret_skewness = self.equity["eq_ret"].drop_nulls().drop_nans().skew()

        self.report.append(
            ReportItem(
                "return_skewness",
                self._round_metric(self._equity_ret_skewness, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def kurtosis(self) -> None:
        """Add excess kurtosis of percentage returns."""
        cfg = self.config.kurtosis
        if not cfg.enabled:
            return

        if self._equity_ret_kurtosis is None:
            self._equity_ret_kurtosis = self.equity["eq_ret"].drop_nulls().drop_nans().kurtosis()

        self.report.append(
            ReportItem(
                "return_kurtosis",
                self._round_metric(self._equity_ret_kurtosis, cfg.decimals),
                cfg.importance,
                "metric",
            )
        )

    def plot_equity_curve(self, tick_count: int = 8) -> None:
        """Save an equity curve plot using the equity timestamps as the x-axis."""
        cfg = self.config.plot_equity_curve_cfg
        base_cfg = cfg.base
        if not base_cfg.enabled:
            return
        if base_cfg.mode != PLOT_MODE.DEFAULT:
            raise ValueError(
                "plot_equity_curve only supports PLOT_MODE.DEFAULT"
            )

        out_path = self.assets_dir / "equity_curve.png"
        timestamps = [
            datetime.fromtimestamp(ts / 1000.0)
            for ts in self.equity["ts"]
        ]

        plot_rows = 1
        if cfg.dd_mode in {DRAWDOWN_PLOT_MODE.DD_NOT, DRAWDOWN_PLOT_MODE.DD_PCT}:
            plot_rows = 2
        elif cfg.dd_mode == DRAWDOWN_PLOT_MODE.ALL:
            plot_rows = 3

        eq_height = base_cfg.figsize_y
        dd_height = eq_height * 0.6
        height_ratios = [eq_height] + [dd_height] * (plot_rows - 1)

        fig, axes = plt.subplots(
            plot_rows,
            1,
            figsize=(base_cfg.figsize_x, sum(height_ratios)),
            gridspec_kw={"height_ratios": height_ratios},
            sharex=True,
        )
        if plot_rows == 1:
            axes = [axes]

        equity_ax = axes[0]
        equity_ax.plot(timestamps, self.equity["equity"], linewidth=1.2)
        equity_ax.set_title("Equity Curve")
        equity_ax.set_ylabel("Equity")
        equity_ax.grid(True, alpha=0.3)

        row = 1
        if cfg.dd_mode in {DRAWDOWN_PLOT_MODE.DD_NOT, DRAWDOWN_PLOT_MODE.ALL}:
            self._plot_drawdown_axis(
                axes[row],
                timestamps,
                self.equity["dd_not"],
                "Drawdown Notional",
                "Drawdown",
            )
            row += 1

        if cfg.dd_mode in {DRAWDOWN_PLOT_MODE.DD_PCT, DRAWDOWN_PLOT_MODE.ALL}:
            self._plot_drawdown_axis(
                axes[row],
                timestamps,
                self.equity["dd_pct"],
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

        self.report.append(
            ReportItem("equity_curve_plot", str(out_path), base_cfg.importance, "plot")
        )

    def plot_equity_ret_distr(self) -> None:
        """Save a histogram of equity curve bar returns."""
        cfg = self.config.plot_equity_ret_distr_cfg
        if not cfg.enabled:
            return
        if cfg.mode != PLOT_MODE.DEFAULT:
            raise ValueError(
                "plot_equity_ret_distr only supports PLOT_MODE.DEFAULT"
            )

        returns = self.equity["eq_ret"].drop_nulls().drop_nans() * 100.0
        if returns.is_empty():
            return

        out_path = self.assets_dir / "equity_return_distribution.png"
        self._equity_ret_skewness = returns.skew()
        self._equity_ret_kurtosis = returns.kurtosis()
        return_values = returns.to_numpy()
        fig, ax = plt.subplots(1, 1, figsize=(cfg.figsize_x, cfg.figsize_y))
        ax.hist(
            return_values,
            bins=50,
            density=True,
            color=(76 / 255, 120 / 255, 168 / 255, 0.85),
            edgecolor=(0.2, 0.2, 0.2, 0.35),
            linewidth=0.35,
        )
        if return_values.size > 1:
            std = return_values.std(ddof=1)
            bandwidth = 1.06 * std * return_values.size ** (-1 / 5) if std > 0 else 0.0
            if bandwidth > 0:
                x_grid = np.linspace(return_values.min(), return_values.max(), 300)
                density = np.exp(
                    -0.5 * ((x_grid[:, None] - return_values[None, :]) / bandwidth) ** 2
                ).sum(axis=1) / (return_values.size * bandwidth * np.sqrt(2 * np.pi))
                ax.plot(
                    x_grid,
                    density,
                    color="black",
                    linewidth=1.2,
                    label=(
                        f"skew={self._round_metric(self._equity_ret_skewness, 2)}, "
                        f"kurt={self._round_metric(self._equity_ret_kurtosis, 2)}"
                    ),
                )
        ax.axvline(0.0, color="black", linewidth=0.9, alpha=0.7)
        ax.set_title("Equity Return Distribution")
        ax.set_xlabel("Return (%)")
        ax.set_ylabel("Density")
        ax.legend()
        ax.grid(True, alpha=0.25)
        fig.tight_layout()
        fig.savefig(out_path, dpi=150)
        plt.close(fig)

        self.report.append(
            ReportItem("equity_return_distribution", str(out_path), cfg.importance, "plot")
        )

    def plot_trade_ret_distr(self) -> None:
        """Save a histogram of completed trade percentage returns."""
        cfg = self.config.plot_trade_ret_distr_cfg
        if not cfg.enabled:
            return

        plot_specs: list[tuple[str, str, str, pl.DataFrame]] = []
        if cfg.mode in {PLOT_MODE.DEFAULT, PLOT_MODE.ALL}:
            plot_specs.append((
                "trade_return_distribution",
                "trade_return_distribution.png",
                "Trade Return Distribution",
                self.trades,
            ))
        if cfg.mode in {PLOT_MODE.LONG, PLOT_MODE.ALL}:
            plot_specs.append((
                "long_trade_return_distribution",
                "long_trade_return_distribution.png",
                "Long Trade Return Distribution",
                self.trades.filter(pl.col("side") == "long")
                if not self.trades.is_empty() else self.trades,
            ))
        if cfg.mode in {PLOT_MODE.SHORT, PLOT_MODE.ALL}:
            plot_specs.append((
                "short_trade_return_distribution",
                "short_trade_return_distribution.png",
                "Short Trade Return Distribution",
                self.trades.filter(pl.col("side") == "short")
                if not self.trades.is_empty() else self.trades,
            ))
        if not plot_specs:
            raise ValueError(f"unknown plot mode: {cfg.mode}")

        for report_name, filename, title, trades in plot_specs:
            returns = trades["pnl_pct"].drop_nulls().drop_nans()
            out_path = self.assets_dir / filename
            fig, ax = plt.subplots(1, 1, figsize=(cfg.figsize_x, cfg.figsize_y))

            if returns.is_empty():
                ax.text(
                    0.5,
                    0.5,
                    "No completed trades",
                    ha="center",
                    va="center",
                    transform=ax.transAxes,
                )
            else:
                trade_skewness = returns.skew()
                trade_kurtosis = returns.kurtosis()
                return_values = returns.to_numpy()
                _, bin_edges, _ = ax.hist(
                    return_values,
                    bins=50,
                    color=(76 / 255, 120 / 255, 168 / 255, 0.85),
                    edgecolor=(0.2, 0.2, 0.2, 0.35),
                    linewidth=0.35,
                )
                if return_values.size > 1:
                    std = return_values.std(ddof=1)
                    bandwidth = 1.06 * std * return_values.size ** (-1 / 5) if std > 0 else 0.0
                    if bandwidth > 0:
                        x_grid = np.linspace(return_values.min(), return_values.max(), 300)
                        density = np.exp(
                            -0.5 * ((x_grid[:, None] - return_values[None, :]) / bandwidth) ** 2
                        ).sum(axis=1) / (
                            return_values.size * bandwidth * np.sqrt(2 * np.pi)
                        )
                        bin_width = bin_edges[1] - bin_edges[0]
                        density *= return_values.size * bin_width
                        ax.plot(
                            x_grid,
                            density,
                            color="black",
                            linewidth=1.2,
                            label=(
                                f"skew={self._round_metric(trade_skewness, 2)}, "
                                f"kurt={self._round_metric(trade_kurtosis, 2)}"
                            ),
                        )
                        ax.legend()
                ax.axvline(0.0, color="black", linewidth=0.9, alpha=0.7)

            ax.set_title(title)
            ax.set_xlabel("Return (%)")
            ax.set_ylabel("Trade Count")
            ax.yaxis.set_major_locator(MaxNLocator(integer=True))
            ax.grid(True, alpha=0.25)
            fig.tight_layout()
            fig.savefig(out_path, dpi=150)
            plt.close(fig)

            self.report.append(
                ReportItem(report_name, str(out_path), cfg.importance, "plot")
            )

    def _plot_drawdown_axis(
        self,
        ax: plt.Axes,
        timestamps: list[datetime],
        drawdown: pl.Series,
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
