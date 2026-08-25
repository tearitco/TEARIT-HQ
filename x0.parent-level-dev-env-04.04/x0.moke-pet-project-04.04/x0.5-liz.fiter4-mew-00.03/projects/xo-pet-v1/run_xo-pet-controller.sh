#!/bin/bash
# run_xo-pet-controller.sh - terminal-facing XO controller runner.

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONTROL_ROOT="$(cd "$PROJECT_DIR/../.." && pwd)"
MANAGER_BIN="$PROJECT_DIR/pieces/manager/+x/xo-pet-v1_manager.+x"
GUI_STATE="$PROJECT_DIR/pieces/manager/gui_state.txt"
MANAGER_LOG="$PROJECT_DIR/pieces/manager/manager.log"
PROJECT_HISTORY="$PROJECT_DIR/history.txt"
LEGACY_KEYBOARD_HISTORY="$CONTROL_ROOT/pieces/keyboard/history.txt"
ACTIVE_TARGET_FILE="$PROJECT_DIR/pieces/manager/active_target.txt"
SIM_CONTROL_FILE="$PROJECT_DIR/pieces/manager/sim_control.txt"
START_MODE="reset"
LAST_SIM_STATUS="PAUSED"
LAST_ACTIVE_TARGET="xelector"
LAST_EPOCH="0"
LAST_STEP_COUNT="0"
LAST_PET_HP="N/A"
LAST_PET_HUNGER="N/A"
LAST_PROJECT_CONTROLS=""
LAST_PET_LIST=""
LAST_PIECE_METHODS=""
LAST_RESPONSE="System Initialized."

for arg in "$@"; do
    case "$arg" in
        reset|--reset)
            START_MODE="reset"
            ;;
        resume|--resume)
            START_MODE="resume"
            ;;
    esac
done

mkdir -p "$CONTROL_ROOT/pieces/keyboard"
mkdir -p "$PROJECT_DIR"

if [ "$START_MODE" = "reset" ]; then
    : > "$PROJECT_HISTORY"
    : > "$LEGACY_KEYBOARD_HISTORY"
    : > "$GUI_STATE"
    : > "$MANAGER_LOG"
    rm -f "$ACTIVE_TARGET_FILE" "$SIM_CONTROL_FILE"
    echo "Resetting XO-PET controller scratch files"
else
    echo "Resuming XO-PET controller scratch files"
fi

if [ ! -x "$MANAGER_BIN" ]; then
    echo "ERROR: Missing manager binary: $MANAGER_BIN"
    exit 1
fi

pkill -f 'xo-pet-v1_manager\.\+x' 2>/dev/null || true

cd "$PROJECT_DIR"
"$MANAGER_BIN" > "$MANAGER_LOG" 2>&1 &
MGR_PID=$!

cleanup() {
    kill "$MGR_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

safe_clear() {
    printf '\033[H\033[2J'
}

write_keypress() {
    local code="$1"
    case "$code" in
        start|pause|step|resume)
            printf '[%s] COMMAND: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$(printf '%s' "$code" | tr '[:lower:]' '[:upper:]')" >> "$PROJECT_HISTORY"
            printf '[%s] COMMAND: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$(printf '%s' "$code" | tr '[:lower:]' '[:upper:]')" >> "$LEGACY_KEYBOARD_HISTORY"
            return
            ;;
    esac

    local ascii_code
    ascii_code=$(printf '%d' "'$code")
    printf '[%s] KEY_PRESSED: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$ascii_code" >> "$PROJECT_HISTORY"
    printf '[%s] KEY_PRESSED: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$ascii_code" >> "$LEGACY_KEYBOARD_HISTORY"
}

write_possess() {
    local pet_id="$1"
    printf '[%s] COMMAND: SET_POSSESS:%s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$pet_id" >> "$PROJECT_HISTORY"
    printf '[%s] COMMAND: SET_POSSESS:%s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$pet_id" >> "$LEGACY_KEYBOARD_HISTORY"
}

extract_field() {
    local file_path="$1"
    local key="$2"
    sed -n "s/^${key}=//p" "$file_path" | head -n 1
}

remember_field() {
    local key="$1"
    local value="$2"
    [ -n "$value" ] || return 0
        case "$key" in
            SIM_STATUS) LAST_SIM_STATUS="$value" ;;
            ACTIVE_TARGET) LAST_ACTIVE_TARGET="$value" ;;
            EPOCH) LAST_EPOCH="$value" ;;
            STEP_COUNT) LAST_STEP_COUNT="$value" ;;
            PET_HP) LAST_PET_HP="$value" ;;
            PET_HUNGER) LAST_PET_HUNGER="$value" ;;
            PROJECT_CONTROLS) LAST_PROJECT_CONTROLS="$value" ;;
            PET_LIST) LAST_PET_LIST="$value" ;;
            PIECE_METHODS) LAST_PIECE_METHODS="$value" ;;
            RESPONSE) LAST_RESPONSE="$value" ;;
        esac
    return 0
}

