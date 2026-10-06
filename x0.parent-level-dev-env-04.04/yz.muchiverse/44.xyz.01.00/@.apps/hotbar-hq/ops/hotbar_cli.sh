#!/bin/sh
# hotbar_cli.sh <mode> <package_dir> <house_root> <typed text...>
# The hotbar's cli_io commit: send the typed line to the CURRENT HOLDER's entity CLI
# (the same entity_cli_commit.sh an entity menu's Cli-io field calls), so what you type in
# the hotbar goes to whichever entity's inventory the hotbar is showing.
MODE="$1"; PKG="$2"; HOUSE="$3"; shift 3
H="$(cat "$PKG/state/$MODE/holder.txt" 2>/dev/null)"
[ -n "$H" ] && [ -d "$H" ] || { echo "hotbar_cli: no holder" >&2; exit 1; }
exec sh "$HOUSE/&.widgits/entity-cli/ops/entity_cli_commit.sh" "$H" "$HOUSE" "$*"
