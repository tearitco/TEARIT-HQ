#!/bin/sh
cd "$(dirname "$0")" && mkdir -p +x && echo "-- cpu_gate -> +x/cpu_gate.+x" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/cpu_gate.+x cpu_gate.c && echo "OK +x/cpu_gate.+x"
# the scoped-commit guard rail (AGENTS.md commit discipline) is built with the gate: see ops/commit_scoped.c and harness commit_scoped.pal
cd "$(dirname "$0")" && echo "-- commit_scoped -> +x/commit_scoped.+x" && ${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/commit_scoped.+x commit_scoped.c && echo "OK +x/commit_scoped.+x"
