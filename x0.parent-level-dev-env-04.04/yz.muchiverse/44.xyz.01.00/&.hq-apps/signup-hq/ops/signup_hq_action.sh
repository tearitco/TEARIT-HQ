#!/bin/sh
# signup_hq_action.sh - write one request line to signup-hq's request
# file. Same invocation contract as colab_hai_action.sh:
#   <cli_io action="'.../signup_hq_action.sh' 'setid'"/>
#     -> setid <package_dir> <house_root> <typed_value>     (argc=4)
#   <cli_io action="'.../signup_hq_action.sh' 'setname'"/>
#     -> setname <package_dir> <house_root> <typed_value>   (argc=4)
#   <item  action="'.../signup_hq_action.sh' 'back'"/>
#     -> back <package_dir> <house_root>                     (argc=3)
#   <item  action="'.../signup_hq_action.sh' 'restart'"/>
#     -> restart <package_dir> <house_root>                  (argc=3)
set -e
VERB="$1"
case "$VERB" in
    setid|setname)
        [ $# -ge 4 ] || { echo "signup_hq_action.sh: $VERB needs 4 args, got $#" >&2; exit 1; }
        HOUSE_ROOT="$3"
        VALUE="$4"
        # strip surrounding whitespace; one line only
        VALUE="$(printf '%s' "$VALUE" | tr -d '\r\n' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
        REQ_LINE="$VERB:$VALUE"
        ;;
    back|restart)
        [ $# -ge 3 ] || { echo "signup_hq_action.sh: $VERB needs 3 args, got $#" >&2; exit 1; }
        HOUSE_ROOT="$3"
        REQ_LINE="$VERB:"
        ;;
    *)
        echo "signup_hq_action.sh: unknown verb '$VERB'" >&2
        exit 1
        ;;
esac
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "signup_hq_action.sh: bad house_root '$HOUSE_ROOT'" >&2; exit 1; }
mkdir -p "$HOUSE_ROOT/#.desktop/signup_hq"
printf '%s\n' "$REQ_LINE" > "$HOUSE_ROOT/#.desktop/signup_hq/request.txt"
