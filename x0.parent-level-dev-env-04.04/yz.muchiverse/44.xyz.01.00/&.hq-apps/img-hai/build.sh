#!/bin/bash
# build.sh - img-hai manager + request op (pure C, no X11).
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x" "$SDIR/ops/+x"
CC=${CC:-gcc}
$CC -std=gnu11 -Wall -O2 -o "$SDIR/+x/img_hai_manager.+x" "$SDIR/img_hai_manager.c" -lm && echo "OK img_hai_manager" || exit 1
$CC -std=gnu11 -Wall -O2 -o "$SDIR/ops/+x/img_hai_req.+x" "$SDIR/ops/img_hai_req.c" && echo "OK img_hai_req" || exit 1
$CC -std=gnu11 -Wall -O2 -o "$SDIR/ops/+x/img_hai_job.+x" "$SDIR/ops/img_hai_job.c" && echo "OK img_hai_job" || exit 1
$CC -std=gnu11 -Wall -O2 -o "$SDIR/ops/+x/img_sheet.+x" "$SDIR/ops/img_sheet.c" -lm && echo "OK img_sheet" || exit 1
