#!/bin/bash
# dashboard_xod.sh - Real-time XOD Tournament Dashboard (CLI / X11 terminal)
#
# Displays live tournament progress: agent standings, event stream,
# generation progress, and winner announcement.
#
# Works in any terminal (xterm, urxvt, alacritty, kitty, etc.).
# Uses ANSI escape codes for colors and cursor positioning.
#
# Usage: ./dashboard_xod.sh [tournament_dir] [refresh_seconds]
#   tournament_dir: path to tournament results (default: ./pieces/tournament_*)
#   refresh_seconds: how often to refresh (default: 1)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
XOD_DIR="${SCRIPT_DIR}"
REFRESH="${2:-1}"

# Find the latest tournament directory
if [ -n "$1" ] && [ -d "$1" ]; then
    TOURNAMENT_DIR="$1"
else
    TOURNAMENT_DIR=$(ls -td ${XOD_DIR}/pieces/tournament_* 2>/dev/null | head -1)
fi

if [ -z "$TOURNAMENT_DIR" ] || [ ! -d "$TOURNAMENT_DIR" ]; then
    echo "No tournament directory found. Run tournament_xod.sh first."
    exit 1
fi

# ANSI colors
C_RESET='\033[0m'
C_BOLD='\033[1m'
C_RED='\033[31m'
C_GREEN='\033[32m'
C_YELLOW='\033[33m'
C_BLUE='\033[34m'
C_MAGENTA='\033[35m'
C_CYAN='\033[36m'
C_WHITE='\033[37m'
C_DIM='\033[2m'

# ANSI cursor control
CURSOR_HOME='\033[H'
CURSOR_HIDE='\033[?25l'
CURSOR_SHOW='\033[?25m'
CLEAR_SCREEN='\033[2J'

# Trap cleanup
cleanup() {
    printf "${CURSOR_SHOW}${C_RESET}\n"
    stty echo
    exit 0
}
trap cleanup SIGINT SIGTERM

# Hide cursor and disable echo
printf "${CURSOR_HIDE}"
stty -echo

# Count agents
AGENT_COUNT=$(ls -1 ${TOURNAMENT_DIR}/agent*_fitness.txt 2>/dev/null | wc -l)
if [ "$AGENT_COUNT" -eq 0 ]; then
    echo "No agent data found in ${TOURNAMENT_DIR}"
    cleanup
fi

draw_dashboard() {
    local now
    now=$(date '+%H:%M:%S')

    # Header
    printf "${CURSOR_HOME}${C_BOLD}${C_CYAN}"
    printf "╔══════════════════════════════════════════════════════════════╗\n"
    printf "║          XOD TOURNAMENT DASHBOARD - %s               ║\n" "$now"
    printf "╚══════════════════════════════════════════════════════════════╝\n"
    printf "${C_RESET}\n"

    # Agent standings
    printf "${C_BOLD}${C_BLUE}┌─ AGENT STANDINGS ─────────────────────────────────────────┐${C_RESET}\n"

    for agent_file in $(ls -1 ${TOURNAMENT_DIR}/agent*_fitness.txt 2>/dev/null | sort); do
        agent_id=$(basename "$agent_file" | sed 's/agent\([0-9]*\)_fitness.txt/\1/')
        if [ -f "$agent_file" ]; then
            fitness=$(grep "^fitness=" "$agent_file" | tail -1 | cut -d= -f2)
            portfolio=$(grep "^portfolio_value=" "$agent_file" | tail -1 | cut -d= -f2)
            cash=$(grep "^cash=" "$agent_file" | tail -1 | cut -d= -f2)
            price=$(grep "^stock_price=" "$agent_file" | tail -1 | cut -d= -f2)
            held=$(grep "^shares_held=" "$agent_file" | tail -1 | cut -d= -f2)
            chain=$(grep "^chain=" "$agent_file" | tail -1 | cut -d= -f2)
            printf "${C_GREEN}│ Agent %s: fitness=%-8s portfolio=%-10s cash=%-8s ║\n" \
                "$agent_id" "$fitness" "$portfolio" "$cash"
            printf "${C_DIM}│          price=%-8s held=%-4s chain=%-20s ║${C_RESET}\n" \
                "$price" "$held" "$chain"
        fi
    done

    printf "${C_BOLD}${C_BLUE}└──────────────────────────────────────────────────────────┘${C_RESET}\n\n"

    # Event bus stream (last 10 events)
    printf "${C_BOLD}${C_MAGENTA}┌─ EVENT BUS STREAM ───────────────────────────────────────┐${C_RESET}\n"

    if [ -f "${TOURNAMENT_DIR}/../interact_relay.txt" ]; then
        tail -10 "${TOURNAMENT_DIR}/../interact_relay.txt" | while IFS= read -r line; do
            if [ -n "$line" ]; then
                printf "${C_DIM}│ ${C_RESET}%s\n" "$line"
            fi
        done
    else
        printf "${C_DIM}│ (no events yet)${C_RESET}\n"
    fi

    printf "${C_BOLD}${C_MAGENTA}└──────────────────────────────────────────────────────────┘${C_RESET}\n\n"

    # Footer
    printf "${C_DIM}Press Ctrl+C to exit. Refresh: %ss | Agents: %d | Dir: %s${C_RESET}\n" \
        "$REFRESH" "$AGENT_COUNT" "$(basename "$TOURNAMENT_DIR")"
}

# Main loop
while true; do
    draw_dashboard
    sleep "$REFRESH"
done