#!/bin/bash
# horn_chat.sh - HORN_CHAT launcher.
#
# Runs the standard 3-process pal-native stack:
#   keyboard_input  - raw termios -> keycodes
#   renderer        - draws pieces/display/current_frame.txt
#   chtpm_parser_pal- parses the .chtpm layout, runs pal/horn_main_loop.pal
#
# No orchestrator process: those three plus the parser's forked pal module
# are the whole system, and this script supervises them with the same
# 3-layer kill the house pattern uses (process group, then the pid ledger
# in pieces/os/proc_list.txt, then the quit flag the renderer watches).
#
# Usage:
#   ./horn_chat.sh            - build if needed, then run
#   ./horn_chat.sh build      - build only
#   ./horn_chat.sh run        - run without rebuilding
#   ./horn_chat.sh send "..." - one non-interactive turn (for testing)
#   ./horn_chat.sh clean      - drop session state (keeps binaries)
#   ./horn_chat.sh kill       - kill any stragglers from a crashed session
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

LAYOUT="layouts/horn_chat.chtpm"
PROC_LIST="pieces/os/proc_list.txt"
QUIT_FLAG="pieces/system/quit_flag.txt"

# Where the OpenRouter key lives. The transport also falls back to this
# path itself, but exporting it keeps the lookup explicit and lets a
# future HALO point at a different entity state dir.
export HORN_ENTITY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)/&.widgits/open-hai/state"
export HORN_SESSIONS="$SCRIPT_DIR/chats/HORN_SESSIONS"
export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
export PRISC_PROJECT_ID="hai-horn"

mkdir -p ops/+x pieces/horn pieces/display pieces/keyboard pieces/system \
         pieces/os pieces/apps/player_app/manager "$HORN_SESSIONS"

# ── process hygiene ───────────────────────────────────────────────────
#
# TWO house rules from CPU-AND-SESSION-SAFETY.md that this file previously
# broke, both found by reading the doc after being told CPU safety was not
# being followed:
#
#   1. "pkill may be sandboxed/blocked in some environments (every
#      invocation can return non-zero regardless of pattern). Use
#      `ps aux | grep` + targeted `kill <pid>` instead of assuming pkill
#      works."  -> every kill below is ps + kill by PID.
#
#   2. "Never touch the user's own live testing sessions - a long-running
#      process you didn't start is very likely the user's own open window."
#      This one was actively dangerous: the previous patterns included bare
#      `keyboard_input` and `./system/renderer`, and BOTH are house-wide
#      names - pieces/system/input_dispatcher/plugins/keyboard_input.c and
#      pieces/display/renderer.c are core house binaries. Killing them
#      would have taken out the user's own open windows.
#
# So: match on THIS project's absolute path only. That is the one string
# that cannot collide with another project, because the path contains the
# project directory name.

OUR_ROOT="$(cd "$SCRIPT_DIR" && pwd)"

# Print the PIDs of our processes: anything whose command line contains our
# own absolute project root. Excludes this script and its own shell.
our_pids() {
    ps -eo pid,args 2>/dev/null       | grep -F "$OUR_ROOT"       | grep -vE "grep -F|horn_chat\.sh" \
      | awk '{print $1}'
}

count_ours() { our_pids | wc -l; }

# TERM first so a turn gets the chance to finish writing, then verify and
# escalate to KILL for whatever is still there.
kill_ours_hard() {
    local pids sig
    for sig in TERM KILL; do
        pids=$(our_pids)
        [ -z "$pids" ] && return 0
        # shellcheck disable=SC2086
        kill -"$sig" $pids 2>/dev/null
        sleep 0.6
    done
}

kill_all() {
    # Layer 1: anything this script's own process group owns.
    kill 0 2>/dev/null
    # Layer 2: the pid ledger, for processes that outlived the group.
    if [ -f "$PROC_LIST" ]; then
        while read -r pid name; do
            [ -n "${pid:-}" ] && kill -TERM "$pid" 2>/dev/null
        done < "$PROC_LIST"
    fi
    # Layer 3: the flag the renderer polls.
    printf 'q' > "$QUIT_FLAG"
    sleep 0.3
    # Layer 4: everything still of ours, by absolute path. This covers the
    # pal module, which chtpm_parser_pal FORKS from the layout's <module>
    # tag and so is not in the launcher's process group - killing the
    # parser alone leaves it tailing the interact relay forever.
    kill_ours_hard
    if [ -f "$PROC_LIST" ]; then
        while read -r pid name; do
            [ -n "${pid:-}" ] && kill -KILL "$pid" 2>/dev/null
        done < "$PROC_LIST"
    fi
}

case "${1:-run}" in
build)
    bash scripts/build.sh
    ;;

clean)
    rm -rf chats/HORN_SESSIONS
    rm -f pieces/apps/player_app/history.txt \
          pieces/apps/player_app/interact_relay.txt \
          pieces/apps/player_app/state.txt \
          pieces/apps/player_app/view.txt \
          pieces/apps/player_app/manager/gui_state.txt \
          pieces/keyboard/history.txt \
          pieces/display/current_frame.txt \
          pieces/display/frame_changed.txt \
          pieces/display/renderer_pulse.txt \
          pieces/horn/last_reply.txt
    : > "$QUIT_FLAG"
    echo "horn_chat: session state cleared (binaries kept)"
    ;;

