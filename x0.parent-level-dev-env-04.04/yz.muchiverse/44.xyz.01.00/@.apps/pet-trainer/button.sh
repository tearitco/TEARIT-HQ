#!/bin/bash
# button.sh - open the pet house window (a toy). Usage: sh button.sh run
# Same shape as @.apps/pdl-read/button.sh: the shared renderer draws pet-trainer.xhtpm, whose <module> starts pet_manager (stops with the window).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
XHTPM="$SCRIPT_DIR/pet-trainer.xhtpm"
[ "${1:-run}" = run ] || exit 0
for b in pet_manager pet_scene pet_physics; do
  [ -x "$SCRIPT_DIR/ops/+x/$b.+x" ] || gcc -std=c11 -O2 -D_DEFAULT_SOURCE -o "$SCRIPT_DIR/ops/+x/$b.+x" "$SCRIPT_DIR/ops/$b.c" -lm -lX11 2>/dev/null || true
done
[ -x "$HOUSE_ROOT/@.apps/layout-studio/ops/+x/pet_gen.+x" ] || sh "$HOUSE_ROOT/@.apps/layout-studio/ops/build_pet_gen.sh" >/dev/null 2>&1 || true
# write the state first (party from the DB, ui.txt) so the window never loads without it
sh "$SCRIPT_DIR/ops/pet_event.sh" status >/dev/null 2>&1 || true
old="$(ps -eo pid,args | awk '/khtpm_core_render.\+x.*pet-trainer\.xhtpm/ && !/awk/{print $1}')"
[ -n "$old" ] && echo "$old" | xargs -r kill -TERM && sleep 1
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/tmp/pet-trainer.log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 2
echo "pet-trainer launched"
