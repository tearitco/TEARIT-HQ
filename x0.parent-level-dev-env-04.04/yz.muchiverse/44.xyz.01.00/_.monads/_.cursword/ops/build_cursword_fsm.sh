#!/bin/sh
# build_cursword_fsm.sh — compile cursword's onboarding FSM.
# Plain C11, no deps (matches the login-signup app's own simple build).
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
# -Wno-format-truncation: the path buffers are 1024, far larger than any
# real house_root+suffix; the warnings are the compiler's worst-case math.
$CC -std=c11 -Wall -Wextra -Wno-format-truncation -O2 -o +x/cursword_fsm.+x cursword_fsm.c
echo "OK +x/cursword_fsm.+x"
