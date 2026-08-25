#!/bin/bash
# run_chtpm.sh - Run the CHTPM v0.01 system (NUCLEAR CLEANUP v3.5 - JOYSTICK FIX)
# Responsibility: Clean environment, set location, launch orchestrator.

# 0. NUCLEAR CLEANUP
echo "Cleaning environment..."
bash pieces/os/kill_all.sh

# JOYSTICK FIX (April 2, 2026): Clear stale GL-OS input focus lock
# Without this, joystick input is ignored because gl_os_has_focus() always returns TRUE
rm -f pieces/apps/gl_os/session/input_focus.lock
echo "Cleared stale GL-OS input_focus.lock (joystick fix)"

# Wraith Alpha focus ownership
rm -f projects/wraith-alpha/session/input_focus.lock
echo "Cleared stale Wraith Alpha input_focus.lock"

# DYNAMIC PATH RESOLUTION
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 1. Set up location_kvp (SMF-Compliant)
mkdir -p pieces/locations
cat > pieces/locations/location_kvp << EOF
project_root=${SCRIPT_DIR}
pieces_dir=${SCRIPT_DIR}/pieces
fuzzpet_app_dir=${SCRIPT_DIR}/pieces/apps/fuzzpet_app
fuzzpet_dir=${SCRIPT_DIR}/pieces/apps/fuzzpet_app/fuzzpet
editor_dir=${SCRIPT_DIR}/pieces/apps/editor
system_dir=${SCRIPT_DIR}/pieces/system
pet_01_dir=${SCRIPT_DIR}/pieces/world/map_01/pet_01
pet_02_dir=${SCRIPT_DIR}/pieces/world/map_01/pet_02
selector_dir=${SCRIPT_DIR}/pieces/world/map_01/selector
os_procs_dir=${SCRIPT_DIR}/pieces/os/procs
clock_daemon_dir=${SCRIPT_DIR}/pieces/system/clock_daemon
manager_dir=${SCRIPT_DIR}/pieces/apps/fuzzpet_app/manager
data_xl=${SCRIPT_DIR}/../^.TPMOS_DATA_XL_67.00
EOF

# 2. Reset all entities to known good state
echo "Resetting entities..."

# Project-based piece structure for op-ed
mkdir -p projects/op-ed/pieces/selector
cat > projects/op-ed/pieces/selector/state.txt << EOF
name=Selector
type=selector
pos_x=5
pos_y=5
pos_z=0
on_map=1
EOF

# 3. Reset Manager State
mkdir -p pieces/apps/fuzzpet_app/manager
echo "active_target_id=selector" > pieces/apps/fuzzpet_app/manager/state.txt
echo "last_key=None" >> pieces/apps/fuzzpet_app/manager/state.txt

# op-ed project state
mkdir -p projects/op-ed/manager
echo "project_id=op-ed" > projects/op-ed/manager/state.txt
echo "active_target_id=selector" >> projects/op-ed/manager/state.txt
echo "current_z=0" >> projects/op-ed/manager/state.txt
echo "last_key=None" >> projects/op-ed/manager/state.txt
> projects/op-ed/history.txt

# fuzz-op project state
mkdir -p projects/fuzz-op/manager
echo "project_id=fuzz-op" > projects/fuzz-op/manager/state.txt
echo "active_target_id=selector" >> projects/fuzz-op/manager/state.txt
echo "current_z=0" >> projects/fuzz-op/manager/state.txt
echo "last_key=None" >> projects/fuzz-op/manager/state.txt
> projects/fuzz-op/history.txt

# mp3-store project state
mkdir -p projects/mp3-store/manager
mkdir -p projects/mp3-store/pieces/mp3_player
echo "project_id=mp3-store" > projects/mp3-store/manager/state.txt
echo "active_target_id=mp3_player" >> projects/mp3-store/manager/state.txt
echo "last_key=None" >> projects/mp3-store/manager/state.txt
> projects/mp3-store/history.txt

# 4. Reset Clock Daemon
mkdir -p pieces/system/clock_daemon
echo "turn=0" > pieces/system/clock_daemon/state.txt
echo "time=08:00:00" >> pieces/system/clock_daemon/state.txt
echo "mode=turn" >> pieces/system/clock_daemon/state.txt
echo "tick_rate=1" >> pieces/system/clock_daemon/state.txt

# 5. Reset FuzzPet State (emoji mode, etc) - ISOLATED FROM EDITOR
mkdir -p pieces/apps/fuzzpet_app/fuzzpet
echo "name=Fuzzball" > pieces/apps/fuzzpet_app/fuzzpet/state.txt
echo "hunger=50" >> pieces/apps/fuzzpet_app/fuzzpet/state.txt
echo "happiness=55" >> pieces/apps/fuzzpet_app/fuzzpet/state.txt
echo "energy=100" >> pieces/apps/fuzzpet_app/fuzzpet/state.txt
echo "level=1" >> pieces/apps/fuzzpet_app/fuzzpet/state.txt
echo "pos_x=5" >> pieces/apps/fuzzpet_app/fuzzpet/state.txt
echo "pos_y=2" >> pieces/apps/fuzzpet_app/fuzzpet/state.txt
echo "pos_z=0" >> pieces/apps/fuzzpet_app/fuzzpet/state.txt
echo "status=active" >> pieces/apps/fuzzpet_app/fuzzpet/state.txt
echo "emoji_mode=0" >> pieces/apps/fuzzpet_app/fuzzpet/state.txt

