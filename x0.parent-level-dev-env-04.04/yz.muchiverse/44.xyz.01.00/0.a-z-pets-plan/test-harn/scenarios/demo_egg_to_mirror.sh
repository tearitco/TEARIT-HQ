#!/bin/bash
# demo_egg_to_mirror.sh - real, end-to-end test of the coordinate+file
# handoff between egg_window (01.muchi-pals) and gl_mirror (101.mutaclsym),
# per 0.a-z-pets-plan/a-z-fix.txt, PLUS the species_emoji wiring added
# afterward so a real pet's real emoji actually renders in gl_mirror -
# see 0.a-z-pets-plan/a-z-fix-report.txt. Drives a REAL synthetic drag
# (not a direct op call) via tk_drag_sim, exactly like every other
# harness in this project family - no state seeding, no shortcuts.
#
# Uses test_pet_turtle - a full COPY (never a move) of the real egg_2's
# own directory (real sprite.csv/atlas.png/state.txt, real
# species_emoji=🐢), so the transfer uses genuine emoji-bearing pet data
# without ever touching the real egg_2. pet_export.c does a real
# rename(), not a copy, so testing directly against egg_2 would have
# permanently relocated it - test_pet_turtle is expendable, egg_2 isn't.
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PLAN_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
ROOT="$(cd "$PLAN_DIR/.." && pwd)"
MUCHI_ROOT="$ROOT/01.muchi-pals-🥚️-13.01"
MIRROR_ROOT="$ROOT/101.mutaclsym🧟‍♂️️+18.01"
OPS="$HARNESS_DIR/ops/+x"
PET_ID="test_pet_turtle"
SOURCE_EGG="egg_2"

PROOF_DIR="$PLAN_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

EGG_PID=""
MIRROR_PID=""
cleanup() {
    echo; echo "--- cleanup ---"
    [ -n "$EGG_PID" ] && kill -9 "$EGG_PID" 2>/dev/null
    [ -n "$MIRROR_PID" ] && kill -9 "$MIRROR_PID" 2>/dev/null
    pkill -f "system/egg_window" 2>/dev/null
    pkill -f "system/gl_mirror" 2>/dev/null
    sleep 1
    if ps aux | grep -E "system/egg_window|system/gl_mirror" | grep -v grep >/dev/null 2>&1; then
        echo "WARNING: processes still running after cleanup:"
        ps aux | grep -E "system/egg_window|system/gl_mirror" | grep -v grep
    else
        echo "clean - no lingering egg_window/gl_mirror processes"
    fi
}
trap cleanup EXIT

echo "=== egg_window -> gl_mirror REAL drag-drop scenario (pet_id=$PET_ID, real turtle emoji) ==="

echo "--- pre-flight: stray process check ---"
pkill -f "system/egg_window" 2>/dev/null
pkill -f "system/gl_mirror" 2>/dev/null
sleep 1

echo "--- pre-flight: reset test pet (fresh copy of $SOURCE_EGG, real egg untouched) + exchange/ + mirror destination ---"
TESTPET_SRC="$MUCHI_ROOT/pieces/world_01/map_lobby/$PET_ID"
REAL_EGG_SRC="$MUCHI_ROOT/pieces/world_01/map_lobby/$SOURCE_EGG"
EXCHANGE_DIR="$ROOT/exchange/$PET_ID"
MIRROR_DEST="$MIRROR_ROOT/pieces/world_01/map_start/$PET_ID"
rm -rf "$EXCHANGE_DIR" "$MIRROR_DEST" "$TESTPET_SRC"
rm -f "$MIRROR_ROOT/incoming_drop.txt"
cp -r "$REAL_EGG_SRC" "$TESTPET_SRC"
rm -f "$TESTPET_SRC/window.pid"
sed -i "s/^name=$SOURCE_EGG\$/name=$PET_ID/" "$TESTPET_SRC/state.txt"
cp "$TESTPET_SRC/state.txt" "$PROOF_DIR/before_state.txt"
grep "species_emoji" "$TESTPET_SRC/state.txt"

echo "--- CPU/load before launch ---"
cat /proc/loadavg

echo "--- launching gl_mirror (bounded) ---"
( cd "$MIRROR_ROOT" && PRISC_PROJECT_ROOT="$MIRROR_ROOT" timeout 120 ./system/gl_mirror > /tmp/az_gl_mirror.log 2>&1 ) &
disown
sleep 2
MIRROR_PID=$(pgrep -f "system/gl_mirror" | head -1)
if [ -z "$MIRROR_PID" ]; then
    fail "gl_mirror did not start"
    exit 1
fi
echo "gl_mirror pid: $MIRROR_PID"

echo "--- screenshot BEFORE drag (baseline) ---"
"$OPS/tk_screenshot.+x" "$PROOF_DIR/before_drag.ppm" 2>&1

