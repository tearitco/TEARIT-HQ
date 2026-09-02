#!/bin/bash
# scripts/build.sh - compile everything, warning-free.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/keyboard_input.c, system/chtpm_parser_pal.c, system/renderer.c,
# ops/palnet_peer.c) - same convention every other project in this
# family follows (see ../shared-ops-manifest.txt). To pull in an
# update from the canonical source, run (from yz.muchiverse/2.muchi-verse/):
#   bash sync_shared_op.sh <op_name> pal-chat-irc/<target_dir>
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x system

CFLAGS="-Wall -Wextra -O2"

echo "--- Building system processes ---"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"

echo "--- Building chtpm_parser_pal (PERSISTENT process, -Wno-unused-result"
echo "    -Wno-stringop-truncation required - see shared-ops/chtpm_parser_pal.c)"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building palnet_peer (local copy - reusable P2P op, see"
echo "    ../PAL-NET-STANDARD.txt) ---"
gcc $CFLAGS -o "ops/+x/palnet_peer.+x" "ops/palnet_peer.c"

echo "--- Building pal-chat-irc ops ---"
gcc $CFLAGS -o "ops/+x/chat_create_user.+x" "ops/chat_create_user.c"
gcc $CFLAGS -o "ops/+x/chat_switch_user.+x" "ops/chat_switch_user.c"
gcc $CFLAGS -o "ops/+x/chat_post_message.+x" "ops/chat_post_message.c"
gcc $CFLAGS -o "ops/+x/chat_inbox_watcher.+x" "ops/chat_inbox_watcher.c"
gcc $CFLAGS -o "ops/+x/chat_menu_input.+x" "ops/chat_menu_input.c"
gcc $CFLAGS -o "ops/+x/chat_compose_frame.+x" "ops/chat_compose_frame.c"
gcc $CFLAGS -o "ops/+x/chat_replay_ledger.+x" "ops/chat_replay_ledger.c"

echo "build ok"
