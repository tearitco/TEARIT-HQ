#!/bin/sh
# doom_event.sh <verb> [arg] [arg]
# Session verbs match the RPG Maker title screen.
# Thing verbs match a Doom page. Every state change autosaves while hp > 0.
# hp 0 stops play and does not overwrite that autosave, so Continue
# returns to the last living state. Restart is the only fresh Hangar.
VERB="${1:-}"
A2="${2:-}"
A3="${3:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PCHQ="$(cd "$HERE/.." && pwd)"
HOUSE="$(cd "$HERE/../../.." && pwd)"
BOOKDIR="$PCHQ/pieces/system/maps/doom"
PM="$HOUSE/#.desktop/khtpm_play_mode.state.txt"
ST="$BOOKDIR/state.pdl"
DP="$BOOKDIR/deadpool.pdl"
INBOX="$PCHQ/pieces/system/widget_cmds/inbox.txt"
WORLD="$PCHQ/pieces/world_01/state.txt"
AS="$BOOKDIR/autosave.pdl"
mkdir -p "$(dirname "$PM")" "$(dirname "$INBOX")" "$BOOKDIR" "$BOOKDIR/autosave"
[ -f "$ST" ] || printf 'hp=100\nammo=50\nkeys=0\nbattle=0\narmor=0\n' > "$ST"
[ -f "$DP" ] || printf 'SECTION | KEY | VALUE\n' > "$DP"

clamp() {
    v=$1; lo=$2; hi=$3
    [ "$v" -lt "$lo" ] && v=$lo
    [ "$v" -gt "$hi" ] && v=$hi
    printf '%s' "$v"
}

add_key() {
    k="$1"
    cur=$(sed -n "s/^$k=//p" "$ST" | head -1)
    cur=${cur:-0}
    n=$(clamp $((cur + $2)) "$3" "$4")
    if grep -q "^$k=" "$ST"; then
        sed -i "s/^$k=.*/$k=$n/" "$ST"
    else
        printf '%s=%s\n' "$k" "$n" >> "$ST"
    fi
}

set_world_desk() {
    desk="$1"
    [ -f "$WORLD" ] || return 0
    grep -q '^map_id=' "$WORLD" && sed -i 's/^map_id=.*/map_id=doom/' "$WORLD" || printf 'map_id=doom\n' >> "$WORLD"
    grep -q '^desk_id=' "$WORLD" && sed -i "s/^desk_id=.*/desk_id=$desk/" "$WORLD" || printf 'desk_id=%s\n' "$desk" >> "$WORLD"
}

one_desk() {
    printf 'CONFIRM_SET_DESK:%s\n' "$1" > "$INBOX"
}

hero_xy() {
    sed -n 's/^desk_id=//p' "$WORLD" 2>/dev/null | head -1 >/dev/null
    x=0; y=0
    d=$(sed -n 's/^desk_id=//p' "$WORLD" 2>/dev/null | head -1)
    s="$BOOKDIR/$d/START.txt"
    if [ -f "$s" ]; then
        x=$(sed -n 's/^x=//p' "$s" | head -1)
        y=$(sed -n 's/^y=//p' "$s" | head -1)
    fi
    printf '%s,%s' "${x:-0}" "${y:-0}"
}

autosave() {
    hp=$(sed -n 's/^hp=//p' "$ST" | head -1)
    hp=${hp:-100}
    [ "$hp" -gt 0 ] || return 0
    map=$(sed -n 's/^map_id=//p' "$WORLD" 2>/dev/null | head -1)
    desk=$(sed -n 's/^desk_id=//p' "$WORLD" 2>/dev/null | head -1)
    mode=off
    grep -q '^mode=on' "$PM" 2>/dev/null && mode=on
    when=$(date '+%Y-%m-%d %H:%M:%S')
    {
        printf 'AUTO | map_id   | %s\n' "${map:-doom}"
        printf 'AUTO | desk_id  | %s\n' "${desk:-title}"
        printf 'AUTO | mode     | %s\n' "$mode"
        printf 'AUTO | hero_xy  | %s\n' "$(hero_xy)"
        printf 'AUTO | saved_at | %s\n' "$when"
    } > "$AS.tmp"
    mv -f "$AS.tmp" "$AS"
    cp "$ST" "$BOOKDIR/autosave/state.pdl"
    cp "$DP" "$BOOKDIR/autosave/deadpool.pdl"
}

died() {
    printf 'mode=off\n' > "$PM"
    set_world_desk title
    one_desk title
}

reset_state() {
    printf 'hp=100\nammo=50\nkeys=0\nbattle=0\narmor=0\n' > "$ST"
}

