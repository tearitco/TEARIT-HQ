#!/bin/sh
# open_board.sh - open the rpg-edit-hq BOARD window (rpg-edit-hq as a module of the pc-hq board engine; the standalone window stays: sh ../button.sh run).
# 1) export the rooms as the book maps/rpg-edit-hq (map.png, events.txt, bar.txt, ascii_legend.txt, 3D set)  2) launch the pc-hq board with THIS folder's template
# (open_pchq_board.sh takes PCHQ_BOARD_TPL; the template must be named pchq-board.xhtpm)  3) tell the board to open the rpg-edit-hq book.
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="$(dirname "$HERE")"; HOUSE="$(cd "$APP/../.." && pwd)"
# rpg-edit-hq runs on its OWN pc-hq instance (PCHQ-SESSIONS-DESIGN.md): data folder <app>/rpg-edit-hq-pchq (world, hero, xelector, maps, engine sessions), programs shared from @.apps/piececraft-hq. The launcher
# creates the folder when it is missing and only ever replaces THIS instance's board window, never another game's.
# Ask first (cpu_gate, hq-cpu-safety.md section 10): a relaunch starts a daemon, an engine and a window; do not start it on a machine that is already hot (waits up to 90 s for it to calm).
GATE="$HOUSE/&.widgits/_shared-lib/ops/+x/cpu_gate.+x"
[ -x "$GATE" ] && { "$GATE" "$HOUSE" check --wait 90 >/dev/null || { echo "open_board: the machine is too busy right now (cpu_gate); not relaunching" >&2; exit 1; }; }
export PCHQ_PKG="$APP/rpg-edit-hq-pchq"
export PCHQ_NO_HERO=1   # this editor drives its own characters (rig: tokens): no hero_01 placeholder in its instance (owner 2026-11-26; pchq_instance_init.sh)
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_instance_init.sh" "$PCHQ_PKG" >/dev/null 2>&1
mkdir -p "$HERE/state"; [ -f "$HERE/state/sides.txt" ] || printf "side_left=1\nside_right=1\n" > "$HERE/state/sides.txt"
"$APP/ops/+x/rpg_edit_hq.+x" export3d >/dev/null 2>&1
# the ONE rpg-edit-hq daemon (walks the pets, chat, clock, writes ../state/ui.txt): not a <module> of this window, a module runs with THIS folder as its app dir and would start a second house in board/state
# Registered through the spawn funnel (AGENTS.md: every process the house starts is recorded in #.desktop/proc_registry.pdl); it starts as lifetime keep and is handed to the window below.
FUNNEL="$HOUSE/&.widgits/_shared-lib/kh_spawn.sh"; REG="$HOUSE/&.widgits/_shared-lib/ops/+x/proc_reg_op.+x"
ps -eo stat,comm | awk '$2=="rpg_edit_hq.+x" && $1 !~ /Z/{f=1} END{exit !f}' || {
    KH_ROLE=daemon KH_APP=rpg-edit-hq sh "$FUNNEL" "$HOUSE" 0 rpg-edit-hq-daemon -- "$APP/ops/+x/rpg_edit_hq.+x" "$HOUSE" "$APP" >/dev/null 2>&1; }
PCHQ_BOARD_TPL="$HERE/pchq-board.xhtpm" sh "$HOUSE/@.apps/piececraft-hq/open_pchq_board.sh" "$HOUSE" || exit 1
# The world stops with its window (owner decision 2026-11-02): hand the daemon to the board window, so closing the window ends the daemon (and the game clock it owns) within ~35 s.
# Opt-in to a world that keeps running with the window closed: SECTION | keep_running | 1 in this app's toy.pdl (the row then stays lifetime keep, still listed in the registry).
KEEP="$(awk -F'|' '$2 ~ /keep_running/ {gsub(/[ \t]/, "", $3); print $3}' "$APP/toy.pdl" 2>/dev/null)"
WPID="$(cat "$PCHQ_PKG/pieces/system/board_window.pid" 2>/dev/null)"
DPID="$([ -x "$REG" ] && "$REG" "$HOUSE" list 2>/dev/null | awk -F' [|] ' '$10 == "rpg-edit-hq-daemon" {print $2}' | tail -1)"
# Relaunch race (found 2026-11-03): when this runs over an OPEN editor, the daemon the check above saw alive belonged to the OLD window, and the registry ended it when that window was replaced.
# If the daemon is gone by now, start a fresh one (through the funnel) before handing it to the new window.
if [ -z "$DPID" ] || [ ! -d "/proc/$DPID" ]; then
    KH_ROLE=daemon KH_APP=rpg-edit-hq sh "$FUNNEL" "$HOUSE" 0 rpg-edit-hq-daemon -- "$APP/ops/+x/rpg_edit_hq.+x" "$HOUSE" "$APP" >/dev/null 2>&1; sleep 2
    DPID="$([ -x "$REG" ] && "$REG" "$HOUSE" list 2>/dev/null | awk -F' [|] ' '$10 == "rpg-edit-hq-daemon" {print $2}' | tail -1)"
