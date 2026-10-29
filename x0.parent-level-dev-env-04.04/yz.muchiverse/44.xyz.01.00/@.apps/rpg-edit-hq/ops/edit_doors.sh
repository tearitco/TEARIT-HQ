#!/bin/sh
# Islands door FSM. No window. No live trainer.
# list <table>
# must <table> <from> <x> <y> <arrow> <to> <tx> <ty>
#   exit 0 when that exact rule is in the table, else exit 2.
# scene <raw> <receipt>
#   exit 0 when the frame has more than 2 sampled colors, else exit 2.
#   Key p's almost-black rgb_frame.raw fails this. scene.raw should pass.
# scene-self good|black
#   the same check on a built-in 2x2 frame, so a harness needs no binary file.
set -u
need() { echo "edit_doors: $1" >&2; exit 2; }
cmd=${1:-}
case "$cmd" in
list|must|scene|scene-self) ;;
*) need "usage: list|must|scene|scene-self" ;;
esac
shift
if [ "$cmd" = scene-self ]; then
    kind=${1:-}
    dir=$(mktemp -d /tmp/edit-doors-XXXX)
    python3 - "$dir" "$kind" <<'PY'
import sys
from pathlib import Path
d, kind = Path(sys.argv[1]), sys.argv[2]
if kind == "good":
    raw = bytes([1,2,3,255, 4,5,6,255, 7,8,9,255, 10,11,12,255])
elif kind == "black":
    raw = bytes([0,0,0,255, 0,0,0,255, 0,0,0,255, 255,255,255,255])
else:
    sys.exit("edit_doors: scene-self needs good or black")
(d/"f.raw").write_bytes(raw)
(d/"f.receipt.txt").write_text("frame_w=2\nframe_h=2\n")
PY
    sh "$0" scene "$dir/f.raw" "$dir/f.receipt.txt"
    rc=$?
    rm -rf "$dir"
    exit $rc
fi
if [ "$cmd" = scene ]; then
    raw=${1:-}; rec=${2:-}
    [ -f "$raw" ] && [ -f "$rec" ] || need "need a raw frame and a receipt"
    python3 - "$raw" "$rec" <<'PY'
import sys
from collections import Counter
from pathlib import Path
raw, rec = Path(sys.argv[1]), Path(sys.argv[2])
w = h = 0
for line in rec.read_text().splitlines():
    if line.startswith("frame_w="): w = int(line.split("=", 1)[1])
    if line.startswith("frame_h="): h = int(line.split("=", 1)[1])
data = raw.read_bytes()
need = w * h * 4
if w < 1 or h < 1 or len(data) < need:
    sys.exit("edit_doors: bad frame")
cols = Counter()
for i in range(0, need - 3, 4):
    cols[(data[i], data[i + 1], data[i + 2])] += 1
print("colors=%d w=%d h=%d" % (len(cols), w, h))
sys.exit(0 if len(cols) > 2 else 2)
PY
    exit $?
fi
table=${1:-}; [ -f "$table" ] || need "need the fsm table"
if [ "$cmd" = list ]; then
    awk -F'|' '$1 ~ /^RULE[[:space:]]*$/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); gsub(/^[ \t]+|[ \t]+$/, "", $3); gsub(/^[ \t]+|[ \t]+$/, "", $4); gsub(/^[ \t]+|[ \t]+$/, "", $5); gsub(/^[ \t]+|[ \t]+$/, "", $6); gsub(/^[ \t]+|[ \t]+$/, "", $7); gsub(/^[ \t]+|[ \t]+$/, "", $8); printf "rule %s %s %s arrow %s -> %s %s %s\n", $2, $3, $4, $5, $6, $7, $8 }' "$table"
    exit 0
fi
from=${2:-}; x=${3:-}; y=${4:-}; arrow=${5:-}; to=${6:-}; tx=${7:-}; ty=${8:-}
[ -n "$from" ] && [ -n "$to" ] || need "must needs from x y arrow to tx ty"
awk -F'|' -v fr="$from" -v x="$x" -v y="$y" -v ar="$arrow" -v to="$to" -v tx="$tx" -v ty="$ty" '
    function t(s) { gsub(/^[ \t]+|[ \t]+$/, "", s); return s }
    $1 ~ /^RULE[[:space:]]*$/ {
        if (t($2)==fr && t($3)==x && t($4)==y && t($5)==ar && t($6)==to && t($7)==tx && t($8)==ty) found=1
    }
    END { exit !found }
' "$table" || need "no such rule"
echo "rule $from $x $y arrow $arrow -> $to $tx $ty"
