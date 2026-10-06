#!/bin/sh
# pc_entity_ctx.sh <project_root>
#
# MILESTONE E (PCHQ-ENTITY-MENU-AND-TASKBAR-DESIGN.md).
# Reads pieces/display/pick.txt (published every frame by bv_render_3d,
# milestone D) and launches the SHARED khtpm_core_render on a generated
# context menu - no pc-hq-private menu renderer (§0). Each row's action=
# appends "CTX_<VERB> <x> <y> <z> <id>" to the game inbox, which
# pc_menu_input.c drains and dispatches.
#
# ALWAYS opens a window - even with no pick.txt or an empty cell - so the
# key -> menu chain is testable on its own. Every run appends one line to
# pieces/display/ctx_menu.log (tail that to confirm 'm' fired).
set -u
ROOT="${1:-}"
[ -n "$ROOT" ] && [ -d "$ROOT" ] || { echo "pc_entity_ctx: need project_root as argv[1]" >&2; exit 1; }
ROOT="$(cd "$ROOT" && pwd)"

LOG="$ROOT/pieces/display/ctx_menu.log"
mkdir -p "$ROOT/pieces/display"

# house root = the dir that holds _.monads
HOUSE=""
d="$ROOT"
while [ "$d" != "/" ]; do
    [ -d "$d/_.monads/_.livedesk-taskbar" ] && { HOUSE="$d"; break; }
    d=$(dirname "$d")
done
[ -n "$HOUSE" ] || HOUSE="$(cd "$ROOT/../../.." && pwd)"
BIN="$HOUSE/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
if [ ! -x "$BIN" ]; then
    echo "$(date '+%H:%M:%S') ERROR no renderer at $BIN" >> "$LOG"
    echo "pc_entity_ctx: missing renderer $BIN" >&2
    exit 1
fi

