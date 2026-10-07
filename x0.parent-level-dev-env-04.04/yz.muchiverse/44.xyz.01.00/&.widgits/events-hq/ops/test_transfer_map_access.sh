#!/bin/bash
# test_transfer_map_access.sh - mr_transfer_desk.+x map access in play modes, on a SCRATCH house (the real house, its entities and its play flag are never touched).
# Design: GAME-SETUP-PDL-DESIGN.md. Build first: sh build_mr_event_ops.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; BIN="${TRANSFER_BIN:-$HERE/+x/mr_transfer_desk.+x}"
[ -x "$BIN" ] || { echo "build first: sh build_mr_event_ops.sh" >&2; exit 2; }
H="$(mktemp -d)"; trap 'rm -rf "$H"' EXIT; fail=0
ck() { if [ "$2" = ok ]; then echo "PASS|$1"; else echo "FAIL|$1|$3"; fail=1; fi; }
mkdir -p "$H/0.user-pal-test/00.login-signup" "$H/#.desktop"
printf 'current_user_uuid | u1\n' > "$H/0.user-pal-test/00.login-signup/current_login.txt"
S="$H/xyzfs/users/u1/home/livedesk/sessions/s1"; mkdir -p "$S/desks"
printf 'STATE | name | g\nSTATE | active_desk | a\n' > "$S/session.pdl"
: > "$S/desks/a.pdl"; : > "$S/desks/town.pdl"; : > "$S/desks/cave.pdl"; : > "$S/desks/secret.pdl"
active() { sed -n 's/.*active_desk *| *//p' "$S/session.pdl" | tr -d ' \r'; }
run() { "$BIN" "$H" s1 "$1" 2>&1; echo "rc=$?"; }
mode() { printf 'mode=%s\n' "$1" > "$H/#.desktop/khtpm_play_mode.state.txt"; }
LED="$H/#.desktop/game_access_ledger.txt"

# 1 legacy: no game.pdl, play ON: allowed (existing houses unchanged)
mode on; out=$(run cave); [ "$(active)" = cave ] && echo "$out" | grep -q 'rc=0' && ck legacy-no-game-pdl-play-allowed ok || ck legacy-no-game-pdl-play-allowed bad "$out / active=$(active)"
printf 'STATE | active_desk | a\n' > "$S/session.pdl"
# 2 game.pdl with MAP rows, play ON
printf 'MAP | town | available | 1\nMAP | a | available | 1\n' > "$S/game.pdl"
out=$(run secret); echo "$out" | grep -q 'rc=3' && echo "$out" | grep -q 'refused: map .secret. is not available' && ck play-unlisted-refused-rc3-with-reason ok || ck play-unlisted-refused-rc3-with-reason bad "$out"
[ "$(active)" = a ] && ck refused-leaves-player-where-they-are ok || ck refused-leaves-player-where-they-are bad "active=$(active)"
grep -q '|refused-map|play|secret|' "$LED" 2>/dev/null && ck refusal-ledgered ok || ck refusal-ledgered bad "$(cat "$LED" 2>/dev/null)"
out=$(run town); echo "$out" | grep -q 'rc=0' && [ "$(active)" = town ] && ck play-listed-allowed ok || ck play-listed-allowed bad "$out / active=$(active)"
# 3 build mode ignores the list
printf 'STATE | active_desk | a\n' > "$S/session.pdl"; mode off
out=$(run secret); echo "$out" | grep -q 'rc=0' && [ "$(active)" = secret ] && ck build-mode-unrestricted ok || ck build-mode-unrestricted bad "$out / active=$(active)"
# 4 missing mode file = build
printf 'STATE | active_desk | a\n' > "$S/session.pdl"; rm -f "$H/#.desktop/khtpm_play_mode.state.txt"
out=$(run cave); echo "$out" | grep -q 'rc=0' && ck no-mode-file-is-build ok || ck no-mode-file-is-build bad "$out"
# 5 file with no MAP rows: unrestricted
printf 'STATE | active_desk | a\n' > "$S/session.pdl"; mode on; printf 'GAME | title | t\n' > "$S/game.pdl"
out=$(run cave); echo "$out" | grep -q 'rc=0' && ck game-pdl-without-map-rows-unrestricted ok || ck game-pdl-without-map-rows-unrestricted bad "$out"
echo "VERDICT|$([ "$fail" = 0 ] && echo PASS || echo FAIL)"; exit "$fail"
