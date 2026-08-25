#!/bin/bash
POSIX_PATH="$(pwd)"
cat > pieces/locations/location_kvp << EOF
project_root=${POSIX_PATH}
pieces_dir=${POSIX_PATH}/pieces
editor_dir=${POSIX_PATH}/pieces/apps/editor
system_dir=${POSIX_PATH}/pieces/system
selector_dir=${POSIX_PATH}/pieces/world/map_01/selector
os_procs_dir=${POSIX_PATH}/pieces/os/procs
clock_daemon_dir=${POSIX_PATH}/pieces/system/clock_daemon
data_xl=${POSIX_PATH}/../^.CYOA_DATA_XL_69
EOF
