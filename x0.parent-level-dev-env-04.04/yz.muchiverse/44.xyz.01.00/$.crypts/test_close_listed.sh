#!/bin/bash
# test_close_listed.sh - proves close_listed.sh + close_on_restart.pdl on a SCRATCH house with throwaway python sleepers (no real window or process is touched).
# A sleeper's command line carries a path argument, which is what the real windows' command lines carry.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; T="$(mktemp -d)"; fail=0; PIDS=""
cleanup() { for p in $PIDS; do kill -KILL "$p" 2>/dev/null; done; rm -rf "$T"; }; trap cleanup EXIT
ck() { if [ "$2" = ok ]; then echo "PASS|$1"; else echo "FAIL|$1|$3"; fail=1; fi; }
mkdir -p "$T/house" "$T/other"
sleeper() { setsid python3 -c 'import time; time.sleep(300)' "$1" >/dev/null 2>&1 & PIDS="$PIDS $!"; echo $!; }
A="$(sleeper "$T/house/@.apps/hotbar-hq/hotbar-desk.xhtpm")"          # listed + inside the house  -> must close
B="$(sleeper "$T/house/@.apps/hotbar-hq/ops/+x/hotbar_manager.+x")"   # listed (manager row)       -> must close
C="$(sleeper "$T/other/@.apps/hotbar-hq/hotbar-desk.xhtpm")"          # listed substring but ANOTHER house -> must survive
D="$(sleeper "$T/house/@.apps/something-else/window.xhtpm")"          # inside the house but NOT listed    -> must survive
sleep 0.5
OUT="$(sh "$HERE/close_listed.sh" "$T/house" --dry-run)"
echo "$OUT" | grep -q "TERM $A " && echo "$OUT" | grep -q "TERM $B " && ! echo "$OUT" | grep -qE "TERM ($C|$D) " && ck dry-run-lists-only-the-right-ones ok || ck dry-run-lists-only-the-right-ones bad "$OUT"
kill -0 "$A" 2>/dev/null && kill -0 "$B" 2>/dev/null && ck dry-run-kills-nothing ok || ck dry-run-kills-nothing bad "dry run killed something"
sh "$HERE/close_listed.sh" "$T/house" >/dev/null 2>&1; sleep 1.5
! kill -0 "$A" 2>/dev/null && ! kill -0 "$B" 2>/dev/null && ck listed-processes-closed ok || ck listed-processes-closed bad "A alive=$(kill -0 $A 2>/dev/null && echo y || echo n) B alive=$(kill -0 $B 2>/dev/null && echo y || echo n)"
kill -0 "$C" 2>/dev/null && ck other-house-untouched ok || ck other-house-untouched bad "another house's window was closed"
kill -0 "$D" 2>/dev/null && ck unlisted-process-untouched ok || ck unlisted-process-untouched bad "an unlisted process was closed"
# the caller must survive even if its own command line matches: run the closer from a shell whose command line holds a listed substring
sh -c 'sh "$0" "$1" >/dev/null 2>&1; echo alive' "$HERE/close_listed.sh" "$T/house/@.apps/hotbar-hq/hotbar-desk.xhtpm" | grep -q alive && ck caller-survives-own-match ok || ck caller-survives-own-match bad "caller killed"
sh "$HERE/close_listed.sh" /nonexistent >/dev/null 2>&1; [ $? = 0 ] && ck bad-house-is-harmless ok || ck bad-house-is-harmless bad "nonzero exit"
echo "VERDICT|$([ "$fail" = 0 ] && echo PASS || echo FAIL)"; exit "$fail"