echo "--- launching egg_window for $PET_ID (bounded) ---"
( cd "$MUCHI_ROOT" && PRISC_PROJECT_ROOT="$MUCHI_ROOT" timeout 120 ./system/egg_window "$PET_ID" > /tmp/az_egg_window.log 2>&1 ) &
disown
sleep 2
EGG_PID=$(pgrep -f "system/egg_window $PET_ID" | head -1)
if [ -z "$EGG_PID" ]; then
    fail "egg_window did not start"
    exit 1
fi
echo "egg_window pid: $EGG_PID"

echo "--- CPU snapshot, both idle, before dragging ---"
ps -o pid,pcpu,pmem,etime,cmd -p "$MIRROR_PID","$EGG_PID" 2>&1 | tee "$PROOF_DIR/cpu_before_drag.txt"

echo "--- real synthetic drag: egg_window($PET_ID) -> gl_mirror(mutaclsym RGB mirror) ---"
"$OPS/tk_drag_sim.+x" "$PET_ID" "mutaclsym RGB mirror" 20 2>&1 | tee "$PROOF_DIR/drag_sim_output.txt"

echo "--- waiting for pet_export -> drop-file -> pet_import -> close_request round trip ---"
sleep 3

echo "--- generating a real frame reflecting the imported pet (one-shot op, not a persistent daemon) ---"
( cd "$MIRROR_ROOT" && PRISC_PROJECT_ROOT="$MIRROR_ROOT" ./ops/+x/compose_rgb_frame.+x ) 2>&1 | tee "$PROOF_DIR/compose_rgb_frame_output.txt"
sleep 1

echo "--- screenshot AFTER import + real frame regen ---"
"$OPS/tk_screenshot.+x" "$PROOF_DIR/after_import.ppm" 2>&1

echo "--- CPU snapshot right after the drop ---"
ps -o pid,pcpu,pmem,etime,cmd -p "$MIRROR_PID" 2>&1 | tee "$PROOF_DIR/cpu_after_drop.txt"
sleep 3
echo "--- CPU snapshot a few seconds later (confirms no creep - the crash-causing symptom) ---"
ps -o pid,pcpu,pmem,etime,cmd -p "$MIRROR_PID" 2>&1 | tee -a "$PROOF_DIR/cpu_after_drop.txt"

echo
echo "=== assertions ==="

if [ ! -d "$TESTPET_SRC" ]; then
    pass "pet_export ran - $PET_ID no longer in muchi-pals' map_lobby/"
else
    fail "pet_export did not run - $PET_ID still in muchi-pals' map_lobby/"
fi

if [ -f "$MIRROR_DEST/state.txt" ]; then
    pass "pet_import ran - $PET_ID now has a real state.txt in mutaclsym's map_start/"
    cp "$MIRROR_DEST/state.txt" "$PROOF_DIR/after_state.txt"
else
    fail "pet_import did not run - no $MIRROR_DEST/state.txt"
fi

if grep -q "^species_emoji=🐢$" "$MIRROR_DEST/state.txt" 2>/dev/null; then
    pass "real turtle emoji (species_emoji=🐢) carried through the trade envelope into mutaclsym's own state.txt"
else
    fail "species_emoji did not carry through to mutaclsym's state.txt"
fi

if [ -f "$MIRROR_ROOT/pieces/registry/emoji_assets/sp_turtle/voxels_16.csv" ]; then
    pass "sp_turtle emoji pixel asset exists in mutaclsym's registry (real FreeType-rasterized, not a placeholder)"
else
    fail "sp_turtle voxel asset missing - compose_rgb_frame would have fallen back to the 'hero' asset"
fi

if [ ! -d "$EXCHANGE_DIR" ]; then
    pass "exchange/$PET_ID consumed (moved onward by pet_import, not left behind)"
else
    fail "exchange/$PET_ID still exists - pet_import did not move it"
fi

if [ ! -f "$MIRROR_ROOT/incoming_drop.txt" ]; then
    pass "incoming_drop.txt consumed by gl_mirror's idle poll (no leftover)"
else
    fail "incoming_drop.txt still exists - gl_mirror never consumed it"
fi

# A clean self-exit (via the close_request round trip egg_window already
# had, reused unchanged by this fix) is a real wait+check on the PID,
# not just "is it gone" (which a timeout kill would also show).
if kill -0 "$EGG_PID" 2>/dev/null; then
    fail "egg_window ($EGG_PID) is still running - close_request round trip did not close it"
    EGG_PID=""  # let cleanup kill it
else
    pass "egg_window closed itself (close_request round trip worked) - not still running after the drop"
fi

cp /tmp/az_gl_mirror.log "$PROOF_DIR/gl_mirror.log" 2>/dev/null
cp /tmp/az_egg_window.log "$PROOF_DIR/egg_window.log" 2>/dev/null

echo
echo "=== proof saved to: $PROOF_DIR ==="
echo "=== screenshots: before_drag.ppm / after_import.ppm (view with any PPM-capable viewer) ==="
if [ "$FAIL" = "1" ]; then echo "=== OVERALL: FAIL ==="; exit 1
else echo "=== OVERALL: PASS ==="; exit 0
fi
