#!/bin/sh
# wo_life_gate.sh - gate of B1 (BOOK 16.game/RPG-SPACE-BLUEPRINT-SYSTEMS.md): the death and respawn PAGES (nodes of the world-ops op) must leave exactly the files the old C verbs leave.
# Needs a scratch copy of the game at @.apps/rpg-space-gate (pdl files, events/, state/, ops/+x/rpg_space.+x = the frozen reference binary from git tag rpg-space-pre-blueprint).
# Prints "death SAME" / "respawn SAME" (or DIFF + diff). Negative test: edit a cmd_N.sh of the scratch death page, expect DIFF. INTERIM: a .sh because case_run fields cannot hold pipes; port to a pal harness (house rule).
# Not run against the live game: it rewrites state/ of the scratch game only.
H="$(cd "$(dirname "$0")/../../.." && pwd)"; S="${S:-/tmp}"
sed -i 's/^\(LIFE *| *death_mode *|\) *[a-z]* /\1 permadeath /' "$G/life.pdl"
reset() { # $1 = dead flag
  printf 'hp=%s\nhunger=3\ndead=%s\ndeaths=0\ninv=bread:5,iron:11,scrap:32\n' "$([ "$1" = 1 ] && echo 0 || echo 20)" "$1" > $G/state/life_ember.txt
  printf 'room=bridge\nx=9\ny=5\ndir=1\nsteps=5\ntx=-1\nty=-1\n' > $G/state/pet_ember.txt
  : > $G/state/drops.txt; : > $G/state/events.txt; echo ember > $G/state/life_actor.txt; }
snap() { echo "== life"; cat $G/state/life_ember.txt; echo "== pos"; cat $G/state/pet_ember.txt; echo "== drops"; cat $G/state/drops.txt; echo "== events"; cut -d'|' -f1,3- $G/state/events.txt; }
play() { MUCHI_TARGET_ENT="$G/state/world" bash "$PLAY" "$G/events/$1" "$H" parallel >/dev/null 2>&1; }
reset 0; "$REF" life die ember >/dev/null 2>&1; snap > $S/A_death.txt
reset 0; play death; snap > $S/B_death.txt
reset 1; "$REF" life respawn ember >/dev/null 2>&1; snap > $S/A_resp.txt
reset 1; play respawn; snap > $S/B_resp.txt
for k in death resp; do if cmp -s $S/A_$k.txt $S/B_$k.txt; then echo "$k SAME"; else echo "$k DIFF"; diff $S/A_$k.txt $S/B_$k.txt | head -12; fi; done
echo ---; cat $S/B_death.txt | head -30
