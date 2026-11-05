#!/bin/sh
# open_pet_observer_hq.sh <house_root> - launch the pet observer window (shape of open_irc_chat_hq.sh; ONE process: the shared renderer, which forks the manager <module>).
# Registered through the kh_spawn funnel (AGENTS.md: every house process is registered). Cost: idle = 1 wake/s in the manager + the renderer's own idle.
HOUSE_ROOT="$(cd "${1:?need house_root}" && pwd)"; HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"; MGR="$HERE/ops/+x/pet_observer_manager.+x"
[ -x "$MGR" ] || sh "$HERE/ops/build_pet_observer_manager.sh" || exit 1
[ -x "$BIN" ] || { echo "no renderer: $BIN" >&2; exit 1; }
chmod +x "$HERE/ops/obs_action.sh"
for p in $(pgrep -f 'khtpm_core_render\.\+x.*pet-observer-hq\.xhtpm'); do kill -TERM "$p"; done; sleep 1
KH_ROLE=tool KH_APP=pet-observer-hq sh "$HOUSE_ROOT/&.widgits/_shared-lib/kh_spawn.sh" "$HOUSE_ROOT" 0 pet-observer-hq -- "$BIN" "$HOUSE_ROOT" "$HERE/pet-observer-hq.xhtpm"
