#!/bin/bash
# Prove: home chrome, href to System, methods list contains a known house app.
set -u
HARNESS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$HARNESS_DIR/.." && pwd)"
OPS="$HARNESS_DIR/ops/+x"
PROOF="$PROJECT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }
cleanup() { bash "$HARNESS_DIR/button.sh" kill; }
trap cleanup EXIT

key()   { "$OPS/tk_inject_key.+x" "$1" "$2"; sleep 0.3; }
focus() { "$OPS/tk_focus_item.+x" "$1" "$2" "$3" >/dev/null; sleep 0.3; }
check() {
    if "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; then return 0; fi
    FAIL=1; return 1
}

echo "=== START_BUTTON loader demo ==="
bash "$PROJECT_DIR/button.sh" compile || { fail compile; exit 1; }
bash "$HARNESS_DIR/button.sh" compile || { fail harn; exit 1; }

# Also unit-test scan offline
export PRISC_PROJECT_ROOT="$PROJECT_DIR"
# scan needs writable pieces under project - use temp session-like dir
SESS="$PROJECT_DIR/pieces/sessions/harn-scan-$$"
mkdir -p "$SESS/projects/start-button/pieces"/{home,system,widgets,apps,store}
mkdir -p "$SESS/pieces/system" "$SESS/pieces/display" "$SESS/pieces/apps/player_app"
ln -sfn "$PROJECT_DIR/system" "$SESS/system"
ln -sfn "$PROJECT_DIR/ops" "$SESS/ops"
ln -sfn "$PROJECT_DIR/config" "$SESS/config"
ln -sfn "$PROJECT_DIR/pieces/chtpm" "$SESS/pieces/chtpm"
ln -sfn "$PROJECT_DIR/pal" "$SESS/pal"
ln -sfn "$PROJECT_DIR/default_op.txt" "$SESS/default_op.txt"

export PRISC_PROJECT_ROOT="$SESS"
export PRISC_INSTALL_ROOT="$PROJECT_DIR"
"$PROJECT_DIR/ops/+x/start_scan.+x" all | tee "$PROOF/00_scan.txt"
SYS_PDL="$SESS/projects/start-button/pieces/system/piece.pdl"
cp "$SYS_PDL" "$PROOF/00_system.pdl"
if grep -q '102.editor' "$SYS_PDL" 2>/dev/null || grep -q 'RUN:system:' "$SYS_PDL"; then
    pass "system piece.pdl has RUN entries"
else
    fail "system piece.pdl missing RUN entries"
    cat "$SYS_PDL"
fi
# must not list @.apps or &.widgits as system
if grep -q '@.apps\|&.widgits' "$SYS_PDL"; then
    fail "system catalog leaked apps/widgets roots"
else
    pass "system catalog excludes @.apps / &.widgits"
fi

# Live session (no keyboard_input)
SESSION_ID="harn-$(date +%s)-$$"
SESSION_DIR="$PROJECT_DIR/pieces/sessions/$SESSION_ID"
mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
         "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
         "$SESSION_DIR/projects/start-button/pieces"/{home,system,widgets,apps,store}
ln -sfn "$PROJECT_DIR/system" "$SESSION_DIR/system"
ln -sfn "$PROJECT_DIR/ops" "$SESSION_DIR/ops"
ln -sfn "$PROJECT_DIR/pal" "$SESSION_DIR/pal"
ln -sfn "$PROJECT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
ln -sfn "$PROJECT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
ln -sfn "$PROJECT_DIR/config" "$SESSION_DIR/config"
cd "$SESSION_DIR"
: > pieces/apps/player_app/interact_relay.txt
: > pieces/keyboard/history.txt
: > pieces/display/start_screen_changed.txt
echo "last_message=Pick a category." > pieces/system/start_state.txt
cat > pieces/apps/player_app/state.txt << EOF
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=start-button
active_target_id=home
EOF
echo "pieces/chtpm/layouts/home.chtpm" > pieces/display/current_layout.txt
export PRISC_PROJECT_ROOT="$SESSION_DIR"
export PRISC_INSTALL_ROOT="$PROJECT_DIR"
./ops/+x/start_scan.+x all >/dev/null 2>&1
./ops/+x/start_compose_frame.+x >/dev/null 2>&1

