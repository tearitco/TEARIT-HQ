#!/bin/bash
# button.sh - open the rpg-pet window (a toy: @.apps/rpg-pet/toy.pdl). Usage: sh button.sh run
# Same shape as @.apps/pet-trainer/button.sh. Matches processes by comm + window name (never a bare command-line grep).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
XHTPM="$SCRIPT_DIR/rpg-pet.xhtpm"
[ "${1:-run}" = run ] || exit 0
mkdir -p "$SCRIPT_DIR/state" "$SCRIPT_DIR/audit"
[ -x "$SCRIPT_DIR/ops/+x/rpg_pet.+x" ] || sh "$SCRIPT_DIR/ops/build_rpg_pet.sh" >/dev/null 2>&1 || true
"$SCRIPT_DIR/ops/+x/rpg_pet.+x" status >/dev/null     # ui.txt + scene.raw must exist before the window loads
old="$(ps -eo pid,comm,args | awk '$2=="khtpm_core_rend" && index($0,"rpg-pet.xhtpm"){print $1}')"
[ -n "$old" ] && echo "$old" | xargs -r kill -TERM && sleep 1
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$SCRIPT_DIR/audit/rpg-pet.log" 2>&1 < /dev/null &
printf '%s %s 0 0 rpg-pet\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 2
echo "rpg-pet launched"
