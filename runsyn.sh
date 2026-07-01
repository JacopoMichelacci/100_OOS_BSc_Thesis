#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
ASSETS_DIR="$ROOT_DIR/output/synthetic_compare/_assets"
SYNTH_GBM_FILE="$ASSETS_DIR/synthetic_paths_gbm.csv"
SYNTH_ROLLING_FILE="$ASSETS_DIR/synthetic_paths_gbm_rolling_vol.csv"
SYNTH_THRESHOLD_FILE="$ASSETS_DIR/synthetic_paths_gbm_rolling_vol_threshold.csv"
REAL_FILE="$ASSETS_DIR/real_path.csv"

mkdir -p "$ASSETS_DIR"

find "$ASSETS_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} +

"$ROOT_DIR/cpp/runsyn.sh"

if [[ ! -f "$SYNTH_GBM_FILE" || ! -f "$SYNTH_ROLLING_FILE" || ! -f "$SYNTH_THRESHOLD_FILE" || ! -f "$REAL_FILE" ]]; then
    echo "missing synthetic comparison output files in $ASSETS_DIR" >&2
    exit 1
fi

cd "$ROOT_DIR"
uv run python py/src/synthetic_compare_main.py
