#!/usr/bin/env bash
# Push canonical TRS pad geometry + 3D transform onto v_01 (KiCad closed).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
PCB="${1:-$REPO/apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb}"
LIB="$REPO/apps/sim/tier-h/kicad/main-pcb/v_01/library/Retr01_Lib.pretty"
SRC_LIB="$REPO/apps/sim/tier-h/skidl/library/Retr01_Lib.pretty"
TRS="Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical"

"$REPO/scripts/retr01_refresh_kicad_footprint_lib.sh"
python3 "$REPO/scripts/retr01_apply_footprint_3d.py"
python3 "$REPO/scripts/retr01_sync_pcb_footprint_pads.py" "$PCB" \
    --library "$LIB" --footprint "$TRS" --full
if [[ -f "$PCB" ]]; then
    python3 "$REPO/scripts/retr01_sync_pcb_3d_models.py" "$PCB" --force
fi
python3 "$REPO/scripts/retr01_verify_trs_footprint.py" "$SRC_LIB/${TRS}.kicad_mod"
echo "Done. Reopen $PCB in KiCad (reload from disk)."
