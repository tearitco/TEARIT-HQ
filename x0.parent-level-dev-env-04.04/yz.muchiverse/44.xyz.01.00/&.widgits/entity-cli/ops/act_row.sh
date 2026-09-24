#!/bin/sh
# argv: command entity_dir
set -u
cmd="${1:-}"
ent="${2:-}"
house=$(cd "$(dirname "$0")/../../.." && pwd)
printf '%s\n' "$cmd" >> "$ent/cli_commands.txt"
case "$cmd" in
  attack)
    sh "$house/#.desktop/harnesses/two-facts/apply_range.sh" a1 b2 2 6 \
      "$house/#.desktop/db_hq_actors.state.txt" \
      "$house/&.widgits/db-hq/data/actors.pdl"
    ;;
  move|use)
    echo "$cmd recorded"
    ;;
  *)
    echo "act: $cmd"
    ;;
esac
