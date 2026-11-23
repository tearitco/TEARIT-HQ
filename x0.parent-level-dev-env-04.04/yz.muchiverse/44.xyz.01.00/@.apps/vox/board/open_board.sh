#!/bin/sh
# open_board.sh - open the VOXEL board window (D112; BOOK 16.game MINECRAFT-EVENTS-HANDOFF section 18). The same shape as teario-make/board/open_board.sh (pc-hq instance, the shared board template, the book opened by widget commands) WITHOUT a game driver:
# the window is a projection of the ledger. Before the window opens, vox_op writes the desk (maps/vox/play: cells.txt, cells.rgba, map.png ...) and the game bar feed state/game_ui.txt from the LIVE ledger #.desktop/vox_ledger.txt (read only for the picture; this script appends no ledger row).
# Each feed button runs vox_op act, which appends the verb's rows, rewrites the desk and the feed; the engine reloads the desk when cells.marker grows.
# The player must exist in the ledger first: a move row spawns it (see the handoff section 18). Without one the feed says so and every button refuses.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; NAME=vox; HOUSE="$(cd "$APP/../.." && pwd)"
GATE="$HOUSE/&.widgits/_shared-lib/ops/+x/cpu_gate.+x"
[ -x "$GATE" ] && { "$GATE" "$HOUSE" check --wait 90 >/dev/null || { echo "open_board: the machine is too busy right now (cpu_gate); not relaunching" >&2; exit 1; }; }
VOX="$APP/ops/+x/vox_op.+x"; LEDGER="$HOUSE/#.desktop/vox_ledger.txt"; PNG="$HOUSE/&.widgits/_shared-lib/ops/+x/raw_to_png.+x"
[ -x "$VOX" ] || { echo "open_board: build $VOX first (gcc -std=gnu11 -O2 -o ops/+x/vox_op.+x ops/vox_op.c)" >&2; exit 1; }
export PCHQ_PKG="$APP/$NAME-pchq"
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
ACTOR="${VOX_ACTOR:-joe}"   # the actor the event pages name (the seeded pages say joe)
DESK="$PCHQ_PKG/pieces/system/maps/$NAME/play"; mkdir -p "$DESK" "$APP/state"
# the board hook (D114): this marker file beside the ledger makes vox_op rewrite the desk and the panel and nudge the engine after every accepted verb, whoever called it (a window button or an event page)
printf 'BOARD | desk | %s\nBOARD | feed | %s\nBOARD | png | %s\nBOARD | self | %s\nBOARD | actor | %s\n' "$DESK" "$APP/state/game_ui.txt" "$PNG" "$VOX" "$ACTOR" > "$HOUSE/#.desktop/vox_board.pdl"
"$VOX" desk "$APP" "$LEDGER" "$DESK" "png=$PNG" >/dev/null || exit 1
"$VOX" feed "$APP" "$LEDGER" "$ACTOR" "desk=$DESK" "feed=$APP/state/game_ui.txt" "png=$PNG" "self=$VOX" >/dev/null || exit 1
mkdir -p "$HERE/state"; [ -f "$HERE/state/sides.txt" ] || printf 'side_stamp=\n' > "$HERE/state/sides.txt"
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
WPID="$(cat "$PCHQ_PKG/pieces/system/board_window.pid" 2>/dev/null)"; REG="$HOUSE/&.widgits/_shared-lib/ops/+x/proc_reg_op.+x"
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"; BOOK=$NAME; PAGE=play
sleep 8; printf 'CONFIRM_START_MAP:%s\n' "$BOOK" >> "$INBOX"
for _i in $(seq 1 20); do sleep 2; [ "$(sed -n 's/^map_id=//p' "$PCHQ_PKG/pieces/world_01/state.txt" 2>/dev/null | head -1)" = "$BOOK" ] && break; done
sleep 6; printf 'CONFIRM_SET_DESK:%s\n' "$PAGE" >> "$INBOX"
if [ -x "$REG" ] && [ -n "$WPID" ] && [ -d "/proc/$WPID" ]; then
    "$REG" "$HOUSE" rule-add "$WPID" "$PCHQ_PKG/pieces/sessions/" engine $NAME pchq-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" rule-add "$WPID" "@file:$HERE/state/ui.txt#bv_session" engine $NAME bv-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" apply-rules >/dev/null 2>&1 || true
fi
# frame the 16x16 desk: the engine's default view shows only part of it (found by the first live window). Key f = reset, 3 = top-down mode, two a presses pan the centre onto the desk (measured in the second live session: pan_x -2 centres the 16x16 desk, the first session needed +6, so the framing is measured per launch: look at the photo); the keys go to the engine session the board opened.
sleep 4; BVS="$(sed -n 's/^bv_session=//p' "$HERE/state/ui.txt" 2>/dev/null | head -1)"; [ -n "$BVS" ] && "$HOUSE/&.widgits/board-viewer/tools/+x/bv_keys.+x" "$BVS" f3aa >/dev/null 2>&1
# KEYS (D115): the window's GAMEKEY rows come from the data table @.apps/vox/keys.pdl (idempotent), and a tailer registered to the board window turns each relayed key into one `vox_op act` (the relay is <host>/../state/interact_relay.txt, filled by the input op only while the player token is possessed; the tailer exits when the window is gone)
KEYS="$APP/ops/+x/vox_keys_op.+x"
if [ -x "$KEYS" ] && [ -n "$WPID" ] && [ -d "/proc/$WPID" ]; then
    "$KEYS" keybinds "$APP/keys.pdl" "$PCHQ_PKG/pieces/system/keybinds.pdl"
    KH_APP=vox KH_ROLE=daemon sh "$HOUSE/&.widgits/_shared-lib/kh_spawn.sh" "$HOUSE" "$WPID" vox-keys -- "$KEYS" tail "$APP" "$LEDGER" "$APP/state/interact_relay.txt" "$WPID" >/dev/null 2>&1
fi
