#!/bin/sh
# build_pet_observer_manager.sh - compile the observer manager into ops/+x/ (run niced via cpu_gate).
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wno-format-truncation -o +x/pet_observer_manager.+x pet_observer_manager.c && echo "OK +x/pet_observer_manager.+x"
