#!/bin/bash
# Prove file-menu × mutaclysm: user save slots, demo-project seed, NEW/SAVE/LOAD
set -u
HARNESS="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE="$(cd "$HARNESS/../.." && pwd)"
MUTA="$(ls -d "$HOUSE"/101.mutaclsym* 2>/dev/null | head -1)"
FMENU="$(find "$HOUSE" -maxdepth 2 -type d -name 'file-menu' 2>/dev/null | head -1)"
OPS_M="$MUTA/ops/+x"
OPS_F="$FMENU/ops/+x"
PROOF="$HARNESS/proof/harness-$(date +%Y%m%d-%H%M%S)"
WORKDIR="$HARNESS/workdir/run-$$"
mkdir -p "$PROOF" "$WORKDIR/user_fs/projects/mutaclysm/saves" "$WORKDIR/session/pieces/system/widget_cmds"

FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() { rm -rf "$WORKDIR" 2>/dev/null || true; }
trap cleanup EXIT

echo "=== file-menu + mutaclysm demo ==="
echo "MUTA=$MUTA FMENU=$FMENU PROOF=$PROOF"

# Session-like live world + template (from install)
SESSION="$WORKDIR/session"
SAVES="$WORKDIR/user_fs/projects/mutaclysm/saves"
mkdir -p "$SESSION/pieces"
cp -a "$MUTA/pieces/world_01" "$SESSION/pieces/world_01"
cp -a "$MUTA/pieces/world_01_template" "$SESSION/pieces/world_01_template"
# ops via install +x
ln -sfn "$MUTA/ops" "$SESSION/ops"
ln -sfn "$MUTA/system" "$SESSION/system" 2>/dev/null || true

export PRISC_PROJECT_ROOT="$SESSION"
export PRISC_INSTALL_ROOT="$MUTA"
export MUTA_SAVES_ROOT="$SAVES"
export MUTA_TEMPLATE_WORLD="$SESSION/pieces/world_01_template"

LIVE="$SESSION/pieces/world_01"
MAP_FILE="$LIVE/map_start/map.txt"
ORIG_MAP="$PROOF/00_original_map_start.txt"
cp "$MAP_FILE" "$ORIG_MAP"

# Publish bridge
"$OPS_M/muta_widget_cmds.+x"
cp "$SESSION/pieces/system/widget_bridge.txt" "$PROOF/01_bridge.txt"

# file-menu focus
WSTATE="$WORKDIR/widget_state"
mkdir -p "$WSTATE"
"$OPS_F/fm_set_focus.+x" "$WSTATE" "$SESSION" | tee "$PROOF/02_focus.txt"
cp "$WSTATE/focus.txt" "$PROOF/02_focus_state.txt"
if grep -q 'kind=game_world' "$WSTATE/focus.txt"; then
    pass "focus kind=game_world (mutaclysm)"
else
    fail "focus not game_world: $(cat "$WSTATE/focus.txt")"
fi

# SEED_DEMO
"$OPS_F/fm_enqueue_cmd.+x" "$WSTATE" SEED_DEMO | tee "$PROOF/03_enqueue_seed.txt"
"$OPS_M/muta_widget_cmds.+x"
cp "$SESSION/pieces/system/widget_cmds/status.txt" "$PROOF/03_status_seed.txt"
if [ -d "$SAVES/demo-project/world_01" ]; then
    pass "demo-project seeded under user saves"
else
    fail "demo-project missing"
fi
if grep -q 'result=ok' "$SESSION/pieces/system/widget_cmds/status.txt"; then
    pass "SEED_DEMO status ok"
else
    fail "SEED_DEMO status: $(cat "$SESSION/pieces/system/widget_cmds/status.txt")"
fi

