#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/bin/sim-tier-c"
if [[ ! -x "$BIN" ]]; then
  echo "missing $BIN — run ./scripts/build-all.sh" >&2
  exit 1
fi
exec "$BIN" "$@"