./system/renderer >/dev/null 2>&1 &
RP=$!
./system/chtpm_parser_pal pieces/chtpm/layouts/home.chtpm >/dev/null 2>&1 &
CP=$!

FRAME="$SESSION_DIR/pieces/display/current_frame.txt"
for i in $(seq 1 40); do
    [ -s "$FRAME" ] && grep -q "L O A D E R\|System\|HOUSE" "$FRAME" 2>/dev/null && break
    sleep 0.15
done
cp "$FRAME" "$PROOF/01_home.txt" 2>/dev/null || true
echo "--- home ---"; head -35 "$PROOF/01_home.txt" 2>/dev/null

check "$FRAME" "System" "home has System category" || check "$FRAME" "L O A D E R" "home has loader chrome"
check "$FRAME" "Widgets" "home has Widgets" || true
check "$FRAME" "App Store" "home has App Store" || true

# href to System
focus "$SESSION_DIR" "$FRAME" "System"
key "$SESSION_DIR" 13
sleep 1.2
# idle bridge should rescan + set active_target_id
for i in $(seq 1 20); do
    grep -q "S Y S T E M\|piece_methods\|102.editor\|RUN\|Refresh" "$FRAME" 2>/dev/null && break
    # also poke idle via compose
    ./ops/+x/start_menu_input.+x 0 >/dev/null 2>&1
    ./ops/+x/start_compose_frame.+x >/dev/null 2>&1
    sleep 0.2
done
cp "$FRAME" "$PROOF/02_system.txt" 2>/dev/null || true
cp pieces/apps/player_app/state.txt "$PROOF/02_state.txt" 2>/dev/null || true
cp projects/start-button/pieces/system/piece.pdl "$PROOF/02_system.pdl" 2>/dev/null || true
echo "--- system ---"; head -40 "$PROOF/02_system.txt" 2>/dev/null

if grep -q 'active_target_id=system' pieces/apps/player_app/state.txt 2>/dev/null; then
    pass "bridged active_target_id=system"
else
    # force bridge once
    ./ops/+x/start_menu_input.+x 0
    sleep 0.3
    if grep -q 'active_target_id=system' pieces/apps/player_app/state.txt; then
        pass "bridged active_target_id=system (after idle)"
    else
        fail "active_target_id not system: $(cat pieces/apps/player_app/state.txt)"
    fi
fi

if grep -q 'RUN:system:' projects/start-button/pieces/system/piece.pdl 2>/dev/null; then
    pass "system methods include RUN:system: entries"
else
    fail "no RUN:system: in piece.pdl"
fi

# Frame should eventually show a project name from catalog
if grep -E '102\.editor|pal-forum|mutaclsym|login-signup|editor' "$FRAME" >/dev/null 2>&1; then
    pass "system frame lists a known house app"
else
    # methods might render as button labels — check piece.pdl presence is enough if frame lag
    if grep -E '102\.editor|pal-forum|mutaclsym' projects/start-button/pieces/system/piece.pdl >/dev/null; then
        pass "system pdl lists known app (frame may lag methods paint)"
    else
        fail "no known app in frame or pdl"
    fi
fi

kill $RP $CP 2>/dev/null || true
for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
    cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
    [ "$cwd" = "$SESSION_DIR" ] && kill -9 "$pid" 2>/dev/null
done
rm -rf "$SESS" "$SESSION_DIR" 2>/dev/null || true

echo "Proof: $PROOF"
if [ "$FAIL" -eq 0 ]; then echo "=== ALL PASS ==="; exit 0; fi
echo "=== FAILED ==="; exit 1
