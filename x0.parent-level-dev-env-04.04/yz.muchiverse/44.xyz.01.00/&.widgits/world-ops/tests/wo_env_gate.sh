#!/bin/sh
# wo_env_gate.sh - gate of B3: events/environment (wo each -> env_hit: run life hurt, log row) leaves the same files as the C `env apply` (frozen reference binary). Needs the scratch copy @.apps/rpg-space-gate
# with events/env_hit; env_pending.txt rows are slot|hp|kind|room|value. INTERIM .sh (port to a pal harness).
H="$(cd "$(dirname "$0")/../../.." && pwd)"; S="${S:-/tmp}"; G="$H/@.apps/rpg-space-gate"; REF="$G/ops/+x/rpg_space.+x"; PLAY="$H/&.widgits/events-hq/ops/play_event.sh"
reset() { for c in harold ember glacine; do printf 'hp=%s\nhunger=0\ndead=0\ndeaths=0\ninv=\n' "$1" > $G/state/life_$c.txt; done; : > $G/state/events.txt; printf '%s\n' "$2" > $G/state/env_pending.txt; }
snap() { for c in harold ember glacine; do echo "== $c"; cat $G/state/life_$c.txt; done; echo "== pending"; cat $G/state/env_pending.txt; echo "== events"; cut -d'|' -f1,3- $G/state/events.txt; }
play() { MUCHI_TARGET_ENT="$G/state/world" bash "$PLAY" "$G/events/environment" "$H" parallel >/dev/null 2>&1; }
ok=0; for rows in "0|13|CHOKE|bridge|9" "2|2|FREEZE|hold|0
0|13|CHOKE|bridge|9
1|2|SCORCH|engine|60" "1|1|CHOKE|hold|4"; do
  reset 40 "$rows"; "$REF" env apply >/dev/null 2>&1; snap > $S/A_e.txt; reset 40 "$rows"; play; snap > $S/B_e.txt
  if cmp -s $S/A_e.txt $S/B_e.txt; then echo "rows=$(echo "$rows" | wc -l) SAME"; else echo "rows=$(echo "$rows" | wc -l) DIFF"; diff $S/A_e.txt $S/B_e.txt | head -8; fi; done
