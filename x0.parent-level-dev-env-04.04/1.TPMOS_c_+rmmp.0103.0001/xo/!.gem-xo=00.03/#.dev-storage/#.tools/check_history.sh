#!/bin/sh
# check_history.sh - Robust Debug for keyboard input routing

# DYNAMIC PATH RESOLUTION
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$SCRIPT_DIR"
cd "$ROOT"

# Resolve current project_id
PROJECT_ID="unknown"
if [ -f "pieces/apps/player_app/manager/state.txt" ]; then
    PROJECT_ID=$(grep "project_id=" pieces/apps/player_app/manager/state.txt | cut -d'=' -f2)
fi

echo "=== KEYBOARD INPUT ROUTING DEBUG ==="
echo "Project Root: $ROOT"
echo "Active Project: $PROJECT_ID"
echo ""

echo "1. SYSTEM KEYBOARD (raw input):"
echo "   File: pieces/keyboard/history.txt"
tail -3 pieces/keyboard/history.txt 2>/dev/null || echo "   (empty)"
echo ""

echo "2. PLAYER APP (parser-injected):"
echo "   File: pieces/apps/player_app/history.txt"
tail -3 pieces/apps/player_app/history.txt 2>/dev/null || echo "   (empty)"
echo ""

if [ "$PROJECT_ID" != "unknown" ]; then
    echo "3. PROJECT HISTORY ($PROJECT_ID):"
    echo "   File: projects/$PROJECT_ID/history.txt"
    tail -3 "projects/$PROJECT_ID/history.txt" 2>/dev/null || echo "   (empty)"
    echo ""
fi

echo "4. APP STATE (player_app):"
echo "   File: pieces/apps/player_app/state.txt"
[ -f pieces/apps/player_app/state.txt ] && cat pieces/apps/player_app/state.txt || echo "   (missing)"
echo ""

echo "5. PROCESS CHECK:"
ps aux | grep -E "op-ed_manager|fuzz-op_manager|chtpm_parser" | grep -v grep
echo ""

echo "=== FILE SIZES ==="
[ -f pieces/keyboard/history.txt ] && wc -l pieces/keyboard/history.txt
[ -f pieces/apps/player_app/history.txt ] && wc -l pieces/apps/player_app/history.txt
if [ "$PROJECT_ID" != "unknown" ]; then
    [ -f "projects/$PROJECT_ID/history.txt" ] && wc -l "projects/$PROJECT_ID/history.txt"
fi
echo ""
