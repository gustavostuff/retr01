#!/usr/bin/env bash
# Run the Release Tier A sim binary from bin/ (build with ./scripts/build-all.sh first).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/bin/sim-tier-a"

die() { echo "error: $*" >&2; exit 1; }

[[ -x "$BIN" ]] || die "missing $BIN -- run ./scripts/build-all.sh first"

exec "$BIN" "$@"