PICK="$ROOT/pieces/display/pick.txt"
SX=0; SY=0; SZ=0; KIND=none; ID=""; TMPL=""; GLYPH=""
NOTE=""
if [ $# -ge 6 ]; then
    # explicit target: pc_entity_ctx.sh <root> <x> <y> <z> <kind> <id>
    # (footer entities-bar cell click - no pick.txt round-trip)
    SX="$2"; SY="$3"; SZ="$4"; KIND="$5"; ID="$6"; TMPL="$6"
    [ "$ID" = _ ] && ID=""
elif [ -f "$PICK" ]; then
    kv() { sed -n "s/^$1=//p" "$PICK" | head -1; }
    SX=$(kv sel_x); SY=$(kv sel_y); SZ=$(kv sel_z)
    KIND=$(kv kind); ID=$(kv id); TMPL=$(kv template); GLYPH=$(kv glyph)
    : "${SX:=0}" "${SY:=0}" "${SZ:=0}" "${KIND:=air}" "${ID:=}"
else
    NOTE="no pick.txt - is milestone D built into this board session?"
fi

echo "$(date '+%H:%M:%S') open  kind=$KIND id=${ID:-.} cell=$SX,$SY,$SZ  $NOTE" >> "$LOG"

# DESK ENTITIES GET THEIR OWN MENU, AUTOMATICALLY (2026-10-05, direct
# instruction: cursword / the terumons / any page entity must have "the exact
# same" context menu in pc-hq as on the desktop). The desk's rule
# (khtpm_entity.c launch_khtpm_menu): if <entity dir>/menu.chtpm exists, open
# THAT file with the shared khtpm_core_render at the click position. Do the
# same here - no per-entity verb list, no copy of the menu: whatever the entity
# defines (Move via Act, Chat, Play, Bookmarks, Events, Inventory, Cli-io, ...)
# is what shows. A page row's path is <house>/xyzfs/users/*/home/livedesk/pals/<name>
# (or #.desktop/entities/<name>), found here by name. pc-hq's own pieces
# (hero_01, trees, chicken, voxels) have no such dir and keep the generated
# menu below.
DESK_MENU=""
if [ -n "$ID" ]; then
    for d in "$HOUSE"/xyzfs/users/*/home/livedesk/pals/"$ID" "$HOUSE/#.desktop/entities/$ID"; do
        [ -f "$d/menu.chtpm" ] && { DESK_MENU="$d/menu.chtpm"; break; }
    done
fi
if [ -n "$DESK_MENU" ]; then
    PIDF="$ROOT/pieces/display/ctx_menu_desk.pid"
    OLD=$(cat "$PIDF" 2>/dev/null)
    case "$OLD" in ""|*[!0-9]*) ;; *) [ "$(cat /proc/$OLD/comm 2>/dev/null)" = khtpm_core_rend ] && kill "$OLD" 2>/dev/null ;; esac
    # also drop a stale generated pc-hq menu, as the generated path does
    for p in $(pgrep -f "khtpm_core_render.+x .*ctx-menu\.xhtpm" 2>/dev/null); do
        [ "$(cat /proc/$p/comm 2>/dev/null)" = khtpm_core_rend ] && kill "$p" 2>/dev/null
    done
    if [ -n "${MENU_X:-}" ] && [ -n "${MENU_Y:-}" ]; then
        setsid nohup "$BIN" "$HOUSE" "$DESK_MENU" "$MENU_X" "$MENU_Y" >/dev/null 2>&1 < /dev/null &
    else
        setsid nohup "$BIN" "$HOUSE" "$DESK_MENU" >/dev/null 2>&1 < /dev/null &
    fi
    echo $! > "$PIDF"
    echo "$(date '+%H:%M:%S') desk menu  $DESK_MENU" >> "$LOG"
    echo "pc_entity_ctx: desk entity menu up [$ID]  ->  $DESK_MENU"
    exit 0
fi

case "$KIND" in
    none)          HEADER="nothing selected";        VERBS="EXIT" ;;
    air|"")        HEADER="nothing here @ $SX,$SY,$SZ"; VERBS="PLACE EXIT" ;;
    hero)          HEADER="hero: ${ID:-hero_01}";     VERBS="INSPECT POSSESS ACT STOP EVENTS INVENTORY DIR EXIT" ;;
    tree)          HEADER="tree: ${ID:-?}";           VERBS="INSPECT COPY PASTE DELETE TOENTITY ACT STOP EVENTS INVENTORY DIR EXIT" ;;
    chicken|entity) HEADER="${KIND}: ${ID:-?}";       VERBS="INSPECT COPY PASTE DELETE ACT STOP EVENTS INVENTORY DIR EXIT" ;;
    xelector)      HEADER="xelector: ${ID:-xelector_01}"; VERBS="INSPECT DIR EXIT" ;;
    voxel)         HEADER="voxel '$GLYPH' @ $SX,$SY,$SZ"; VERBS="INSPECT COPY PASTE DELETE PLACE EXIT" ;;
    *)             HEADER="$KIND: ${ID:-?}";          VERBS="INSPECT EXIT" ;;
esac

PKG="$ROOT/pieces/display/ctx_menu"
mkdir -p "$PKG"
INBOX="$ROOT/pieces/system/widget_cmds/inbox.txt"
mkdir -p "$(dirname "$INBOX")"

label_for() {
    case "$1" in
        INSPECT) echo "Inspect" ;; COPY) echo "Copy" ;; PASTE) echo "Paste" ;;
        DELETE) [ "$KIND" = voxel ] && echo "Mine (delete)" || echo "Delete" ;;
        PLACE) echo "Place..." ;; POSSESS) echo "Possess" ;;
        TOENTITY) echo "Convert to entity" ;; EXIT) echo "Exit" ;;
        # REAL FIX 2026-09-30, direct instruction ("when i click theyre
        # entity i expect to see same kind of context menu that the desk
        # entities get, nothing different") - same three verbs a desk
        # pal's meta.pdl already ships (Events (hq)/Inventory/Dir), only
        # added to entity-like kinds above (hero/tree/chicken/entity),
        # never voxel/air which have no real pieces/<id> dir.
        EVENTS) echo "Events (hq)" ;; INVENTORY) echo "Inventory" ;;
        DIR) echo "Dir" ;;
        # REAL, NEW 2026-09-29, direct instruction ("give it all the
        # context options asa has... act and its sub options") - same
        # real Play/Stop METHOD rows a desk pal's meta.pdl already has
        # (asa/ava's own: Play runs <ent_dir>/event_pkg/pages/page_1/
        # event.pal via prisc+x if it exists, else no-ops; Stop is a
        # real, deliberate `void` stub house-wide - no pal actually
        # implements a real Stop yet, this matches that exactly).
        ACT) echo "Act" ;; STOP) echo "Stop" ;;
        *) echo "$1" ;;
    esac
}

# SAME shape/look as a desktop entity menu (#.desktop/entities/*/menu.chtpm):
# class="entity-menu", flat <item> rows straight under <page>, no
# sidebar/panel, styled by the shared entity_menu_default.css. Popup
# mode auto-closes on an action; the explicit Close row + append.sh's
# kill are belt-and-suspenders.
{
    printf '<window class="entity-menu">\n'
    printf '  <page name="main">\n'
    printf '    <text label="%s" />\n' "$HEADER"
    [ -n "$NOTE" ] && printf '    <text label="%s" />\n' "$NOTE"
    for v in $VERBS; do
        [ "$v" = EXIT ] && continue
        # REAL FIX 2026-09-29, direct live report ("act submenu doesn't
        # open sub context menu like it does on desk") - a desk entity's
        # own menu.chtpm wires Act straight to entity-cli/
        # open_entity_act.sh as its action= (a real, synchronous UI
        # launch the shared renderer's own dispatch_action() runs
        # directly - see that file's own header: it opens a NEW
        # khtpm_core_render window built from the entity's skills.pdl,
        # e.g. "move/use/attack/back"). Every OTHER verb here goes
        # through append.sh -> the game's own CTX_<VERB> inbox because
        # it's a real game-state mutation pc_menu_input.c's tick loop
        # must own - Act is not one of those, it's a pure UI launch, so
        # routing it through that same async inbox could only ever run a
        # background command, never pop a window. Same real distinction,
        # not an inconsistency: STOP still routes through the inbox
        # (kept a real, dispatchable game verb, matching asa/ava's own
        # meta.pdl shape) even though it's a stub today.
        if [ "$v" = ACT ]; then
            ENT_DIR="$ROOT/pieces/${ID:-$KIND}"
            mkdir -p "$ENT_DIR"
            sh "$HOUSE/&.widgits/entity-cli/act_menu_row.sh" "$ENT_DIR" "$HOUSE" "$(label_for "$v")"
            continue
        fi
        printf '    <item label="%s" action="sh %s/append.sh %s %s %s %s %s %s %s %s"/>\n' \
            "$(label_for "$v")" "$PKG" "$v" "$SX" "$SY" "$SZ" "${ID:-_}" \
            "${KIND:-_}" "${GLYPH:-_}" "${TMPL:-_}"
    done
    printf '    <item label="Close" action="CLOSE"/>\n'
    # Same cli_io field a desk entity's menu.chtpm embeds (action = the
    # shared entity-cli commit script). cli.sh (below) supplies the entity
    # dir the renderer cannot know: it passes this menu's own package dir.
    # Entity-like kinds only - a bare voxel/air cell has no pieces/<id>.
    [ -n "${ID:-}" ] && printf '    <cli_io id="cmd" target_id="cmd" label="Cli-io: " action="%s/cli.sh"/>\n' "$PKG"
    printf '  </page>\n</window>\n'
} > "$PKG/ctx-menu.xhtpm"

