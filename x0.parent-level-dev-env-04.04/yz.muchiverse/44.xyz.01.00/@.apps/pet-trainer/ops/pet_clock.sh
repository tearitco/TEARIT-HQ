#!/bin/sh
# pet_clock.sh <install|start|stop|rate <r>|advance <1h|1d>|status|reinstall> - the pet's TIME, on the house's livedesk clock (&.widgits/livedesk-clock/ops/+x/lc_clock.+x).
# The pet has its OWN mini root (<state>/clock_root: its own clock "pet", reminders and daemon), so it never touches the main campaign clock. Start = resume + daemon; Stop = pause + stop daemon (zero CPU).
# The reminders come from time.pdl and run ops/pet_clock_runner.sh -> pet_event.sh clock_event <event>. `status` prints time_label= time_rate= time_running= for ui.txt (reads the clock file, no process).
HERE="$(cd "$(dirname "$0")/.." && pwd)"; HOUSE="$(cd "$HERE/../.." && pwd)"; SHARED="${PET_SHARED:-$HERE/state}"; ROOT="$SHARED/clock_root"
LC="${LC_CLOCK_BIN:-$HOUSE/&.widgits/livedesk-clock/ops/+x/lc_clock.+x}"; CF="$ROOT/#.desktop/clocks/pet.pdl"; V="${1:-status}"; ARG="${2:-}"
kv() { sed -n "s/^$1=//p" "$CF" 2>/dev/null | head -1; }
install() {
    [ -x "$LC" ] || return 1; mkdir -p "$ROOT/#.desktop/clocks" "$ROOT/common_events"
    if [ ! -f "$CF" ]; then "$LC" "$ROOT" new pet user "pet trainer time" >/dev/null 2>&1; "$LC" "$ROOT" rate pet min >/dev/null 2>&1
        "$LC" "$ROOT" cmd pet settime 21600000 --source pet-install >/dev/null 2>&1; "$LC" "$ROOT" cmd pet pause --source pet-install >/dev/null 2>&1; "$LC" "$ROOT" step 5 >/dev/null 2>&1      # D1 starts at 06:00, paused
        echo 0 > "$SHARED/clock_day0.txt"; fi
    ver=$(cat "$HERE/time.pdl" "$HERE/ops/pet_clock.sh" | cksum | cut -d' ' -f1)
    if [ "$(cat "$ROOT/.schedule_installed" 2>/dev/null)" != "$ver" ]; then
        rm -f "$ROOT/#.desktop/clocks/reminders.pdl" "$ROOT/#.desktop/clocks/schedule_ledger.txt"; now=$(kv game_time_epoch_ms); now=${now:-0}
        # a game clock reads HH:MM as an absolute ms (it is only a time of day on the wall clock), so the next occurrence is worked out here
        awk -F'|' '/^AT/{e=$2; w=$3; r=$4; n=$5; gsub(/^ +| +$/,"",e); gsub(/^ +| +$/,"",w); gsub(/^ +| +$/,"",r); gsub(/^ +| +$/,"",n); print e "\t" w "\t" r "\t" n}' "$HERE/time.pdl" |
        while IFS="$(printf '\t')" read -r e w r n; do
            case "$w" in *:*) hh=${w%%:*}; mm=${w#*:}; hh=${hh#0}; mm=${mm#0}; hh=${hh:-0}; mm=${mm:-0}; base=$(( now - now % 86400000 )); at=$(( base + hh * 3600000 + mm * 60000 )); [ "$at" -le "$now" ] && at=$(( at + 86400000 )); w=$at;; esac
            "$LC" "$ROOT" reminder-add pet "$w" "common:$e" "$n" "$r" >/dev/null 2>&1; done
        echo "$ver" > "$ROOT/.schedule_installed"; fi
}
phase_now() {      # the right daylight phase for the clock's current hour (used after start / skips; the clock events keep it right afterwards)
    ms=$(kv game_time_epoch_ms); [ -n "$ms" ] || return 0; h=$(( ms / 3600000 % 24 ))
    if [ "$h" -ge 19 ] || [ "$h" -lt 6 ]; then p=night; elif [ "$h" -lt 8 ]; then p=dawn; elif [ "$h" -ge 17 ]; then p=dusk; else p=day; fi
    printf 'phase=%s\n' "$p" > "$SHARED/daylight.txt"
}
case "$V" in
    install) install ;;
    reinstall) "$LC" "$ROOT" daemon-stop >/dev/null 2>&1; rm -f "$ROOT/.schedule_installed"; install ;;
    start) install || exit 0; phase_now; "$LC" "$ROOT" cmd pet resume --source pet-start >/dev/null 2>&1
        LC_CLOCK_EVENT_RUNNER="$HERE/ops/pet_clock_runner.sh" LC_CLOCK_NO_POPUP=1 PET_SHARED="$SHARED" "$LC" "$ROOT" daemon-start >/dev/null 2>&1 ;;
    stop) [ -f "$CF" ] || exit 0; "$LC" "$ROOT" cmd pet pause --source pet-stop >/dev/null 2>&1; sleep 1; "$LC" "$ROOT" daemon-stop >/dev/null 2>&1 ;;
    rate) case "$ARG" in cent|sec|min|hour|day) install; "$LC" "$ROOT" cmd pet rate "$ARG" --source pet-menu >/dev/null 2>&1;; esac ;;
    advance) case "$ARG" in [0-9]*[smhd]) install; "$LC" "$ROOT" cmd pet advance "$ARG" --source pet-menu >/dev/null 2>&1; sleep 2; phase_now;; esac ;;
    status) ms=$(kv game_time_epoch_ms); if [ -z "$ms" ]; then printf 'time_label=--:--\ntime_rate=-\ntime_running=0\n'; else d0=$(cat "$SHARED/clock_day0.txt" 2>/dev/null || echo 0)
        printf 'time_label=%s\ntime_rate=%s\ntime_running=%s\n' "$(awk -v ms="$ms" -v d0="$d0" 'BEGIN{d=int(ms/86400000); s=int((ms-d*86400000)/1000); printf "D%d %02d:%02d", d-d0+1, int(s/3600), int((s%3600)/60)}')" "$(kv rate)" "$(kv running)"; fi ;;
esac
exit 0
