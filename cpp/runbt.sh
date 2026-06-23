#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cmake -S "$SCRIPT_DIR" -B "$SCRIPT_DIR/build"
cmake --build "$SCRIPT_DIR/build"

cd "$REPO_ROOT"
"$SCRIPT_DIR/build/thesis"
