#!/bin/bash
# build.sh - sfx-hai manager + request op + say op (pure C, no X11).
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x" "$SDIR/ops/+x"
CC=${CC:-gcc}
$CC -std=gnu11 -Wall -Wno-format-truncation -Wno-misleading-indentation -O2 -o "$SDIR/+x/sfx_hai_manager.+x" "$SDIR/sfx_hai_manager.c" && echo "OK sfx_hai_manager" || exit 1
$CC -std=gnu11 -Wall -Wno-format-truncation -Wno-misleading-indentation -O2 -o "$SDIR/ops/+x/sfx_hai_req.+x" "$SDIR/ops/sfx_hai_req.c" && echo "OK sfx_hai_req" || exit 1
$CC -std=gnu11 -Wall -Wno-format-truncation -Wno-misleading-indentation -O2 -o "$SDIR/ops/+x/sfx_make.+x" "$SDIR/ops/sfx_make.c" -lm && echo "OK sfx_make" || exit 1
