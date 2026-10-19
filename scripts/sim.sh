#!/usr/bin/env bash
# Run the Release sim binary from bin/ (build with ./scripts/build-all.sh first).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

die() { echo "error: $*" >&2; exit 1; }

BIN="$ROOT/bin/sim"
[[ -x "$BIN" ]] || die "missing $BIN -- run ./scripts/build-all.sh first"

exec "$BIN" "$@"
