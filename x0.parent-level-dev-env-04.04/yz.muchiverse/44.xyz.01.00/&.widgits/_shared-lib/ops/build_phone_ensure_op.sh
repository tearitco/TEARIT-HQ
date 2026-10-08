#!/bin/sh
# build_phone_ensure_op.sh - builds the phone + wordbank ops into +x/: phone_ensure_op (identity + phones, khtpm_phone.c), phone_send_op (the phone.send event),
# server_route_op (the server.route event), game_slot_op (save-game/load-game slots, wordbank_ensure_op (word/synonym bank seed), wordbank_score_op (hand-scoring review). Checks:  ./+x/phone_ensure_op.+x --selftest   and   ./+x/wordbank_ensure_op.+x --selftest
set -e
cd "$(dirname "$0")"
mkdir -p +x
for n in phone_ensure_op phone_send_op server_route_op game_slot_op wordbank_ensure_op wordbank_score_op; do
    ${CC:-gcc} -std=c11 -Wall -Wextra -O2 -o +x/$n.+x $n.c
    echo "OK +x/$n.+x"
done
