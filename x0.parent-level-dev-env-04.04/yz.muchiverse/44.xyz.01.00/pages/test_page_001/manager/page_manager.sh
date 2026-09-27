#!/bin/bash
# page_manager.sh - Central page state manager
#
# Manages a single page's world state via append-only ledgers.
# Polls entity positions, runs common events on triggers,
# manages animations. Mirrors RPG Maker GameManager pattern.
#
# Run via: ./page_manager.sh
# Or launch from page_load event

PAGE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOUSE_ROOT="$(cd "$PAGE_ROOT/../.." && pwd)"
LOG_FILE="$PAGE_ROOT/state/page_manager.log"

# Function to log debug output (write only, don't output to stdout)
log_debug() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >> "$LOG_FILE"
}

# Ensure all directories and files exist
init_ledgers() {
    mkdir -p "$PAGE_ROOT/state"
    mkdir -p "$PAGE_ROOT/event_pkg"

    # Create empty ledgers if they don't exist
    for ledger in entities_live world_events animation_queue; do
        touch "$PAGE_ROOT/state/${ledger}.txt"
    done

    # Create cursor marker file if it doesn't exist
    if [ ! -f "$PAGE_ROOT/state/page_manager.cursor" ]; then
        cat > "$PAGE_ROOT/state/page_manager.cursor" << EOF
entities_live: 0
world_events: 0
animation_queue: 0
EOF
    fi

    # Initialize log
    > "$LOG_FILE"
}

# Read cursor positions from marker file (returns three values: ent events anim)
read_cursors() {
    local ent=0 ev=0 anim=0

    if [ -f "$PAGE_ROOT/state/page_manager.cursor" ]; then
        while IFS=': ' read -r key value; do
            case "$key" in
                entities_live) ent=$value ;;
                world_events) ev=$value ;;
                animation_queue) anim=$value ;;
            esac
        done < "$PAGE_ROOT/state/page_manager.cursor"
    fi

    echo "$ent $ev $anim"
}

# Write cursor positions back to marker file
write_cursors() {
    local ent=$1 ev=$2 anim=$3
    cat > "$PAGE_ROOT/state/page_manager.cursor" << EOF
entities_live: $ent
world_events: $ev
animation_queue: $anim
EOF
}