fi
if [ "$KEEP" != "1" ] && [ -x "$REG" ] && [ -n "$WPID" ] && [ -n "$DPID" ] && [ -d "/proc/$WPID" ] && [ -d "/proc/$DPID" ]; then
    "$REG" "$HOUSE" register "$DPID" owner "$WPID" daemon rpg-edit-hq rpg-edit-hq-daemon "$APP/ops/+x/rpg_edit_hq.+x $HOUSE $APP" >/dev/null 2>&1 || true
fi
# The joystick (owner 2026-11-26: "make sure it starts when rpg-edit starts, if plugged in"). The house already has the daemon (_.monads/_.livedesk-taskbar/ops/khtpm_joystick_daemon.c, arrows from the D-pad into the FOCUSED window's relay file) but nothing launched it, so the controller did nothing anywhere.
# Started through the funnel like the daemon above, ALWAYS, with or without a pad (v4 of the daemon waits in the kernel for a pad to appear and survives an unplug, 2026-11-26: it used to exit and stay dead after a re-plug), once (a registered live one is reused), and handed to the board window so closing the editor ends it.
JSD="$HOUSE/_.monads/_.livedesk-taskbar/ops/+x/khtpm_joystick_daemon.+x"
jreg() { [ -x "$REG" ] && "$REG" "$HOUSE" list 2>/dev/null | awk -F' [|] ' '$10 == "joystick-daemon" {print $2}' | tail -1; }
JPID="$(jreg)"
if [ -x "$JSD" ] && { [ -z "$JPID" ] || [ ! -d "/proc/$JPID" ]; }; then
    KH_ROLE=daemon KH_APP=rpg-edit-hq sh "$FUNNEL" "$HOUSE" 0 joystick-daemon -- "$JSD" "$HOUSE" >/dev/null 2>&1; sleep 1; JPID="$(jreg)"
fi
if [ "$KEEP" != "1" ] && [ -x "$REG" ] && [ -n "$WPID" ] && [ -n "$JPID" ] && [ -d "/proc/$WPID" ] && [ -d "/proc/$JPID" ]; then
    "$REG" "$HOUSE" register "$JPID" owner "$WPID" daemon rpg-edit-hq joystick-daemon "$JSD $HOUSE" >/dev/null 2>&1 || true
fi
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
# Re-open where the owner WAS (2026-11-17, owner: "make sure the last book page opens"): nothing called edit_last.sh save since 2026-11-08, so state/last_book.txt was stale (deskopoly/board) and every relaunch
# fell back to it. The page this editor had open is in its own persisted world_01/state.txt (map_id, desk_id); save it as the last book first. A page that no longer exists is refused by save and the older row is used.
CURB=$(sed -n 's/^map_id=//p' "$PCHQ_PKG/pieces/world_01/state.txt" 2>/dev/null | head -1); CURP=$(sed -n 's/^desk_id=//p' "$PCHQ_PKG/pieces/world_01/state.txt" 2>/dev/null | head -1)
if [ -n "$CURB" ] && [ -n "$CURP" ]; then sh "$APP/ops/edit_last.sh" save "$APP/state/last_book.txt" "$CURB" "$CURP" --house "$HOUSE" --maps "$PCHQ_PKG/pieces/system/maps" >/dev/null 2>&1 || true; fi
# Last edited book, or the Islands demo (page start) when the row is missing or names a book this instance cannot open.
LINE=$(sh "$APP/ops/edit_last.sh" resolve "$APP/state/last_book.txt" --house "$HOUSE" --maps "$PCHQ_PKG/pieces/system/maps" --demo-book rpg-edit-hq --demo-page start) || exit 1
BOOK=${LINE#book=}; BOOK=${BOOK%% page=*}; PAGE=${LINE#*page=}
# User preference (state/prefs.txt, key=value, set from the editor's own Settings tab by ops/edit_prefs.c): editor_open=last (default: the book you edited last) | own (always this editor's own Islands book,
# the only book where its voxel placement is allowed: the daemon refuses on any other book).
case "$(sed -n 's/^editor_open=//p' "$APP/state/prefs.txt" 2>/dev/null | tail -1)" in own) BOOK=rpg-edit-hq; PAGE=start ;; esac
case "$BOOK" in
    *[!A-Za-z0-9_-]*|"") exit 1 ;;
