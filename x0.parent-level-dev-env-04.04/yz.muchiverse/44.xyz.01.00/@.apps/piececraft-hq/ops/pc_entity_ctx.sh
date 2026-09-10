#!/bin/sh
# pc_entity_ctx.sh <project_root>
#
# MILESTONE E slice 1 (PCHQ-ENTITY-MENU-AND-TASKBAR-DESIGN.md).
# Reads pieces/display/pick.txt (published every frame by bv_render_3d,
# milestone D), builds a context-menu package dir, and launches the
# SHARED khtpm_core_render on it - no pc-hq-private menu renderer (§0).
# Each menu row's action= appends
#     CTX_<VERB> <x> <y> <z> <id>
# to the game inbox (pieces/system/widget_cmds/inbox.txt), which
# pc_menu_input.c already drains and dispatches - reusing the existing
# key->inbox->dispatch pipe, no new plumbing.
#
# Standalone-testable: write a pick.txt by hand, run this, click a row,
# tail the inbox. Nothing is wired into the live key path yet (slice 2).
set -u
ROOT="${1:-}"
[ -n "$ROOT" ] && [ -d "$ROOT" ] || { echo "pc_entity_ctx: need project_root as argv[1]" >&2; exit 1; }
ROOT="$(cd "$ROOT" && pwd)"

# house root = up from 44.xyz.01.00/@.apps/piececraft-hq  (ROOT may be a
# session dir; walk up to the dir that holds *.monads)
HOUSE=""
d="$ROOT"
while [ "$d" != "/" ]; do
    [ -d "$d/*.monads/*.livedesk-taskbar" ] && { HOUSE="$d"; break; }
    d=$(dirname "$d")
done
[ -n "$HOUSE" ] || HOUSE="$(cd "$ROOT/../../.." && pwd)"
BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
[ -x "$BIN" ] || { echo "pc_entity_ctx: missing renderer $BIN" >&2; exit 1; }

PICK="$ROOT/pieces/display/pick.txt"
[ -f "$PICK" ] || { echo "pc_entity_ctx: no pick.txt yet ($PICK) - move the selector first" >&2; exit 1; }

# parse pick.txt (key=value)
kv() { sed -n "s/^$1=//p" "$PICK" | head -1; }
SX=$(kv sel_x); SY=$(kv sel_y); SZ=$(kv sel_z)
KIND=$(kv kind); ID=$(kv id); TMPL=$(kv template); GLYPH=$(kv glyph)
: "${SX:=0}" "${SY:=0}" "${SZ:=0}" "${KIND:=air}" "${ID:=}"

LABEL="$KIND"
[ -n "$ID" ] && LABEL="$KIND: $ID"
[ "$KIND" = voxel ] && LABEL="voxel '$GLYPH'"

PKG="$ROOT/pieces/display/ctx_menu"
mkdir -p "$PKG/state"
INBOX="$ROOT/pieces/system/widget_cmds/inbox.txt"
mkdir -p "$(dirname "$INBOX")"

# one row per verb. action= is a shell command the renderer runs with
# '<pkg>' '<house>' appended; we ignore those and append to the inbox.
row() { # <verb> <label>
    printf '      <item id="v_%s" class="data-item" label="%s" action="sh %s/append.sh %s %s %s %s %s"/>\n' \
        "$1" "$2" "$PKG" "$1" "$SX" "$SY" "$SZ" "${ID:-_}"
}

{
cat <<XHTPM
<window label="ctx: $LABEL" class="pchq-ctx database-window" vars="state/ui.txt">
  <page name="main">
    <sidebar id="sidebar" class="sidebar">
      <text label="$LABEL  @ $SX,$SY,$SZ" class="block-title"/>
XHTPM
case "$KIND" in
  hero)            row INSPECT Inspect; row POSSESS Possess ;;
  chicken|entity)  row INSPECT Inspect; row COPY Copy; row PASTE Paste; row DELETE Delete ;;
  tree)            row INSPECT Inspect; row COPY Copy; row PASTE Paste; row DELETE Delete; row TOENTITY "Convert to entity" ;;
  voxel)           row INSPECT Inspect; row COPY Copy; row PASTE Paste; row DELETE "Mine (delete)"; row PLACE "Place..." ;;
  *)               row INSPECT Inspect; row PLACE "Place..." ;;
esac
cat <<XHTPM
      <item id="v_EXIT" class="data-item" label="Exit" action="sh $PKG/append.sh EXIT $SX $SY $SZ ${ID:-_}"/>
    </sidebar>
    <panel id="detail" class="settings-block">
      <text label="pick.txt" class="block-title"/>
      <text label="kind=$KIND  id=${ID:-(none)}  template=${TMPL:-(none)}"/>
      <text label="cell = $SX, $SY, $SZ"/>
    </panel>
  </page>
</window>
XHTPM
} > "$PKG/ctx-menu.xhtpm"

# the row action helper: append one verb line to the inbox, then close
# the menu window (its own pid file).
cat > "$PKG/append.sh" <<APP
#!/bin/sh
V="\$1"; X="\$2"; Y="\$3"; Z="\$4"; ID="\$5"
[ "\$ID" = _ ] && ID=""
[ "\$V" != EXIT ] && printf 'CTX_%s %s %s %s %s\n' "\$V" "\$X" "\$Y" "\$Z" "\$ID" >> "$INBOX"
# close this menu window
for p in \$(pgrep -f "khtpm_core_render.+x .*ctx-menu\\.xhtpm" 2>/dev/null); do
    [ "\$(cat /proc/\$p/comm 2>/dev/null)" = khtpm_core_rend ] && kill "\$p" 2>/dev/null
done
APP
chmod +x "$PKG/append.sh"

# single instance
for p in $(pgrep -f "khtpm_core_render.+x .*ctx-menu\.xhtpm" 2>/dev/null); do
    [ "$(cat /proc/$p/comm 2>/dev/null)" = khtpm_core_rend ] && kill "$p" 2>/dev/null
done
setsid nohup "$BIN" "$HOUSE" "$PKG/ctx-menu.xhtpm" >/dev/null 2>&1 < /dev/null &
echo "pc_entity_ctx: menu up for [$LABEL] @ $SX,$SY,$SZ  ->  $INBOX"
