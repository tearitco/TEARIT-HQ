#!/bin/bash
# pmo_cpu_health.sh - PMO Project CPU Health Monitor
# 
# Purpose: Monitor CPU health across ALL PMO versioned folders,
#          not just the current working directory.
# 
# Features:
#   - Lists all running PMO-related processes
#   - Shows CPU usage per process
#   - Detects runaway process counts
#   - Optional: Kill processes interactively
#   - Works across all versioned PMO folders
#
# Usage:
#   ./pmo_cpu_health.sh           # Show processes
#   ./pmo_cpu_health.sh --kill    # Interactive kill mode
#   ./pmo_cpu_health.sh --nuke    # Kill all without confirmation
#   ./pmo_cpu_health.sh --watch   # Continuous monitoring mode

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PMO_PATTERNS=(
    "\.\\+x"
    "prisc\\+x"
    "fuzz_legacy"
    "editor_module"
    "op-ed_module"
    "playrm_module"
    "chtpm_parser"
    "orchestrator"
    "renderer"
    "gl_renderer"
    "pal_watchdog"
)

# Build regex pattern from array
PMO_REGEX=""
for pattern in "${PMO_PATTERNS[@]}"; do
    if [ -z "$PMO_REGEX" ]; then
        PMO_REGEX="$pattern"
    else
        PMO_REGEX="$PMO_REGEX|$pattern"
    fi
done

