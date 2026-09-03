#!/bin/sh
# Stub actions — rewrite state/ui.txt only. No glut, no xhtpm edits.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
UI="$HERE/state/ui.txt"
[ -f "$UI" ] || exit 1
g() { grep "^$1=" "$UI" | cut -d= -f2-; }
playing=$(g playing); recording=$(g recording); cycle=$(g cycle)
mixer_open=$(g mixer_open); lcd=$(g lcd); bpm=$(g bpm)
ch_mute=$(g ch_mute); ch_solo=$(g ch_solo); ch_rec=$(g ch_rec)
ch_vol=$(g ch_vol); ch_pan=$(g ch_pan); ch_eq=$(g ch_eq)
ch_reverb=$(g ch_reverb); ch_dist=$(g ch_dist)
cmd="${1:-}"; a="${2:-}"; b="${3:-}"
tog() { [ "$1" = on ] && echo off || echo on; }
case "$cmd" in
  transport)
    case "$a" in
      play) [ "$playing" = 1 ] && playing=0 || playing=1; recording=0 ;;
      stop) playing=0; recording=0; lcd=1.1.000 ;;
      start) lcd=1.1.000; playing=0 ;;
      rec) recording=1; playing=1 ;;
      cycle) cycle=$( [ "$cycle" = 1 ] && echo 0 || echo 1) ;;
    esac
    ;;
  mixer)
    [ "$mixer_open" = 1 ] && mixer_open=0 || mixer_open=1
    ;;
  ch)
    case "$a" in
      mute) ch_mute=$(tog "$ch_mute") ;;
      solo) ch_solo=$(tog "$ch_solo") ;;
      rec) ch_rec=$(tog "$ch_rec") ;;
      eq) ch_eq=$(tog "$ch_eq") ;;
      reverb) ch_reverb=$(tog "$ch_reverb") ;;
      dist) ch_dist=$(tog "$ch_dist") ;;
    esac
    ;;
  file|track) ;;
esac
is_mixer=0
[ "$mixer_open" = 1 ] && is_mixer=1
status="play=$playing rec=$recording cycle=$cycle mixer=$mixer_open"
{
  echo "playing=$playing"
  echo "recording=$recording"
  echo "cycle=$cycle"
  echo "mixer_open=$mixer_open"
  echo "is_mixer=$is_mixer"
  echo "lcd=$lcd"
  echo "bpm=$bpm"
  echo "status=$status"
  echo "n_tracks=2"
  echo "track_0_text=[ ] 1. Keys  M S R"
  echo "track_1_text=[ ] 2. Bass  M S R"
  echo "active_track=1 Keys"
  echo "ch_mute=$ch_mute"
  echo "ch_solo=$ch_solo"
  echo "ch_rec=$ch_rec"
  echo "ch_vol=$ch_vol"
  echo "ch_pan=$ch_pan"
  echo "ch_eq=$ch_eq"
  echo "ch_reverb=$ch_reverb"
  echo "ch_dist=$ch_dist"
  echo "n_notes=3"
  echo "note_0_text=C4  1.1"
  echo "note_1_text=E4  1.2"
  echo "note_2_text=G4  1.3"
  echo "n_mix=2"
  echo "mix_0_text=1. Keys  --80--  C"
  echo "mix_1_text=2. Bass  --70--  L"
} > "$UI.tmp"
mv "$UI.tmp" "$UI"
