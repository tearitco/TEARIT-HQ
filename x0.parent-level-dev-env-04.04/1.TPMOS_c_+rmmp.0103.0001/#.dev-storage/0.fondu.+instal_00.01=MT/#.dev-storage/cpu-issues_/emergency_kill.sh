#!/bin/bash

# emergency_kill.sh - NUCLEAR OPTION for TPMOS Zombie Processes
# Kills ANY process matching TPMOS architecture patterns, regardless of directory.
# Use when: regular kill_all.sh fails, or processes are running from unknown locations.
#
# WARNING: This is a brute-force tool. It will kill ALL matching processes system-wide.

set -e

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo -e "${RED}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${RED}║  TPMOS EMERGENCY KILL - Nuclear Zombie Cleanup          ║${NC}"
echo -e "${RED}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

# Count killed processes
KILLED=0

# Function: kill by binary name pattern (any path)
nuke_by_name() {
    local name="$1"
    # Match the binary name anywhere in process list
    local count=$(pgrep -f "${name}" 2>/dev/null | wc -l)
    if [ "$count" -gt 0 ]; then
        echo -e "${YELLOW}[KILL]${NC} ${name} (${count} processes)..."
        pkill -9 -f "${name}" 2>/dev/null || true
        KILLED=$((KILLED + count))
    fi
}

# Function: kill by path pattern (any directory containing the pattern)
nuke_by_path_pattern() {
    local pattern="$1"
    local desc="$2"
    local count=$(pgrep -f "${pattern}" 2>/dev/null | wc -l)
    if [ "$count" -gt 0 ]; then
        echo -e "${YELLOW}[KILL]${NC} ${desc} (${count} processes)..."
        pkill -9 -f "${pattern}" 2>/dev/null || true
        KILLED=$((KILLED + count))
    fi
}

echo "Phase 1: Core System Components"
echo "────────────────────────────────"
nuke_by_name "orchestrator"
nuke_by_name "chtpm_parser"
nuke_by_name "chtpm_player"
nuke_by_name "renderer"
nuke_by_name "gl_renderer"
nuke_by_name "clock_daemon"
nuke_by_name "response_handler"
nuke_by_name "keyboard_input"
nuke_by_name "joystick_input"
nuke_by_name "input_capture"
echo ""

echo "Phase 2: GL-OS Components"
echo "────────────────────────────────"
nuke_by_name "gl_desktop"
nuke_by_name "gl_os_session"
nuke_by_name "gl_os_renderer"
nuke_by_name "gl_os_loader"
nuke_by_name "gl_os"
echo ""

echo "Phase 3: App Modules"
echo "────────────────────────────────"
nuke_by_name "player_manager"
nuke_by_name "mp3_player"
nuke_by_name "mp3_store_manager"
nuke_by_name "playrm_module"
nuke_by_name "man-ops_module"
nuke_by_name "man-pal_module"
nuke_by_name "man-add_module"
nuke_by_name "op-ed_manager"
nuke_by_name "fuzzpet_manager"
nuke_by_name "fuzz_legacy_manager"
nuke_by_name "fuzzpet_v2_module"
echo ""

echo "Phase 4: Project Managers"
echo "────────────────────────────────"
nuke_by_name "test_fondu_manager"
nuke_by_name "user_manager"
nuke_by_name "fuzz-op_manager"
nuke_by_name "op-ed_manager"
nuke_by_name "p2p_manager"
nuke_by_name "ai_manager"
nuke_by_name "gl_os_manager"
echo ""

echo "Phase 5: Ops (Muscles)"
echo "────────────────────────────────"
nuke_by_name "move_player"
nuke_by_name "move_entity"
nuke_by_name "move_z"
nuke_by_name "move_selector"
nuke_by_name "interact"
nuke_by_name "render_map"
nuke_by_name "menu_op"
nuke_by_name "project_loader"
nuke_by_name "piece_manager"
nuke_by_name "proc_manager"
nuke_by_name "pdl_reader"
nuke_by_name "scan_op"
nuke_by_name "place_tile"
nuke_by_name "place_op"
nuke_by_name "toggle_selector"
nuke_by_name "stat_decay"
nuke_by_name "undo_action"
nuke_by_name "inspect_tile"
nuke_by_name "get_ops_list"
nuke_by_name "fuzzpet_action"
nuke_by_name "create_piece"
nuke_by_name "console_print"
nuke_by_name "collect_op"
nuke_by_name "inventory_op"
nuke_by_name "prisc"
echo ""

echo "Phase 6: P2P-NET Ops"
echo "────────────────────────────────"
nuke_by_name "broadcast_join"
nuke_by_name "broadcast_leave"
nuke_by_name "configure_subnet"
nuke_by_name "connect_peer"
nuke_by_name "disconnect_peer"
nuke_by_name "list_peers"
nuke_by_name "ping_peer"
nuke_by_name "check_inbox"
nuke_by_name "compose_message"
nuke_by_name "read_message"
echo ""

echo "Phase 7: Legacy & Misc"
echo "────────────────────────────────"
nuke_by_name "zombie_ai"
nuke_by_name "pal_watchdog"
nuke_by_name "player_render"
echo ""

echo "Phase 8: Nuclear Sweep (Path Patterns)"
echo "────────────────────────────────────────"
# Kill anything running from pieces/ or projects/ with .+x extension
nuke_by_path_pattern "pieces/.*/\+x/.*\.+x" "pieces/**/+x/*.+x"
nuke_by_path_pattern "projects/.*/\+x/.*\.+x" "projects/**/+x/*.+x"
nuke_by_path_pattern "pieces/system/.*" "pieces/system/**"
echo ""

echo "Phase 9: Cleanup Lock Files & State"
echo "─────────────────────────────────────"
# Remove stale lock files that might prevent clean restart
rm -f pieces/os/proc_list.txt 2>/dev/null && echo -e "${GREEN}[CLEAN]${NC} proc_list.txt" || true
rm -f pieces/apps/gl_os/session/input_focus.lock 2>/dev/null && echo -e "${GREEN}[CLEAN]${NC} input_focus.lock" || true
rm -f pieces/keyboard/history.txt 2>/dev/null && echo -e "${GREEN}[CLEAN]${NC} keyboard history" || true
rm -f pieces/apps/player_app/history.txt 2>/dev/null && echo -e "${GREEN}[CLEAN]${NC} player_app history" || true
echo ""

echo "Phase 10: Port Cleanup (P2P-NET)"
echo "────────────────────────────────"
for port in {8000..8010}; do
    if command -v fuser > /dev/null 2>&1; then
        if fuser ${port}/tcp > /dev/null 2>&1; then
            echo -e "${YELLOW}[PORT]${NC} Killing process on port ${port}..."
            fuser -k ${port}/tcp 2>/dev/null || true
        fi
    fi
done
echo ""

# Final verification
echo "Verification..."
REMAINING=$(pgrep -f "\.+x" 2>/dev/null | wc -l)
if [ "$REMAINING" -gt 0 ]; then
    echo -e "${RED}[WARN]${NC} ${REMAINING} .+x processes still running:"
    pgrep -af "\.+x" 2>/dev/null || true
else
    echo -e "${GREEN}[OK]${NC} No TPMOS processes remaining"
fi

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  Cleanup complete. Killed ~${KILLED} processes.              ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════╝${NC}"
