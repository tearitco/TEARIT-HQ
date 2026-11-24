#!/bin/sh
# Taskbar launcher shim (the network cell runs `bash <this> <house_root>`): ALL logic is in the C op, none here.
exec "$(dirname "$0")/ops/+x/tear_offline_manager.+x" --open "$1"
