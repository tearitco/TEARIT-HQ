#!/bin/bash
# world_manager entry point
# Compiles prisc+x and executes world_manager.pal

ACTION="${1:-run}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    run|start|boot)
        # REAL FIX 2026-09-28 (cpu_loop_analysis.txt): no singleton
        # guard meant a second launch (e.g. a relaunch-on-restart race,
        # or a stray leftover from a crash loop) could run a SECOND
        # world_manager.pal concurrently - doubling every real cost
        # this file already had (tick-loop exec rate, sync forks).
        # Same PID-file pattern already proven in cpu_watch_daemon.sh:
        # self-healing (a stale file whose PID is dead is overwritten,
        # never trusted blindly), refuses only if a REAL live PID holds it.
        PIDFILE="$SCRIPT_DIR/system/.world_manager.pid"
        mkdir -p "$SCRIPT_DIR/system"
        if [ -f "$PIDFILE" ]; then
            OLD_PID=$(cat "$PIDFILE" 2>/dev/null)
            if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
                echo "world_manager: already running as pid $OLD_PID - refusing second instance" >&2
                exit 0
            fi
        fi

        echo "Starting world_manager..."

        # Compile prisc+x from canonical location (use -O0 to avoid optimizer issues)
        _pcd="$SCRIPT_DIR"; while [ "$_pcd" != "/" ] && [ ! -d "$_pcd/&.widgits/_shared-lib" ]; do _pcd="$(dirname "$_pcd")"; done
        [ -f "$_pcd/&.widgits/_shared-lib/system/prisc+x.c" ] && \
            gcc -O0 -std=c11 -w -o "$SCRIPT_DIR/system/prisc+x" "$_pcd/&.widgits/_shared-lib/system/prisc+x.c" 2>/dev/null || true

        # Create system directory if needed
        mkdir -p "$SCRIPT_DIR/system"

        # cd into world_manager directory so prisc+x resolves relative paths correctly
        cd "$SCRIPT_DIR"

        # `exec` below replaces this shell's own process image with
        # prisc+x, so it keeps this same PID - writing it now means the
        # PID file stays valid for as long as prisc+x actually runs, no
        # separate cleanup-on-exit needed (self-healing check above
        # already handles the "process is really dead" case).
        echo "$$" > "$PIDFILE"

        # Execute world_manager.pal via prisc+x with relative paths
        exec "./system/prisc+x" "./world_manager.pal"
        ;;

    kill|stop)
        pkill -f "world_manager.pal" 2>/dev/null || true
        pkill -f "world_manager_init" 2>/dev/null || true
        pkill -f "world_manager_tick" 2>/dev/null || true
        pkill -f "sync_entity_positions" 2>/dev/null || true
        rm -f "$SCRIPT_DIR/system/.world_manager.pid"
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
