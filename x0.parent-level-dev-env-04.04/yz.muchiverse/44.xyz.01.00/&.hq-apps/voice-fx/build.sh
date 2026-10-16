#!/bin/bash
# build.sh - voice-fx manager + request op + say op (pure C, no X11).
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x" "$SDIR/ops/+x"
CC=${CC:-gcc}
$CC -std=gnu11 -Wall -Wno-format-truncation -Wno-misleading-indentation -O2 -o "$SDIR/+x/voice_fx_manager.+x" "$SDIR/voice_fx_manager.c" -lm && echo "OK voice_fx_manager" || exit 1
$CC -std=gnu11 -Wall -Wno-format-truncation -Wno-misleading-indentation -O2 -o "$SDIR/ops/+x/voice_fx_req.+x" "$SDIR/ops/voice_fx_req.c" && echo "OK voice_fx_req" || exit 1
$CC -std=gnu11 -Wall -Wno-format-truncation -Wno-misleading-indentation -O2 -o "$SDIR/ops/+x/voice_say.+x" "$SDIR/ops/voice_say.c" && echo "OK voice_say" || exit 1
