#!/bin/bash
# Prove map-picker: list maps, SWITCH_MAP teleports hero+xlector (map_id field)
set -u
HARNESS="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE="$(cd "$HARNESS/../.." && pwd)"
MUTA="$(ls -d "$HOUSE"/101.mutaclsym* 2>/dev/null | head -1)"
FMENU="$(find "$HOUSE" -maxdepth 2 -type d -name 'file-menu' 2>/dev/null | head -1)"
MPICK="$(find "$HOUSE" -maxdepth 2 -type d -name 'map-picker' 2>/dev/null | head -1)"
OPS_M="$MUTA/ops/+x"
OPS_F="$FMENU/ops/+x"
OPS_P="$MPICK/ops/+x"
PROOF="$HARNESS/proof/harness-$(date +%Y%m%d-%H%M%S)"
WORKDIR="$HARNESS/workdir/run-$$"
mkdir -p "$PROOF" "$WORKDIR"

FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }
cleanup() { rm -rf "$WORKDIR" 2>/dev/null || true; }
trap cleanup EXIT

echo "=== map-picker + mutaclysm demo ==="

SESSION="$WORKDIR/session"
mkdir -p "$SESSION/pieces/system/widget_cmds"
cp -a "$MUTA/pieces/world_01" "$SESSION/pieces/world_01"
ln -sfn "$MUTA/ops" "$SESSION/ops"

export PRISC_PROJECT_ROOT="$SESSION"
export PRISC_INSTALL_ROOT="$MUTA"
export MUTA_SAVES_ROOT="$WORKDIR/saves"
mkdir -p "$MUTA_SAVES_ROOT"

HERO="$SESSION/pieces/world_01/map_start/hero/state.txt"
# normalize start on map_start
"$OPS_M/muta_map_io.+x" switch "$SESSION" map_start 5 4 | tee "$PROOF/00_switch_start.txt"
cp "$HERO" "$PROOF/00_hero_after_start.txt"

# publish bridge + focus
"$OPS_M/muta_widget_cmds.+x"
WSTATE="$WORKDIR/wstate"
mkdir -p "$WSTATE"
"$OPS_F/fm_set_focus.+x" "$WSTATE" "$SESSION" | tee "$PROOF/01_focus.txt"
cp "$WSTATE/focus.txt" "$PROOF/01_focus_state.txt"

# list maps via map-picker
"$OPS_P/mp_list_maps.+x" "$WSTATE" | tee "$PROOF/02_map_list.txt"
if grep -q '^map_start$' "$PROOF/02_map_list.txt" && grep -q '^map_02$' "$PROOF/02_map_list.txt"; then
    pass "list includes map_start and map_02"
else
    fail "map list incomplete: $(cat "$PROOF/02_map_list.txt")"
fi

# switch via map-picker enqueue + widget cmds drain
"$OPS_P/mp_switch_map.+x" "$WSTATE" map_02 8 3 | tee "$PROOF/03_enqueue_switch.txt"
"$OPS_M/muta_widget_cmds.+x"
cp "$SESSION/pieces/system/widget_cmds/status.txt" "$PROOF/03_status.txt"
cp "$HERO" "$PROOF/03_hero_after_map02.txt"

MAP_ID=$(grep '^map_id=' "$HERO" | cut -d= -f2-)
PX=$(grep '^pos_x=' "$HERO" | cut -d= -f2-)
PY=$(grep '^pos_y=' "$HERO" | cut -d= -f2-)
XX=$(grep '^xlector_pos_x=' "$HERO" | cut -d= -f2-)
XY=$(grep '^xlector_pos_y=' "$HERO" | cut -d= -f2-)

if [ "$MAP_ID" = "map_02" ]; then pass "map_id=map_02 after switch"; else fail "map_id=$MAP_ID expected map_02"; fi
if [ "$PX" = "8" ] && [ "$PY" = "3" ]; then pass "hero pos=8,3"; else fail "hero pos=$PX,$PY"; fi
if [ "$XX" = "8" ] && [ "$XY" = "3" ]; then pass "xlector pos=8,3"; else fail "xlector=$XX,$XY"; fi
if grep -q 'result=ok' "$PROOF/03_status.txt"; then pass "SWITCH_MAP status ok"; else fail "status: $(cat "$PROOF/03_status.txt")"; fi

# switch back to map_start via file-menu enqueue path
"$OPS_F/fm_enqueue_cmd.+x" "$WSTATE" SWITCH_MAP "map_start:2:2" | tee "$PROOF/04_enqueue_start.txt"
"$OPS_M/muta_widget_cmds.+x"
cp "$HERO" "$PROOF/04_hero_after_start.txt"
MAP_ID=$(grep '^map_id=' "$HERO" | cut -d= -f2-)
if [ "$MAP_ID" = "map_start" ]; then pass "switched back to map_start"; else fail "map_id=$MAP_ID"; fi

# current helper
"$OPS_M/muta_map_io.+x" current "$SESSION" | tee "$PROOF/05_current.txt"
if grep -q 'current_map=map_start' "$PROOF/05_current.txt"; then
    pass "muta_map_io current reports map_start"
else
    fail "current: $(cat "$PROOF/05_current.txt")"
fi

echo
echo "Proof: $PROOF"
ls -la "$PROOF"
if [ "$FAIL" -eq 0 ]; then
    echo "=== ALL PASS — map-picker × mutaclysm ==="
    exit 0
fi
echo "=== FAILED ==="
exit 1
