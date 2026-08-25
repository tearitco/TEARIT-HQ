#!/bin/bash

# Get the directory of the script
SCRIPT_DIR=$(dirname "$0")

# Find processes running from +x/ directory and kill them
find "$SCRIPT_DIR/+x" -type f -executable | while read -r file; do
    # Get PIDs of processes running this executable
    pids=$(lsof -t "$file" 2>/dev/null)
    if [ ! -z "$pids" ]; then
        for pid in $pids; do
            echo "Killing process $pid running $file"
            kill -9 "$pid"
        done
    fi
done
