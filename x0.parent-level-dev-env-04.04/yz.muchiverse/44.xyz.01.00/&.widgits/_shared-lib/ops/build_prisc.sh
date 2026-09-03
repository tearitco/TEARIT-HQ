#!/bin/sh
# build_prisc.sh - compile the shared PAL interpreter (with string ops)
# to _shared-lib/system/+x/prisc+x.+x . This is the copy khtpm-world
# <module src="…/prisc+x.+x foo.pal"/> launches; project-local
# system/prisc+x.c copies are separate and untouched.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SYS="$HERE/../system"
mkdir -p "$SYS/+x"
CC="${CC:-gcc}"
$CC -std=c11 -Wall -Wextra -Wno-format-truncation -O2 \
    -o "$SYS/+x/prisc+x.+x" "$SYS/prisc+x.c"
echo "OK $SYS/+x/prisc+x.+x"
