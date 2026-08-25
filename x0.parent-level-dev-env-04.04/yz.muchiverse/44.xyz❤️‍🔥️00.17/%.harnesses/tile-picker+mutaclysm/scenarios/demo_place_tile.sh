#!/bin/bash
set -u
HARNESS="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE="$(cd "$HARNESS/../.." && pwd)"
MUTA="$(ls -d "$HOUSE"/101.mutaclsym* 2>/dev/null | head -1)"
FMENU="$(find "$HOUSE" -maxdepth 2 -type d -name 'file-menu' 2>/dev/null | head -1)"
TP="$(find "$HOUSE" -maxdepth 2 -type d -name 'tile-picker' 2>/dev/null | head -1)"
OPS_M="$MUTA/ops/+x"
OPS_F="$FMENU/ops/+x"
OPS_T="$TP/ops/+x"
PROOF="$HARNESS/proof/harness-$(date +%Y%m%d-%H%M%S)"
WD="$HARNESS/workdir/run-$$"
mkdir -p "$PROOF" "$WD"
FAIL=0
pass(){ echo "PASS: $1"; }
fail(){ echo "FAIL: $1"; FAIL=1; }
trap 'rm -rf "$WD"' EXIT

echo "=== tile-picker + mutaclysm ==="
SESSION="$WD/session"
mkdir -p "$SESSION/pieces/system/widget_cmds"
cp -a "$MUTA/pieces/world_01" "$SESSION/pieces/world_01"
ln -sfn "$MUTA/ops" "$SESSION/ops"
export PRISC_PROJECT_ROOT="$SESSION"
export PRISC_INSTALL_ROOT="$MUTA"
export MUTA_SAVES_ROOT="$WD/saves"

MAP="$SESSION/pieces/world_01/map_start/map.txt"
# sample cell (5,2) before
BEFORE=$(python3 -c "
p=open('$MAP').read().splitlines()
print(p[2][5] if len(p)>2 and len(p[2])>5 else '?')
")
echo "before cell(5,2)=$BEFORE" | tee "$PROOF/00_before.txt"
cp "$MAP" "$PROOF/00_map_before.txt"

"$OPS_M/muta_widget_cmds.+x"
WSTATE="$WD/wstate"; mkdir -p "$WSTATE"
"$OPS_F/fm_set_focus.+x" "$WSTATE" "$SESSION" | tee "$PROOF/01_focus.txt"

# set brush T and place at 5,2
"$OPS_T/tp_set_brush.+x" "$WSTATE" T | tee "$PROOF/02_brush.txt"
"$OPS_M/muta_widget_cmds.+x"
cp "$SESSION/pieces/system/paint_brush.txt" "$PROOF/02_paint_brush.txt" 2>/dev/null || true
if grep -q '^T' "$SESSION/pieces/system/paint_brush.txt" 2>/dev/null; then
    pass "SET_BRUSH stored T"
else
    fail "brush not set"
fi

"$OPS_T/tp_place.+x" "$WSTATE" map_start 5 2 | tee "$PROOF/03_place.txt"
"$OPS_M/muta_widget_cmds.+x"
cp "$SESSION/pieces/system/widget_cmds/status.txt" "$PROOF/03_status.txt"
cp "$MAP" "$PROOF/03_map_after.txt"

AFTER=$(python3 -c "
p=open('$MAP').read().splitlines()
print(p[2][5] if len(p)>2 and len(p[2])>5 else '?')
")
echo "after cell(5,2)=$AFTER" | tee "$PROOF/03_after_cell.txt"
if [ "$AFTER" = "T" ]; then pass "PLACE_TILE map_start(5,2)='T'"; else fail "cell=$AFTER expected T"; fi
if grep -q 'result=ok' "$PROOF/03_status.txt"; then pass "PLACE_TILE status ok"; else fail "status $(cat "$PROOF/03_status.txt")"; fi

# place different glyph directly
"$OPS_T/tp_place.+x" "$WSTATE" map_start 6 2 X | tee "$PROOF/04_place_x.txt"
"$OPS_M/muta_widget_cmds.+x"
XCELL=$(python3 -c "
p=open('$MAP').read().splitlines()
print(p[2][6] if len(p)>2 and len(p[2])>6 else '?')
")
if [ "$XCELL" = "X" ]; then pass "PLACE_TILE explicit X at (6,2)"; else fail "cell=$XCELL"; fi

echo "Proof: $PROOF"
if [ "$FAIL" -eq 0 ]; then echo "=== ALL PASS — tile-picker ==="; exit 0; fi
echo "=== FAILED ==="; exit 1
