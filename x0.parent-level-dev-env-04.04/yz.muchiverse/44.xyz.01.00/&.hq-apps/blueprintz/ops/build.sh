#!/bin/sh
# build.sh - compile the blueprintz ops into ops/+x/ (plain cc, no external libs).
HERE="$(cd "$(dirname "$0")" && pwd)"; mkdir -p "$HERE/+x"; rc=0
for n in bp_ctl bp_scan; do
  if cc -O2 -Wall -o "$HERE/+x/$n.+x" "$HERE/$n.c" 2>"$HERE/+x/$n.err"; then echo "OK $n"; [ -s "$HERE/+x/$n.err" ] && cat "$HERE/+x/$n.err"; else echo "FAILED $n"; cat "$HERE/+x/$n.err"; rc=1; fi; rm -f "$HERE/+x/$n.err"; done
exit $rc
