#!/bin/sh
# 03 — force regicide via direct setup is hard; simulate by attacking until king dies
# Using small C helper inline: start match, teleport-free legal approach is long.
# Instead: run match, use internal-style: write a unit contact scenario via ledger tools.
# Practical MVP: start match, manually place AI king adjacent via editing unit state then attack.
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$DIR"
BIN=./ops/+x/ttg_loop
mkdir -p pieces/apps/player_app pieces/display pieces/system data pieces/units
: > pieces/apps/player_app/history.txt
: > pieces/system/quit_flag.txt

# Compile tiny helper that uses same core to place king adjacent and attack
cat > /tmp/ttg_regicide_test.c << EOF
#include "ttg.h"
int main(void) {
  Game g;
  Unit *ak, *ek;
  ttg_init_empty(&g);
  ttg_set_root(&g, "$DIR");
  ttg_init_match(&g, 300000, 50);
  ak = ttg_unit_by_id(&g, "u_s0_king_01");
  ek = ttg_unit_by_id(&g, "u_s1_king_01");
  if (!ak || !ek) return 2;
  /* place enemy king adjacent */
  ek->x = ak->x + 1; ek->y = ak->y;
  ak->moved = 0; ak->acted = 0;
  if (ttg_attack(&g, ak, ek) != 0) return 3;
  /* finish king: loop attacks */
  while (ek->alive) {
    ak->acted = 0;
    if (ttg_attack(&g, ak, ek) != 0) break;
  }
  ttg_check_end(&g);
  ttg_save_all(&g);
  ttg_compose_frame(&g);
  ttg_compose_rgb(&g);
  return (g.phase == PH_END && g.winner[0] == '0') ? 0 : 4;
}
EOF
gcc -std=c11 -O2 -I./src -o /tmp/ttg_regicide_test /tmp/ttg_regicide_test.c \
  src/ttg_core.c src/ttg_compose.c src/ttg_input.c -lm
/tmp/ttg_regicide_test
grep -q 'regicide\|match_end' data/master_ledger.txt && echo "PASS 03 regicide ledger" || {
  echo "FAIL 03"; tail -15 data/master_ledger.txt; exit 1
}
grep -q 'MATCH END\|WINNER' pieces/display/current_frame.txt && echo "PASS 03 frame end" || echo "WARN frame text"
echo "OK harness 03_attack_regicide"
