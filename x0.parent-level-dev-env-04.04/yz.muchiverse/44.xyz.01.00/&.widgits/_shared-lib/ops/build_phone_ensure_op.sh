#!/bin/sh
# build_phone_ensure_op.sh - builds the phone ops into +x/: phone_ensure_op (identity + phones, khtpm_phone.c), phone_send_op (the phone.send event),
# server_route_op (the server.route event). Checks:  ./+x/phone_ensure_op.+x --selftest   and   bash ../../../^.grave/quests/Q009-first-events-phone-send-and-route/verify.sh
set -e
cd "$(dirname "$0")"
mkdir -p +x
for n in phone_ensure_op phone_send_op server_route_op; do
    ${CC:-gcc} -std=c11 -Wall -Wextra -O2 -o +x/$n.+x $n.c
    echo "OK +x/$n.+x"
done
