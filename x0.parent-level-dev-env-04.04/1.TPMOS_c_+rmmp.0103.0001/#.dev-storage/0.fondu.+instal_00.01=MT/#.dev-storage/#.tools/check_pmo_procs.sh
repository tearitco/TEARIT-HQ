#!/bin/bash
# check_pmo_procs.sh - PMO Process Diagnostic Tool
# Checks for any runaway PMO processes in the workspace

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "========================================"
echo "PMO Process Diagnostic Report"
echo "========================================"
echo "Project Root: $PROJECT_ROOT"
echo "Timestamp: $(date)"
echo ""

# Function to check processes matching a pattern (excludes system processes)
check_pattern() {
    local pattern="$1"
    local desc="$2"
    # Exclude common system processes to reduce false positives
    local count=$(ps aux | grep -v grep | grep -v "check_pmo_procs" | grep -v "chrome" | grep -v "dbus-daemon" | grep -v "avahi-daemon" | grep -v "NetworkManager" | grep -v "accounts-daemon" | grep -v "power-profiles-daemon" | grep -v "pulseaudio" | grep -v "gnome-keyring" | grep -v "rtkit-daemon" | grep -v "goa-daemon" | grep -v "ibus-daemon" | grep -v "gnome-remote" | grep "$pattern" | wc -l)
    
    if [ "$count" -gt 0 ]; then
        echo "[FOUND] $desc: $count process(es)"
        ps aux | grep -v grep | grep -v "check_pmo_procs" | grep -v "chrome" | grep -v "dbus-daemon" | grep -v "avahi-daemon" | grep -v "NetworkManager" | grep -v "accounts-daemon" | grep -v "power-profiles-daemon" | grep -v "pulseaudio" | grep -v "gnome-keyring" | grep -v "rtkit-daemon" | grep -v "goa-daemon" | grep -v "ibus-daemon" | grep -v "gnome-remote" | grep "$pattern" | head -10
        echo ""
        return 1
    fi
    return 0
}

FOUND=0

echo "--- Checking PMO-specific patterns ---"
check_pattern "prisc" "Prisc VM processes" && : || FOUND=1
check_pattern "_module" "Module processes" && : || FOUND=1
check_pattern "orchestrator" "Orchestrator processes" && : || FOUND=1
check_pattern "chtpm_parser" "Parser processes" && : || FOUND=1
check_pattern "fuzzpet" "Fuzzpet processes" && : || FOUND=1
check_pattern "playrm" "PlayRM processes" && : || FOUND=1
check_pattern "op-ed" "Op-Ed processes" && : || FOUND=1
check_pattern "editor_module" "Editor processes" && : || FOUND=1
check_pattern "pal_module" "PAL processes" && : || FOUND=1
check_pattern "ops_module" "Ops processes" && : || FOUND=1

echo "--- Checking workspace path patterns ---"
check_pattern "$PROJECT_ROOT" "Processes from project path" && : || FOUND=1
check_pattern "pieces/.*/\+x/" "Processes with pieces/.../+x/ in path" && : || FOUND=1
check_pattern "projects/.*/\+x/" "Processes with projects/.../+x/ in path" && : || FOUND=1

echo "--- Checking .+x binary pattern ---"
check_pattern "\.\+x" ".+x binaries" && : || FOUND=1

echo "--- All PMO-related processes (full list) ---"
ps aux | grep -v grep | grep -v "check_pmo_procs" | grep -v "chrome" | grep -v "dbus-daemon" | grep -v "avahi-daemon" | grep -v "NetworkManager" | grep -v "accounts-daemon" | grep -v "power-profiles-daemon" | grep -v "pulseaudio" | grep -v "gnome-keyring" | grep -v "rtkit-daemon" | grep -v "goa-daemon" | grep -v "ibus-daemon" | grep -v "gnome-remote" | grep -E "prisc|_module|orchestrator|chtpm_parser|fuzzpet|playrm|op-ed|editor_module|pal_module|ops_module|pieces/.*/\+x/|projects/.*/\+x/|\.\+x"

echo ""
echo "--- Process count summary ---"
TOTAL_PMO=$(ps aux | grep -v grep | grep -v "check_pmo_procs" | grep -v "chrome" | grep -v "dbus-daemon" | grep -v "avahi-daemon" | grep -v "NetworkManager" | grep -v "accounts-daemon" | grep -v "power-profiles-daemon" | grep -v "pulseaudio" | grep -v "gnome-keyring" | grep -v "rtkit-daemon" | grep -v "goa-daemon" | grep -v "ibus-daemon" | grep -v "gnome-remote" | grep -E "prisc|_module|orchestrator|chtpm_parser|fuzzpet|playrm|op-ed|editor_module|pal_module|ops_module|pieces/.*/\+x/|projects/.*/\+x/|\.\+x" | wc -l)
echo "Total PMO-related processes: $TOTAL_PMO"

if [ "$FOUND" -eq 0 ]; then
    echo ""
    echo "[OK] No runaway PMO processes detected."
else
    echo ""
    echo "[WARNING] Potential runaway processes found!"
    echo "Run './kill_all.sh' to clean them up."
fi

echo ""
echo "========================================"
