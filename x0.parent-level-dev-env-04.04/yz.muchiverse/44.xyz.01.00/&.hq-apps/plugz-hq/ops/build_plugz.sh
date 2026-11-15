#!/bin/sh
# builds the two ops of the plugz-hq window (D76): the UI projector (the <module>) and the action op. Run once after a clone; needs the harness_menu_op build too (&.widgits/_shared-lib/ops/build_harness_menu_op.sh).
cd "$(dirname "$0")" && mkdir -p +x && echo "-- plugz_projector, plugz_act -> +x/" && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/plugz_projector.+x plugz_projector.c && ${CC:-gcc} -std=gnu11 -Wall -O2 -o +x/plugz_act.+x plugz_act.c && echo "OK plugz-hq ops"
