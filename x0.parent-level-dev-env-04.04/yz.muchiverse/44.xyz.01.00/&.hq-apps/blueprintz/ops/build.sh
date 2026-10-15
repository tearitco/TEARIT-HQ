#!/bin/sh
# build.sh - compile the blueprintz ops into ops/+x/ (plain cc, no external libs).
HERE="$(cd "$(dirname "$0")" && pwd)"; mkdir -p "$HERE/+x"; rc=0
for n in bp_ctl bp_scan bp_req; do
  if cc -O2 -Wall -o "$HERE/+x/$n.+x" "$HERE/$n.c" 2>"$HERE/+x/$n.err"; then echo "OK $n"; [ -s "$HERE/+x/$n.err" ] && cat "$HERE/+x/$n.err"; else echo "FAILED $n"; cat "$HERE/+x/$n.err"; rc=1; fi; rm -f "$HERE/+x/$n.err"; done
# the window manager lives at the app root (+x/), like sfx-hai
APP="$(dirname "$HERE")"; mkdir -p "$APP/+x"
if cc -O2 -Wall -o "$APP/+x/blueprintz_manager.+x" "$APP/blueprintz_manager.c" 2>"$APP/+x/m.err"; then echo "OK blueprintz_manager"; [ -s "$APP/+x/m.err" ] && cat "$APP/+x/m.err"; else echo "FAILED blueprintz_manager"; cat "$APP/+x/m.err"; rc=1; fi; rm -f "$APP/+x/m.err"
exit $rc
