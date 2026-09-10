#!/bin/sh
# pc_toggle_pdl_opt.sh <pdl_path> <opt_name>
# Flip an  "OPT | <name> | <0|1>"  line 0<->1 in a pipe-delimited .pdl,
# creating the file / line if missing. Used by pchq-board's
# Menu > Entities bar toggle.
set -u
PDL="${1:-}"; NAME="${2:-}"
[ -n "$PDL" ] && [ -n "$NAME" ] || { echo "usage: pc_toggle_pdl_opt.sh <pdl> <name>" >&2; exit 1; }
mkdir -p "$(dirname "$PDL")"
[ -f "$PDL" ] || : > "$PDL"
cur=$(awk -F'|' -v n="$NAME" '{gsub(/ /,"",$2)} $2==n {gsub(/ /,"",$3); print $3; exit}' "$PDL")
new=1; [ "${cur:-0}" = "1" ] && new=0
if grep -qE "\|[[:space:]]*$NAME[[:space:]]*\|" "$PDL"; then
    awk -F'|' -v n="$NAME" -v v="$new" 'BEGIN{OFS="|"}
        { k=$2; gsub(/ /,"",k) }
        k==n { print "OPT        | " n " | " v; next }
        { print }' "$PDL" > "$PDL.tmp" && mv "$PDL.tmp" "$PDL"
else
    printf 'OPT        | %s | %s\n' "$NAME" "$new" >> "$PDL"
fi
echo "pc_toggle_pdl_opt: $NAME -> $new"
