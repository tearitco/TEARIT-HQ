#!/bin/sh
# argv from the renderer: package_dir house_root typed_text
#
# REAL FIX 2026-09-24: the cli_io field is now embedded directly in
# each entity's own menu.chtpm (${PKG} in the action= there resolves
# to the renderer's g_package_dir, which IS the entity's own dir when
# invoked from that window) instead of a separate open_entity_cli.sh-
# launched window that read the target back out of a state file. $pkg
# (argv[1]) is the entity dir itself now - no indirection needed.
set -u
pkg="${1:-}"
house="${2:-}"
text="${3:-}"
ent="$pkg"
[ -n "$ent" ] && [ -d "$ent" ] || { echo "entity-cli: no target entity" >&2; exit 1; }
mkdir -p "$ent"
printf '%s\n' "$text" >> "$ent/cli_commands.txt"
set -- $text
cmd="${1:-}"
if [ "$cmd" = "range" ] && [ "$#" -ge 5 ]; then
  sh "$house/#.desktop/harnesses/two-facts/apply_range.sh" "$2" "$3" "$4" "$5" \
    "$house/#.desktop/db_hq_actors.state.txt" \
    "$house/&.widgits/db-hq/data/actors.pdl"
elif [ "$cmd" = "advance" ] && [ -n "${2:-}" ]; then
  printf 'which=%s\n' "$2" > "$house/#.desktop/harnesses/two-facts/weight.txt"
  sh "$house/#.desktop/harnesses/two-facts/advance.sh" \
    "$house/#.desktop/harnesses/two-facts/facts.txt" \
    "$house/#.desktop/harnesses/two-facts/weight.txt"
  printf 'which=page\n' > "$house/#.desktop/harnesses/two-facts/weight.txt"
fi
echo "COMMITTED $text"
