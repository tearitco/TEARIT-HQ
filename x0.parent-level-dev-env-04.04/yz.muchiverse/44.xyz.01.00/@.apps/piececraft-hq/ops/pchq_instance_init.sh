#!/bin/sh
# pchq_instance_init.sh <instance_data_dir>   (PCHQ-SESSIONS-DESIGN.md)
# Create the DATA folder of one pc-hq instance (a game's own world / hero / xelector / maps / engine sessions). Plain folders and plain copies only: no symlinks (they do not work on Windows) and
# no copy of any program: the programs stay in @.apps/piececraft-hq and every script finds them from there. The instance folder NAME must be unique in the house (board-viewer scopes its engine
# session by it): e.g. @.apps/dsr-rpg/dsr-rpg-pchq. Safe to run twice (never overwrites a file that exists). open_pchq_board.sh runs it when PCHQ_PKG names a folder without .instance-ready.
set -eu
DATA="${1:-}"
[ -n "$DATA" ] || { echo "usage: pchq_instance_init.sh <instance_data_dir>" >&2; exit 1; }
PKG="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$DATA"
DATA="$(cd "$DATA" && pwd)"
[ "$DATA" != "$PKG" ] || { echo "pchq_instance_init: refusing to initialise the default instance itself" >&2; exit 1; }
for d in pieces/system/maps pieces/system/chunks pieces/system/widget_cmds pieces/sessions pieces/apps/player_app pieces/display pieces/keyboard data state; do mkdir -p "$DATA/$d"; done
# the per-piece state files (the game's daemon rewrites world / hero / xelector as it plays; these are only the starting shape)
for p in hero_01 xelector_01 sun_01 moon_01 world_01; do
    # PCHQ_NO_HERO=1 (set by a board that drives its own characters, e.g. rpg-edit-hq, owner 2026-11-26): the hero placeholder is not part of that instance. What it did is kept by the engine for the page's own rig: tokens
    # (8 snaps to the character, 9 jumps back to the last one, Enter and [ ] take one; BOOK 16.game/TUTORIAL-HARNESS-...-2026-11-26.md section 15). Default (unset) = every instance gets the hero as before.
    if [ "$p" = hero_01 ] && [ "${PCHQ_NO_HERO:-0}" = 1 ]; then rm -rf "$DATA/pieces/hero_01"; continue; fi
    [ -d "$DATA/pieces/$p" ] || { [ -d "$PKG/pieces/$p" ] && cp -R "$PKG/pieces/$p" "$DATA/pieces/$p"; } || mkdir -p "$DATA/pieces/$p"
done
# with no hero the xelector must not remember one: its "last possessed" would be a piece that does not exist (a harmless no-op, but a vestige that agents keep copying)
if [ "${PCHQ_NO_HERO:-0}" = 1 ] && [ -f "$DATA/pieces/xelector_01/state.txt" ]; then sed -i 's/^last_possessed_id=hero_01$/last_possessed_id=none/; s/^possessed_id=hero_01$/possessed_id=none/' "$DATA/pieces/xelector_01/state.txt"; fi
# pieces/registry = the assets the engine looks up UNDER the game root: the HUD font (fonts/ascii/<code>/glyph.txt: without it the 3D HUD is black on black), emoji and phymoji assets (1.3 MB, plain copies)
[ -d "$DATA/pieces/registry" ] || { [ -d "$PKG/pieces/registry" ] && cp -R "$PKG/pieces/registry" "$DATA/pieces/registry"; } || mkdir -p "$DATA/pieces/registry"
# the small configuration files of pieces/system (keys, hud, legend, view, manifest ...); not the maps, the generated chunks, pids or session pointers
for f in "$PKG"/pieces/system/*; do
    [ -f "$f" ] || continue
    b="$(basename "$f")"
    case "$b" in *.pid|session_dir.txt|house_root.txt|locations.pdl|board_widget_bridge.txt|.engine-start.lock) continue ;; esac
    [ -f "$DATA/pieces/system/$b" ] || cp -p "$f" "$DATA/pieces/system/$b"
done
[ -f "$DATA/pieces/system/widget_cmds/inbox.txt" ] || : > "$DATA/pieces/system/widget_cmds/inbox.txt"
[ -f "$DATA/pieces/keyboard/history.txt" ] || : > "$DATA/pieces/keyboard/history.txt"
printf 'instance=%s\ncreated=%s\n' "$(basename "$DATA")" "$(date '+%Y-%m-%d %H:%M:%S')" > "$DATA/pieces/system/.instance-ready"
echo "pchq_instance_init: $DATA ready"