revive_all() {
    printf 'SECTION | KEY | VALUE\n' > "$DP"
    for ev in "$BOOKDIR"/*/events.pdl; do
        [ -f "$ev" ] || continue
        desk=$(basename "$(dirname "$ev")")
        awk -F'|' -v desk="$desk" '
            BEGIN { n=0 }
            $1 ~ /EVENT/ {
                n++
                x=y=g=tr=""
                split($2, a, " ")
                for (i in a) {
                    split(a[i], kv, "=")
                    if (kv[1]=="x") x=kv[2]
                    if (kv[1]=="y") y=kv[2]
                    if (kv[1]=="glyph") g=kv[2]
                }
                split($3, b, " ")
                for (i in b) {
                    split(b[i], kv, "=")
                    if (kv[1]=="trigger") tr=kv[2]
                }
                if (tr=="player-touch" || tr=="on-touch" || tr=="event-touch") tr="on-touch"
                if (tr=="") tr="on-click"
                name=g
                if (g=="N") name="New Game"
                if (g=="Q") name="Quit"
                if (g=="H") name="Health"
                if (g=="A") name="Ammo"
                if (g=="K") name="Key"
                if (g=="E") name="Monster"
                if (g=="S") name="Start"
                if (g=="X") name="Exit"
                if (g=="~") name="Nukage"
                printf "%d\t%s\t%s\t%s\t%s\n", n, name, x, y, tr
            }
        ' "$ev" > "$BOOKDIR/$desk/bar.txt"
    done
}

is_dead() {
    desk="$1"; id="$2"
    awk -F'|' -v desk="$desk" -v id="$id" '
        { gsub(/^[ \t]+|[ \t]+$/, "", $2); gsub(/^[ \t]+|[ \t]+$/, "", $3) }
        $2=="desk" { d=$3 }
        $2=="ev" && $3==id && d==desk { hit=1 }
        END { exit hit ? 0 : 1 }
    ' "$DP"
}

kill_event() {
    desk="$1"; id="$2"
    [ -n "$desk" ] && [ -n "$id" ] || return 0
    is_dead "$desk" "$id" && return 0
    evf="$BOOKDIR/$desk/events.pdl"
    [ -f "$evf" ] || return 0
    line=$(awk -v id="$id" 'BEGIN{n=0} $1=="EVENT"{n++; if (n==id+0) { print; exit }}' "$evf")
    [ -n "$line" ] || return 0
    x=$(printf '%s' "$line" | sed -n 's/.*x=\([0-9]*\).*/\1/p')
    y=$(printf '%s' "$line" | sed -n 's/.*y=\([0-9]*\).*/\1/p')
    g=$(printf '%s' "$line" | sed -n 's/.*glyph=\([^ ]*\).*/\1/p')
    when=$(date '+%Y-%m-%d %H:%M:%S')
    {
        printf 'DEAD | desk           | %s\n' "$desk"
        printf 'DEAD | ev             | %s\n' "$id"
        printf 'DEAD | x_y            | %s,%s\n' "$x" "$y"
        printf 'DEAD | glyph          | %s\n' "$g"
        printf 'DEAD | sprite         | frames/%s.rgba\n' "$g"
        printf 'DEAD | pal            | ev/%s\n' "$id"
        printf 'DEAD | died_at        | %s\n' "$when"
    } >> "$DP"
    if [ -f "$BOOKDIR/$desk/bar.txt" ]; then
        awk -F'\t' -v id="$id" '$1 != id { print }' "$BOOKDIR/$desk/bar.txt" > "$BOOKDIR/$desk/bar.txt.tmp"
        mv -f "$BOOKDIR/$desk/bar.txt.tmp" "$BOOKDIR/$desk/bar.txt"
    fi
}