# Functions
print_header() {
    echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║${NC}  ${GREEN}PMO CPU Health Monitor${NC}                              ${BLUE}║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

print_section() {
    echo -e "${YELLOW}─────────────────────────────────────────────────────────────${NC}"
    echo -e "${YELLOW}$1${NC}"
    echo -e "${YELLOW}─────────────────────────────────────────────────────────────${NC}"
}

get_pmo_processes() {
    # Get all PMO-related processes with full details
    ps aux | grep -E "$PMO_REGEX" | grep -v grep | grep -v "pmo_cpu_health.sh"
}

count_pmo_processes() {
    get_pmo_processes | wc -l
}

get_cpu_usage() {
    local pid=$1
    ps -p "$pid" -o %cpu= 2>/dev/null | tr -d ' '
}

get_process_summary() {
    echo -e "${BLUE}=== PMO Process Summary ===${NC}"
    echo ""
    
    local count
    count=$(count_pmo_processes)
    
    if [ "$count" -eq 0 ]; then
        echo -e "${GREEN}✓ No PMO processes running${NC}"
        echo ""
        echo -e "Status: ${GREEN}HEALTHY${NC}"
        return 0
    fi
    
    echo -e "Total PMO processes: ${YELLOW}$count${NC}"
    echo ""
    
    # Categorize by status
    local healthy=0
    local warning=0
    local critical=0
    
    while IFS= read -r line; do
        if [ -z "$line" ]; then continue; fi
        
        local pid cpu mem
        pid=$(echo "$line" | awk '{print $2}')
        cpu=$(echo "$line" | awk '{print $3}')
        mem=$(echo "$line" | awk '{print $4}')
        
        # Remove decimal for comparison
        local cpu_int
        cpu_int=$(echo "$cpu" | cut -d'.' -f1)
        
        if [ "$cpu_int" -lt 5 ]; then
            ((healthy++))
        elif [ "$cpu_int" -lt 20 ]; then
            ((warning++))
        else
            ((critical++))
        fi
    done < <(get_pmo_processes)
    
    echo -e "  ${GREEN}Healthy (<5% CPU):${NC}  $healthy"
    echo -e "  ${YELLOW}Warning (5-20%):${NC}  $warning"
    echo -e "  ${RED}Critical (>20%):${NC}   $critical"
    echo ""
    
    # Overall status
    if [ "$critical" -gt 0 ]; then
        echo -e "Status: ${RED}CRITICAL - Immediate action recommended${NC}"
        return 2
    elif [ "$warning" -gt 0 ]; then
        echo -e "Status: ${YELLOW}WARNING - Monitor closely${NC}"
        return 1
    else
        echo -e "Status: ${GREEN}HEALTHY${NC}"
        return 0
    fi
}

list_processes_detailed() {
    echo -e "${BLUE}=== Detailed Process List ===${NC}"
    echo ""
    
    if [ -z "$(get_pmo_processes)" ]; then
        echo -e "${GREEN}No PMO processes found.${NC}"
        return 0
    fi
    
    # Header
    printf "%-8s %-10s %-8s %-8s %-10s %s\n" \
        "${BLUE}PID${NC}" \
        "${BLUE}CPU%${NC}" \
        "${BLUE}MEM%${NC}" \
        "${BLUE}STAT${NC}" \
        "${BLUE}TIME${NC}" \
        "${BLUE}COMMAND${NC}"
    echo "─────────────────────────────────────────────────────────────────────────"
    
    while IFS= read -r line; do
        if [ -z "$line" ]; then continue; fi
        
        local pid cpu mem stat time cmd
        pid=$(echo "$line" | awk '{print $2}')
        cpu=$(echo "$line" | awk '{print $3}')
        mem=$(echo "$line" | awk '{print $4}')
        stat=$(echo "$line" | awk '{print $8}')
        time=$(echo "$line" | awk '{print $10}')
        cmd=$(echo "$line" | awk '{for(i=11;i<=NF;i++) printf $i" "; print ""}')
        
        # Color-code by CPU usage
        local cpu_int
        cpu_int=$(echo "$cpu" | cut -d'.' -f1)
        local color="$GREEN"
        if [ "$cpu_int" -ge 20 ]; then
            color="$RED"
        elif [ "$cpu_int" -ge 5 ]; then
            color="$YELLOW"
        fi
        
        printf "%-8s ${color}%-10s${NC} %-8s %-8s %-10s %s\n" \
            "$pid" "$cpu" "$mem" "$stat" "$time" "$cmd"
    done < <(get_pmo_processes)
    
    echo ""
}

list_processes_by_folder() {
    echo -e "${BLUE}=== Processes by Version Folder ===${NC}"
    echo ""
    
    # Find all PMO version folders
    local pmo_dirs
    pmo_dirs=$(find /home/no/Desktop/Piecemark-IT -type d -name "^.PMO_c_+rmmp_*" 2>/dev/null | sort)
    
    if [ -z "$pmo_dirs" ]; then
        echo -e "${YELLOW}No PMO version folders found.${NC}"
        return 0
    fi
    
    while IFS= read -r dir; do
        local dirname
        dirname=$(basename "$dir")
        local count
        count=$(get_pmo_processes | grep -c "$dir" 2>/dev/null || echo "0")
        
        if [ "$count" -gt 0 ]; then
            echo -e "  ${YELLOW}$dirname${NC}: $count process(es)"
        else
            echo -e "  ${GREEN}$dirname${NC}: 0 processes"
        fi
    done <<< "$pmo_dirs"
    
    echo ""
}

interactive_kill() {
    echo -e "${RED}=== Interactive Kill Mode ===${NC}"
    echo ""
    echo "Select a process to kill by PID, or type 'all' to kill all PMO processes."
    echo "Type 'q' to quit."
    echo ""
    
    while true; do
        list_processes_detailed
        
        echo -n "Enter PID to kill (or 'all'/'q'): "
        read -r choice
        
        if [ "$choice" = "q" ] || [ "$choice" = "Q" ]; then
            echo "Exiting."
            return 0
        elif [ "$choice" = "all" ] || [ "$choice" = "ALL" ]; then
            echo -e "${RED}Killing all PMO processes...${NC}"
            kill_all_pmo
            return 0
        elif [[ "$choice" =~ ^[0-9]+$ ]]; then
            # Verify PID exists
            if ps -p "$choice" > /dev/null 2>&1; then
                local cmd
                cmd=$(ps -p "$choice" -o comm= 2>/dev/null)
                echo -e "${RED}Killing PID $choice ($cmd)...${NC}"
                kill -TERM "$choice" 2>/dev/null || kill -9 "$choice" 2>/dev/null
                echo -e "${GREEN}Done.${NC}"
                sleep 1
            else
                echo -e "${YELLOW}PID $choice not found.${NC}"
            fi
        else
            echo -e "${YELLOW}Invalid input. Try again.${NC}"
        fi
    done
}

kill_all_pmo() {
    echo -e "${RED}Sending SIGTERM to all PMO processes...${NC}"
    
    while IFS= read -r line; do
        if [ -z "$line" ]; then continue; fi
        local pid
        pid=$(echo "$line" | awk '{print $2}')
        kill -TERM "$pid" 2>/dev/null &
    done < <(get_pmo_processes)
    
    sleep 2
    
    echo -e "${RED}Force-killing remaining processes with SIGKILL...${NC}"
    while IFS= read -r line; do
        if [ -z "$line" ]; then continue; fi
        local pid
        pid=$(echo "$line" | awk '{print $2}')
        kill -9 "$pid" 2>/dev/null &
    done < <(get_pmo_processes)
    
    sleep 1
    echo -e "${GREEN}Cleanup complete.${NC}"
}

watch_mode() {
    echo -e "${BLUE}=== Continuous Monitoring Mode ===${NC}"
    echo "Press Ctrl+C to exit."
    echo ""
    
    while true; do
        clear
        print_header
        
        local timestamp
        timestamp=$(date '+%Y-%m-%d %H:%M:%S')
        echo -e "${BLUE}Last updated:${NC} $timestamp"
        echo ""
        
        get_process_summary
        echo ""
        list_processes_detailed
        list_processes_by_folder
        
        echo ""
        echo -e "${BLUE}Refreshing in 3 seconds...${NC}"
        sleep 3
    done
}

show_help() {
    echo "PMO CPU Health Monitor - Usage"
    echo ""
    echo "  ./pmo_cpu_health.sh           Show all PMO processes and health status"
    echo "  ./pmo_cpu_health.sh --kill    Interactive kill mode"
    echo "  ./pmo_cpu_health.sh --nuke    Kill all PMO processes (no confirmation)"
    echo "  ./pmo_cpu_health.sh --watch   Continuous monitoring mode"
    echo "  ./pmo_cpu_health.sh --help    Show this help message"
    echo ""
    echo "Features:"
    echo "  - Monitors all PMO version folders, not just current directory"
    echo "  - Color-coded CPU usage (green=healthy, yellow=warning, red=critical)"
    echo "  - Process count thresholds (normal: 0-5, warning: 6-15, critical: 16+)"
    echo "  - Detects orphaned processes from crashes"
    echo ""
}

# Main
main() {
    print_header
    
    case "${1:-}" in
        --kill|-k)
            interactive_kill
            ;;
        --nuke|-n)
            echo -e "${RED}NUKE MODE: Killing all PMO processes without confirmation...${NC}"
            kill_all_pmo
            ;;
        --watch|-w)
            watch_mode
            ;;
        --help|-h)
            show_help
            ;;
        "")
            get_process_summary
            echo ""
            list_processes_detailed
            list_processes_by_folder
            
            # Final recommendations
            print_section "Recommendations"
            local count
            count=$(count_pmo_processes)
            
            if [ "$count" -eq 0 ]; then
                echo -e "${GREEN}✓ System is clean. No action needed.${NC}"
            elif [ "$count" -le 5 ]; then
                echo -e "${GREEN}✓ Process count is normal.${NC}"
                echo "  If you're done working, consider running: ${BLUE}./kill_all.sh${NC}"
            elif [ "$count" -le 15 ]; then
                echo -e "${YELLOW}⚠ Process count is elevated.${NC}"
                echo "  Consider running: ${BLUE}./pmo_cpu_health.sh --kill${NC}"
            else
                echo -e "${RED}⚠ CRITICAL: Too many processes!${NC}"
                echo "  Run immediately: ${BLUE}./pmo_cpu_health.sh --nuke${NC}"
                echo "  Or use the surgical tool: ${BLUE}./kill_all.sh${NC}"
            fi
            
            echo ""
            echo -e "For detailed documentation, see: ${BLUE}#.docs/!.cpu_health.txt${NC}"
            echo -e "For crash analysis, see: ${BLUE}#.docs/!.cpu_fix.txt${NC}"
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo ""
            show_help
            exit 1
            ;;
    esac
}

main "$@"
