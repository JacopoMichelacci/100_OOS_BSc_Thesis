# Strategy Optimization via Synthetic Market Generation

This repository contains the code for my bachelor's thesis at the University of Bologna.

The project studies one main question:

> Can optimizing trading strategies on synthetic market paths reduce overfitting compared with the
> standard historical IS/OOS workflow?

The repository implements a C++/Python research pipeline for:

- historical backtesting;
- grid-based parameter optimization;
- synthetic market generation;
- report and figure generation;
- IS/OOS comparison of optimized strategies.

The C++ side handles the simulation-heavy work. The Python side handles plotting, metrics, and report
generation. This is research software, not live trading infrastructure.

## Research idea

The benchmark workflow is the standard IS/OOS approach:

1. split the historical data chronologically;
2. optimize a strategy on the IS period;
3. freeze the selected parameters;
4. test those parameters on the real OOS period.

The alternative workflow uses synthetic markets:

1. generate multiple synthetic paths from the IS data;
2. optimize the strategy on the mean performance across those synthetic IS paths;
3. freeze the selected parameters;
4. test those parameters on the same real OOS period.

The main comparison is the deterioration from IS performance to OOS performance:

```text
Diff = OOS - IS
```

If the synthetic workflow produces a smaller deterioration, it is interpreted as less exposed to
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
│   │   ├── core/                      # market events, orders, price fields, synthetic generator
│   │   ├── indicators/                # moving average, standard deviation, indicator base
│   │   ├── strategies/                # strategy base and built-in strategies
│   │   └── utils/                     # CSV loading, data windows, timers
│   └── CMakeLists.txt
├── py/
│   └── src/
│       ├── backtest_main.py           # backtest report generation
│       ├── optimization_main.py       # optimization report generation
│       ├── synthetic_compare_main.py  # synthetic path plots/report
│       └── report/                    # shared reporting utilities
├── data/
│   └── _data/equity/                  # OHLCV CSV data used by the examples
├── output/                            # generated reports and CSV assets
├── thesis/                            # LaTeX thesis source and thesis figures
├── runbt.sh                           # root backtest pipeline
├── runopt.sh                          # root optimization pipeline
└── runsyn.sh                          # root synthetic comparison pipeline
```

There are also equivalent run scripts under `cpp/`, but the root scripts are the intended entry
points.

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

Runs the configured backtest and generates:

```text
output/backtest/
```

Then:

```bash
./runopt.sh
```

Runs the configured optimizer and generates:

```text
output/optimization/
```

Then:

```bash
./runsyn.sh
```

Generates synthetic paths and comparison plots:

```text
output/synthetic_compare/
thesis/figures/
```

## Backtesting model

The backtester operates on OHLCV bars.

Normal strategy orders are generated after a bar is known and filled at the next bar open. This avoids
lookahead bias: the strategy cannot observe one bar and trade at a price from that same bar.

Stop losses are treated as protective orders attached to open lots. If the bar high/low crosses the
stop level, the stop is considered triggered inside that bar and filled using the stop-loss logic.

The backtester tracks:

- cash;
- equity curve;
- net open position;
- individual open lots;
- order history;
- reconstructed closed trades.

Open lots are tracked separately, so stacked entries can have independent entry prices and independent
stop losses.

## Built-in strategies

### Moving-average crossover

File:

```text
cpp/include/strategies/built_in/ma_cross.hpp
```

The strategy compares a fast moving average with a slow moving average. The price field used by each
average can be configured separately.

### Standard-deviation mean reversion

File:

```text
cpp/include/strategies/built_in/std_mrev.hpp
```

The strategy compares the latest price move with a rolling standard deviation of recent moves. Large
positive moves can trigger short entries; large negative moves can trigger long entries.

## Optimization

The optimizer is configured in:

```text
cpp/optimize_main.cpp
```

It supports:

- one-parameter grid searches;
- two-parameter grid searches;
- historical IS optimization;
- synthetic-mean IS optimization;
- configurable objective metric.

The optimizer records:

- net profit;
- average trade;
- Sharpe ratio;
- maximum drawdown;
- trades per year.

Raw optimization CSV files are written to:

```text
output/optimization/_assets/
```

Python then turns those CSV files into plots and a PDF report.

The optimizer is currently not parallelized. This is intentional for the thesis version, but it is also
one of the main practical limitations: synthetic optimization requires many repeated backtests.

## Synthetic market generation

Synthetic path generation is implemented in:

```text
cpp/include/core/synthetic_market_generator.hpp
cpp/synthetic_main.cpp
```

Current generators:

1. basic geometric Brownian motion;
2. rolling-volatility GBM;
3. threshold-constrained rolling-volatility GBM.

The threshold-constrained version is the main generator used in the thesis. It uses rolling volatility
estimated from log returns and accepts the next synthetic price only if it remains inside a
volatility-based band around the original market path.

The generated paths are written under:

```text
output/synthetic_compare/_assets/
```

The comparison figures are written under:

```text
output/synthetic_compare/_assets/
thesis/figures/
```

## Changing experiments

Configuration is currently done directly in the C++ entry points:

```text
cpp/backtest_main.cpp      # data path, sample split, strategy, backtest settings
cpp/optimize_main.cpp      # strategy, parameter grids, data mode, objective
cpp/synthetic_main.cpp     # number of paths, generator settings
```

Typical things to change:

- `data_path`;
- `start_date` / `end_date`;
- OOS test percentage;
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

Each report folder contains an `_assets/` subfolder with the raw CSV files used by the Python
reporting scripts.

## Thesis

The LaTeX thesis source is in:

```text
thesis/
```

The thesis focuses on:

- the overfitting problem in strategy optimization;
- IS/OOS testing;
- synthetic market generation;
- comparison between historical optimization and synthetic-market optimization.

The thesis does not discuss every code implementation detail. The full source code is provided here so
the implementation can be inspected directly.

## Main references

The thesis is mainly connected to three literature areas:

- bootstrap and resampling methods for dependent time series;
- data snooping and backtest overfitting;
- technical trading rule evaluation.

The key references currently used are:

```text
Politis, D. N., and Romano, J. P. (1994).
The stationary bootstrap.
Journal of the American Statistical Association, 89(428), 1303–1313.

White, H. (2000).
A reality check for data snooping.
Econometrica, 68(5), 1097–1126.

Bailey, D. H., Borwein, J. M., López de Prado, M., and Zhu, Q. J. (2017).
The probability of backtest overfitting.
Journal of Computational Finance, 20(4), 39–69.
```

## Limitations

Important limitations:

- configuration is currently hardcoded in the C++ entry points;
- the optimizer is grid-based and not parallelized;
- synthetic generators are experimental;
- synthetic optimization could, in theory, reserve more real historical data for testing, but this
  thesis keeps the 70/30 split to make the comparison with the benchmark workflow direct;
- data used to calibrate or anchor synthetic paths should not be treated as fully OOS;
- intrabar price paths are unknown because the backtester uses OHLCV bars;
- stop-loss fills are approximated using high/low crossing logic;
- the project is research code, not live trading infrastructure.

## License

See `LICENSE`.
