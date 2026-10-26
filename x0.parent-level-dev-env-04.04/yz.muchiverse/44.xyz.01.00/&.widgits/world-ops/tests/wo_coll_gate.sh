#!/bin/sh
# wo_coll_gate.sh - gate of B4: events/collision (+ col_hit, col_breach) leaves the same files as the C `collision hit <speed>` (frozen reference binary). Scratch copy @.apps/rpg-space-gate needed.
# Speeds 1..4 (breach at ENV breach_speed 4): life files of all five characters, ship_damage.txt (last= is time: compared without it), air.txt, events rows. INTERIM .sh.
H="$(cd "$(dirname "$0")/../../.." && pwd)"; S="${S:-/tmp}"; G="$H/@.apps/rpg-space-gate"; REF="$G/ops/+x/rpg_space.+x"; PLAY="$H/&.widgits/events-hq/ops/play_event.sh"
reset() { for c in harold ember glacine murmur solvent; do printf 'hp=%s\nhunger=0\ndead=0\ndeaths=0\ninv=\n' "$([ $c = harold ] && echo 450 || echo 40)" > $G/state/life_$c.txt; done; : > $G/state/events.txt; rm -f $G/state/air.txt; printf 'hits=2 dmg=3 last=1\n' > $G/state/ship_damage.txt; printf '%s\n' "$1" > $G/state/coll_speed.txt; }
snap() { for c in harold ember glacine murmur solvent; do echo "== $c"; grep hp= $G/state/life_$c.txt; done; echo "== damage"; sed 's/ last=.*//' $G/state/ship_damage.txt; echo "== air"; cat $G/state/air.txt 2>/dev/null; echo "== events"; cut -d'|' -f1,3- $G/state/events.txt; }
play() { MUCHI_TARGET_ENT="$G/state/world" bash "$PLAY" "$G/events/collision" "$H" parallel >/dev/null 2>&1; }
for sp in 1 2 3 4 9; do reset $sp; "$REF" collision hit >/dev/null 2>&1; snap > $S/A_c.txt; reset $sp; play; snap > $S/B_c.txt
  if cmp -s $S/A_c.txt $S/B_c.txt; then echo "speed $sp SAME"; else echo "speed $sp DIFF"; diff $S/A_c.txt $S/B_c.txt | head -6; fi; done
