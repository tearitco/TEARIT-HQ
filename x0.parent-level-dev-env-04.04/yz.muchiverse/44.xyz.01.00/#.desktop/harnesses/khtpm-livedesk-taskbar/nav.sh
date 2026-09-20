#!/bin/bash
# nav.sh - agent-relay harness for the khtpm taskbar strip and HQ windows.
#
# 2026-09-19 RETARGET (BUG-LOG "nav.sh's primary test commands ... silent
# no-ops"): khtpm_strip_parser.c and its poll_agent_relay() - the only reader
# of #.desktop/livedesk_agent_relay.txt - were folded into khtpm_core_render.c
# (2026-09-01/06, see khtpm_strip_keyboard_ascii.c's header), so that file has
# NO reader any more. The commands below now write to the LIVE paths:
#
#   default (taskbar strip):  #.desktop/strip_history.txt, one bare decimal
#     code per line, read by khtpm_taskbar_manager_main.c poll_strip_history()
#     -> dispatch_code(). It still handles ASCII digits 48-57, Enter 13,
#     Escape 27, Backspace 8, printable 32-126, KSC_FOCUS_LEFT/RIGHT 1001/1002,
#     KSC_HQ_HEADER_BASE 4000+n, KSC_HQ_ITEM_BASE 5000+n.
#
#   NAV_PID=<pid> (any khtpm_core_render.c / khtpm_entity window):
#     #.desktop/entity_menu_history/<pid>.txt, `KEY_PRESSED: <decimal>` /
#     `MOUSE_EVENT: <button> <x> <y> <is_press>` / `STRING: <text>` lines.
#     Find the pid with `ps -eo pid,args | grep khtpm_core_render`. Arrows are
#     200/201/202/203 (Up/Down/Left/Right), PageUp/Down 204/205. Dispatch does
#     not need X focus (see RELAY-WINDOW-TARGETING-DESIGN.md).
#     NOTE digits: the code for the character '5' is 53, not 5 (`nav`/`key`
#     do this for you).
#
# Commands:
#   nav.sh nav <n> | row <n>   type digits <n>, then Enter (global nav jump, or
#                              select that row of an open menu)
#   nav.sh key <name>          Return/Enter, Escape/Esc, BackSpace, Up/Down/
#                              Left/Right/PageUp/PageDown (NAV_PID mode only),
#                              or a single character
#   nav.sh esc                 Escape
#   nav.sh type <text>         each char in turn (no trailing Enter)
#   nav.sh click <x> <y> [b]   NAV_PID only: press+release (b default 1;
#                              b=3 opens the window's context menu)
#   nav.sh string <text>       NAV_PID only: `STRING: <text>` (Cli-io text
#                              commands, e.g. `string mv 25 26`)
#   nav.sh frame [n]           last n lines of the strip frame history
#   nav.sh wait [sec]
#   nav.sh hqcell <n>          strip: resolved header-cell click (4000+n)
#   nav.sh mgrcode <code>      strip: any resolved decimal code
#
# Env: HOUSE=<house_root> (defaults to $PWD), NAV_PID=<pid> (optional)

set -u
HOUSE="${HOUSE:-$PWD}"
MGR_RELAY="$HOUSE/#.desktop/strip_history.txt"
NAV_PID="${NAV_PID:-}"
if [ -n "$NAV_PID" ]; then
  RELAY="$HOUSE/#.desktop/entity_menu_history/$NAV_PID.txt"
else
  RELAY="$MGR_RELAY"
fi
FRAME_LOG="$HOUSE/#.desktop/khtpm_strip_frame_history.txt"

# One code per line. Strip mode: bare decimal. Window mode: KEY_PRESSED: <dec>.
emit_code() {
  if [ -n "$NAV_PID" ]; then echo "KEY_PRESSED: $1" >> "$RELAY"; else echo "$1" >> "$RELAY"; fi
}
send_code() {
  emit_code "$1"
  sleep 0.35
}