esac
# The edit package only ships its own map. Every other book is FOUND where its app keeps it (book_path_op, the shared locator: no symlinks, owner 2026-11-18), so the book dropdown lists it and a new book appears without a code change.
BPO="$HOUSE/&.widgits/_shared-lib/ops/+x/book_path_op.+x"
if ! "$BPO" "$PCHQ_PKG" "$HOUSE" "$BOOK" >/dev/null 2>&1; then
    BOOK=rpg-edit-hq
    PAGE=start
fi
# START_MAP regenerates the book on its default page (centroid: test) and that finishes AFTER a SET_DESK sent in the same instant, so the desk the owner was on was lost again (2026-11-17, found after the last-book fix: resolve said live, the editor opened test). Send START_MAP, wait until the engine's world_01/state.txt names the book (up to 40 s), let the generate settle, then SET_DESK.
PSTART="$HOUSE/&.widgits/_shared-lib/ops/+x/page_start_op.+x"
sleep 8; printf 'CONFIRM_START_MAP:%s\n' "$BOOK" >> "$INBOX"
for _i in $(seq 1 20); do sleep 2; [ "$(sed -n 's/^map_id=//p' "$PCHQ_PKG/pieces/world_01/state.txt" 2>/dev/null | head -1)" = "$BOOK" ] && break; done
sleep 6; printf 'CONFIRM_SET_DESK:%s\n' "$PAGE" >> "$INBOX"
# 2026-11-17 (owner: start over land so there is something to see): once the page has loaded, put the cursor on the page's start cell (a `start | x y` row of the page's view.pdl, else its middle): the same shared op the phone editor uses.
if [ -x "$PSTART" ] && [ -n "$PAGE" ]; then sleep 8; _S="$(sed -n 's/^bv_session=//p' "$HERE/state/ui.txt" 2>/dev/null | head -1)"; [ -n "$_S" ] && [ -d "$_S" ] && "$PSTART" "$PCHQ_PKG" "$BOOK" "$PAGE" "$_S" >/dev/null 2>&1; fi
# Declare the window the OWNER of the engine's detached stack: it leaves the process we registered, and it RESPAWNS when the board reloads its book, which is how a closed board
# used to leave a trigger watcher, a clock daemon and the board-viewer prisc / parser / renderer running for hours. Two explicit rules in #.desktop/proc_adopt_rules.pdl, applied on
# every registry pass (the guard, every ~30 s) while the window lives: this instance's session folder, and the board-viewer session this window reports in state/ui.txt
# (bv_session, re-read each pass). What they adopt becomes rows in proc_registry.pdl (auditable) and ends with the window.
if [ -x "$REG" ] && [ -n "$WPID" ] && [ -d "/proc/$WPID" ]; then
    "$REG" "$HOUSE" rule-add "$WPID" "$PCHQ_PKG/pieces/sessions/" engine rpg-edit-hq pchq-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" rule-add "$WPID" "@file:$HERE/state/ui.txt#bv_session" engine rpg-edit-hq bv-session >/dev/null 2>&1 || true
    "$REG" "$HOUSE" apply-rules >/dev/null 2>&1 || true
fi
# Sidebar start layout: last used (state/sides_history.pdl), or the `sidebar_start=` preference in state/prefs.txt (last|pets|palettes|none); first run = palettes with pets minimized.
[ -x "$APP/ops/+x/edit_prefs.+x" ] && "$APP/ops/+x/edit_prefs.+x" "$HOUSE" show >/dev/null 2>&1
[ -x "$APP/ops/+x/pal_side.+x" ] && "$APP/ops/+x/pal_side.+x" "$HOUSE" restore >/dev/null 2>&1 || true
