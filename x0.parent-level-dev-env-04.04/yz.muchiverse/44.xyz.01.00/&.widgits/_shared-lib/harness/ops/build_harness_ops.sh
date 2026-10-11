#!/bin/sh
# build_harness_ops.sh - builds the compiled ops of the pal harness into +x/ (picked up by $.crypts/compile-runner.sh as ops/build_*.sh).
# Added 2026-10-07: these ops were compiled by hand during testing and no build script covered them, so a fresh checkout had no harness binaries.
# Two ops text-include shared sources and need -I paths (their own header comments give the same lines); the rest are self-contained.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC="${CC:-gcc}"
for n in args_echo_op game_setup_query_op harness_bank_op harness_case_op harness_verdict_op music_wav_audit_op race_audit_op ring_audit_op; do
    $CC -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-unused-result -O2 -o "+x/$n.+x" "$n.c" -lm
    echo "OK +x/$n.+x"
done
# proc_reap_op includes kh_proc_registry.h from _shared-lib
$CC -std=gnu11 -Wall -I "../.." -o +x/proc_reap_op.+x proc_reap_op.c && echo "OK +x/proc_reap_op.+x"
# player_menu_query_op text-includes the taskbar manager source (needs the taskbar ops dir and the shared lib on the include path)
$CC -std=c11 -O2 -w -I "../../../../_.monads/_.livedesk-taskbar/ops" -I .. -I ../.. -o +x/player_menu_query_op.+x player_menu_query_op.c && echo "OK +x/player_menu_query_op.+x"
