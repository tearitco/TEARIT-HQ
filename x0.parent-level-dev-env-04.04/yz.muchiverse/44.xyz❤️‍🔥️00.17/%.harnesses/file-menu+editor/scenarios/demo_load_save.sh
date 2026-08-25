#!/bin/bash
# demo_load_save.sh — prove file-menu ops × editor widget cmd bus.
# Multi-project harness (%.harnesses/file-menu+editor).
#
# Flow:
#   1) Boot editor session (no keyboard; module idle drains cmds)
#   2) Publish bridge; file-menu fm_set_focus → that session
#   3) fm_enqueue LOAD fixture → editor_widget_cmds
#   4) assert buffer == fixture
#   5) overwrite buffer, SAVE via file-menu, assert disk
#   6) NEW via file-menu, assert empty buffer
set -u
HARNESS="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE="$(cd "$HARNESS/../.." && pwd)"
EDITOR_DIR="$(ls -d "$HOUSE"/102.editor* 2>/dev/null | head -1)"
FMENU_DIR="$(find "$HOUSE" -maxdepth 2 -type d -name 'file-menu' 2>/dev/null | head -1)"
OPS_E="$EDITOR_DIR/ops/+x"
OPS_F="$FMENU_DIR/ops/+x"
OPS_H="$HARNESS/ops/+x"
PROOF="$HARNESS/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF" "$HARNESS/fixtures" "$HARNESS/workdir"

FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() {
    echo "--- cleanup ---"
    if [ -n "${ED_SESSION:-}" ] && [ -d "$ED_SESSION" ]; then
        for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null || true)"
            [ "$cwd" = "$ED_SESSION" ] && kill -9 "$pid" 2>/dev/null || true
        done
        for pid in $(pgrep -f "system/chtpm_parser_pal" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null || true)"
            [ "$cwd" = "$ED_SESSION" ] && kill -9 "$pid" 2>/dev/null || true
        done
        for pid in $(pgrep -f "system/renderer" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null || true)"
            [ "$cwd" = "$ED_SESSION" ] && kill -9 "$pid" 2>/dev/null || true
        done
        # keep session for proof copy then remove
        cp -a "$ED_SESSION/pieces/system" "$PROOF/editor_system" 2>/dev/null || true
        rm -rf "$ED_SESSION"
    fi
}
trap cleanup EXIT

echo "=== file-menu + editor multi-project demo ==="
echo "HOUSE=$HOUSE"
echo "EDITOR=$EDITOR_DIR"
echo "FMENU=$FMENU_DIR"
echo "PROOF=$PROOF"

[ -n "$EDITOR_DIR" ] && [ -d "$EDITOR_DIR" ] || { fail "no editor dir"; exit 1; }
[ -n "$FMENU_DIR" ] && [ -d "$FMENU_DIR" ] || { fail "no file-menu dir"; exit 1; }
[ -x "$OPS_E/editor_widget_cmds.+x" ] || { fail "editor_widget_cmds missing — compile first"; exit 1; }
[ -x "$OPS_F/fm_set_focus.+x" ] || { fail "fm_set_focus missing"; exit 1; }
[ -x "$OPS_F/fm_enqueue_cmd.+x" ] || { fail "fm_enqueue_cmd missing"; exit 1; }

# --- fixture ---
FIXTURE="$HARNESS/fixtures/hello_from_harness.txt"
printf 'hello from file-menu harness\nline2\n' > "$FIXTURE"
cp "$FIXTURE" "$PROOF/00_fixture.txt"

# --- boot editor session (headless UI stack) ---
ED_SESSION="$EDITOR_DIR/pieces/sessions/harn-fm-$(date +%s)-$$"
mkdir -p "$ED_SESSION/pieces/system/widget_cmds" \
         "$ED_SESSION/pieces/display" \
         "$ED_SESSION/pieces/apps/player_app" \
         "$ED_SESSION/pieces/keyboard" \
         "$ED_SESSION/projects/agy-editor/manager" \
         "$ED_SESSION/docs"
ln -sfn "$EDITOR_DIR/system" "$ED_SESSION/system"
ln -sfn "$EDITOR_DIR/ops" "$ED_SESSION/ops"
ln -sfn "$EDITOR_DIR/pal" "$ED_SESSION/pal"
ln -sfn "$EDITOR_DIR/default_op.txt" "$ED_SESSION/default_op.txt"
ln -sfn "$EDITOR_DIR/pieces/chtpm" "$ED_SESSION/pieces/chtpm"
ln -sfn "$EDITOR_DIR/projects/agy-editor/pieces" "$ED_SESSION/projects/agy-editor/pieces"
ln -sfn "$EDITOR_DIR/docs" "$ED_SESSION/docs"

