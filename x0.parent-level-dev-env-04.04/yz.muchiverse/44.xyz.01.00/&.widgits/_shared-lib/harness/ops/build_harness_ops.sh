#!/bin/sh
# build_harness_ops.sh - builds the compiled ops of the pal harness into +x/ (picked up by $.crypts/compile-runner.sh as ops/build_*.sh).
# Added 2026-10-07: these ops were compiled by hand during testing and no build script covered them, so a fresh checkout had no harness binaries.
# Two ops text-include shared sources and need -I paths (their own header comments give the same lines); the rest are self-contained.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC="${CC:-gcc}"
for n in args_echo_op game_setup_query_op harness_bank_op harness_case_op harness_verdict_op music_wav_audit_op playthrough_op race_audit_op ring_audit_op; do
    $CC -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-unused-result -O2 -o "+x/$n.+x" "$n.c" -lm
    echo "OK +x/$n.+x"
done
# proc_reap_op includes kh_proc_registry.h from _shared-lib
$CC -std=gnu11 -Wall -I "../.." -o +x/proc_reap_op.+x proc_reap_op.c && echo "OK +x/proc_reap_op.+x"
# player_menu_query_op text-includes the taskbar manager source (needs the taskbar ops dir and the shared lib on the include path)
$CC -std=c11 -O2 -w -I "../../../../_.monads/_.livedesk-taskbar/ops" -I .. -I ../.. -o +x/player_menu_query_op.+x player_menu_query_op.c && echo "OK +x/player_menu_query_op.+x"
# img_op (PNG numbers for live window cases), video_op (slides + narrated video, needs ffmpeg at run time), py_lint (ratchet against new Python): self-contained, 2026-11-04
$CC -std=gnu11 -Wall -Wextra -Wno-misleading-indentation -O2 -o +x/img_op.+x img_op.c -lm && echo "OK +x/img_op.+x"
$CC -std=gnu11 -Wall -Wextra -Wno-format-truncation -O2 -o +x/video_op.+x video_op.c && echo "OK +x/video_op.+x"
# class_audio_op (2026-11-09): narrate a DAY/NIGHT class text to one mp3 on the Kokoro machine over ssh, voices from voices/class_voices.pdl (no Python)
$CC -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -O2 -o +x/class_audio_op.+x class_audio_op.c && echo "OK +x/class_audio_op.+x"
$CC -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/py_lint.+x py_lint.c && echo "OK +x/py_lint.+x"
$CC -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/blank_page_op.+x blank_page_op.c && echo "OK +x/blank_page_op.+x"
$CC -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/legend_page_op.+x legend_page_op.c && echo "OK +x/legend_page_op.+x"
# dsk_label_probe_op (2026-11-09): numbers out of a Teropoly play desk so case dsk_labels can check the board names (no Python)
$CC -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/dsk_label_probe_op.+x dsk_label_probe_op.c && echo "OK +x/dsk_label_probe_op.+x"
# data_op (2026-11-09, data-hq P0): the dataset layer (list, sniff, check, show, stats, pick, provider, index); source and binary live in &.hq-apps/data-hq/ops, no Python
DH="../../../../&.hq-apps/data-hq/ops"; mkdir -p "$DH/+x"
$CC -std=gnu11 -Wall -Wextra -Wno-misleading-indentation -Wno-format-truncation -Wno-stringop-truncation -O2 -o "$DH/+x/data_op.+x.tmp" "$DH/data_op.c" -lm && mv "$DH/+x/data_op.+x.tmp" "$DH/+x/data_op.+x" && echo "OK $DH/+x/data_op.+x"
# chart_op (2026-11-09, data-hq P1): the chart rasteriser (line, bar, histogram, scatter -> PNG or RGBA raw); text-includes data_op.c (-DDATA_OP_AS_LIB inside the file) and font57.inc, links stb_image only for its `pixel` read-back verb; `data_op autodash` runs it, so build it with data_op
$CC -std=gnu11 -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation -Wno-format-truncation -Wno-stringop-truncation -O2 -o "$DH/+x/chart_op.+x.tmp" "$DH/chart_op.c" -lm && mv "$DH/+x/chart_op.+x.tmp" "$DH/+x/chart_op.+x" && echo "OK $DH/+x/chart_op.+x"
# game_core + game_new_op (2026-11-09): the GAME FOUNDATION GENERATOR (BOOK 16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md). game_core is the ONE shared engine + driver + button op of every generated game
# (source _shared-lib/ops/game_core.c, binary beside the other shared ops; built to .tmp then renamed because a running driver may be executing the old file); game_new_op makes the games from harness/templates/game.
mkdir -p ../../ops/+x
$CC -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -O2 -o ../../ops/+x/game_core.+x.tmp ../../ops/game_core.c && mv ../../ops/+x/game_core.+x.tmp ../../ops/+x/game_core.+x && echo "OK ../../ops/+x/game_core.+x"
$CC -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -O2 -o +x/game_new_op.+x game_new_op.c && echo "OK +x/game_new_op.+x"
# track_theory_op (2026-11-09): the EXACT theory of a track race (Markov chain: expected turns, game length by player count, seat win shares, per-square table) and the engine's dice replayed, for the Chutes cases; read-only, no process, no files
$CC -std=gnu11 -Wall -Wextra -O2 -o +x/track_theory_op.+x track_theory_op.c -lm && echo "OK +x/track_theory_op.+x"
# proc_cost_op (2026-11-09): what one process costs, measured with children and wakeups (CPU-OPTIMIZATION-PITFALLS 26); used by the realtime spikes (BOOK 16.game/REALTIME-SPIKES-HELD-KEYS-TICK-RATE-PICTURE-RATE-2026-11-09.md)
$CC -std=gnu11 -Wall -Wextra -Werror -O2 -o +x/proc_cost_op.+x proc_cost_op.c && echo "OK +x/proc_cost_op.+x"