# Digits accumulate server-side (manager's own digit_buf/hq_digit_accum)
# regardless of pacing, so these can be sent back-to-back with a shorter
# gap; only the FINAL Enter needs the full settle time.
send_digits() {
  local n="$1" i c
  for ((i = 0; i < ${#n}; i++)); do
    c="${n:$i:1}"
    emit_code "$(printf '%d' "'$c")"
    sleep 0.1
  done
}

send_char() {
  local code
  code=$(printf '%d' "'$1")
  send_code "$code"
}

cmd_nav_or_row() {
  touch "$RELAY"
  send_digits "$1"
  # Real timing note (2026-08-11, found live: a bare digit+Enter with
  # legacy's own 0.1s inter-digit gap sent Enter before the digit had
  # round-tripped, activating the WRONG (previous) focus target). khtpm's
  # architecture is a two-process relay: agent -> parser -> manager (next
  # ~300ms poll) -> parser (next ~300ms poll) before the digit's effect on
  # focus is even visible again — legacy is single-process, so its own
  # nav.sh never needed this extra settle. Bump the pre-Enter pause well
  # past two full poll cycles.
  sleep 0.8
  send_code 13   # Enter
}

case "${1:-}" in
  nav)
    cmd_nav_or_row "${2:?usage: nav.sh nav <n>}"
    ;;
  row)
    cmd_nav_or_row "${2:?usage: nav.sh row <n>}"
    ;;
  key)
    k="${2:?usage: nav.sh key <Return|Escape|BackSpace|char>}"
    case "$k" in
      Return|Enter) send_code 13 ;;
      Escape|Esc)   send_code 27 ;;
      BackSpace)    send_code 8 ;;
      Up)           send_code 200 ;;
      Down)         send_code 201 ;;
      Left)         send_code 202 ;;
      Right)        send_code 203 ;;
      PageUp)       send_code 204 ;;
      PageDown)     send_code 205 ;;
      [0-9])        send_code "$(printf '%d' "'$k")" ;;
      ?)            send_char "$k" ;;
      *)            echo "key: unrecognized '$k' (use Return/Escape/BackSpace or a single char)" >&2; exit 1 ;;
    esac
    ;;
  esc)
    send_code 27
    ;;
  type)
    text="${2:?usage: nav.sh type <text>}"
    for ((i = 0; i < ${#text}; i++)); do
      send_char "${text:$i:1}"
    done
    ;;
  click)
    [ -n "$NAV_PID" ] || { echo "click needs NAV_PID=<pid>" >&2; exit 1; }
    x="${2:?usage: nav.sh click <x> <y> [button]}"; y="${3:?usage: nav.sh click <x> <y> [button]}"; b="${4:-1}"
    printf 'MOUSE_EVENT: %s %s %s 1\nMOUSE_EVENT: %s %s %s 0\n' "$b" "$x" "$y" "$b" "$x" "$y" >> "$RELAY"
    sleep 0.5
    ;;
  string)
    [ -n "$NAV_PID" ] || { echo "string needs NAV_PID=<pid>" >&2; exit 1; }
    printf 'STRING: %s\n' "${2:?usage: nav.sh string <text>}" >> "$RELAY"
    sleep 0.5
    ;;
  frame)
    tail -n "${2:-1}" "$FRAME_LOG" 2>/dev/null
    ;;
  wait)
    sleep "${2:-0.6}"
    ;;
  hqcell)
    n="${2:?usage: nav.sh hqcell <n>  (n = header cell 'which', e.g. 2 = USER)}"
    echo "$((4000 + n))" >> "$MGR_RELAY"
    sleep 0.5
    ;;
  mgrcode)
    code="${2:?usage: nav.sh mgrcode <decimal code>}"
    echo "$code" >> "$MGR_RELAY"
    sleep 0.5
    ;;
  *)
    echo "usage: nav.sh {nav <n>|row <n>|key <name>|esc|type <text>|click <x> <y> [b]|string <text>|frame [n]|wait [sec]|hqcell <n>|mgrcode <code>}" >&2
    exit 1
    ;;
esac