cd "$ED_SESSION"
: > pieces/apps/player_app/interact_relay.txt
: > pieces/keyboard/history.txt
: > pieces/display/editor_screen_changed.txt
: > pieces/system/widget_cmds/inbox.txt
printf 'SEED-SHOULD-BE-REPLACED\n' > pieces/system/editor_buffer.txt
cat > pieces/system/editor_state.txt << 'EOF'
file_path=docs/untitled.txt
cursor_pos=-1
last_message=harness session
EOF
cat > pieces/apps/player_app/state.txt << 'EOF'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=agy-editor
active_target_id=editor
EOF

export PRISC_PROJECT_ROOT="$ED_SESSION"
export PRISC_PROJECT_ID="agy-editor"

# publish bridge
"$OPS_E/editor_widget_cmds.+x" >/dev/null 2>&1 || true
cp pieces/system/widget_bridge.txt "$PROOF/01_bridge.txt" 2>/dev/null || true

# optional: start pal loop so idle also drains (also call op explicitly)
./system/renderer >/dev/null 2>&1 &
RP=$!
./system/chtpm_parser_pal pieces/chtpm/layouts/editor.chtpm >/dev/null 2>&1 &
CP=$!
sleep 0.5

# --- file-menu focus ---
WSTATE="$HARNESS/workdir/widget_state_$$"
mkdir -p "$WSTATE"
"$OPS_F/fm_set_focus.+x" "$WSTATE" "$ED_SESSION" | tee "$PROOF/02_focus.txt"
cp "$WSTATE/focus.txt" "$PROOF/02_focus_state.txt"

if grep -q "inbox_path=" "$WSTATE/focus.txt"; then
    pass "file-menu focused on editor session"
else
    fail "focus.txt missing inbox_path"
fi

# --- LOAD via file-menu enqueue + editor drain ---
"$OPS_F/fm_enqueue_cmd.+x" "$WSTATE" LOAD "$FIXTURE" | tee "$PROOF/03_enqueue_load.txt"
# explicit drain (don't rely only on idle timing)
"$OPS_E/editor_widget_cmds.+x" | tee "$PROOF/03_drain_load.txt" || true
sleep 0.2

cp pieces/system/editor_buffer.txt "$PROOF/03_buffer_after_load.txt"
cp pieces/system/widget_cmds/status.txt "$PROOF/03_status_load.txt" 2>/dev/null || true

if cmp -s pieces/system/editor_buffer.txt "$FIXTURE"; then
    pass "LOAD: editor buffer == fixture"
else
    fail "LOAD: buffer mismatch"
    echo "got:"; od -c pieces/system/editor_buffer.txt | head -3
    echo "exp:"; od -c "$FIXTURE" | head -3
fi

if grep -q 'result=ok' pieces/system/widget_cmds/status.txt 2>/dev/null; then
    pass "LOAD status result=ok"
else
    fail "LOAD status not ok: $(cat pieces/system/widget_cmds/status.txt 2>/dev/null)"
fi

# --- mutate buffer, SAVE via file-menu ---
SAVE_PATH="$HARNESS/workdir/saved_by_widget.txt"
printf 'edited-in-editor-then-saved\n' > pieces/system/editor_buffer.txt
# point file_path at SAVE_PATH via SAVE_AS
"$OPS_F/fm_enqueue_cmd.+x" "$WSTATE" SAVE_AS "$SAVE_PATH" | tee "$PROOF/04_enqueue_save.txt"
"$OPS_E/editor_widget_cmds.+x" | tee "$PROOF/04_drain_save.txt" || true
sleep 0.2

cp "$SAVE_PATH" "$PROOF/04_saved_file.txt" 2>/dev/null || true
cp pieces/system/widget_cmds/status.txt "$PROOF/04_status_save.txt" 2>/dev/null || true

if [ -f "$SAVE_PATH" ] && cmp -s "$SAVE_PATH" pieces/system/editor_buffer.txt; then
    pass "SAVE_AS: disk file == buffer"
else
    fail "SAVE_AS failed"
    ls -la "$SAVE_PATH" 2>/dev/null || true
fi

# --- NEW via file-menu ---
"$OPS_F/fm_enqueue_cmd.+x" "$WSTATE" NEW | tee "$PROOF/05_enqueue_new.txt"
"$OPS_E/editor_widget_cmds.+x" | tee "$PROOF/05_drain_new.txt" || true
cp pieces/system/editor_buffer.txt "$PROOF/05_buffer_after_new.txt"

if [ ! -s pieces/system/editor_buffer.txt ]; then
    pass "NEW: buffer empty"
else
    fail "NEW: buffer not empty ($(wc -c < pieces/system/editor_buffer.txt) bytes)"
fi

# kill UI
kill $RP $CP 2>/dev/null || true

echo
echo "Proof: $PROOF"
ls -la "$PROOF"
if [ "$FAIL" -eq 0 ]; then
    echo "=== ALL PASS — file-menu + editor work together via cmd bus ==="
    exit 0
fi
echo "=== FAILED ($FAIL) ==="
exit 1
