#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
ASSETS_DIR="$ROOT_DIR/output/backtest/_assets"
EQUITY_FILE="$ASSETS_DIR/equity.csv"
ORDERS_FILE="$ASSETS_DIR/orders.csv"

mkdir -p "$ASSETS_DIR"

find "$ASSETS_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} +

"$ROOT_DIR/cpp/runbt.sh"

if [[ ! -f "$EQUITY_FILE" || ! -f "$ORDERS_FILE" ]]; then
    echo "missing backtest output files in $ASSETS_DIR" >&2
    exit 1
fi

cd "$ROOT_DIR"
uv run python py/src/backtest_main.py
