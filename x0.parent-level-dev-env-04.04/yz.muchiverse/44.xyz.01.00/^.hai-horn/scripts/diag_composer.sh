#!/bin/bash
# One-shot diagnostic: is the composer actually broken, or is the test
# reading state that is stale for a different reason?
#
# Activates the composer, types two characters, then samples BOTH the
# parser's own buffer mirror (gui_state) and the rendered frame over time.
# If gui_state shows the text and the frame never does, it is a render
# problem. If both show it, the suite was reading too early. If neither
# does, the keystrokes are not arriving.
set -u
cd "$(dirname "$0")/.." || exit 1
export PRISC_PROJECT_ROOT="$PWD" PRISC_PROJECT_ID="hai-horn"
export HORN_SESSIONS="$PWD/chats/HORN_SESSIONS"
export HORN_ENTITY_DIR="${HORN_ENTITY_DIR:-$(cd .. && pwd)/&.widgits/open-hai/state}"

KEYS=pieces/keyboard/history.txt
FRAME=pieces/display/current_frame.txt
GUI=pieces/apps/player_app/manager/gui_state.txt
TYP=pieces/display/active_gui_is_typing.txt

SC=pieces/apps/player_app/state_changed.txt
FC=pieces/display/frame_changed.txt
sample() { printf '  t=%-3s typing=%s gui=[%s] frame=[%s] state_ch=%s frame_ch=%s\n' \
    "$1" "$(cat $TYP 2>/dev/null|tr -d '\n')" \
    "$(sed -n 's/^horn_prompt=//p' $GUI 2>/dev/null|tr -d '\n')" \
    "$(grep -oE 'you: : \[[^]]*\]' $FRAME 2>/dev/null|head -1)" \
    "$(wc -c < $SC 2>/dev/null|tr -d ' ')" \
    "$(wc -c < $FC 2>/dev/null|tr -d ' ')"; }

./horn_chat.sh clean >/dev/null 2>&1
: > "$KEYS"; : > pieces/apps/player_app/interact_relay.txt
# Stale prompt files survive horn_chat.sh clean and would make
# horn_publish_pending republish on every tick, muddying this.
rm -f pieces/horn/pending.json pieces/horn/pending_stamp.txt pieces/horn/decision.txt
ops/+x/horn_publish.+x
nohup ./system/renderer >/tmp/dx_r.log 2>&1 &
nohup ./system/chtpm_parser_pal layouts/horn_chat.chtpm >/tmp/dx_p.log 2>&1 &
sleep 3
sample "boot"

printf 'KEY_PRESSED: 13\n' >> "$KEYS"   # activate the composer
for i in 1 2 3 4 5 6 7 8; do
    [ "$(cat $TYP 2>/dev/null)" = "1" ] && break
    sleep 0.5
done
sample "after-13"

for ch in H i; do
    printf 'KEY_PRESSED: %d\n' "'$ch" >> "$KEYS"
    sleep 0.4
    sample "typed-$ch"
done

for n in 1 2 3 4 5 6; do sleep 0.5; sample "idle$n"; done

pkill -9 -f "horn_main_loop.pal"         2>/dev/null
pkill -9 -f "chtpm_parser_pal layouts"   2>/dev/null
pkill -9 -f "renderer$"                  2>/dev/null
exit 0
