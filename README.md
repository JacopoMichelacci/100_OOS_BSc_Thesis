# Backtesting and Synthetic Market Evaluation of Trading Strategies

This repository contains the research code for my bachelor thesis at the University of Bologna.

The project studies a specific question:

> Can strategy optimization on synthetic market paths reduce overfitting compared with standard
> historical in-sample optimization?

The software implements a small C++/Python research pipeline for:

- historical backtesting;
- parameter optimization;
- synthetic market generation;
- report and figure generation;
- IS/OOS comparison of optimized strategies.

The C++ side handles the heavy simulation work. The Python side handles plots, metrics, and PDF/PNG
reporting.

This is research software, not a production trading system. The current entry points are intentionally
simple and mostly configured directly in the C++ `*_main.cpp` files.

## Project idea

The benchmark workflow is the standard IS/OOS approach:

1. split the historical data chronologically;
2. optimize a strategy on the IS period;
3. freeze the selected parameters;
4. test the strategy on the real OOS period.

The alternative workflow uses synthetic markets:

1. use the IS data to generate multiple synthetic market paths;
2. optimize the strategy on average performance across those synthetic paths;
3. freeze the selected parameters;
4. test the strategy on the same real OOS period.

The main comparison is the deterioration from optimization performance to OOS performance. If the
synthetic method has a smaller IS-OOS performance gap, it is interpreted as less exposed to
overfitting.

## Repository structure

```text
.
├── cpp/
│   ├── backtest_main.cpp              # C++ backtest entry point
│   ├── optimize_main.cpp              # C++ optimization entry point
│   ├── synthetic_main.cpp             # C++ synthetic path generation entry point
│   ├── include/
│   │   ├── backtest/                  # backtester and CSV output helpers
│   │   ├── core/                      # market events, orders, synthetic generator
│   │   ├── indicators/                # SMA, standard deviation, indicator base
│   │   ├── strategies/                # strategy base and built-in strategies
│   │   └── utils/                     # CSV loader, date filtering, timers
│   └── CMakeLists.txt
├── py/
│   └── src/
│       ├── backtest_main.py           # backtest report generation
│       ├── optimization_main.py       # optimization report generation
│       ├── synthetic_compare_main.py  # synthetic path figures/report
│       └── report/                    # shared reporting utilities
├── data/
│   └── _data/equity/                  # OHLCV CSV data used by the examples
├── output/                            # generated reports and CSV assets
├── thesis/                            # LaTeX thesis source and figures
├── runbt.sh                           # full backtest pipeline
├── runopt.sh                          # full optimization pipeline
└── runsyn.sh                          # full synthetic comparison pipeline
```

## Requirements

You need:

