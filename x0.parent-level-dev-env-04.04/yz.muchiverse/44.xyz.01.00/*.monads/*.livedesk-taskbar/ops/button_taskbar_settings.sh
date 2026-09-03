#!/bin/bash

# macOS leg (2026-08-23): macOS has no setsid(2) wrapper binary - expand
# to nothing there, keep real setsid on Linux. Unquoted $SETSID so the
# empty case vanishes from the command line entirely.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# button_taskbar_settings.sh — launch the taskbar HQ menu's "Settings"
# window as its own detached X11 process, same launch shape (build-if-
# missing, single-instance guard via pgrep -f full-cmdline match, TERM
# then KILL escalation, confirm exactly one PID) as ai-cell's own
# button.sh - real fix ported from the 2026-08-13 concurrent-process
# incident (_.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13"), not
# re-invented per-window.
# Usage: button_taskbar_settings.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "taskbar-settings button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

# 2026-09-03 static-xhtpm port: route to the taskbar-settings-pal window.
# TBSET_ROLLBACK=1 keeps the old taskbar_settings.chtpm / g_is_swatch_picker path.
_TBP="$HOUSE_ROOT/&.widgits/taskbar-settings/button-pal.sh"
[ -z "${TBSET_ROLLBACK:-}" ] && [ -f "$_TBP" ] && exec sh "$_TBP" "$HOUSE_ROOT"

# REAL Stage 5 §5d.3 step 6 (2026-08-16, khtpm-merge-how2.md §5d) - the
# real, literal binary merge: taskbar-settings now runs through the
# SAME compiled khtpm_core_render.+x entity-menu already uses,
# mode-selected by `<window class="swatch-picker">` in
# taskbar_settings.chtpm - genuinely one binary, not two, verified live
# both ways before this launcher was retargeted. khtpm_taskbar_settings_
# render.c/build_taskbar_settings.sh are kept as real, working reference/
# rollback (still build their own separate binary if invoked directly),
# just no longer what this launcher points at.
OPS_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "taskbar-settings button.sh: build failed, missing $BIN" >&2
    exit 1
fi

AUDIT_DIR="$HOUSE_ROOT/#.desktop/taskbar-settings-audit"
mkdir -p "$AUDIT_DIR"

CHTPM_PATH="$OPS_DIR/taskbar_settings.chtpm"
# REAL, deliberately specific - matches by the real chtpm PATH too, not
# just the binary name, since khtpm_core_render.+x is a real,
# genuinely shared binary now - a bare binary-name match here would
# incorrectly kill/confuse itself with any other, unrelated, legitimately-
# open entity right-click menu using the exact same executable.
settings_pids() { pgrep -f "khtpm_core_render\.\+x .*taskbar_settings\.chtpm" 2>/dev/null || true; }

pids="$(settings_pids)"
if [ -n "$pids" ]; then
    echo "taskbar-settings button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(settings_pids)"
    if [ -n "$pids" ]; then
        echo "taskbar-settings button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

$SETSID nohup "$BIN" "$HOUSE_ROOT" "$CHTPM_PATH" \
    >"$AUDIT_DIR/taskbar-settings.log" 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(settings_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "taskbar-settings launched (PID $pids, log=$AUDIT_DIR/taskbar-settings.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "taskbar-settings button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "taskbar-settings button.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/taskbar-settings.log" 2>/dev/null >&2
    exit 1
fi
