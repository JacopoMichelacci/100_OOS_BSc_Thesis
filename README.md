### Objective
Test whether strategy optimization on calibrated synthetic market data can produce real-market out-of-sample results that are more reliable than standard historical train/test optimization.

## How to run

This project uses `uv` for dependency management.

Install dependencies:

```bash
uv sync --frozen
```

Run the project:

```bash
uv run python main.py
```

### How
Compare two optimization protocols on the same real test window:

1. **Standard historical optimization**
   - Optimize on historical in-sample data.
   - Test on real out-of-sample data.

2. **Synthetic-market optimization**
   - Calibrate a synthetic market generator from historical data.
   - Optimize strategies on synthetic market paths.
   - Test the frozen strategies on the same real out-of-sample window.

### Optimization methodology
Strategy parameters are selected discretionally in both protocols rather than by blindly maximizing a single metric. The goal is to avoid clear overfitting by choosing parameter regions that look robust across nearby values.

For historical optimization, this means inspecting the in-sample optimization profile and selecting stable areas rather than isolated peaks. For synthetic-market optimization, results should be averaged across multiple synthetic runs when possible, so the selected parameter reflects performance that is robust across generated market paths rather than lucky on one path.

### Strategies
- Volatility strategy
- Momentum strategy
- Mean-reversion strategy

### Main comparison
For each strategy, compare standard OOS performance against synthetic-trained OOS performance on the same real test window.
