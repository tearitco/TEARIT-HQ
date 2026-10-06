#!/bin/bash
# vm_rv_li_input.sh — append one human-typed line to vm-rv-li's input relay.
#
# Real invocation shape (house §9 / §13): the generic renderer fires a
# <cli_io action="'...ops/vm_rv_li_input.sh' 'type'"> on Enter by
# appending "<package_dir> <house_root> <typed_value>" after the action's
# own args, so we get:
#   vm_rv_li_input.sh type <package_dir> <house_root> <typed_value>   (argc=4)
# A live human typing and an external agent writing the same relay file
# produce byte-identical input to the guest emulator.
set -u
VERB="${1:-}"
case "$VERB" in
    type)
        [ $# -ge 4 ] || { echo "vm_rv_li_input.sh: type needs <pkg> <house> <text>" >&2; exit 1; }
        HOUSE_ROOT="$3"
        TEXT="$4"
        ;;
    paste)
        [ $# -ge 4 ] || { echo "vm_rv_li_input.sh: paste needs <pkg> <house> <text>" >&2; exit 1; }
        HOUSE_ROOT="$3"
        TEXT="$4"
        ;;
    *)
        echo "vm_rv_li_input.sh: unknown verb '$VERB'" >&2
        exit 1
        ;;
esac

if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "vm_rv_li_input.sh: bad house_root" >&2
    exit 1
fi

FILE="$HOUSE_ROOT/#.desktop/vm_rv_li/human_input.txt"
mkdir -p "$(dirname "$FILE")"

# real append-only queue; vm_rv_li_manager drains + truncates each tick
case "$VERB" in
    type)   printf 'type:%s\n' "$TEXT"   >> "$FILE" ;;
    paste)  printf 'type:%s'   "$TEXT"   >> "$FILE" ;;
esac
