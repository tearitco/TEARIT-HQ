#!/bin/sh
# Taskbar launcher shim (the network cell runs `bash <this> <house_root>`): ALL logic is in the C op, none here.
exec "$(dirname "$0")/ops/+x/tear_on_op.+x" --open "$1"
