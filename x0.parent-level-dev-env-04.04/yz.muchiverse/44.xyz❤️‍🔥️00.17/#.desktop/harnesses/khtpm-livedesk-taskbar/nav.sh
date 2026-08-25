#!/bin/bash
# nav.sh - agent-relay harness for the NEW khtpm layout-based taskbar
# (khtpm_strip_parser.c), modeled directly on
# #.desktop/harnesses/livedesk-taskbar/nav.sh (the legacy tp_taskbar.c
# harness), reviewed in full before writing this — same relay file, same
# bare-decimal-ASCII-per-line contract, same command shape. Ported rather
# than reinvented (2026-08-11, direct instruction: "there were some test
# harnesses for legacy desk, u may review them and make ur own for this
# new desk").
#
# The relay itself (#.desktop/livedesk_agent_relay.txt) is the SAME file
# legacy's harness uses — khtpm_strip_parser.c grew its own
# poll_agent_relay()/dispatch_key_code() reading it this same session (see
# that file's own header comment for the full contract: digits 48-57,
# Enter=13, Escape=27, Backspace=8, printable 32-126, no arrow codes).
#
# Difference from legacy's harness: khtpm has ONE combined frame-history
# log (#.desktop/khtpm_strip_frame_history.txt), not separate
# strip_frame_log.txt/popup_frame_log.txt — 'frame' replaces 'cells'/
# 'popups' here.
#
# Commands:
#   nav.sh nav <n>            type digits <n>, Enter -> global nav jump or
#                              (if a submenu is open) select+run that
#                              submenu row — server-side state (which one
#                              applies) resolves this, same as legacy
#   nav.sh row <n>             alias of 'nav' (see above — khtpm's own
#                              digit_buf/hq_focus split decides which
#                              applies, matching legacy's own "context
#                              resolved server-side" design)
#   nav.sh key <name>         send one key: Return/Enter, Escape/Esc,
#                              BackSpace, or a single literal character
#   nav.sh esc                send Escape
#   nav.sh type <text>        type each character of <text> in turn (for
#                              cli-io text entry, e.g. a save-as name) -
#                              does NOT send a trailing Enter; call
#                              'nav.sh key Return' after if you want to
#                              commit
#   nav.sh frame               print last frame-history line
#   nav.sh wait [sec]           sleep (default 0.6)
#
#   nav.sh hqcell <n>          manager-relay-level: inject the RESOLVED
#                              header-cell-click code (4000+n, see
#                              khtpm_strip_codes.h's KSC_HQ_HEADER_BASE)
#                              directly into strip_history.txt instead of
#                              a raw keycode into livedesk_agent_relay.txt.
#   nav.sh mgrcode <code>      manager-relay-level: inject any raw decimal
#                              code straight into strip_history.txt.
#
# TWO RELAY LAYERS (real architecture, found live 2026-08-11 debugging why
# `nav.sh nav 2` didn't open the USER header cell — it instead jumped a
# DIFFERENT, unrelated nav-claim number, because digit+Enter over THIS
# relay drives ktb_digit_push()/ktb_jump_nav()'s nav-CLAIM system, not
# header-cell indices at all):
#   1) PARSER layer (`nav`/`row`/`key`/`esc`/`type`, this file's original
#      contract) — raw ASCII-ish keycodes into livedesk_agent_relay.txt,
#      read by khtpm_strip_parser.c, which resolves clicks/keys locally
#      (hit-testing, ACTIVATE scope, etc.) and forwards a RESOLVED action
#      code to strip_history.txt for the manager. Real header-cell arrow
#      navigation (ktb_nav_arm + KSC_FOCUS_LEFT/RIGHT) happens as REAL X11
#      key events inside the parser's own event loop and has NO file-relay
#      equivalent — arrow keys are explicitly unsupported over this file
#      (see poll_agent_relay()'s own contract: digits/Enter/Escape/
#      Backspace/printable only, no arrow codes).
#   2) MANAGER layer (`hqcell`/`mgrcode`, added 2026-08-11) — inject an
#      already-resolved code straight into strip_history.txt, the SAME
#      file/format/dispatch_code() path a real click's resolved output
#      would use. This is still real relay/IPC injection into the actual
#      production file boundary between the two real binaries — NOT a
#      shortcut into either binary's internals — it's the correct way to
#      reach header-cell ACTIVATE (KSC_HQ_HEADER_BASE=4000+which) and any
#      other manager-side code when the parser-layer contract has no path
#      there (e.g. no arrow-key relay support).
#
# Env: HOUSE=<house_root> (defaults to $PWD)

set -u
HOUSE="${HOUSE:-$PWD}"
RELAY="$HOUSE/#.desktop/livedesk_agent_relay.txt"
MGR_RELAY="$HOUSE/#.desktop/strip_history.txt"
FRAME_LOG="$HOUSE/#.desktop/khtpm_strip_frame_history.txt"

# One decimal ASCII code per line - exact contract of poll_agent_relay()
# in khtpm_strip_parser.c. The parser polls on a ~300ms tick (matches
# legacy's own POLL_INTERVAL_USEC=300000), so a small sleep after each
# send lets it actually get consumed before the next one.
send_code() {
  echo "$1" >> "$RELAY"
  sleep 0.35
}

# Digits accumulate server-side (manager's own digit_buf/hq_digit_accum)
# regardless of pacing, so these can be sent back-to-back with a shorter
# gap; only the FINAL Enter needs the full settle time.
send_digits() {
  local n="$1" i c
  for ((i = 0; i < ${#n}; i++)); do
    c="${n:$i:1}"
    echo -n "${c}" | od -An -tu1 | tr -d ' ' >> "$RELAY"
    printf '\n' >> "$RELAY"
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
    echo "usage: nav.sh {nav <n>|row <n>|key <name>|esc|type <text>|frame [n]|wait [sec]|hqcell <n>|mgrcode <code>}" >&2
    exit 1
    ;;
esac
