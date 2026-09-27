#!/bin/bash
# world_manager/button.sh - HQ world state orchestrator
# Manages shared world state: entities, animations, events
# Auto-starts via autostart.pdl on HQ boot

ACTION="${1:-run}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Find canonical prisc+x and compile if needed
_pcd="$HOUSE"
while [ "$_pcd" != "/" ] && [ ! -d "$_pcd/&.widgits/_shared-lib" ]; do
    _pcd="$(dirname "$_pcd")"
done

PRISC_SRC="$_pcd/&.widgits/_shared-lib/system/prisc+x.c"
PRISC_BIN="$SCRIPT_DIR/system/prisc+x"

mkdir -p "$SCRIPT_DIR/system"

case "$ACTION" in
    run|r|start)
        # Compile prisc+x if needed
        if [ -f "$PRISC_SRC" ]; then
            gcc -O2 -std=c11 -w -o "$PRISC_BIN" "$PRISC_SRC" 2>/dev/null || true
        fi

        # Verify prisc binary exists
        if [ ! -x "$PRISC_BIN" ]; then
            echo "Error: prisc+x not found at $PRISC_BIN"
            exit 1
        fi

        # Execute world_manager.pal via prisc
        echo "[world-manager] Starting world orchestrator..."
        exec "$PRISC_BIN" "$SCRIPT_DIR/world_manager.pal"
        ;;

    kill|k|stop)
        echo "[world-manager] Stopping world orchestrator..."
        pkill -f "prisc+x.*world_manager.pal" 2>/dev/null || true
        pkill -f "world_manager_init" 2>/dev/null || true
        pkill -f "world_manager_tick" 2>/dev/null || true
        pkill -f "sync_entity_positions" 2>/dev/null || true
        echo "[world-manager] Stopped"
        ;;

    status)
        echo "[world-manager] Status:"
        pgrep -f "prisc+x.*world_manager.pal" >/dev/null && echo "  RUNNING (prisc)" || echo "  STOPPED"
        [ -f "$SCRIPT_DIR/state/page_manager.log" ] && \
            echo "  Last log entry:" && tail -1 "$SCRIPT_DIR/state/page_manager.log" || \
            echo "  No log file"
        ;;

    *)
        echo "Usage: $0 {run|kill|status}"
        echo ""
        echo "Actions:"
        echo "  run    - Start world manager (default)"
        echo "  kill   - Stop world manager"
        echo "  status - Show world manager status"
        exit 1
        ;;
esac
