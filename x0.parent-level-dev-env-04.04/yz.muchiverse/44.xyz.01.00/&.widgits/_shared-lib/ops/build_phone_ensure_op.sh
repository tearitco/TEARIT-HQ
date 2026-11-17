#!/bin/sh
# build_phone_ensure_op.sh - builds the phone + wordbank ops into +x/: phone_ensure_op (identity + phones, khtpm_phone.c), phone_send_op (the phone.send event),
# server_route_op (the server.route event), game_slot_op (save-game/load-game slots, verify: the pal harness harness/game_slots.pal (run from &.widgits/_shared-lib/harness, README there)),
# trunk_op (shared versioned data: trunk + branches of rows, BOOK 02-architecture/TRUNK-AND-BRANCH-SHARED-DATA-STANDARD-2026-11-16.md; verify: the pal harness harness/trunk_op.pal), wordbank_ensure_op (word/synonym bank seed), wordbank_score_op (hand-scoring review). Checks:  ./+x/phone_ensure_op.+x --selftest   and   ./+x/wordbank_ensure_op.+x --selftest   and   bash ../../../^.grave/quests/Q009-first-events-phone-send-and-route/verify.sh
set -e
cd "$(dirname "$0")"
mkdir -p +x
for n in phone_ensure_op phone_send_op server_route_op game_slot_op wordbank_ensure_op wordbank_score_op trunk_op; do
    ${CC:-gcc} -std=c11 -Wall -Wextra -O2 -o +x/$n.+x $n.c
    echo "OK +x/$n.+x"
done
