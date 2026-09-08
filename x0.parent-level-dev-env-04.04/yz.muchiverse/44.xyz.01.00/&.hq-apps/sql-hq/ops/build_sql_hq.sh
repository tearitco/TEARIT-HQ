#!/bin/sh
# build_sql_hq.sh - build sql-hq's engine (bundles the vendored sqlite3
# amalgamation). SQL-HQ-DESIGN.md.
#
# Produces:
#   +x/sql_hq.+x        the query engine + REPL (one binary, no deps)
#   +x/sqlite3.o        cached amalgamation object (recompiled only if
#                       vendor-sqlite/sqlite3.c is newer)
#   +x/sh_adapt.+x      the adapters self-test (gcc -DSH_ADAPTERS_TEST)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
mkdir -p +x

CC="${CC:-gcc}"
# amalgamation build knobs: no loadable extensions, no shell, threadsafe
# off (single process), FTS/RTREE off - small + fast to compile.
SQLITE_DEFS="-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION \
 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_ENABLE_MATH_FUNCTIONS \
 -DSQLITE_DQS=0"

if [ vendor-sqlite/sqlite3.c -nt +x/sqlite3.o ] || [ ! -f +x/sqlite3.o ]; then
    echo "-- compiling vendored sqlite3.c (once, ~15s) --"
    $CC -O2 -w $SQLITE_DEFS -c vendor-sqlite/sqlite3.c -o +x/sqlite3.o
fi

echo "-- sql_hq engine --"
$CC -std=c11 -Wall -Wextra -O2 -I vendor-sqlite \
    sql_hq_engine.c sql_hq_adapters.c +x/sqlite3.o \
    -o +x/sql_hq.+x -lm
echo "OK +x/sql_hq.+x"

echo "-- adapters self-test --"
$CC -std=c11 -Wall -Wextra -O2 -DSH_ADAPTERS_TEST \
    sql_hq_adapters.c -o +x/sh_adapt.+x
echo "OK +x/sh_adapt.+x"
