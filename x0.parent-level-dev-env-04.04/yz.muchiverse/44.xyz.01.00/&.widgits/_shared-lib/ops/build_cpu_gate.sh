#!/bin/sh
cd "$(dirname "$0")" && mkdir -p +x && echo "-- cpu_gate -> +x/cpu_gate.+x" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/cpu_gate.+x cpu_gate.c && echo "OK +x/cpu_gate.+x"
