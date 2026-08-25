#!/bin/bash
# kill_all.sh - FAST avatar-creation cleanup
#
# Optional $1 = session directory (cwd-scoped for UI trio only).
# Desktop avatar_window always killed globally (they detach via setsid).
#
# Why this is fast:
#   - pgrep -x / pkill -x by exact process NAME (kernel-side), not a
#     bash loop over every /proc/PID with readlink -f
#   - window.pid + pid-list first (usually enough)
#   - one short grace only if something was still alive after TERM
#   - no workspace-wide find
set -u
SESSION_DIR="${1:-}"
INSTALL_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

echo "=== avatar-creation kill_all ${SESSION_DIR:+(session)} ==="

# Soft stop
for root in ${SESSION_DIR:+"$SESSION_DIR"} "$INSTALL_DIR"; do
    [ -d "$root/pieces/system" ] || mkdir -p "$root/pieces/system" 2>/dev/null || true
    printf '1\n' > "$root/pieces/system/quit_flag.txt" 2>/dev/null || true
done

# Kill exact process names. -x = name only (safe: chrome is "chrome", not "renderer").
# Returns 0 if any matched.
term_name() { pkill -x -TERM "$1" 2>/dev/null || true; }
kill_name() { pkill -x -KILL "$1" 2>/dev/null || true; }

# Session-scoped: only kill PIDs whose cwd matches SESSION_DIR.
term_name_session() {
    local name="$1" pid cwd
    [ -n "$SESSION_DIR" ] || { term_name "$name"; return; }
    for pid in $(pgrep -x "$name" 2>/dev/null); do
        cwd=$(readlink "/proc/$pid/cwd" 2>/dev/null) || continue
        cwd=${cwd% (deleted)}
        if [ "$cwd" = "$SESSION_DIR" ]; then
            kill -TERM "$pid" 2>/dev/null || true
        fi
    done
}
kill_name_session() {
    local name="$1" pid cwd
    [ -n "$SESSION_DIR" ] || { kill_name "$name"; return; }
    for pid in $(pgrep -x "$name" 2>/dev/null); do
        cwd=$(readlink "/proc/$pid/cwd" 2>/dev/null) || continue
        cwd=${cwd% (deleted)}
        if [ "$cwd" = "$SESSION_DIR" ]; then
            kill -KILL "$pid" 2>/dev/null || true
        fi
    done
}

alive=0

# --- Fast path: known PIDs from window.pid / lists ---
if [ -d "$INSTALL_DIR/pieces/world_01/map_lobby" ]; then
    for pf in "$INSTALL_DIR/pieces/world_01/map_lobby"/*/window.pid; do
        [ -f "$pf" ] || continue
        pid=$(tr -d ' \n\r' < "$pf" 2>/dev/null || true)
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill -TERM "$pid" 2>/dev/null || true
            alive=1
        fi
        rm -f "$pf"
    done
fi
for list in \
    ${SESSION_DIR:+"$SESSION_DIR/pieces/system/avatar_window_pids.txt"} \
    "$INSTALL_DIR/pieces/system/avatar_window_pids.txt"; do
    [ -f "$list" ] || continue
    while read -r pid; do
        [ -n "${pid:-}" ] || continue
        if kill -0 "$pid" 2>/dev/null; then
            kill -TERM "$pid" 2>/dev/null || true
            alive=1
        fi
    done < "$list" 2>/dev/null || true
    : > "$list" 2>/dev/null || true
done

# --- By exact name ---
# avatar_window: always global
if pgrep -x avatar_window >/dev/null 2>&1; then
    term_name avatar_window
    alive=1
fi
for name in renderer keyboard_input chtpm_parser_pal; do
    if pgrep -x "$name" >/dev/null 2>&1; then
        term_name_session "$name"
        alive=1
    fi
done
# prisc+x has a + in the name — pgrep -x still works with the literal name
if pgrep -x 'prisc+x' >/dev/null 2>&1; then
    term_name_session 'prisc+x'
    alive=1
fi

# Only wait if something was still running
if [ "$alive" = "1" ]; then
    sleep 0.12
    kill_name avatar_window
    for name in renderer keyboard_input chtpm_parser_pal; do
        kill_name_session "$name"
    done
    kill_name_session 'prisc+x'
fi

# Cheap cleanup (no recursive find of whole tree)
rm -f "$INSTALL_DIR/pieces/world_01/map_lobby"/*/window.pid 2>/dev/null || true
: > "$INSTALL_DIR/pieces/system/avatar_window_pids.txt" 2>/dev/null || true

# Residual check: only pgrep -x (no full /proc walk)
left=""
for name in avatar_window renderer keyboard_input chtpm_parser_pal 'prisc+x'; do
    if pgrep -x "$name" >/dev/null 2>&1; then
        left="$left $name"
    fi
done
if [ -n "$left" ]; then
    echo "WARNING still running:$left"
else
    echo "clean"
fi
