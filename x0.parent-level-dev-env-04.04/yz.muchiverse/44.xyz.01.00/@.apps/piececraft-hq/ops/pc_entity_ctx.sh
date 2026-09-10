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

# house root = the dir that holds *.monads
HOUSE=""
d="$ROOT"
while [ "$d" != "/" ]; do
    [ -d "$d/*.monads/*.livedesk-taskbar" ] && { HOUSE="$d"; break; }
    d=$(dirname "$d")
done
[ -n "$HOUSE" ] || HOUSE="$(cd "$ROOT/../../.." && pwd)"
BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
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

case "$KIND" in
    none)          HEADER="nothing selected";        VERBS="EXIT" ;;
    air|"")        HEADER="nothing here @ $SX,$SY,$SZ"; VERBS="PLACE EXIT" ;;
    hero)          HEADER="hero: ${ID:-hero_01}";     VERBS="INSPECT POSSESS EXIT" ;;
    tree)          HEADER="tree: ${ID:-?}";           VERBS="INSPECT COPY PASTE DELETE TOENTITY EXIT" ;;
    chicken|entity) HEADER="${KIND}: ${ID:-?}";       VERBS="INSPECT COPY PASTE DELETE EXIT" ;;
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
        printf '    <item label="%s" action="sh %s/append.sh %s %s %s %s %s %s %s %s"/>\n' \
            "$(label_for "$v")" "$PKG" "$v" "$SX" "$SY" "$SZ" "${ID:-_}" \
            "${KIND:-_}" "${GLYPH:-_}" "${TMPL:-_}"
    done
    printf '    <item label="Close" action="CLOSE"/>\n'
    printf '  </page>\n</window>\n'
} > "$PKG/ctx-menu.xhtpm"

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
setsid nohup "$BIN" "$HOUSE" "$PKG/ctx-menu.xhtpm" >/dev/null 2>&1 < /dev/null &
echo "pc_entity_ctx: menu up [$HEADER]  ->  $INBOX"
