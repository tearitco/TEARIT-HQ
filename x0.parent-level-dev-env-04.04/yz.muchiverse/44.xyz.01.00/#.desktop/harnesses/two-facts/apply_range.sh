#!/bin/sh
# If target cell lies in the NxN square whose top-left is origin,
# subtract 1 from that actor's page_value. numeric_value and mhp
# are not touched. Out of range writes nothing and exits 0.
# Cells are a letter column and a 1-based row: a1, b2, c7.
# Usage: apply_range.sh <origin> <target> <range> <actor_id> <state> <mirror>
set -u
origin="${1:-}"
target="${2:-}"
range="${3:-}"
id="${4:-}"
state="${5:-}"
mirror="${6:-}"
[ -n "$origin" ] && [ -n "$target" ] && [ -n "$range" ] && [ -n "$id" ] && [ -f "$state" ] || {
  echo "Usage: apply_range.sh <origin> <target> <range> <actor_id> <state> <mirror>" >&2
  exit 1
}
case "$range" in *[!0-9]*|"") echo "apply_range: range not decimal" >&2; exit 1 ;; esac
cell() {
  python3 - "$1" << 'PY'
import sys
s=sys.argv[1].strip().lower()
letters=""; digits=""
for ch in s:
    if ch.isalpha(): letters += ch
    elif ch.isdigit(): digits += ch
if not letters or not digits:
    sys.exit(2)
col=0
for ch in letters:
    col = col*26 + (ord(ch)-ord("a")+1)
col -= 1
row=int(digits)-1
if col<0 or row<0:
    sys.exit(2)
print(col, row)
PY
}
oc=$(cell "$origin") || { echo "apply_range: bad origin" >&2; exit 1; }
tc=$(cell "$target") || { echo "apply_range: bad target" >&2; exit 1; }
set -- $oc; oc_c=$1; oc_r=$2
set -- $tc; tc_c=$1; tc_r=$2
n=$range
if [ "$tc_c" -lt "$oc_c" ] || [ "$tc_r" -lt "$oc_r" ] || \
   [ "$tc_c" -ge $((oc_c + n)) ] || [ "$tc_r" -ge $((oc_r + n)) ]; then
  echo "OUT id=$id"
  exit 0
fi
python3 - "$state" "$mirror" "$id" << 'PY'
import sys
from pathlib import Path
state, mirror, aid = sys.argv[1], sys.argv[2], sys.argv[3]
def bump(path):
    if not path or not Path(path).is_file():
        return None
    lines=Path(path).read_text().splitlines()
    out=[]; on=False; seen=False; val=None
    for line in lines:
        parts=[p.strip() for p in line.split("|")]
        if len(parts)>=3 and parts[0]=="ACTOR" and parts[1]=="id":
            on = parts[2]==aid
        elif on and len(parts)>=3 and parts[0]=="ACTOR" and parts[1]=="page_value":
            val=int(parts[2])
            if val>0: val-=1
            line="%-12s | %-18s | %s" % ("ACTOR","page_value",val)
            seen=True
        out.append(line)
    if not seen:
        return None
    Path(path).write_text("\n".join(out)+"\n")
    return val
v=bump(state)
if v is None:
    sys.exit(1)
bump(mirror)
print(v)
PY
echo "HIT id=$id"