render_buttons() {
    local input="$1"
    local mode="$2"
    if [ -z "$input" ]; then
        echo "  (none)"
        return
    fi

    if [ "$mode" = "method" ]; then
        printf '%s\n' "$input" | grep -o 'label="[^"]*" onClick="KEY:[^"]*"' | \
            sed 's/label="\([^"]*\)" onClick="KEY:\([^"]*\)"/\1  [key \2]/' || true
    elif [ "$mode" = "command" ]; then
        printf '%s\n' "$input" | grep -o 'label="[^"]*" onClick="COMMAND:[^"]*"' | \
            sed 's/label="\([^"]*\)" onClick="COMMAND:\([^"]*\)"/\1  [cmd \2]/' || true
    else
        printf '%s\n' "$input" | grep -o 'label="[^"]*" onClick="SET_POSSESS:[^"]*"' | \
            sed 's/label="Possess \([^"]*\)" onClick="SET_POSSESS:[^"]*"/  [possess \1]/' || true
    fi
}

render_screen() {
    if [ ! -f "$GUI_STATE" ]; then
        safe_clear
        echo "XO-PET controller is starting..."
        echo
        echo "Waiting for gui_state.txt"
        return
    fi

    local snapshot
    snapshot="$(mktemp)"
    cp "$GUI_STATE" "$snapshot" 2>/dev/null || true

    local sim_status active_target epoch pet_hp pet_hunger project_controls pet_list piece_methods last_response
    sim_status="$(extract_field "$snapshot" sim_status)"
    active_target="$(extract_field "$snapshot" active_target)"
    epoch="$(extract_field "$snapshot" epoch)"
    step_count="$(extract_field "$snapshot" step_count)"
    pet_hp="$(extract_field "$snapshot" pet_hp)"
    pet_hunger="$(extract_field "$snapshot" pet_hunger)"
    project_controls="$(extract_field "$snapshot" project_controls)"
    pet_list="$(extract_field "$snapshot" pet_list)"
    piece_methods="$(extract_field "$snapshot" piece_methods)"
    last_response="$(extract_field "$snapshot" last_response)"
    rm -f "$snapshot"

    # Some manager writes still collapse pet fields onto one line.
    if printf '%s' "$pet_hp" | grep -q 'pet_hunger='; then
        local merged rest
        merged="$pet_hp"
        pet_hp="${merged%%pet_hunger=*}"
        rest="${merged#*pet_hunger=}"
        if [ "$rest" != "$merged" ]; then
            pet_hunger="${rest%%pet_list=*}"
        fi
        if printf '%s' "$rest" | grep -q 'pet_list='; then
            pet_list="${rest#*pet_list=}"
        fi
    fi

    remember_field SIM_STATUS "$sim_status"
    remember_field ACTIVE_TARGET "$active_target"
    remember_field EPOCH "$epoch"
    remember_field STEP_COUNT "$step_count"
    remember_field PET_HP "$pet_hp"
    remember_field PET_HUNGER "$pet_hunger"
    remember_field PROJECT_CONTROLS "$project_controls"
    remember_field PET_LIST "$pet_list"
    remember_field PIECE_METHODS "$piece_methods"
    remember_field RESPONSE "$last_response"

    safe_clear
    echo "============================================================"
    echo " XO-PET Controller"
    echo "============================================================"
    echo " STATUS: ${LAST_SIM_STATUS:-UNKNOWN}"
    echo " ACTIVE: ${LAST_ACTIVE_TARGET:-none}"
    echo " EPOCH:  ${LAST_EPOCH:-0}"
    echo " STEPS:  ${LAST_STEP_COUNT:-0}"
    if [ -n "$LAST_PET_HP" ] && [ "$LAST_PET_HP" != "N/A" ]; then
        echo " HP:     $LAST_PET_HP"
        echo " HUNGER: $LAST_PET_HUNGER"
    fi
    echo "------------------------------------------------------------"
    echo " PROJECT CONTROLS"
    render_buttons "$LAST_PROJECT_CONTROLS" command
    echo "------------------------------------------------------------"
    echo " SWITCHER / PETS"
    render_buttons "$LAST_PET_LIST" possess
    echo "------------------------------------------------------------"
    echo " METHODS"
    render_buttons "$LAST_PIECE_METHODS" method
    echo "------------------------------------------------------------"
    echo " LOG: ${LAST_RESPONSE:-waiting}"
    echo "------------------------------------------------------------"
    echo " Commands:"
    echo "   2..12         send method key"
    echo "   start         send project start command"
    echo "   pause         send project pause command"
    echo "   step          send project step command"
    echo "   possess NAME  attach to a pet"
    echo "   q             quit controller"
    echo "============================================================"
    printf "> "
}

sleep 1
while true; do
    render_screen
    if ! read -r cmd; then
        break
    fi

    case "$cmd" in
        q|quit|exit)
            break
            ;;
        1|start)
            write_keypress start
            ;;
        2|pause)
            write_keypress pause
            ;;
        3|step)
            write_keypress step
            ;;
        possess\ *)
            write_possess "${cmd#possess }"
            ;;
        [2-9])
            write_keypress "$(printf '%d' "'$cmd")"
            ;;
        "")
            ;;
        *)
            printf '[%s] Unknown command: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$cmd" >> "$MANAGER_LOG"
            ;;
    esac

done