# Parse trigger table from event_triggers.pdl (returns via echo, one per line)
load_trigger_table() {
    local trigger_file="$PAGE_ROOT/event_pkg/event_triggers.pdl"

    if [ ! -f "$trigger_file" ]; then
        return
    fi

    local line_num=0
    while IFS= read -r line; do
        ((line_num++))
        # Skip header and empty lines
        if [ $line_num -le 1 ] || [ -z "$line" ]; then
            continue
        fi

        # Parse pipe-separated values
        IFS='|' read -ra parts <<< "$line"
        if [ ${#parts[@]} -ge 3 ]; then
            local trigger_name="${parts[0]// }"
            local condition="${parts[1]// }"
            local event_func="${parts[2]// }"
            echo "$trigger_name|$condition|$event_func"
        fi
    done < "$trigger_file"
}

# Check if entity movement triggers any events
check_entity_triggers() {
    local entity_line="$1"

    # Parse entity line: "entity_id | x=N | y=N | state=..."
    IFS='|' read -ra parts <<< "$entity_line"
    local entity_id="${parts[0]// }"

    local x_val="" y_val=""
    for part in "${parts[@]}"; do
        part="${part// }"
        if [[ "$part" =~ ^x=([0-9]+)$ ]]; then
            x_val="${BASH_REMATCH[1]}"
        elif [[ "$part" =~ ^y=([0-9]+)$ ]]; then
            y_val="${BASH_REMATCH[1]}"
        fi
    done

    log_debug "Entity moved: $entity_id at ($x_val, $y_val)"

    # Load trigger table
    while IFS='|' read -r trigger_name condition event_func; do
        if [ "$trigger_name" = "entity_moved" ]; then
            # Call the event function if it exists
            if type "$event_func" &>/dev/null; then
                log_debug "  -> Calling: $event_func($entity_id, $x_val, $y_val)"
                "$event_func" "$entity_id" "$x_val" "$y_val" || true
            fi
        elif [ "$trigger_name" = "zone_enter" ]; then
            # Check if entity is in zone_1 bounds (x_min=140, x_max=160, y_min=190, y_max=210)
            if [ "$x_val" -ge 140 ] && [ "$x_val" -le 160 ] && [ "$y_val" -ge 190 ] && [ "$y_val" -le 210 ]; then
                if type "$event_func" &>/dev/null; then
                    log_debug "  -> Zone trigger! $event_func($entity_id, zone_1)"
                    "$event_func" "$entity_id" "zone_1" || true
                fi
            fi
        fi
    done < <(load_trigger_table)
}

# Poll entities_live.txt for new position changes
poll_entities() {
    local start_line=$1
    local current_line=$start_line
    local entities_file="$PAGE_ROOT/state/entities_live.txt"

    if [ ! -f "$entities_file" ]; then
        echo $start_line
        return
    fi

    local line_num=0
    while IFS= read -r line; do
        ((line_num++))
        if [ $line_num -gt $start_line ] && [ -n "$line" ]; then
            # Process this new entity position change
            check_entity_triggers "$line"
            current_line=$line_num
        fi
    done < "$entities_file"

    echo $current_line
}

# Poll animation queue and run animations
poll_animations() {
    local start_line=$1
    local current_line=$start_line
    local anim_file="$PAGE_ROOT/state/animation_queue.txt"

    if [ ! -f "$anim_file" ]; then
        echo $start_line
        return
    fi

    local line_num=0
    while IFS= read -r line; do
        ((line_num++))
        if [ $line_num -gt $start_line ] && [ -n "$line" ]; then
            # Parse animation entry: "entity_id | target_x=N | target_y=N"
            IFS='|' read -ra parts <<< "$line"
            local entity_id="${parts[0]// }"

            local target_x="" target_y=""
            for part in "${parts[@]}"; do
                part="${part// }"
                if [[ "$part" =~ ^target_x=([0-9]+)$ ]]; then
                    target_x="${BASH_REMATCH[1]}"
                elif [[ "$part" =~ ^target_y=([0-9]+)$ ]]; then
                    target_y="${BASH_REMATCH[1]}"
                fi
            done

            # Find entity path
            local ent_path=$(find "$HOUSE_ROOT/xyzfs/users/*/home/livedesk/pals" -name "$entity_id" -type d 2>/dev/null | head -1)

            if [ -n "$ent_path" ] && [ -d "$ent_path" ]; then
                log_debug "Animating $entity_id to ($target_x, $target_y)"

                # Call move_entity_with_animation.sh in background
                local anim_script="$HOUSE_ROOT/&.widgits/entity-cli/ops/move_entity_with_animation.sh"
                if [ -f "$anim_script" ]; then
                    bash "$anim_script" "$HOUSE_ROOT" "$ent_path" "$target_x" "$target_y" &
                    log_debug "  -> Animation started"
                else
                    log_debug "  -> Warning: animation script not found at $anim_script"
                fi
            fi

            current_line=$line_num
        fi
    done < "$anim_file"

    echo $current_line
}

# Main loop
main_loop() {
    local tick=0

    log_debug "Page Manager started for: $PAGE_ROOT"
    echo "Page Manager started for: $PAGE_ROOT"

    while true; do
        ((tick++))

        # Read current cursors (returns three space-separated values)
        read -r entities_cursor events_cursor anim_cursor < <(read_cursors)

        # Poll each ledger
        entities_cursor=$(poll_entities $entities_cursor)
        anim_cursor=$(poll_animations $anim_cursor)

        # Write updated cursors
        write_cursors $entities_cursor $events_cursor $anim_cursor

        # Sleep briefly to avoid busy-polling (16ms = ~60fps)
        sleep 0.016
    done
}

# Entry point
init_ledgers
main_loop
