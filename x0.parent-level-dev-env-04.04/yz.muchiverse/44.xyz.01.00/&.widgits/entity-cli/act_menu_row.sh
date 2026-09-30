#!/bin/sh
# act_menu_row.sh <entity_dir> <house_root> [label]
#
# Emits one correctly-escaped <item> row that wires an entity-menu
# button straight to the shared Act submenu (open_entity_act.sh) -
# real, deliberate STANDARDIZATION, 2026-09-29, direct instruction
# ("can we standardize that behavior? its gonna be used a lot in both
# desk and pc-hq"). Before this, every caller hand-rolled its own copy
# of this action= string (XML attribute quoting nested inside sh -c
# quoting nested inside a shell positional-arg convention) - asa's own
# desk menu.chtpm has one such hand-typed copy (a MANUAL edit, not even
# sourced from its own meta.pdl - see that file's own header warning
# it's lost on regen), and pc-hq's pc_entity_ctx.sh briefly grew a
# second, independently-broken copy (wrong printf escape - \x27 is a
# bash-ism, not POSIX printf) before this helper existed. One real
# place this string is built now, not N hand-typed copies drifting
# apart.
#
# dispatch_action() (khtpm_core_render.c) always runs an action= string
# as `<action> '<package_dir>' '<house_root>' &` via system() - for a
# real desk pal window, package_dir naturally IS that pal's own folder,
# so a plain $0/$1 convention (asa's own Play/Dir/etc METHOD rows) is
# enough. A GENERATED popup window (pc-hq's ctx-menu, any future
# shared-popup caller) has its own unrelated package_dir (e.g.
# pieces/display/ctx_menu) - $0/$1 would point at the wrong directory
# there, so this helper bakes the real entity_dir/house_root in as
# literal text at generation time instead of relying on that
# convention. Works for both calling shapes either way.
set -u
ENT="${1:?act_menu_row.sh: entity_dir required}"
HOUSE="${2:?act_menu_row.sh: house_root required}"
LABEL="${3:-Act}"
printf '    <item label="%s" action="sh -c \047exec &quot;%s/&.widgits/entity-cli/open_entity_act.sh&quot; &quot;%s&quot; &quot;%s&quot; &quot;${WIN_X}&quot; &quot;${WIN_Y}&quot;\047"/>\n' \
    "$LABEL" "$HOUSE" "$ENT" "$HOUSE"
