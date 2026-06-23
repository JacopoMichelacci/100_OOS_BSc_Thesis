#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
ASSETS_DIR="$ROOT_DIR/output/optimization/_assets"
RESULTS_FILE="$ASSETS_DIR/optimization_results.csv"

mkdir -p "$ASSETS_DIR"

find "$ASSETS_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} +

"$ROOT_DIR/cpp/runopt.sh"

if [[ ! -f "$RESULTS_FILE" ]]; then
    echo "missing optimization output file in $ASSETS_DIR" >&2
    exit 1
fi

cd "$ROOT_DIR"
uv run python py/src/optimization_main.py
