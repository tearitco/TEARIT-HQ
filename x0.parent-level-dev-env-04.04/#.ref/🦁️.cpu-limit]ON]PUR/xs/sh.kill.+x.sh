#!/bin/bash

# Find processes running from +x/ directory and kill them
for pid in $(lsof +D +x/ 2>/dev/null | awk '{print $2}' | tail -n +2 | sort -u); do
    if [ ! -z "$pid" ]; then
        echo "Killing process $pid"
        kill -9 $pid
    fi
done

echo "All processes in +x/ directory have been terminated."
