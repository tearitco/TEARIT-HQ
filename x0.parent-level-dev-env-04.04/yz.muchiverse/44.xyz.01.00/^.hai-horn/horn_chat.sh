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
    # Layer 4: the pal module. chtpm_parser_pal FORKS this from the
    # layout's <module> tag, so it is the parser's child, not the
    # orchestrator's - killing the parser leaves it running and it goes on
    # tailing the interact relay forever.
    #
    # Match on the .pal argument only. pkill -f takes an EXTENDED REGEX,
    # so a pattern containing the binary's own name would treat the '+' in
    # "prisc+x" as a quantifier ("prisc" + one-or-more "c" + "x") and
    # match nothing - which is why this layer silently did nothing until
    # the pattern was narrowed to a plain substring.
    pkill -TERM -f "horn_main_loop.pal" 2>/dev/null
    sleep 0.3
    pkill -KILL -f "horn_main_loop.pal" 2>/dev/null
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
    ;;

*)
    echo "usage: ./horn_chat.sh [build|run|send <msg>|clean|kill]" >&2
    exit 1
    ;;
esac