#!/bin/sh
# open_board.sh - open the rpg-dust BOARD window (R3 first visible slice, BOOK 16.game/RPG-DUST-DESIGN.md): the current zoom address from the ledger, as a pc-hq book.
# Two desks of ONE book maps/rpg-dust: "region" (the 16 x 16 height page of the current region address) and "area" (the zoom into one of its cells; the ledger's cell when
# the current address is an area address, else cell 7,9). Heights become glyph bands (dust_gen export) the board engine extrudes in 3D. Own pc-hq instance: <app>/rpg-dust-pchq.
# Usage: sh board/open_board.sh [region-address]     (no argument: the ledger's current address, else globe:3,4/region:0,1 after a START event)
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
OPS="$APP/ops/+x"; export PCHQ_PKG="$APP/rpg-dust-pchq"
[ -x "$OPS/dust_gen.+x" ] && [ -x "$OPS/dust_zoom.+x" ] || sh "$APP/ops/build_dust_check.sh" >/dev/null || { echo "rpg-dust: build failed" >&2; exit 1; }
export DUST_STATE="${DUST_STATE:-$APP/state}"; mkdir -p "$DUST_STATE"
CUR="$("$OPS/dust_zoom.+x" where 2>/dev/null)"
if [ -n "$1" ]; then "$OPS/dust_zoom.+x" start "$1" >/dev/null || exit 1; CUR="$1"; fi
[ -n "$CUR" ] || { CUR="globe:3,4/region:0,1"; "$OPS/dust_zoom.+x" start "$CUR" >/dev/null; }
REGION="$CUR"; CX=7; CY=9
case "$CUR" in */cell:*) REGION="${CUR%/cell:*}"; C="${CUR##*/cell:}"; CX="${C%,*}"; CY="${C#*,}" ;; esac
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
BOOK="$PCHQ_PKG/pieces/system/maps/rpg-dust"; rm -rf "$BOOK"; mkdir -p "$BOOK/region" "$BOOK/area"
T="$(mktemp -d)"; SC="$APP/scale.pdl"
"$OPS/dust_gen.+x" page "$SC" "$REGION" "$T/region.txt" && "$OPS/dust_gen.+x" export "$T/region.txt" "$BOOK/region" || exit 1
"$OPS/dust_gen.+x" page "$SC" "$REGION/cell:$CX,$CY" "$T/area.txt" && "$OPS/dust_gen.+x" export "$T/area.txt" "$BOOK/area" || exit 1
cp "$BOOK/region/terrain_legend.txt" "$PCHQ_PKG/pieces/system/terrain_legend.txt"; cp "$T/region.txt" "$BOOK/region/page.txt"; cp "$T/area.txt" "$BOOK/area/page.txt"; rm -rf "$T"
cat > "$BOOK/game.pdl" <<GAME
SECTION      | KEY                | VALUE
----------------------------------------------------------------------
GAME         | type               | board-game
GAME         | icon               | 🪐
GAME         | label              | rpg-dust
GAME         | n_chunks           | 1
GAME         | chunk_0_x          | 0
GAME         | chunk_0_y          | 0
GAME         | n_desks            | 2
GAME         | desk_1_id          | region
GAME         | desk_1_label       | Region $REGION
GAME         | desk_2_id          | area
GAME         | desk_2_label       | Area cell $CX,$CY
GAME
mkdir -p "$HERE/state"
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
sleep 8; printf 'CONFIRM_START_MAP:rpg-dust\n' >> "$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
echo "rpg-dust board: region $REGION, area cell $CX,$CY"
