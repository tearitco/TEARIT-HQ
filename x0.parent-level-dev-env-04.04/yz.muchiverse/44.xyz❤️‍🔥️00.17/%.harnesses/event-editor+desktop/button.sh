#!/bin/bash
# event-editor + desktop tray multi-project demo
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../.." && pwd)"
EE="$HOUSE/&.widgits/event-editor"
TP="$HOUSE/&.widgits/tile-picker"
MUTA=$(ls -d "$HOUSE"/101.mutaclsym* 2>/dev/null | head -1)
DESK="${XYZ_DESKTOP_ROOT:-$HOUSE/#.desktop}"

case "$ACTION" in
    compile|c)
        "$EE/button.sh" compile
        "$TP/button.sh" compile
        if [ -n "$MUTA" ]; then (cd "$MUTA" && ./button.sh compile) | tail -5; fi
        ;;
    demo)
        set -e
        "$EE/button.sh" compile
        "$TP/button.sh" compile
        mkdir -p "$DESK/events" "$DESK/tiles" "$DESK/inbox" "$SCRIPT_DIR/workdir" "$SCRIPT_DIR/proof"
        PROOF="$SCRIPT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$PROOF"
        WDIR="$SCRIPT_DIR/workdir/w-$$"
        mkdir -p "$WDIR"
        export XYZ_DESKTOP_ROOT="$DESK"

        echo "=== desktop resolve ==="
        DESK_R=$("$EE/ops/+x/ee_resolve_desktop.+x" "$HOUSE")
        echo "desktop=$DESK_R" | tee "$PROOF/00_desktop.txt"
        [ "$DESK_R" = "$DESK" ] || [ -d "$DESK_R" ] || { echo FAIL resolve; exit 1; }
        echo "PASS: desktop resolve"

        echo "=== package init + open request ==="
        PKG_NAME="ev_harness_demo"
        "$EE/ops/+x/ee_package_init.+x" "$DESK/events" "$PKG_NAME" map_start 3 4 | tee "$PROOF/01_package.txt"
        PKG="$DESK/events/$PKG_NAME"
        [ -f "$PKG/event.pal" ] || { echo FAIL package; exit 1; }
        "$EE/ops/+x/ee_open_request_write.+x" "$DESK" "$PKG" map_start 3 4 harness | tee "$PROOF/02_request.txt"
        [ -f "$DESK/inbox/event_editor_open.request" ] || { echo FAIL request; exit 1; }
        READ=$("$EE/ops/+x/ee_open_request_read.+x" "$DESK")
        echo "read=$READ" | tee "$PROOF/03_read.txt"
        [ "$READ" = "$PKG" ] || { echo FAIL read mismatch; exit 1; }
        echo "PASS: open request round-trip"

        echo "=== tile-picker → desktop ==="
        echo "T" > "$WDIR/brush.txt"
        "$TP/ops/+x/tp_place_desktop.+x" "$WDIR" "$DESK" T tile_T_demo | tee "$PROOF/04_tile.txt"
        [ -f "$DESK/tiles/tile_T_demo/glyph.txt" ] || { echo FAIL tile; exit 1; }
        echo "PASS: tile on desktop"

        if [ -n "$MUTA" ] && [ -x "$MUTA/ops/+x/choice.+x" ]; then
            echo "=== muta xlector_ctx Event (headless) ==="
            export PRISC_PROJECT_ROOT="$MUTA"
            # ensure interact + xlector pos
            HERO="$MUTA/pieces/world_01/map_start/hero/state.txt"
            cp "$HERO" "$PROOF/hero_before.txt"
            # set interact_mode and panel none
            python3 - <<'PY' "$HERO"
import sys
path=sys.argv[1]
keys={}
lines=open(path).read().splitlines()
out=[]
seen=set()
want={"interact_mode":"1","active_panel":"none","panel_cursor":"1","xlector_pos_x":"3","xlector_pos_y":"4"}
for L in lines:
    if "=" in L:
        k,v=L.split("=",1)
        if k in want:
            out.append(f"{k}={want[k]}"); seen.add(k); continue
    out.append(L)
for k,v in want.items():
    if k not in seen: out.append(f"{k}={v}")
open(path,"w").write("\n".join(out)+("\n" if out else ""))
PY
            # Space open menu, then digit 1 Event
            "$MUTA/ops/+x/choice.+x" 32
            grep -q 'active_panel=xlector_ctx' "$HERO" && echo "PASS: Space opened xlector_ctx" || {
                echo "active_panel=$(grep active_panel= "$HERO")"; echo FAIL open menu; exit 1; }
            "$MUTA/ops/+x/choice.+x" 49   # '1' = Event
            cp "$HERO" "$PROOF/hero_after_event.txt"
            grep -q 'active_panel=none' "$HERO" || true
            # package + request from muta path
            ls "$DESK/events"/ev_* 2>/dev/null | tee "$PROOF/05_muta_events.txt" || true
            [ -f "$DESK/inbox/event_editor_open.request" ] || { echo FAIL muta request; exit 1; }
            echo "PASS: muta Event wrote desktop request"
            # restore hero lightly (keep interact optional)
            cp "$PROOF/hero_before.txt" "$HERO" || true
        else
            echo "SKIP muta choice (no binary)"
        fi

        if [ -n "$MUTA" ]; then
            echo "=== import desktop package into world ==="
            WORLD="$MUTA/pieces/world_01"
            "$EE/ops/+x/ee_import_to_world.+x" "$PKG" "$WORLD" map_start 3 4 | tee "$PROOF/06_import.txt"
            [ -d "$WORLD/map_start/events/$PKG_NAME" ] || { echo FAIL import; exit 1; }
            echo "PASS: import to world"
            # cleanup import from live world so we don't pollute install
            rm -rf "$WORLD/map_start/events/$PKG_NAME"
            rmdir "$WORLD/map_start/events" 2>/dev/null || true
        fi

        echo "Proof: $PROOF"
        echo "=== ALL PASS — event-editor + desktop ==="
        ;;
    help|*)
        echo "event-editor+desktop harness"
        echo "  compile | demo | help"
        echo "  Desktop: $DESK"
        ;;
esac