- CMake `>= 3.20`;
- a C++20 compiler;
- Python `>= 3.13`;
- [`uv`](https://docs.astral.sh/uv/) for Python dependency management.

Install Python dependencies:

```bash
uv sync --frozen
```

The C++ executables are built automatically by the run scripts.

## Quick start

From the repository root:

```bash
./runbt.sh
```

Runs one backtest and generates:

```text
output/backtest/
```

Then:

```bash
./runopt.sh
```

Runs the optimizer and generates:

```text
output/optimization/
```

Then:

```bash
./runsyn.sh
```

Generates synthetic paths and comparison figures in:

```text
output/synthetic_compare/
thesis/figures/
```

## Backtesting

The backtester operates on OHLCV bars.

Normal strategy orders are generated after a bar is known and filled at the next bar open. This avoids
lookahead bias: the strategy cannot observe a bar and trade at a price from that same bar.

Stop losses are treated differently. A stop is a protective order attached to an already open position.
If the bar high/low crosses the stop level, the stop is considered triggered inside that bar and filled
at the stop price.

The backtester tracks:

- cash;
- equity curve;
- net open position;
- individual open lots;
- order history;
- trade reconstruction.

Open lots are tracked separately, so stacked entries can have independent entry prices and independent
stop losses.

## Built-in strategies

The current built-in strategies are:

### Moving-average crossover

File:

```text
cpp/include/strategies/built_in/ma_cross.hpp
```

The strategy compares a fast moving average with a slow moving average. The price field used by each
average can be configured separately:

```cpp
PRICE_FIELD fast_price_field = PRICE_FIELD::CLOSE;
PRICE_FIELD slow_price_field = PRICE_FIELD::CLOSE;
```

Supported fields are:

```cpp
OPEN, HIGH, LOW, CLOSE, VOLUME
```

### Standard-deviation mean reversion

File:

```text
cpp/include/strategies/built_in/std_mrev.hpp
```

The strategy computes the latest price move and compares it with a rolling standard deviation of recent
moves. Large positive moves can trigger short entries, while large negative moves can trigger long
entries.

## Indicators

Current indicators:

- simple moving average;
- rolling standard deviation.

Indicator code is in:

```text
cpp/include/indicators/
```

## Optimization

The optimizer is currently configured in:

```text
cpp/optimize_main.cpp
```

The current example runs a grid search on the moving-average crossover stop-loss percentage. It records
for each parameter value:

- net profit;
- average trade;
- Sharpe ratio;
- maximum drawdown;
- number of trades;
- trades per year.

The C++ optimizer writes raw CSV files to:

```text
output/optimization/_assets/
```

Python then turns those CSVs into plots and a report.

The C++ optimization code is compiled in release mode by `cpp/runopt.sh`.

## Synthetic market generation

Synthetic path generation is implemented in:

```text
cpp/include/core/synthetic_market_generator.hpp
cpp/synthetic_main.cpp
```

Current generators:

1. basic GBM;
2. rolling-volatility GBM;
3. rolling-volatility GBM with threshold constraints.

The threshold-constrained generator is the most relevant one for the thesis. It keeps randomness, but
anchors the generated path around the original market path by construction.

The synthetic comparison pipeline writes:

```text
output/synthetic_compare/_assets/synthetic_paths_gbm.csv
output/synthetic_compare/_assets/synthetic_paths_gbm_rolling_vol.csv
output/synthetic_compare/_assets/synthetic_paths_gbm_rolling_vol_threshold.csv
```

and PNG figures:

```text
output/synthetic_compare/_assets/synthetic_gbm.png
output/synthetic_compare/_assets/synthetic_gbm_rolling_vol.png
output/synthetic_compare/_assets/synthetic_gbm_rolling_vol_threshold.png
```

The same figures are also copied to:

```text
thesis/figures/
```

## Changing the experiment

For now, configuration is mostly done by editing the C++ entry points.

Useful files:

```text
cpp/backtest_main.cpp      # choose data, date window/OOS split, strategy, backtest settings
cpp/optimize_main.cpp      # choose optimization grid, strategy, metrics, objective
cpp/synthetic_main.cpp     # choose synthetic generator settings and number of paths
```

Typical things to change:

- `data_path`;
- `start_date` / `end_date`;
- `oos_test_pct`;
- strategy parameters;
- strategy type;
- optimization grid;
- transaction cost in basis points;
- number of synthetic paths;
- rolling volatility window;
- threshold multiplier.

After changing one of these files, rerun the relevant root script.

## Data format

The CSV loader expects OHLCV rows with:

```text
timestamp,open,high,low,close,volume
```

The examples currently use Yahoo Finance-style equity data stored in:

```text
data/_data/equity/
```

## Outputs

Generated outputs are written under:

```text
output/
```

Main folders:

```text
output/backtest/
output/optimization/
output/synthetic_compare/
```

Each report folder contains an `_assets/` subfolder with the raw CSV files used by the Python reporting
scripts.

## Thesis

The LaTeX thesis source is in:

```text
thesis/
```

The thesis discusses:

- the overfitting problem in strategy optimization;
- IS/OOS testing;
- the C++/Python backtesting framework;
- synthetic market generation;
- comparison between historical optimization and synthetic-market optimization.

## Limitations

Important limitations:

- configuration is currently hardcoded in the C++ entry points;
- the optimizer is grid-based and not parallelized;
- the synthetic generators are experimental;
- intrabar price paths are unknown because the backtester uses OHLCV bars;
- stop-loss fills are approximated using high/low crossing logic;
- this is research code, not live trading infrastructure.

## License

See `LICENSE`.