kill)
    kill_all
    echo "horn_chat: stragglers signalled"
    ;;

send)
    # One turn with no UI. Same code path the pal loop drives, so a
    # successful `send` is real evidence the op wiring works - just
    # without the frame.
    if [ ! -x ops/+x/horn_turn.+x ]; then
        echo "horn_chat: not built yet - run './horn_chat.sh build'" >&2
        exit 1
    fi
    shift
    msg="$*"
    [ -n "$msg" ] || { echo "usage: ./horn_chat.sh send \"<message>\"" >&2; exit 1; }

    gp="pieces/apps/player_app/manager/gui_state.txt"
    mkdir -p "$(dirname "$gp")"
    printf 'horn_prompt=%s\n' "$msg" > "$gp"

    # HORN_FOREGROUND: horn_turn detaches by default so the pal loop can
    # service an approval prompt while a turn waits. `send` wants to block
    # and print the transcript, so it opts out of detaching.
    HORN_FOREGROUND=1 ops/+x/horn_turn.+x
    echo "--- transcript ---"
    cat "$HORN_SESSIONS/transcript.txt" 2>/dev/null
    ;;

run)
    # Refuse to stack on top of a previous session. Killed and confirmed
    # rather than warned about: an orphan left behind here is the exact
    # thing that compounded into the 2.6-hour strays.
    pre=$(count_ours)
    if [ "$pre" -gt 0 ]; then
        echo "horn_chat: $pre process(es) from a previous session are still alive; clearing" >&2
        kill_ours_hard
    fi

    if [ ! -x system/prisc+x ] || [ ! -x ops/+x/horn_turn.+x ]; then
        echo "horn_chat: building..."
        bash scripts/build.sh || exit 1
    fi

    # Fresh session state each launch, but NOT the persistent transcript:
    # that is the user's chat history and survives restarts on purpose.
    : > pieces/apps/player_app/history.txt
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    : > "$PROC_LIST"
    : > "$QUIT_FLAG"
    printf 'horn_prompt=\n' > pieces/apps/player_app/manager/gui_state.txt

    # Publish once before anyone is listening, so the very first frame
    # already shows the model name and turn count instead of blanks.
    ops/+x/horn_publish.+x

    trap 'kill_all; exit 0' INT TERM

    ./system/renderer &
    ./system/keyboard_input &
    echo $$ >> "$PROC_LIST"

    ./system/chtpm_parser_pal "$LAYOUT"

    kill_all
    # Belt and braces: kill_all sweeps, but PDEATHSIG on a detached turn is
    # what actually guarantees this. Sweeping too means an orphan cannot
    # survive even if it was reparented somewhere the sweep does not reach.
    kill_ours_hard
    left=$(count_ours)
    [ "$left" -gt 0 ] && echo "horn_chat: WARNING $left process(es) survived teardown" >&2
    ;;

*)
    echo "usage: ./horn_chat.sh [build|run|send <msg>|clean|kill]" >&2
    exit 1
fi

# Build OpenRouter op if needed
HORN_BIN="$HORN_DIR/ops/horn_chat_openrouter.+x"
if [ ! -f "$HORN_BIN" ] || [ "$HORN_DIR/ops/horn_chat_openrouter.c" -nt "$HORN_BIN" ]; then
    echo "Building horn_chat_openrouter..."
    gcc -o "$HORN_BIN" "$HORN_DIR/ops/horn_chat_openrouter.c" 2>&1
    chmod +x "$HORN_BIN"
fi

# Setup session dir
mkdir -p "$HORN_DIR/.horn-sessions"
HISTORY="$HORN_DIR/.horn-sessions/chat_history.txt"
RELAY="$HORN_DIR/.horn-sessions/relay.txt"
STATE="$HORN_DIR/.horn-sessions/state.txt"

# Init files if not present
touch "$HISTORY" "$RELAY" "$STATE"

# Main loop
cd "$HORN_DIR"

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║       H O R N   C H A T   v 0 . 1       ║"
echo "║     OpenRouter models, terminal UI       ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "Commands: type a prompt, press Enter"
echo "          type @ for completions"
echo "          type q or Ctrl+C to quit"
echo ""

while true; do
    printf "you: "
    read -r user_input || break

    # Exit on 'q' or empty
    if [ "$user_input" = "q" ] || [ "$user_input" = "quit" ]; then
        echo "Goodbye."
        break
    fi

    if [ -z "$user_input" ]; then
        continue
    fi

    # Handle @ completion (simple file listing)
    if [[ "$user_input" == "@"* ]]; then
        echo "  → Files:"
        ls -1 "$HORN_DIR" 2>/dev/null | head -8 | sed 's/^/     /'
        continue
    fi

    # Send to OpenRouter
    printf "horn: "
    if output=$(HORN_DIR="$HORN_DIR" "$HORN_BIN" "$HOUSE_ROOT" "$user_input" 2>&1); then
        echo "$output"
    else
        echo "[error - check API key or network]"
    fi
    echo ""
done

# vim:set et sw=4 ts=4:
