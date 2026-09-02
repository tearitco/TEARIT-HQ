#!/bin/bash

# Find and kill processes running from +x/
pids=$(ps aux | grep '+x/' | grep -v grep | awk '{print $2}')

if [ -z "$pids" ]; then
    echo "No processes found running from +x/"
    exit 0
fi

for pid in $pids; do
    echo "Killing process $pid"
    kill -9 $pid
done

echo "All +x/ processes terminated"
