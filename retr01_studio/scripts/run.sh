#!/usr/bin/env bash
# Run retr01 Studio (expects an existing build).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
BIN="$ROOT/build/retr01_studio"
if [[ ! -x "$BIN" ]]; then
  echo "missing $BIN — run scripts/build-run.sh or: cmake --build build" >&2
  exit 1
fi
exec "$BIN" "$@"
