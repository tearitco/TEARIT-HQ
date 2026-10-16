#!/bin/bash
# browser_pid.sh - print the PID of the live network-browser renderer.
#
# WHY THIS EXISTS, and why every suite used to do it by hand:
#
# The obvious `pgrep -f 'khtpm_core_render.+x .*network-browser-hq'` is
# wrong twice over, and both failures read as "the feature is broken":
#
#  1. pgrep -f matches ANY process whose command line CONTAINS the
#     pattern - including the shell that is running the search. An agent
#     (or a suite) whose own command line mentions the pattern gets
#     counted as a renderer. Measured 2026-10-13: a browser count came
#     back as 3 with exactly ONE renderer alive; the other two were the
#     searching shell and the tool process running it.
#  2. A relay write to a PID nobody reads is silently lost, so a suite
#     that picks the wrong renderer clicks nothing, tints nothing and
#     reports four FAILs that have nothing to do with the code.
#
# So: match on the EXECUTABLE (via /proc/<pid>/exe, which cannot be
# faked by a command line) AND on the window template in argv, and
# require the window's ascii mirror to exist - a renderer that has never
# drawn has no mirror and cannot answer a relay.
#
# Usage:
#   PID="$(sh browser_pid.sh)"        # newest live renderer, or empty
#   sh browser_pid.sh --require-one   # fail loudly unless exactly one
#   sh browser_pid.sh --quiet         # print nothing, just set the exit code
#
# Exit: 0 a pid was printed, 1 none (or not exactly one with
# --require-one).
set -u
CHTPM_MATCH='network-browser-hq'
QUIET=0
REQUIRE_ONE=0
for a in "$@"; do
    case "$a" in
        --quiet) QUIET=1 ;;
        --require-one) REQUIRE_ONE=1 ;;
    esac
done

pids="$(python3 - <<PY
import os
want = "$CHTPM_MATCH"
found = []
for pid in os.listdir('/proc'):
    if not pid.isdigit():
        continue
    try:
        exe = os.path.basename(os.readlink('/proc/%s/exe' % pid)).replace(' (deleted)', '')
        argv = open('/proc/%s/cmdline' % pid, 'rb').read().split(b'\\0')
    except OSError:
        continue
    if 'khtpm_core_render' not in exe:
        continue
    args = [a.decode('utf-8', 'replace') for a in argv if a]
    if not any(want in a for a in args):
        continue
    found.append(int(pid))
found.sort()
print(' '.join(str(p) for p in found))
PY
)"

# REAL FIX 2026-10-16: this counted LINES, but $pids is a single line of
# space-separated numbers, so `grep -c .` returned 1 for ANY number of
# renderers. --require-one was therefore a guard that could never fail - it
# passed while two browser windows were alive, which is worse than no guard
# because it still reads as a passing check. Count WORDS.
n="$(printf '%s\n' "$pids" | wc -w | tr -d ' ')"
if [ "$REQUIRE_ONE" = "1" ] && [ "$n" != "1" ]; then
    [ "$QUIET" = "1" ] || echo "browser_pid.sh: expected exactly 1 live renderer, found $n (${pids:-none})" >&2
    exit 1
fi
# REAL FIX 2026-10-16: this printed the WHOLE space-joined list, not one pid.
# `printf '%s\n' "$pids" | tail -1` looks like "last of many lines", but $pids
# is ONE line of space-separated numbers, so tail -1 returned all of them.
# Every caller does PID="$(sh browser_pid.sh)" and then builds a relay path
# from it, so with several renderers alive they built
# entity_menu_history/1612545 1942491 ...txt, the relay write went to a path
# that does not exist, and G2.2 died with "no frame dump after relay 112" -
# a harness failure dressed up as a browser failure.
#
# Highest pid is the best available "newest" signal; it is a proxy, not a
# start time, and the --require-one mode is the honest way to insist on
# there being exactly one.
[ "$QUIET" = "1" ] && exit 0
[ -z "$pids" ] && exit 0
if [ "$n" != "1" ]; then
    echo "browser_pid.sh: WARNING $n renderers alive (${pids}); using the newest" >&2
fi
printf '%s\n' "$pids" | tr ' ' '\n' | grep . | sort -n | tail -1
exit 0