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
INBOX="$PCHQ_PKG/pieces/system/widget_cmds/inbox.txt"
# Last edited book, or the Islands demo (page start) when the row is missing or names a book this instance cannot open.
LINE=$(sh "$APP/ops/edit_last.sh" resolve "$APP/state/last_book.txt" --house "$HOUSE" --maps "$PCHQ_PKG/pieces/system/maps" --demo-book rpg-edit-hq --demo-page start) || exit 1
BOOK=${LINE#book=}; BOOK=${BOOK%% page=*}; PAGE=${LINE#*page=}
# User preference (state/prefs.txt, key=value, set from the editor's own Settings tab by ops/edit_prefs.c): editor_open=last (default: the book you edited last) | own (always this editor's own Islands book,
# the only book where its voxel placement is allowed: the daemon refuses on any other book).
case "$(sed -n 's/^editor_open=//p' "$APP/state/prefs.txt" 2>/dev/null | tail -1)" in own) BOOK=rpg-edit-hq; PAGE=start ;; esac
case "$BOOK" in
    *[!A-Za-z0-9_-]*|"") exit 1 ;;
esac
# The edit package only ships its own map. Every other book with a game.pdl is a link, so the book dropdown can list it. A new book appears without a code change.
for src in "$HOUSE"/@.apps/*/*-pchq/pieces/system/maps/*/game.pdl; do
    [ -f "$src" ] || continue
    bookdir=$(dirname "$src")
    bookname=$(basename "$bookdir")
    link="$PCHQ_PKG/pieces/system/maps/$bookname"
    [ -e "$link" ] || [ -L "$link" ] && continue
    ln -s "$bookdir" "$link"
done
LINK="$PCHQ_PKG/pieces/system/maps/$BOOK"
SRC="$HOUSE/@.apps/$BOOK/$BOOK-pchq/pieces/system/maps/$BOOK"
if [ ! -d "$LINK" ] && [ -d "$SRC" ]; then
    ln -s "$SRC" "$LINK"
fi
if [ ! -d "$PCHQ_PKG/pieces/system/maps/$BOOK" ]; then
    BOOK=rpg-edit-hq
    PAGE=start
fi
sleep 8; printf 'CONFIRM_START_MAP:%s\nCONFIRM_SET_DESK:%s\n' "$BOOK" "$PAGE" >> "$INBOX"
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
