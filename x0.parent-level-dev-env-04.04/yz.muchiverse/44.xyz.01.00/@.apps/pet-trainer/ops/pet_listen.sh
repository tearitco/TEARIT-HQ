#!/bin/sh
# pet_listen.sh - the ♨ talk-back: record from the microphone until a sustained pause (sox silence effect), turn it into text with the offline vosk model (ops/pet_stt.py, state/stt) and say it to the pet through the normal chat path. recording.txt = 1 while the mic is open (ui.txt shows it). A second press while recording cancels. Max 25 s.
HERE="$(cd "$(dirname "$0")/.." && pwd)"; SHARED="${PET_SHARED:-$HERE/state}"; REC="$SHARED/recording.txt"; T="$SHARED/stt/rec_$$.wav"
mkdir -p "$SHARED/stt"
if [ "$(cat "$REC" 2>/dev/null)" = 1 ]; then pkill -x rec 2>/dev/null; echo 0 > "$REC"; exit 0; fi
echo 1 > "$REC"; trap 'echo 0 > "$REC"; rm -f "$T"' EXIT
# start when sound begins (0.1 s above 4%), stop after 1.8 s below 4%; give up after 25 s
timeout 25 nice -n 10 rec -q -r 16000 -c 1 -b 16 "$T" silence 1 0.1 4% 1 1.8 4% >/dev/null 2>&1
echo 0 > "$REC"
[ -s "$T" ] || exit 0
text=$(nice -n 15 python3 "$HERE/ops/pet_stt.py" "$T" 2>/dev/null)
rm -f "$T"
[ -n "$text" ] || { printf 'you: (i did not catch that)\n' >> "$SHARED/stt/last.txt"; exit 0; }
printf '%s\n' "$text" > "$SHARED/stt/last.txt"
PET_DIR= sh "$HERE/ops/pet_event.sh" chat_input x y "$text"
