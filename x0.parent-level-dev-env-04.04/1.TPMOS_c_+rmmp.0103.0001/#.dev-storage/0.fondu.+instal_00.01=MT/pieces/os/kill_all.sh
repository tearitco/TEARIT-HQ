#!/bin/bash

# kill_all.sh - Surgical TPM Component Cleanup
# Targets specifically binaries in pieces/ and projects/ directories.
# Updated: April 1, 2026 - Added Fondu project support

# ═══════════════════════════════════════════════════════════════
# ⚠ CRITICAL: KILL REGEX — DO NOT CHANGE WITHOUT TESTING
# ═══════════════════════════════════════════════════════════════
# The "Nuclear Option" regex at the bottom uses [.] and [+] character classes.
# If you revert these to \+x or \.+x, NO processes will be killed.
# Old processes will accumulate → CPU spikes → crashes on low-power machines.
# Test ANY regex change with:
#   echo "pieces/apps/playrm/plugins/+x/playrm_module.+x" | grep -E 'pieces/.*/[+]x/.*[.][+]x'
#   (Should print the path — if blank, regex is broken)
# ═══════════════════════════════════════════════════════════════

# Optional: Run diagnostic first if check script exists
if [ -f "#.tools/check_pmo_procs.sh" ]; then
    echo "=== Running PMO Process Diagnostic ==="
    bash "#.tools/check_pmo_procs.sh"
    echo ""
fi

# Function for surgical killing
surgical_kill() {
    local name="$1"
    # Match specific paths to avoid killing browsers/system tools, allow common extensions
    local pattern_pieces="pieces/.*/${name}(\.\+x|\.exe|\.bin)?"
    local pattern_projects="projects/.*/${name}(\.\+x|\.exe|\.bin)?"
    local pattern_system="pieces/system/.*/${name}"

    # Use pgrep -f to check if process exists anywhere in the command line.
    if pgrep -f "$pattern_pieces" > /dev/null; then
        echo "Killing $name (pieces)..."
        pkill -9 -f "$pattern_pieces"
    fi
    if pgrep -f "$pattern_projects" > /dev/null; then
        echo "Killing $name (projects)..."
        pkill -9 -f "$pattern_projects"
    fi
    if pgrep -f "$pattern_system" > /dev/null; then
        echo "Killing $name (system)..."
        pkill -9 -f "$pattern_system"
    fi
}

echo "Cleaning environment..."

# System components
surgical_kill "orchestrator"
surgical_kill "chtpm_parser"
surgical_kill "chtpm_player"
surgical_kill "renderer"
surgical_kill "gl_renderer"
surgical_kill "clock_daemon"
surgical_kill "response_handler"
surgical_kill "keyboard_input"
surgical_kill "joystick_input"
surgical_kill "input_capture"

# Legacy apps
surgical_kill "fuzzpet_manager"
surgical_kill "fuzz_legacy_manager"
surgical_kill "playrm_module"
surgical_kill "fuzzpet_v2_module"

# System apps (man-*)
surgical_kill "man-ops_module"
surgical_kill "man-pal_module"
surgical_kill "man-add_module"
surgical_kill "op-ed_module"
surgical_kill "user_module"
surgical_kill "loader_module"
surgical_kill "editor_module"
surgical_kill "db_editor_module"
surgical_kill "pal_editor_module"

# Ops (muscles)
surgical_kill "move_player"
surgical_kill "move_z"
surgical_kill "move_entity"
surgical_kill "interact"
surgical_kill "render_map"
surgical_kill "menu_op"
surgical_kill "scan_op"
surgical_kill "place_op"
surgical_kill "collect_op"
surgical_kill "project_loader"
surgical_kill "piece_manager"
surgical_kill "proc_manager"
surgical_kill "pdl_reader"
surgical_kill "ai_manager"
surgical_kill "player_manager"
surgical_kill "mp3_player"
surgical_kill "mp3-store_manager"
surgical_kill "play_mp3"
surgical_kill "player_render"
surgical_kill "prisc"
surgical_kill "zombie_ai"
surgical_kill "gl_os"
surgical_kill "gl_os_manager"
surgical_kill "gl_os_renderer"
surgical_kill "gl_desktop"

# Fondu projects (test_fondu, user, etc.)
surgical_kill "test_fondu_manager"
surgical_kill "op-ed_manager"
surgical_kill "slop-ed-dev_manager"
surgical_kill "fuzz-op_manager"
surgical_kill "fuzz-op-gl_manager"
surgical_kill "piececraft-3d_manager"
surgical_kill "groq-ollama_manager"
surgical_kill "gem-api_manager"
surgical_kill "gem-dev_manager"
surgical_kill "cpp-llm_manager"
surgical_kill "tsots-online_manager"

# Kill Watchdog
pkill -9 -f "pal_watchdog.sh" 2>/dev/null

# Nuclear Option: Kill any CHTPM-related executables in pieces/ or projects/ directories
# This aims to catch residual processes that might not fit the specific surgical kill patterns.
echo "Checking for residual CHTPM processes..."
# Target processes whose command line contains 'pieces/' or 'projects/' and a path component.
pkill -9 -f "/(pieces|projects)/.+/.+" 2>/dev/null

# Port Cleanup: Ensure P2P ports are released
echo "Cleaning up network ports (8000-8010)..."
for port in {8000..8010}; do
    if command -v fuser > /dev/null; then
        fuser -k ${port}/tcp 2>/dev/null
    fi
done

# Reset input focus locks
rm -f pieces/apps/gl_os/session/input_focus.lock

# Clear the process list log to ensure a clean state for the next run.
rm -f pieces/os/proc_list.txt

echo "Cleanup complete."