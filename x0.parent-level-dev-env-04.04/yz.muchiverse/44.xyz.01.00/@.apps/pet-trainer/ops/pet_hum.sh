#!/bin/sh
# pet_hum.sh <species>   - the pet's hum: a short melody from voices.pdl (HUM row), built once with sox, cached, played quietly. One sound at a time (shared lock with pet_voice.sh).
HERE="$(cd "$(dirname "$0")/.." && pwd)"; SHARED="${PET_SHARED:-$HERE/state}"; SP="${1:-0}"; D="$SHARED/audio"; mkdir -p "$D"; WAV="$D/hum_$SP.wav"
if [ ! -s "$WAV" ]; then
    row=$(awk -F'|' -v s="$SP" '/^HUM/{a=$2; gsub(/ /,"",a); if (a==s) {w=$3; b=$4; gsub(/ /,"",w); gsub(/ /,"",b); n=$6; gsub(/ /,"",n); st=$5; gsub(/^ +| +$/,"",st); print w "|" b "|" n "|" st; exit}}' "$HERE/voices.pdl")
    [ -z "$row" ] && exit 0
    wave=${row%%|*}; r=${row#*|}; base=${r%%|*}; r=${r#*|}; ms=${r%%|*}; steps=${r#*|}; dur=$(awk -v m="$ms" 'BEGIN{printf "%.3f", m/1000}'); i=0; list=""
    for s in $steps; do i=$((i+1)); f=$(awk -v b="$base" -v s="$s" 'BEGIN{printf "%.1f", b*(2^(s/12))}'); seg="$D/.seg_${SP}_$i.wav"
        nice -n 15 sox -n -r 22050 -c 1 "$seg" synth "$dur" "$wave" "$f" fade q 0.02 "$dur" 0.08 vol 0.35 2>/dev/null; list="$list $seg"; done
    nice -n 15 sox $list "$WAV" 2>/dev/null; rm -f "$D"/.seg_${SP}_*.wav
fi
[ -s "$WAV" ] || exit 0
exec 9>"$D/play.lock"; flock -n 9 || exit 0
nice -n 10 paplay --volume=26000 "$WAV" >/dev/null 2>&1
