#!/bin/bash
# test_minimize.sh - proves the pc-hq hotbar minimize path on a SCRATCH package dir (the real board and its state are never touched; the manager only READS the house).
# Owner report 2026-10-06: "_" did not minimize. Cause (from the human-input log): the old path showed nothing for ~0.5-1 s, the owner clicked again, and the toggle flipped back.
# Checks: hide is idempotent, show restores, toggle still flips, the manager republishes hb-visible within 300 ms of a change (it used to take up to 400 ms + reparse), two quick clicks stay hidden.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; HOUSE="$(cd "$HERE/../../.." && pwd)"; MGR="${HOTBAR_MGR:-$HERE/+x/hotbar_manager.+x}"; TG="$HERE/hotbar_toggle.sh"   # HOTBAR_MGR=<binary> scores another build (e.g. the previous one)
[ -x "$MGR" ] || { echo "build first: sh build_hotbar_manager.sh" >&2; exit 2; }
T="$(mktemp -d)"; fail=0; MP=""; cleanup() { [ -n "$MP" ] && kill "$MP" 2>/dev/null; rm -rf "$T"; }; trap cleanup EXIT
ck() { if [ "$2" = ok ]; then echo "PASS|$1"; else echo "FAIL|$1|$3"; fail=1; fi; }
mkdir -p "$T/pkg/state/pchq"; UI="$T/pkg/state/pchq/ui.txt"; VIS="$T/pkg/state/pchq/visible.txt"
setsid "$MGR" "$HOUSE/pchq" "$HOUSE" "$T/pkg" >/dev/null 2>&1 & MP=$!
for i in $(seq 1 40); do [ -s "$UI" ] && break; sleep 0.1; done
[ -s "$UI" ] && ck manager-publishes-ui ok || { ck manager-publishes-ui bad "no ui.txt after 4 s (no holder?)"; echo "VERDICT|FAIL"; exit 1; }
hbvis() { sed -n 's/^hb_visible=//p' "$UI"; }
now_ms() { date +%s%3N; }
wait_vis() {   # wait_vis <want> -> prints latency in ms, or 9999
    local t0 t i; t0=$(now_ms); for i in $(seq 1 200); do [ "$(hbvis)" = "$1" ] && { echo $(( $(now_ms) - t0 )); return; }; sleep 0.01; done; echo 9999; }
[ "$(hbvis)" = 1 ] && ck starts-visible ok || ck starts-visible bad "hb_visible=$(hbvis)"
sh "$TG" pchq "$T/pkg" hide; L="$(wait_vis 0)"; [ "$L" -lt 300 ] && ck hide-published-within-300ms ok || ck hide-published-within-300ms bad "${L} ms"
sh "$TG" pchq "$T/pkg" hide; sh "$TG" pchq "$T/pkg" hide; sleep 0.8; [ "$(hbvis)" = 0 ] && ck repeated-hide-stays-hidden ok || ck repeated-hide-stays-hidden bad "hb_visible=$(hbvis)"
sh "$TG" pchq "$T/pkg" show; L="$(wait_vis 1)"; [ "$L" -lt 300 ] && ck show-published-within-300ms ok || ck show-published-within-300ms bad "${L} ms"
sh "$TG" pchq "$T/pkg" toggle; L="$(wait_vis 0)"; [ "$L" -lt 300 ] && ck toggle-still-flips ok || ck toggle-still-flips bad "${L} ms"
sh "$TG" pchq "$T/pkg" show; sleep 0.8; sh "$TG" pchq "$T/pkg" hide; sleep 0.28; sh "$TG" pchq "$T/pkg" hide; sleep 0.8   # the owner's two clicks 280 ms apart
[ "$(hbvis)" = 0 ] && ck two-clicks-280ms-apart-stay-hidden ok || ck two-clicks-280ms-apart-stay-hidden bad "hb_visible=$(hbvis)"
[ "$(sed -n 's/^visible=//p' "$VIS")" = 0 ] && ck state-file-agrees ok || ck state-file-agrees bad "$(cat "$VIS")"
echo "VERDICT|$([ "$fail" = 0 ] && echo PASS || echo FAIL)"; exit "$fail"
