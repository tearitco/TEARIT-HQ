#!/bin/sh
# wo_port_gate.sh - gate of B5: the same death + destroyed page script runs on scratch copies of rpg-space and rpg-dust (@.apps/<game>-port, removed after) and must leave identical files.
# usage: sh wo_port_gate.sh <house> <out-prefix>   INTERIM .sh (port to a pal harness). Not a comparison with C: rpg-dust has no life code.
H="$1"; PLAY="$H/&.widgits/events-hq/ops/play_event.sh"; out="$2"
for g in rpg-space rpg-dust; do
  S="$H/@.apps/$g-port"; rm -rf "$S"; mkdir -p "$S/state"; cp "$H/@.apps/$g"/*.pdl "$S/"; cp -r "$H/@.apps/$g/events" "$S/"
  printf 'hp=0\nhunger=3\ndead=1\ndeaths=0\ninv=bread:5,iron:11,scrap:32\n' > $S/state/life_ember.txt; printf 'room=bridge\nx=9\ny=5\ndir=1\nsteps=5\ntx=-1\nty=-1\n' > $S/state/pet_ember.txt
  : > $S/state/drops.txt; : > $S/state/events.txt; echo ember > $S/state/life_actor.txt; printf 'bridge|7|6|4|ore\n' > $S/state/damage.txt; printf 'bridge|7|6|ore|2\n' > $S/state/nodes.txt; printf 'ore|bridge|7|6|beam\n' > $S/state/destroyed.txt
  printf 'hp=40\nhunger=3\ndead=0\ndeaths=0\ninv=bread:5,iron:11,scrap:32\n' > $S/state/life_ember.txt
  sed -i 's/^\(LIFE *| *death_mode *|\) *[a-z]* /\1 respawn /;s/^\(BLOCK *| *ore *|[^|]*|[^|]*|[^|]*|\) *[0-9]* /\1 0 /' $S/life.pdl $S/blocks.pdl
  for e in death destroyed; do MUCHI_TARGET_ENT="$S/state/world" bash "$PLAY" "$S/events/$e" "$H" parallel >/dev/null 2>&1; done
  { for f in life_ember pet_ember drops nodes damage; do echo "== $f"; cat $S/state/$f.txt; done; echo "== events"; cut -d'|' -f1,3- $S/state/events.txt; } > "$out.$g.txt"
done
if cmp -s "$out.rpg-space.txt" "$out.rpg-dust.txt"; then echo "space/dust SAME"; else echo DIFF; diff "$out.rpg-space.txt" "$out.rpg-dust.txt" | head; fi
rm -rf "$H/@.apps/rpg-space-port" "$H/@.apps/rpg-dust-port"
