#!/bin/sh
# wo_world_gate.sh - gate of B2: the destroyed PAGE (world-ops nodes) leaves the same files as the C verb `destroy last` (frozen reference binary, git tag rpg-space-pre-blueprint).
# Needs the scratch copy @.apps/rpg-space-gate (see wo_life_gate.sh). Runs every case at damaged-chance 0 and 100 (the roll is random in both).  INTERIM .sh, port to a pal harness.
H="$(cd "$(dirname "$0")/../../.." && pwd)"; S="${S:-/tmp}"; G="$H/@.apps/rpg-space-gate"; REF="$G/ops/+x/rpg_space.+x"; PLAY="$H/&.widgits/events-hq/ops/play_event.sh"
reset() { # $1 kind  $2 amount
  printf '%s|7|6|%s|ore\n' bridge 4 > $G/state/damage.txt; printf 'bridge|7|6|%s|%s\nbridge|3|10|crystal|2\n' "$1" "$2" > $G/state/nodes.txt
  printf '1|bridge|2|2|3|scrap|0|0|x\n' > $G/state/drops.txt; : > $G/state/events.txt; printf '%s|bridge|7|6|beam\n' "$1" > $G/state/destroyed.txt; echo harold > $G/state/life_actor.txt; }
snap() { for f in drops damage nodes; do echo "== $f"; cat $G/state/$f.txt; done; echo "== events"; cut -d'|' -f1,3- $G/state/events.txt; }
play() { MUCHI_TARGET_ENT="$G/state/world" bash "$PLAY" "$G/events/destroyed" "$H" parallel >/dev/null 2>&1; }
for pct in 0 100; do for case in "ore 4" "ore 1" "rock -1" "crystal 2"; do set -- $case
  sed -i "s/^\(BLOCK *| *$1 *|[^|]*|[^|]*|[^|]*|\) *[0-9]* /\1 $pct /" $G/blocks.pdl
  reset $1 $2; "$REF" destroy last >/dev/null 2>&1; snap > $S/A_w.txt; reset $1 $2; play; snap > $S/B_w.txt
  if cmp -s $S/A_w.txt $S/B_w.txt; then echo "pct $pct $1 x$2 SAME"; else echo "pct $pct $1 x$2 DIFF"; diff $S/A_w.txt $S/B_w.txt | head -8; fi
done; done
