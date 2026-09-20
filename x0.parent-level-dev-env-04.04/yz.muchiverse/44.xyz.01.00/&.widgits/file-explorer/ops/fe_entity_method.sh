#!/bin/sh
# fe_entity_method.sh <entity_dir> <method_label> [<popup_pkg> <house_root> appended by the popup dispatcher]
# Runs one METHOD row of <entity_dir>/meta.pdl exactly as the desktop right-click
# does: `<action> '<entity_dir>' '<house_root>'`, so $0 = the entity dir, $1 = house.
# Used by the File Explorer's right-click menu on a pal shown in an Inventory.
ENT="$1"; LABEL="$2"; HOUSE="$4"
[ -d "$ENT" ] && [ -f "$ENT/meta.pdl" ] || exit 1
ACT="$(awk -F'|' -v L="$LABEL" '
  /^METHOD/ { lab=$2; gsub(/^ +| +$/, "", lab);
    if (lab == L) { a=$0; sub(/^[^|]*\|[^|]*\|[ ]*/, "", a); sub(/[ \r]+$/, "", a); print a; exit } }' "$ENT/meta.pdl")"
[ -n "$ACT" ] || exit 1
cd "$ENT" 2>/dev/null || true
eval "$ACT '$ENT' '$HOUSE' >/dev/null 2>&1 &"
