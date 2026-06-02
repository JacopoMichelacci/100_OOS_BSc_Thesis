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

### Strategies
- Volatility strategy
- Momentum strategy
- Mean-reversion strategy

### Main comparison
For each strategy, compare standard OOS performance against synthetic-trained OOS performance on the same real test window.