# SAVE_GAME_AS t1
"$OPS_F/fm_enqueue_cmd.+x" "$WSTATE" SAVE_GAME_AS t1 | tee "$PROOF/04_enqueue_save.txt"
"$OPS_M/muta_widget_cmds.+x"
cp "$SESSION/pieces/system/widget_cmds/status.txt" "$PROOF/04_status_save.txt"
if [ -f "$SAVES/t1/world_01/map_start/map.txt" ]; then
    pass "SAVE_GAME_AS t1 created map tree"
    cp "$SAVES/t1/world_01/map_start/map.txt" "$PROOF/04_t1_map.txt"
else
    fail "t1 save missing"
fi

# Mutate live map
printf 'HARNESS_MUTATED_MAP_CELL\n' > "$MAP_FILE"
cp "$MAP_FILE" "$PROOF/05_mutated_live_map.txt"
if ! cmp -s "$MAP_FILE" "$ORIG_MAP"; then
    pass "live map mutated (differs from original)"
else
    fail "mutation did not change map"
fi

# LOAD t1 restores
"$OPS_F/fm_enqueue_cmd.+x" "$WSTATE" LOAD_GAME t1 | tee "$PROOF/06_enqueue_load.txt"
"$OPS_M/muta_widget_cmds.+x"
cp "$SESSION/pieces/system/widget_cmds/status.txt" "$PROOF/06_status_load.txt"
cp "$MAP_FILE" "$PROOF/06_live_after_load_t1.txt"
if cmp -s "$MAP_FILE" "$PROOF/04_t1_map.txt"; then
    pass "LOAD t1 restored map to saved snapshot"
else
    fail "LOAD t1 did not restore map"
fi

# NEW_GAME resets from template (not mutated, not necessarily = t1)
"$OPS_F/fm_enqueue_cmd.+x" "$WSTATE" NEW_GAME | tee "$PROOF/07_enqueue_new.txt"
"$OPS_M/muta_widget_cmds.+x"
cp "$SESSION/pieces/system/widget_cmds/status.txt" "$PROOF/07_status_new.txt"
cp "$MAP_FILE" "$PROOF/07_live_after_new.txt"
if [ -f "$MAP_FILE" ]; then
    pass "NEW_GAME left a map_start/map.txt in place"
else
    fail "NEW_GAME wiped map file entirely"
fi
# demo still loadable
if [ -d "$SAVES/demo-project/world_01" ]; then
    pass "demo-project still present after NEW_GAME"
else
    fail "demo-project gone after NEW"
fi

# LOAD demo-project
"$OPS_F/fm_enqueue_cmd.+x" "$WSTATE" LOAD_GAME demo-project | tee "$PROOF/08_enqueue_load_demo.txt"
"$OPS_M/muta_widget_cmds.+x"
cp "$SESSION/pieces/system/widget_cmds/status.txt" "$PROOF/08_status_load_demo.txt"
cp "$MAP_FILE" "$PROOF/08_live_after_demo.txt"
if grep -q 'result=ok' "$SESSION/pieces/system/widget_cmds/status.txt"; then
    pass "LOAD demo-project status ok"
else
    fail "LOAD demo failed: $(cat "$SESSION/pieces/system/widget_cmds/status.txt")"
fi
if [ -s "$MAP_FILE" ]; then
    pass "LOAD demo-project restored non-empty map"
else
    fail "demo load left empty map"
fi

# list slots
"$OPS_M/muta_world_io.+x" list "$SAVES" | tee "$PROOF/09_list.txt"
if grep -q demo-project "$PROOF/09_list.txt" && grep -q '^t1$' "$PROOF/09_list.txt"; then
    pass "list shows demo-project and t1"
else
    fail "list incomplete: $(cat "$PROOF/09_list.txt")"
fi

# Keep proof of saves tree listing
find "$SAVES" -maxdepth 3 -type d | sort > "$PROOF/10_saves_tree.txt"

echo
echo "Proof: $PROOF"
ls -la "$PROOF"
if [ "$FAIL" -eq 0 ]; then
    echo "=== ALL PASS — file-menu + mutaclysm user saves ==="
    # preserve proof beyond workdir cleanup
    exit 0
fi
echo "=== FAILED ==="
exit 1
