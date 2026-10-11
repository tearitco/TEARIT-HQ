#!/bin/sh
# pet_voice.sh <species> <text...>   - say a pet's words aloud: edge-tts (online, free) in the species voice from voices.pdl, cached as wav per species+text, played quietly.
# Quiet failure: no network / no player = no sound, never an error. One sound at a time (a lock); heavy steps run niced. State dir: $PET_SHARED (default ../state).
HERE="$(cd "$(dirname "$0")/.." && pwd)"; SHARED="${PET_SHARED:-$HERE/state}"; SP="${1:-0}"; shift; TEXT="$*"; [ -z "$TEXT" ] && exit 0
D="$SHARED/audio"; mkdir -p "$D"
row=$(awk -F'|' -v s="$SP" '/^VOICE/{a=$2; gsub(/ /,"",a); if (a==s) {v=$3; r=$4; p=$5; gsub(/ /,"",v); gsub(/ /,"",r); gsub(/ /,"",p); print v " " r " " p; exit}}' "$HERE/voices.pdl")
set -- $row; VOICE="${1:-en-US-AnaNeural}"; RATE="${2:-+0%}"; PITCH="${3:-+0Hz}"
key=$(printf '%s|%s' "$SP" "$TEXT" | md5sum | cut -c1-12); WAV="$D/say_${SP}_$key.wav"
if [ ! -s "$WAV" ]; then
    MP3="$D/say_${SP}_$key.mp3"
    timeout 20 nice -n 15 /usr/bin/python3 -m edge_tts --voice "$VOICE" --rate="$RATE" --pitch="$PITCH" --text "$TEXT" --write-media "$MP3" >/dev/null 2>&1 || exit 0
    nice -n 15 ffmpeg -loglevel quiet -y -i "$MP3" -ar 22050 -ac 1 "$WAV" >/dev/null 2>&1; rm -f "$MP3"
fi
[ -s "$WAV" ] || exit 0
exec 9>"$D/play.lock"; flock -n 9 || exit 0          # something is already sounding: skip
nice -n 10 paplay --volume=32000 "$WAV" >/dev/null 2>&1
