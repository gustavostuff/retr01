#!/usr/bin/env bash
# Run a Release tier sim binary from bin/ (build with ./scripts/build-all.sh first).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

die() { echo "error: $*" >&2; exit 1; }

[[ $# -ge 1 ]] || die "usage: ./scripts/sim-tier.sh <a|b|c|h> [args...]"

case "$1" in
  a|A|b|B|c|C|h|H) tier="${1,,}" ;;
  *) die "unknown tier '$1' -- expected a, b, c, or h" ;;
esac
shift

BIN="$ROOT/bin/sim-tier-$tier"
[[ -x "$BIN" ]] || die "missing $BIN -- run ./scripts/build-all.sh first"

exec "$BIN" "$@"
