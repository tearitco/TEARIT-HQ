#!/bin/sh
# export_hq_action.sh - writes one request line to export-hq's request
# file. Same real invocation contract as signup_hq_action.sh:
#   <item   action="'.../export_hq_action.sh' 'toggle' '${c.id}'"/>
#     -> toggle <id> <package_dir> <house_root>          (item: extra
#        literal args come BEFORE the renderer's own auto-appended
#        package_dir/house_root - see khtpm_core_render.c's own
#        dispatch_action())
#   <cli_io action="'.../export_hq_action.sh' 'setdest'"/>
#     -> setdest <package_dir> <house_root> <typed_value>  (cli_io:
#        typed_value comes AFTER the auto-appended pair - see
#        signup_hq_action.sh's own header comment for the same real
#        ordering)
#   <item   action="'.../export_hq_action.sh' 'package'"/>
#     -> package <package_dir> <house_root>
set -e
VERB="$1"
case "$VERB" in
    toggle)
        # item action="'...' 'toggle' '${c.id}'" -> extra literal args
        # come BEFORE the renderer's own auto-appended package_dir/
        # house_root pair: $1=toggle $2=<id> $3=package_dir $4=house_root.
        [ $# -ge 4 ] || { echo "export_hq_action.sh: toggle needs 4 args, got $#" >&2; exit 1; }
        HOUSE_ROOT="$4"
        REQ_LINE="toggle:$2"
        ;;
    setdest)
        [ $# -ge 4 ] || { echo "export_hq_action.sh: setdest needs 4 args, got $#" >&2; exit 1; }
        HOUSE_ROOT="$3"
        VALUE="$4"
        VALUE="$(printf '%s' "$VALUE" | tr -d '\r\n' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
        REQ_LINE="setdest:$VALUE"
        ;;
    package|rescan)
        [ $# -ge 3 ] || { echo "export_hq_action.sh: $VERB needs 3 args, got $#" >&2; exit 1; }
        HOUSE_ROOT="$3"
        REQ_LINE="$VERB:"
        ;;
    *)
        echo "export_hq_action.sh: unknown verb '$VERB'" >&2
        exit 1
        ;;
esac
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "export_hq_action.sh: bad house_root '$HOUSE_ROOT'" >&2; exit 1; }
mkdir -p "$HOUSE_ROOT/#.desktop/export_hq"
printf '%s\n' "$REQ_LINE" > "$HOUSE_ROOT/#.desktop/export_hq/request.txt"
