#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cmake -S "$SCRIPT_DIR" -B "$SCRIPT_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$SCRIPT_DIR/build" --target optimizer

cd "$REPO_ROOT"
"$SCRIPT_DIR/build/optimizer"
