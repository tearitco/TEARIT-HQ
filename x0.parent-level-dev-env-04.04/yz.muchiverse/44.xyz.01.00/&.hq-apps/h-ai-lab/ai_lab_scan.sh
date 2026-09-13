#!/bin/sh
# ai_lab_scan.sh - h-ai-lab's real backend. Reads the house-wide AI
# registry (&.widgits/ai-lab/ops/ai_registry.sh) + a "which row is
# selected" marker, publishes state/ui.txt - the same generic
# module+refresh+publish shape proc-mon already proves (mon_scan.sh/
# mon_refresh.sh), not a new architecture, and NO new per-project C in
# khtpm_core_render.c (house standing rule) - the sidebar/panel/detail
# split is done entirely with the already-generic <repeat>/${var}/
# action= vocabulary.
#
# H-AI-LAB-DESIGN.md Part 3, smallest first step: ONE sidebar row per
# registry entry, ONE embedded viewer panel (read-only) showing that
# entry's own real PATH content. No chat/retrain tiers yet - those are
# real, separate, later steps once this embed pattern is proven.
set -u
SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="${1:-${KHTPM_HOUSE:-}}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "ai_lab_scan.sh: need house_root" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
REG_SH="$HOUSE_ROOT/&.widgits/ai-lab/ops/ai_registry.sh"
SEL_FILE="$SELF_DIR/state/selected.txt"

MODE="${2:-publish}"

case "$MODE" in
select)
    # called from a sidebar row's own action=, argv3 = the NAME to
    # select. Plain, one-writer marker file - no lock needed, this
    # process is the only writer, publish (below) is the only reader.
    NAME="${3:-}"
    mkdir -p "$SELF_DIR/state"
    printf '%s\n' "$NAME" > "$SEL_FILE"
    sh "$0" "$HOUSE_ROOT" publish >/dev/null 2>&1 || true
    ;;

publish)
    OUT="${3:-$SELF_DIR/state/ui.txt}"
    mkdir -p "$SELF_DIR/state"
    tmp="$OUT.tmp.$$"
    SEL=""
    [ -f "$SEL_FILE" ] && SEL="$(cat "$SEL_FILE" 2>/dev/null)"

    REG_LINES="$(sh "$REG_SH" list "$HOUSE_ROOT" 2>/dev/null)"

    {
        echo "scan_time=$(date '+%H:%M:%S')"
        i=0
        det_name=""; det_kind=""; det_path=""; det_iface=""
        printf '%s\n' "$REG_LINES" | while IFS='|' read -r f1 f2 f3 f4; do
            [ -z "$f1" ] && continue
            name=${f1#NAME=}
            kind=${f2#KIND=}
            path=${f3#PATH=}
            iface=${f4#IFACE=}
            echo "inst_${i}_name=$name"
            echo "inst_${i}_kind=$kind"
            echo "inst_${i}_selected=$([ "$name" = "$SEL" ] && echo 1 || echo 0)"
            echo "inst_${i}_action=sh '$SELF_DIR/ai_lab_scan.sh' '$HOUSE_ROOT' select '$name'"
            i=$((i+1))
        done > "$tmp.rows"
        n=$(printf '%s\n' "$REG_LINES" | awk -F'|' '$1!=""' | wc -l | tr -d ' ')
        echo "n_instances=$n"
        cat "$tmp.rows"
        rm -f "$tmp.rows"
        [ "$n" -eq 0 ] && echo "no_instances=1" || echo "no_instances=0"

        # detail panel: only the currently-selected row's own real fields.
        # kh_load_vars() is strictly one KEY=VALUE per line (no multi-line
        # values) - so a real multi-line file can't be dumped raw here.
        # <text_area content="${var}"> is the one already-generic element
        # with a real, existing "\n" (literal backslash-n) -> newline
        # conversion applied to its content= attribute (apply_attr(),
        # khtpm_core_render.c ~line 1005) - so the real fix is encoding
        # the file's content into ONE line with literal \n escapes, not
        # inventing a new multi-line var mechanism.
        if [ -n "$SEL" ]; then
            SEL_LINE="$(printf '%s\n' "$REG_LINES" | awk -F'|' -v n="NAME=$SEL" '$1==n')"
            if [ -n "$SEL_LINE" ]; then
                d_kind=$(printf '%s' "$SEL_LINE" | awk -F'|' '{print $2}' | sed 's/^KIND=//')
                d_path=$(printf '%s' "$SEL_LINE" | awk -F'|' '{print $3}' | sed 's/^PATH=//')
                d_iface=$(printf '%s' "$SEL_LINE" | awk -F'|' '{print $4}' | sed 's/^IFACE=//')
                echo "detail_name=$SEL"
                echo "detail_kind=$d_kind"
                echo "detail_path=$d_path"
                echo "detail_iface=$d_iface"
                echo "has_detail=1"
                echo "no_detail=0"
                if [ -f "$d_path" ]; then
                    DETAIL_RAW=$(head -c 4000 "$d_path")
                elif [ -d "$d_path" ]; then
                    DETAIL_RAW="(directory - real viewer for this KIND not built yet)
$(ls -1 "$d_path" | head -30)"
                else
                    DETAIL_RAW="(PATH not readable: $d_path)"
                fi
                DETAIL_ESC=$(printf '%s' "$DETAIL_RAW" | sed ':a;N;$!ba;s/\n/\\n/g')
                # printf %s, NOT echo - dash's builtin echo interprets a
                # literal \n in its argument as XSI-echo escape and turns
                # it back into a real newline, silently undoing the sed
                # escaping above and corrupting the one-line-per-KEY=VALUE
                # vars format every other line here depends on.
                printf '%s\n' "detail_text=$DETAIL_ESC"
            else
                echo "has_detail=0"
                echo "no_detail=1"
                echo "detail_text=(selected instance no longer in the registry)"
            fi
        else
            echo "has_detail=0"
            echo "no_detail=1"
            echo "detail_text=(select an instance from the list)"
        fi
    } > "$tmp"
    mv -f "$tmp" "$OUT"
    ;;

*)
    echo "usage: ai_lab_scan.sh <house_root> [select <name>|publish [out_path]]" >&2
    exit 2
    ;;
esac
