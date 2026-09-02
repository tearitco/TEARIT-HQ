#!/bin/sh
# 01_move — start match, select soldier @2,0, move to 2,1, assert ledger
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$DIR"
BIN=./ops/+x/ttg_loop
mkdir -p pieces/apps/player_app pieces/display pieces/system data
: > pieces/apps/player_app/history.txt
: > pieces/system/quit_flag.txt

# With --start-match cursor is 6,2. Soldier is at 2,0.
# LEFT x4 (6→2), UP x2 (2→0), Enter select, DOWN, Enter move.
i=0
while [ "$i" -lt 4 ]; do printf '1000\n' >> pieces/apps/player_app/history.txt; i=$((i+1)); done
i=0
while [ "$i" -lt 2 ]; do printf '1002\n' >> pieces/apps/player_app/history.txt; i=$((i+1)); done
printf '13\n' >> pieces/apps/player_app/history.txt
printf '1003\n' >> pieces/apps/player_app/history.txt
printf '13\n' >> pieces/apps/player_app/history.txt

"$BIN" --root "$DIR" --start-match --ticks 80

grep -q '|move' data/master_ledger.txt && echo "PASS 01_move ledger has move" || {
  echo "FAIL 01_move no move in ledger"
  tail -25 data/master_ledger.txt
  echo "--- msg frame ---"
  grep Msg pieces/display/current_frame.txt || true
  exit 1
}
grep -q 'TTG' pieces/display/current_frame.txt && echo "PASS 01_move frame has TTG" || exit 1
test -f pieces/display/rgb_frame.raw && echo "PASS 01_move rgb raw exists" || exit 1
echo "OK harness 01_move"