slot_dir() {
    n="$1"
    case "$n" in
        ''|*[!0-9]*) return 1 ;;
    esac
    [ "$n" -ge 1 ] && [ "$n" -le 16 ] || return 1
    uuid=""
    login=$(ls -d "$HOUSE"/0.user-pal*/00.login-signup/current_login.txt 2>/dev/null | head -1)
    [ -n "$login" ] && uuid=$(sed -n 's/^current_user_uuid=//p' "$login" | head -1)
    if [ -z "$uuid" ]; then
        for d in "$HOUSE"/xyzfs/users/*/home/livedesk; do
            [ -d "$d" ] || continue
            uuid=$(basename "$(dirname "$(dirname "$d")")")
            break
        done
    fi
    if [ -n "$uuid" ]; then
        printf '%s/xyzfs/users/%s/home/livedesk/savegames/slot_%02d\n' "$HOUSE" "$uuid" "$n"
    else
        printf '%s/#.desktop/savegames/slot_%02d\n' "$HOUSE" "$n"
    fi
}

case "$VERB" in
    start_game|restart_game)
        printf 'mode=on\n' > "$PM"
        revive_all
        reset_state
        set_world_desk e1m1_hangar
        one_desk e1m1_hangar
        autosave
        ;;
    player_start)
        printf 'mode=on\n' > "$PM"
        autosave
        ;;
    stop_game)
        printf 'mode=off\n' > "$PM"
        ;;
    continue_game)
        [ -f "$AS" ] || exit 0
        map=$(awk -F'|' '/map_id/ { gsub(/ /,"",$3); print $3; exit }' "$AS")
        desk=$(awk -F'|' '/desk_id/ { gsub(/ /,"",$3); print $3; exit }' "$AS")
        mode=$(awk -F'|' '/AUTO/ && $2 ~ /mode/ { gsub(/ /,"",$3); print $3; exit }' "$AS")
        [ -f "$BOOKDIR/autosave/state.pdl" ] && cp "$BOOKDIR/autosave/state.pdl" "$ST"
        [ -f "$BOOKDIR/autosave/deadpool.pdl" ] && cp "$BOOKDIR/autosave/deadpool.pdl" "$DP"
        printf 'mode=%s\n' "${mode:-on}" > "$PM"
        set_world_desk "${desk:-title}"
        one_desk "${desk:-title}"
        ;;
    change_hp) add_key hp 10 0 200 ;;
    change_armor) add_key armor 1 0 200 ;;
    nukage) add_key hp -10 0 200 ;;
    change_ammo) add_key ammo 10 0 999 ;;
    change_items) add_key keys 1 0 3 ;;
    start_battle) add_key battle 1 0 9999 ;;
    next_level)
        cur=$(sed -n 's/^desk_id=//p' "$WORLD" 2>/dev/null | head -1)
        case "$cur" in
            e1m1_hangar) nxt=e1m2_nukage ;;
            e1m2_nukage) nxt=e1m3_toxin ;;
            e1m3_toxin) nxt=e1m4_command ;;
            e1m4_command) nxt=e1m5_lab ;;
            e1m5_lab) nxt=e1m6_central ;;
            e1m6_central) nxt=e1m7_computer ;;
            e1m7_computer) nxt=e1m8_anomaly ;;
            e1m8_anomaly) nxt=e1m9_base ;;
            *) nxt=title ;;
        esac
        set_world_desk "$nxt"
        one_desk "$nxt"
        ;;
    kill_event) kill_event "$A2" "$A3" ;;
    revive_all) revive_all ;;
    autosave) autosave ;;
    save_slot|load_slot)
        slot=$(slot_dir "$A2") || exit 0
        if [ "$VERB" = save_slot ]; then
            mkdir -p "$slot"
            map=$(sed -n 's/^map_id=//p' "$WORLD" 2>/dev/null | head -1)
            desk=$(sed -n 's/^desk_id=//p' "$WORLD" 2>/dev/null | head -1)
            mode=off
            grep -q '^mode=on' "$PM" 2>/dev/null && mode=on
            {
                printf 'map_id=%s\ndesk_id=%s\nmode=%s\n' "${map:-doom}" "${desk:-title}" "$mode"
            } > "$slot/book.pdl"
            cp "$ST" "$slot/state.pdl"
            cp "$DP" "$slot/deadpool.pdl"
            printf 'SLOT | n | %s\nSLOT | saved_at | %s\n' "$A2" "$(date '+%Y-%m-%d %H:%M:%S')" > "$slot/meta.pdl"
        else
            [ -f "$slot/book.pdl" ] || exit 0
            map=$(sed -n 's/^map_id=//p' "$slot/book.pdl" | head -1)
            desk=$(sed -n 's/^desk_id=//p' "$slot/book.pdl" | head -1)
            mode=$(sed -n 's/^mode=//p' "$slot/book.pdl" | head -1)
            cp "$slot/state.pdl" "$ST" 2>/dev/null || true
            [ -f "$slot/deadpool.pdl" ] && cp "$slot/deadpool.pdl" "$DP"
            printf 'mode=%s\n' "${mode:-on}" > "$PM"
            set_world_desk "${desk:-title}"
            one_desk "${desk:-title}"
        fi
        ;;
    *) exit 0 ;;
esac
hp=$(sed -n 's/^hp=//p' "$ST" | head -1)
hp=${hp:-100}
if [ "$hp" -le 0 ]; then
    died
else
    case "$VERB" in
        stop_game|save_slot|load_slot|continue_game|kill_event) ;;
        *) autosave ;;
    esac
fi
exit 0
