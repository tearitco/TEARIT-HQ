#!/bin/bash
# world_manager entry point
# Compiles prisc+x and executes world_manager.pal

ACTION="${1:-run}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    run|start|boot)
        echo "Starting world_manager..."

        # Compile prisc+x from canonical location
        _pcd="$SCRIPT_DIR"; while [ "$_pcd" != "/" ] && [ ! -d "$_pcd/&.widgits/_shared-lib" ]; do _pcd="$(dirname "$_pcd")"; done
        [ -f "$_pcd/&.widgits/_shared-lib/system/prisc+x.c" ] && \
            gcc -O2 -std=c11 -w -o "$SCRIPT_DIR/system/prisc+x" "$_pcd/&.widgits/_shared-lib/system/prisc+x.c" 2>/dev/null || true

        # Create system directory if needed
        mkdir -p "$SCRIPT_DIR/system"

        # Execute world_manager.pal via prisc+x
        exec "$SCRIPT_DIR/system/prisc+x" "$SCRIPT_DIR/world_manager.pal"
        ;;

    kill|stop)
        pkill -f "world_manager.pal" 2>/dev/null || true
        pkill -f "world_manager_init" 2>/dev/null || true
        pkill -f "world_manager_tick" 2>/dev/null || true
        pkill -f "sync_entity_positions" 2>/dev/null || true
        echo "Killed world_manager."
        ;;

    clean)
        pkill -f "world_manager.pal" 2>/dev/null || true
        pkill -f "world_manager_init" 2>/dev/null || true
        pkill -f "world_manager_tick" 2>/dev/null || true
        pkill -f "sync_entity_positions" 2>/dev/null || true
        rm -rf "$SCRIPT_DIR/system"
        echo "Cleaned world_manager."
        ;;

    *)
        echo "Usage: $0 [run|kill|clean]"
        exit 1
        ;;
esac
