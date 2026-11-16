#!/bin/sh
# builds the plus phone's two ops (first slice 2026-11-15): the lifecycle/action op and the UI projector.
cd "$(dirname "$0")" && mkdir -p +x && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/phone_op.+x phone_op.c && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/phone_projector.+x phone_projector.c && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/app_check.+x app_check.c && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/hostlist_op.+x hostlist_op.c && echo "OK +x/phone_op.+x +x/phone_projector.+x +x/hostlist_op.+x"
