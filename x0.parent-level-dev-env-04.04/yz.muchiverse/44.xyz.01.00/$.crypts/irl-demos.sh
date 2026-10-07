#!/bin/bash
# irl-demos.sh - read the owner's recorded demonstrations (#.desktop/human_input/<pid>.txt, written by the renderer ONLY from real X key/click events).
# Merges every window's log, sorted by time, one readable line per human action with its context. Read-only; changes nothing.
# Usage: irl-demos.sh [-n N] [--gaps]     -n N = last N actions (default 40);  --gaps = also print the pause (ms) before each action
# Line format in the logs (see khtpm_core_render.c kh_human_log):
#   <epoch_ms>|<pid>|<window label>|KEY|<code>|focus=<nav>|id=<element id>|label=<element label>
#   <epoch_ms>|<pid>|<window label>|CLICK|<button>|<x>|<y>|nav=<n>|id=<element id>|act=<its onclick>|label=<its label>
set -u
HOUSE="$(cd "$(dirname "$0")/.." && pwd)"
N=40; GAPS=0
while [ $# -gt 0 ]; do case "$1" in -n) N="${2:-40}"; shift 2 ;; --gaps) GAPS=1; shift ;; *) echo "usage: irl-demos.sh [-n N] [--gaps]" >&2; exit 2 ;; esac; done
DIR="$HOUSE/#.desktop/human_input"
[ -d "$DIR" ] && ls "$DIR"/*.txt >/dev/null 2>&1 || { echo "no human input recorded yet ($DIR)"; exit 0; }
cat "$DIR"/*.txt | sort -t'|' -k1,1n | tail -n "$N" | awk -F'|' -v gaps="$GAPS" '
function key(c) { if (c>=32 && c<=126) return sprintf("%c", c); if (c==13) return "Enter"; if (c==27) return "Esc"; if (c==9) return "Tab"; if (c==8) return "Backspace";
                  if (c==200) return "Up"; if (c==201) return "Down"; if (c==202) return "Left"; if (c==203) return "Right"; return "key#" c }
{
  t = strftime("%H:%M:%S", $1/1000); gap = (prev ? $1 - prev : 0); prev = $1;
  g = gaps ? sprintf("  (+%dms)", gap) : "";
  if ($4 == "KEY")        printf "%s  %-22s KEY   %-9s %s %s %s%s\n", t, $3, key($5), $6, $7, $8, g;
  else if ($4 == "CLICK") printf "%s  %-22s CLICK b%s @%s,%s  %s %s %s %s%s\n", t, $3, $5, $6, $7, $8, $9, $10, $11, g;
  else                    printf "%s  %-22s %s\n", t, $3, $0;
}'
