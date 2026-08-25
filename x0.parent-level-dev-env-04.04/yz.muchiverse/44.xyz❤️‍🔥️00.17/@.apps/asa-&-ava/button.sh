#!/bin/bash
# button.sh - asa-&-ava: opens both deskpals (asa, ava) as real live desktop
# GL windows, plus their own state folders, so you can look around and
# confirm things are on track before any real behavior-tree/farming logic
# exists.
#
# Reuses tile-picker's own real, already-proven desktop-window pipeline
# (tp_desktop_window.+x) wholesale, per this house's "reuse ops, don't
# reinvent" convention - see &.widgits/tile-picker/TILE_PICKER_DESIGN.md.
# Packages live under #.desktop/entities/ (the real, already-documented
# folder for pets/NPCs, per #.desktop/README.txt - distinct from tiles/,
# which tile-picker's own tile stamps use), not reinvented here.
#
# Each deskpal also gets its own INDEPENDENT start script
# (pieces/<name>/button.sh) - direct instruction ("make sure they have
# their own independent start buttons as well") - this file's own "run"
# action just calls both of those, it doesn't duplicate their logic.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-run}"

case "$ACTION" in
    run|r|start)
        bash "$SCRIPT_DIR/pieces/asa/button.sh" run
        bash "$SCRIPT_DIR/pieces/ava/button.sh" run
        ;;
    kill|k|stop)
        bash "$SCRIPT_DIR/pieces/asa/button.sh" kill
        bash "$SCRIPT_DIR/pieces/ava/button.sh" kill
        ;;
    check|verify)
        bash "$SCRIPT_DIR/pieces/asa/button.sh" check
        bash "$SCRIPT_DIR/pieces/ava/button.sh" check
        ;;
    help|h|-h|--help|*)
        echo "asa-&-ava — opens asa + ava as live desktop windows + their state folders"
        echo "  run | r | start   - Spawn both deskpals + open their folders"
        echo "  kill | k | stop    - Close both deskpal windows"
        echo "  check | verify     - Verify binaries/folders are present"
        echo ""
        echo "Each deskpal also has its own independent start script:"
        echo "  pieces/asa/button.sh run"
        echo "  pieces/ava/button.sh run"
        ;;
esac
