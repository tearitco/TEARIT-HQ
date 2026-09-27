#!/bin/sh
# move_entity_to.sh <house_root> <entity_dir> <x> <y>
#
# REAL FIX 2026-09-27, direct instruction ("sh should only be used for
# launching .pal and ops, for the rest ... no logic"): this used to
# write desktop_pos.txt directly and do its own kill+relaunch loop in
# shell. That's now move_entity_init.+x's job (it also grid-snaps the
# target, same as the interactive placer path, for one consistent
# source of Move logic instead of two). This script is a pure
# launcher.
#
# Older history (still true): the real, reliable, NO-UI alternative
# for an automated caller (agent or test) that already knows the
# target REFERENCE px - no grid, no click, no keyboard, nothing that
# depends on real/synthetic input delivery (see HOUSE_CODE_PITFALLS.md
# #23 on why the interactive placer is unreliable for synthetic input).
set -eu
HOUSE="${1:-}"
ENT="${2:-}"
X="${3:-}"
Y="${4:-}"
[ -n "$HOUSE" ] && [ -d "$ENT" ] || { echo "move_entity_to.sh: need house_root and entity_dir" >&2; exit 1; }
case "$X" in *[!0-9-]*|"") echo "move_entity_to.sh: x must be a real integer" >&2; exit 1 ;; esac
case "$Y" in *[!0-9-]*|"") echo "move_entity_to.sh: y must be a real integer" >&2; exit 1 ;; esac

INIT="$HOUSE/&.widgits/entity-cli/ops/move_entity_init.+x"
[ -x "$INIT" ] || { echo "move_entity_to.sh: no move_entity_init op" >&2; exit 1; }
"$INIT" "$HOUSE" "$ENT" "$X" "$Y"