# argv from the renderer: package_dir house_root typed_text. Forwarded to the
# SHARED entity_cli_commit.sh with the real entity dir as its target.
if [ -n "${ID:-}" ]; then
mkdir -p "$ROOT/pieces/$ID"
cat > "$PKG/cli.sh" <<CLI
#!/bin/sh
exec sh "$HOUSE/&.widgits/entity-cli/ops/entity_cli_commit.sh" "$ROOT/pieces/$ID" "\$2" "\$3"
CLI
chmod +x "$PKG/cli.sh"
fi

cat > "$PKG/append.sh" <<APP
#!/bin/sh
# CTX_<VERB> x y z id kind glyph template   ('_' = empty field)
V="\$1"; X="\$2"; Y="\$3"; Z="\$4"; ID="\$5"; KIND="\$6"; GLYPH="\$7"; TMPL="\$8"
[ "\$V" != EXIT ] && printf 'CTX_%s %s %s %s %s %s %s %s\n' \
    "\$V" "\$X" "\$Y" "\$Z" "\${ID:-_}" "\${KIND:-_}" "\${GLYPH:-_}" "\${TMPL:-_}" >> "$INBOX"
echo "\$(date '+%H:%M:%S') click \$V \$X,\$Y,\$Z \${ID} \${KIND}" >> "$LOG"
for p in \$(pgrep -f "khtpm_core_render.+x .*ctx-menu\\.xhtpm" 2>/dev/null); do
    [ "\$(cat /proc/\$p/comm 2>/dev/null)" = khtpm_core_rend ] && kill "\$p" 2>/dev/null
done
APP
chmod +x "$PKG/append.sh"

for p in $(pgrep -f "khtpm_core_render.+x .*ctx-menu\.xhtpm" 2>/dev/null); do
    [ "$(cat /proc/$p/comm 2>/dev/null)" = khtpm_core_rend ] && kill "$p" 2>/dev/null
done
if [ -n "${MENU_X:-}" ] && [ -n "${MENU_Y:-}" ]; then
    setsid nohup "$BIN" "$HOUSE" "$PKG/ctx-menu.xhtpm" "$MENU_X" "$MENU_Y" >/dev/null 2>&1 < /dev/null &
else
    setsid nohup "$BIN" "$HOUSE" "$PKG/ctx-menu.xhtpm" >/dev/null 2>&1 < /dev/null &
fi
echo "pc_entity_ctx: menu up [$HEADER]  ->  $INBOX"
