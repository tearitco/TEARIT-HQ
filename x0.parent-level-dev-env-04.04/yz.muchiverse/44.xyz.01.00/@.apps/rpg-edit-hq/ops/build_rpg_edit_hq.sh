#!/bin/sh
# build_rpg_edit_hq.sh - compile ops/rpg_edit_hq.c (a skin of ../../../&.widgits/_shared-lib/petslike_core.c; -I. so the core can re-include this file) to ops/+x/rpg_edit_hq.+x (stb_image is text-included from &.widgits/_shared-lib; -lm only).
cd "$(dirname "$0")" && mkdir -p +x && nice -n 15 gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter  -I. -o +x/rpg_edit_hq.+x rpg_edit_hq.c -lm && echo "OK $(pwd)/+x/rpg_edit_hq.+x"

# the left-bar switch (pets <-> palettes), see pal_side.c
nice -n 15 gcc -std=c11 -O2 -Wall -Wextra -o +x/pal_side.+x pal_side.c || exit 1
# the editor's own preferences (the Settings tab in its toolbar), see edit_prefs.c
nice -n 15 gcc -std=c11 -O2 -Wall -Wextra -o +x/edit_prefs.+x edit_prefs.c || exit 1
# the editor h-ai (locate session, context, API ask, proposal, apply), see hai_op.c
nice -n 15 gcc -std=gnu11 -O2 -Wall -Wextra -Wno-format-truncation -o +x/hai_op.+x hai_op.c || exit 1
nice -n 15 gcc -std=gnu11 -O2 -Wall -Wextra -Wno-format-truncation -o +x/item_ledger_op.+x item_ledger_op.c || exit 1   # the quarantined item ledger, see item_ledger_op.c and harness item_ledger.pal
nice -n 15 gcc -std=gnu11 -O2 -Wall -Wextra -Wno-format-truncation -o +x/pet_net_say.+x pet_net_say.c || exit 1   # a pet chooses its network line from its own state, see pet_net_say.c and harness pet_net_say.pal
nice -n 15 gcc -std=gnu11 -O2 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -o +x/survivor_op.+x survivor_op.c || exit 1   # quarantined early-survivor slice (smash/needs/eat/wield/light), see survivor_op.c and harness survivor_slice.pal
nice -n 15 gcc -std=gnu11 -O2 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -o +x/gen_map_op.+x gen_map_op.c || exit 1   # deterministic map writer (cdda house, df column), quarantined, see gen_map_op.c and harness gen_map.pal
nice -n 15 gcc -std=gnu11 -O2 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -o +x/world_common_op.+x world_common_op.c || exit 1   # flow / rain / fire common events (quarantined), see world_common_op.c and harness world_common.pal
nice -n 15 gcc -std=gnu11 -O2 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -o +x/mcl_op.+x mcl_op.c || exit 1   # MCL bench/3x3/furnace/bed/burst (quarantined), see mcl_op.c and harness mcl_bench.pal
