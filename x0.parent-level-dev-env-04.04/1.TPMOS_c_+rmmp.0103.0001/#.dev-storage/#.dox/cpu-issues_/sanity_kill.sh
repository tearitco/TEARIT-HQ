#!/bin/bash
# TPMOS Global Sanity Kill Script (Aggressive Edition)
# Lists and kills orphaned processes from ANY version/directory on the system.

echo "--- TPMOS GLOBAL SANITY CLEANUP ---"

# Define exclusion pattern to avoid killing system tools or the agent itself
FILTER_EXCLUDE="chrome|firefox|gemini|node|grep|ps aux|pgrep|sanity_kill.sh|bash|ssh|systemd"

echo "1. DIAGNOSTIC: Current Top CPU Consumers (Non-TPMOS included for context)"
echo "-------------------------------------------------------------------------------"
ps aux --sort=-%cpu | head -n 6 | grep -v "USER"
echo "-------------------------------------------------------------------------------"
echo "NOTE: If you see 'tracker-miner-fs' high, it's a system file indexer."
echo ""

echo "2. SEARCHING: Looking for TPMOS/CHTPM processes system-wide..."

# Aggressive keyword list based on TPMOS architecture
KEYWORDS="pieces|projects|tpmos|chtpm|fuzzpet|playrm|gl_os|orchestrator|renderer|man-ops|man-pal|man-add|op-ed|pdl_reader|mp3_player|zombie_ai"

# Search full command lines (-f) for any mention of the keywords or .+x binaries
PROCS=$(pgrep -a -f "$KEYWORDS" | grep -vE "$FILTER_EXCLUDE" | grep -E "(/pieces/|/projects/|\.\+x|tpmos|chtpm|fuzzpet)")

if [ -z "$PROCS" ]; then
    echo "No matching TPMOS processes found."
    exit 0
fi

echo ""
echo "IDENTIFIED CANDIDATES FOR CLEANUP:"
echo "-------------------------------------------------------------------------------"
echo "$PROCS"
echo "-------------------------------------------------------------------------------"
echo ""

# Ask for confirmation
echo "Preparing to terminate these processes in 3 seconds... (Ctrl+C to abort)"
sleep 1
echo "2..."
sleep 1
echo "1..."
sleep 1

# Extract PIDs and kill
PIDS=$(echo "$PROCS" | awk '{print $1}')
for pid in $PIDS; do
    echo "Killing PID $pid..."
    kill -9 "$pid" 2>/dev/null
done

echo ""
echo "Cleanup complete. System should be breathing easier now."
