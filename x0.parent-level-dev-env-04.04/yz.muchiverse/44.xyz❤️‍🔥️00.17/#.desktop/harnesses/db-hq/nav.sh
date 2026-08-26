#!/bin/bash
# nav.sh - agent history harness for db-hq (khtpm_entity_menu_render.c,
# the merged binary - the old standalone khtpm_hq_render.c was archived
# during the manager rebuild), same bare-decimal-ASCII-per-line contract
# as #.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh (reviewed before
# writing this; ported not reinvented, per au11-hq/_.0.aigent-testing-
# k9.txt's documented "third option" for raw-Xlib programs). db-hq's own
# history file is #.desktop/db_hq_history.txt (renamed 2026-08-25 from
# db_hq_agent_relay.txt - same append-only/cursor-based reader as always,
# only the name changed), consumed by
# poll_agent_history()/dispatch_relay_code() in khtpm_entity_menu_render.c.
# A line starting with '#' is an audit comment - it advances the read
# cursor but is never dispatched as a command.
#
# Unlike the taskbar (two-process parser+manager relay, ~300ms manager
# poll cycle), db-hq is a single process polling its own relay every
# ~150ms - shorter settle times are enough here.
#
# Commands:
#   nav.sh jump <n>            type digits <n>, Enter - moves focus live
#                               as digits are typed (do_jump), then Enter
#                               activates the focused nav item
#   nav.sh key <name>          send one key: Return/Enter, Escape/Esc,
#                               BackSpace, or a single literal character
#   nav.sh esc                 send Escape
#   nav.sh close                convenience: jump to nav 1 (chrome close
#                               button is always nav 1) + Enter
#
# Env: HOUSE=<house_root> (defaults to $PWD)

set -u
HOUSE="${HOUSE:-$PWD}"
RELAY="$HOUSE/#.desktop/db_hq_history.txt"

send_code() {
  echo "$1" >> "$RELAY"
  sleep 0.2
}

send_digits() {
  local n="$1" i c
  for ((i = 0; i < ${#n}; i++)); do
    c="${n:$i:1}"
    echo -n "${c}" | od -An -tu1 | tr -d ' ' >> "$RELAY"
    printf '\n' >> "$RELAY"
    sleep 0.05
  done
}

send_char() {
  local code
  code=$(printf '%d' "'$1")
  send_code "$code"
}

case "${1:-}" in
  jump)
    touch "$RELAY"
    send_digits "${2:?usage: nav.sh jump <n>}"
    sleep 0.3
    send_code 13   # Enter
    ;;
  key)
    k="${2:?usage: nav.sh key <Return|Escape|BackSpace|char>}"
    case "$k" in
      Return|Enter) send_code 13 ;;
      Escape|Esc)   send_code 27 ;;
      BackSpace)    send_code 8 ;;
      ?)            send_char "$k" ;;
      *)            echo "key: unrecognized '$k'" >&2; exit 1 ;;
    esac
    ;;
  esc)
    send_code 27
    ;;
  close)
    touch "$RELAY"
    send_code 49  # '1' - chrome close is always nav 1
    sleep 0.3
    send_code 13
    ;;
  *)
    echo "usage: nav.sh {jump <n>|key <name>|esc|close}" >&2
    exit 1
    ;;
esac
