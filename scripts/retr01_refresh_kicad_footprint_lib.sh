#!/usr/bin/env bash
# Copy canonical Retr01_Lib into the v_01 KiCad project (no netlist rebuild).
# Use when Footprint Editor or "Update footprints from library" still shows old TRS spacing.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$REPO/apps/sim/tier-h/skidl/library/Retr01_Lib.pretty"
DST="$REPO/apps/sim/tier-h/kicad/main-pcb/v_01/library/Retr01_Lib.pretty"
TRS="$SRC/Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical.kicad_mod"
SRC_3D="$REPO/apps/sim/tier-h/skidl/library/Retr01_Lib.3dshapes"
DST_3D="$REPO/apps/sim/tier-h/kicad/main-pcb/v_01/library/Retr01_Lib.3dshapes"

if [[ ! -d "$SRC" ]]; then
    echo "missing $SRC" >&2
    exit 1
fi

mkdir -p "$(dirname "$DST")"
rm -rf "$DST"
cp -a "$SRC" "$DST"
if [[ -d "$SRC_3D" ]]; then
    rm -rf "$DST_3D"
    mkdir -p "$DST_3D"
    cp -a "$SRC_3D/." "$DST_3D/"
fi

python3 "$REPO/scripts/retr01_verify_trs_footprint.py" "$TRS"
echo "Installed Retr01_Lib -> $DST"
echo "KiCad: fully quit the app (not just the project), reopen v_01."
echo "  Footprint Editor -> Retr01_Lib -> Jack_3.5mm_... : pad 5 X=11.8, Retr01_fp_rev=3."
echo "  Avoid 'Update footprints from library' for TRS (KiCad may use a stale RAM cache)."
echo "  Instead, with KiCad closed:"
echo "    python3 scripts/retr01_sync_pcb_footprint_pads.py \\"
echo "      apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb --full \\"
echo "      --library apps/sim/tier-h/kicad/main-pcb/v_01/library/Retr01_Lib.pretty"