mkdir -p pieces/world/map_01/selector
cat > pieces/world/map_01/selector/state.txt << EOF
name=Selector
type=selector
pos_x=5
pos_y=5
pos_z=0
on_map=1
emoji=🎯
EOF

# 6. Clear Sessions and Historical Data - ISOLATED PER-APP
> pieces/keyboard/history.txt
> pieces/keyboard/ledger.txt
> pieces/apps/player_app/history.txt  # Clear injected keys
> pieces/apps/player_app/state.txt    # Clear stale state

# Editor history (isolated from fuzzpet)
> pieces/apps/editor/history.txt
> pieces/apps/editor/view.txt
> pieces/apps/editor/view_changed.txt
> pieces/apps/editor/response.txt

# FuzzPet history (legacy - do not modify)
> pieces/apps/fuzzpet_app/fuzzpet/history.txt
> pieces/apps/fuzzpet_app/fuzzpet/ledger.txt
> pieces/apps/fuzzpet_app/fuzzpet/view_changed.txt
> pieces/apps/fuzzpet_app/fuzzpet/last_response.txt

# Global display
> pieces/display/frame_changed.txt
> pieces/display/current_frame.txt
> pieces/display/ledger.txt

# System ledger
> pieces/chtpm/ledger.txt
> pieces/joystick/ledger.txt
> pieces/master_ledger/master_ledger.txt
> pieces/master_ledger/ledger.txt
> pieces/master_ledger/frame_changed.txt

# Debug
> pieces/apps/fuzzpet_app/manager/debug_log.txt
> pieces/os/proc_list.txt
> debug.txt  # Module debug log (cleared each run)

# World maps (fuzzpet-specific)
> pieces/apps/fuzzpet_app/world/map.txt
> pieces/apps/fuzzpet_app/world/ledger.txt
> pieces/apps/fuzzpet_app/clock/ledger.txt
rm -f pieces/apps/fuzzpet_app/world/map_z*.txt 2>/dev/null
rm -rf pieces/debug/frames/* 2>/dev/null
mkdir -p pieces/debug/frames

# 7. Reset World Map (copy from static map) - FUZZPET WORLD ONLY
cat > pieces/apps/fuzzpet_app/world/map.txt << 'EOF'
####################
#  ...............T#
#  ...............T#
#  ....R..........T#
#  ....R..........T#
#  ....R..........T#
#  ....R..........T#
#  ................#
#                  #
####################
EOF

# 8. Initialize Project Maps (fuzz-op)
mkdir -p projects/fuzz-op/pieces/world_01/map_01
cat > projects/fuzz-op/pieces/world_01/map_01/map_01_z0.txt << 'EOF'
####################
#  ...............T#
#  ...............T#
#  ....R...@......T#
#  ....R.Z........T#
#  ....R......@...T#
#  ....R..........T#
#  ................#
#                  #
####################
EOF

# Initialize zombie and pet states for fuzz-op
mkdir -p projects/fuzz-op/pieces/world_01/map_01/zombie_01
cat > projects/fuzz-op/pieces/world_01/map_01/zombie_01/state.txt << EOF
name=Zombie
type=zombie
pos_x=9
pos_y=4
pos_z=0
on_map=1
behavior=aggressive
speed=1
map_id=map_01_z0.txt
EOF

mkdir -p projects/fuzz-op/pieces/world_01/map_01/pet_01
cat > projects/fuzz-op/pieces/world_01/map_01/pet_01/state.txt << EOF
name=Fuzzball
type=pet
pos_x=11
pos_y=1
pos_z=0
on_map=1
hunger=0
happiness=95
energy=91
level=1
owner=world
emoji=🐶
map_id=map_01_z0.txt
EOF

mkdir -p projects/fuzz-op/pieces/world_01/map_01/pet_02
cat > projects/fuzz-op/pieces/world_01/map_01/pet_02/state.txt << EOF
name=StorageCat
type=pet
pos_x=14
pos_y=5
pos_z=0
on_map=1
hunger=50
happiness=55
energy=68
level=1
owner=world
emoji=🐱
map_id=map_01_z0.txt
EOF

mkdir -p projects/fuzz-op/pieces/world_01/map_01/xlector
cat > projects/fuzz-op/pieces/world_01/map_01/xlector/state.txt << EOF
name=Selector
type=xlector
pos_x=12
pos_y=2
pos_z=0
on_map=1
map_id=map_01_z0.txt
EOF

echo "Launching TPM Pipeline (Nuclear Cleanup Complete)..."
echo "  - Editor: Isolated (pieces/apps/editor/)"
echo "  - FuzzPet: Isolated (pieces/apps/fuzzpet_app/)"
echo "  - fuzz-op: Project-based (projects/fuzz-op/)"
echo "  - op-ed: Project-based (projects/op-ed/)"

# Launch PAL Watchdog
if [ -x "./pal_watchdog.sh" ]; then
    chmod +x ./pal_watchdog.sh
    ./pal_watchdog.sh &
fi

exec ./pieces/chtpm/plugins/+x/orchestrator.+x
