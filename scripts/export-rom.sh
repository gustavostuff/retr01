#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/r01-common.sh
source "$ROOT/scripts/r01-common.sh"

PROJECT="$(r01_resolve_path "${1:-$R01_DEFAULT_PROJECT}")"
STEM="${2:-$R01_ROM_DIR/test}"
STUDIO="$R01_ROOT/retr01_studio"

r01_build "$STUDIO" --target export_rom
"$STUDIO/build/export_rom" "$PROJECT" "$STEM"
