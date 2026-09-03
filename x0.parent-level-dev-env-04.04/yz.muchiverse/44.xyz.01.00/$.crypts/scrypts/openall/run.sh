#!/bin/bash

# macOS leg (2026-08-23): macOS has no setsid(2) wrapper binary - expand
# to nothing there, keep real setsid on Linux. Unquoted $SETSID so the
# empty case vanishes from the command line entirely.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# openall/run.sh - always-open the monads we want, no questions.
# Launches the full desired desktop set unconditionally (ignores the
# autostart.pdl STATE|enabled toggle). Idempotent: each target is only
# launched if a process isn't already hosting it, so re-running never
# duplicates windows. Each launch is detached (setsid nohup), the same
# mechanism crypt_autostart uses.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CRYPTS_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
HOUSE_DIR="$(cd "$CRYPTS_DIR/.." && pwd)"

TPWIN="$HOUSE_DIR/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
TASKBAR="$HOUSE_DIR/*.monads/*.livedesk-taskbar/ops/+x/khtpm_strip_parser.+x"
RESTORE_LIST="$CRYPTS_DIR/restore-list.txt"

# Ensure the shared asset drive is mounted (book-stack's bible assets
# live on it) - silent + idempotent, so openall is self-sufficient.
# Uses the SHARED, retrying helper (the old inline one-shot failed at
# login: it called udisksctl exactly once while the polkit local session
# was still coming up as Active => silent deny, then never retried). The
# shared version retries and also serves the book-stack reader branches.
ensure_mount() {
    local uuid="b7ced73c-5231-4462-b98d-64e38fe2df9e"
    local mp="/media/no/$uuid"
    [ -d "$mp" ] && mountpoint -q "$mp" 2>/dev/null && return 0
    local shared="$HOUSE_DIR/*.monads/*.book-stack/pieces/_shared/ensure_book_mount.sh"
    if [ -f "$shared" ]; then
        # shellcheck disable=SC1090  # sourced path is computed
        . "$shared"
        ensure_book_mount >/dev/null 2>&1 && return 0
    fi
    # Fallback (shared helper unavailable): retrying udisks mount.
    local i
    for i in 1 2 3 4 5 6 7 8; do
        udisksctl mount -b "/dev/disk/by-uuid/$uuid" >/dev/null 2>&1
        local j
        for j in 1 2 3 4 5; do
            mountpoint -q "$mp" 2>/dev/null && { echo "mounted $uuid"; return 0; }
            sleep 0.5
        done
    done
    echo "WARN: could not mount $uuid (book assets unavailable)"
}

# pgrep -f treats its pattern as a regex, and the literal '*' globs in
# monad paths (*.monads/*.book-stack/...) would be eaten as metachars,
# making idempotency checks miss running windows. Escape regex specials
# so full literal paths match.
escape_re() {
    printf '%s' "$1" | sed 's/[][{}.*+?^$|\\]/\\&/g'
}

entity_up() {
    pgrep -f "$(escape_re "$1")" >/dev/null 2>&1
}

launch_entity() {
    local ent="$1"
    if entity_up "$ent"; then
        echo "already open: $(basename "$ent")"
        return 0
    fi
    if [ ! -x "$TPWIN" ]; then
        echo "MISSING tp_desktop_window: $TPWIN"
        return 1
    fi
    $SETSID nohup "$TPWIN" "$ent" >/dev/null 2>&1 &
    echo "opened: $(basename "$ent")"
}

launch_by_name() {
    case "$1" in
        tool-bar) echo "skipping toolbar row" ;;
        ava) setsid nohup bash "$HOUSE_DIR/@.apps/asa-&-ava/pieces/ava/button.sh" run >/dev/null 2>&1 & echo "opened: ava" ;;
        asa) setsid nohup bash "$HOUSE_DIR/@.apps/asa-&-ava/pieces/asa/button.sh" run >/dev/null 2>&1 & echo "opened: asa" ;;
        hard-vvar-agent-Q0000) launch_entity "$HOUSE_DIR/*.monads/*.hard-vvar-agent-Q0000/entities/self" ;;
        m1_ninjadragon) launch_entity "$HOUSE_DIR/*.monads/*.muchi-pet/entities/m1_ninjadragon" ;;
        m8_redhorned) launch_entity "$HOUSE_DIR/*.monads/*.muchi-pet/entities/m8_redhorned" ;;
        book-stack) launch_entity "$HOUSE_DIR/*.monads/*.book-stack/entities/book-stack" ;;
        *) echo "unknown restore target: $1" ;;
    esac
}

ensure_mount

if [ -f "$RESTORE_LIST" ]; then
    while IFS= read -r name; do
        case "$name" in
            ''|\#*) continue ;;
            *) launch_by_name "$name" ;;
        esac
    done < "$RESTORE_LIST"
else
    launch_by_name ava
    launch_by_name asa
    launch_by_name hard-vvar-agent-Q0000
    launch_by_name m1_ninjadragon
    launch_by_name m8_redhorned
    launch_by_name book-stack
fi

if [ -x "$TASKBAR" ] && ! pgrep -f "$(escape_re "$TASKBAR")" >/dev/null 2>&1; then
    $SETSID nohup "$TASKBAR" "$HOUSE_DIR" >/dev/null 2>&1 &
    echo "opened: taskbar"
fi

echo "openall done"
