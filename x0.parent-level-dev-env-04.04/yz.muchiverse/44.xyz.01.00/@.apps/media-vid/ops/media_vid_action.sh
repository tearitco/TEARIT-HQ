#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
UI="$HERE/state/ui.txt"
[ -f "$UI" ] || exit 1
g() { grep "^$1=" "$UI" | cut -d= -f2-; }
playing=$(g playing); lcd=$(g lcd)
ins_name=$(g ins_name); ins_lane=$(g ins_lane)
cmd="${1:-}"; a="${2:-}"; b="${3:-}"
case "$cmd" in
  transport)
    case "$a" in
      play) [ "$playing" = 1 ] && playing=0 || playing=1 ;;
      stop) playing=0 ;;
      zero) playing=0; lcd=00:00:00.00 ;;
    esac
    ;;
  clip)
    ins_lane=$(echo "$a" | tr 'a-z' 'A-Z')
    ins_name="${a}_${b}"
    ;;
  file|edit) ;;
esac
status="play=$playing lcd=$lcd sel=$ins_name"
{
  echo "playing=$playing"
  echo "lcd=$lcd"
  echo "status=$status"
  echo "ins_name=$ins_name"
  echo "ins_in=00:00:00.00"
  echo "ins_out=00:00:03.00"
  echo "ins_dur=3.00s"
  echo "ins_lane=$ins_lane"
  echo "n_v1=2"
  echo "v1_0_text=[ ] orange  0.0–3.0"
  echo "v1_1_text=[ ] green   3.0–6.0"
  echo "n_v2=1"
  echo "v2_0_text=[ ] blue    1.0–4.0"
  echo "n_a1=1"
  echo "a1_0_text=[ ] tone    0.0–6.0"
  echo "n_a2=0"
  echo "a2_empty=1"
  echo "preview=preview (poster frame — decode on scrub only)"
} > "$UI.tmp"
mv "$UI.tmp" "$UI"
