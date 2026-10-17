#!/usr/bin/env bash
# Run the Release Tier H sim binary from bin/ (build with ./scripts/build-all.sh first).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

die() { echo "error: $*" >&2; exit 1; }

if [[ $# -ge 1 ]]; then
  case "$1" in
    h|H) shift ;;
    a|A|b|B|c|C) die "unknown tier '$1'" ;;
  esac
fi

BIN="$ROOT/bin/sim-tier-h"
[[ -x "$BIN" ]] || die "missing $BIN -- run ./scripts/build-all.sh first"

exec "$BIN" "$@"
