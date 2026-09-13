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

# REAL, generic cli_io submit convention (khtpm_core_render.c
# default_cli_io_run_action(), ~line 6447): a cli_io's own action= is
# invoked as `<action> '<package_dir>' '<house_root>' '<typed_value>'
# &` - a DIFFERENT argv order than every other action= in this file
# (which are plain, hand-built `sh ai_lab_scan.sh <house_root> <mode>
# ...` strings). Handled as its own branch, first, before the normal
# house_root-first parsing below assumes argv1 is a path.
if [ "${1:-}" = "create_state" ]; then
    PKG_ARG="${2:-}"
    HOUSE_ARG="${3:-}"
    SPEC_VAL="${4:-}"
    [ -n "$PKG_ARG" ] && [ -n "$HOUSE_ARG" ] || { echo "ai_lab_scan.sh create_state: missing pkg/house" >&2; exit 1; }
    REG_SH="$HOUSE_ARG/&.widgits/ai-lab/ops/ai_registry.sh"
    SEL_FILE="$PKG_ARG/state/selected.txt"
    RESULT_FILE="$PKG_ARG/state/last_action_result.txt"
    SEL=""
    [ -f "$SEL_FILE" ] && SEL="$(cat "$SEL_FILE" 2>/dev/null)"
    # ONE field, "name" or "name|NEXT1,NEXT2" - split here, never a
    # second cli_io (see this file's own real-bug writeup above).
    NAME_VAL="${SPEC_VAL%%|*}"
    case "$SPEC_VAL" in
        *\|*) NEXT_VAL="${SPEC_VAL#*|}" ;;
        *) NEXT_VAL="" ;;
    esac
    if [ -z "$SEL" ]; then
        echo "no instance selected - select an fsm instance first" > "$RESULT_FILE"
    elif [ -z "$NAME_VAL" ]; then
        echo "type a state name (optionally name|NEXT1,NEXT2) first" > "$RESULT_FILE"
    else
        SEL_LINE="$(sh "$REG_SH" list "$HOUSE_ARG" 2>/dev/null | awk -F'|' -v n="NAME=$SEL" '$1==n')"
        d_kind=$(printf '%s' "$SEL_LINE" | awk -F'|' '{print $2}' | sed 's/^KIND=//')
        d_path=$(printf '%s' "$SEL_LINE" | awk -F'|' '{print $3}' | sed 's/^PATH=//')
        if [ "$d_kind" != "fsm" ]; then
            echo "'$SEL' is not an fsm instance - can't add a STATE row" > "$RESULT_FILE"
        elif [ ! -f "$d_path" ]; then
            echo "'$SEL' has no real table file at $d_path" > "$RESULT_FILE"
        elif awk -F'|' -v n="$NAME_VAL" '$1 ~ /^STATE / { s=$2; gsub(/^ +| +$/,"",s); if (s==n) f=1 } END{exit !f}' "$d_path"; then
            echo "state '$NAME_VAL' already exists in $SEL - pick a different name" > "$RESULT_FILE"
        else
            printf 'STATE | %-14s | NEXT=%s\n' "$NAME_VAL" "$NEXT_VAL" >> "$d_path"
            echo "added state '$NAME_VAL' (NEXT=${NEXT_VAL:-none, terminal}) to $SEL" > "$RESULT_FILE"
        fi
    fi
    sh "$0" "$HOUSE_ARG" publish "$PKG_ARG/state/ui.txt" >/dev/null 2>&1 || true
    exit 0
fi

HOUSE_ROOT="${1:-${KHTPM_HOUSE:-}}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "ai_lab_scan.sh: need house_root" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
REG_SH="$HOUSE_ROOT/&.widgits/ai-lab/ops/ai_registry.sh"
SEL_FILE="$SELF_DIR/state/selected.txt"
RESULT_FILE="$SELF_DIR/state/last_action_result.txt"
VIEW_FILE="$SELF_DIR/state/view_mode.txt"

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

view)
    # a real <tabbar>/<tab> click (Viewer / New State) - drives which
    # panel content shows via show="${view_is_*}" vars below, the SAME
    # proven mechanism as select's own sidebar action, not the native
    # tab/scope internals (kept deliberately simple/predictable, same
    # reasoning as every other real choice made in this file).
    MODENAME="${3:-viewer}"
    mkdir -p "$SELF_DIR/state"
    printf '%s\n' "$MODENAME" > "$VIEW_FILE"
    sh "$0" "$HOUSE_ROOT" publish >/dev/null 2>&1 || true
    ;;

publish)
    OUT="${3:-$SELF_DIR/state/ui.txt}"
    mkdir -p "$SELF_DIR/state"
    tmp="$OUT.tmp.$$"
    SEL=""
    [ -f "$SEL_FILE" ] && SEL="$(cat "$SEL_FILE" 2>/dev/null)"
    VIEW="viewer"
    [ -f "$VIEW_FILE" ] && VIEW="$(cat "$VIEW_FILE" 2>/dev/null)"
    [ -n "$VIEW" ] || VIEW="viewer"
    LAST_RESULT=""
    [ -f "$RESULT_FILE" ] && LAST_RESULT="$(cat "$RESULT_FILE" 2>/dev/null)"

    REG_LINES="$(sh "$REG_SH" list "$HOUSE_ROOT" 2>/dev/null)"

    {
        echo "scan_time=$(date '+%H:%M:%S')"
        echo "view_is_viewer=$([ "$VIEW" = "new_state" ] && echo 0 || echo 1)"
        echo "view_is_new=$([ "$VIEW" = "new_state" ] && echo 1 || echo 0)"
        echo "last_result=${LAST_RESULT:-(no action yet)}"
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

                # H-AI-LAB-DESIGN.md Part 5/6: real "scratch block" view
                # for a KIND=fsm entry - reuses events-hq's own real,
                # live publish_scratch_blocks() precedent (labeled,
                # bordered blocks, not a raw text dump) instead of
                # forcing cursword's real conditional branches through
                # events-hq's event.pal pipeline, which has NO native
                # branching (show_choices only records a pick, never
                # jumps commands - confirmed by direct code research,
                # not assumed). View-only for now - editing (dragging a
                # new NEXT= option in) is a real, separate, later step.
                is_fsm=0
                n_blocks=0
                if [ "$d_kind" = "fsm" ] && [ -f "$d_path" ]; then
                    is_fsm=1
                    live_state=""
                    [ -n "$d_iface" ] && [ "$d_iface" != "-" ] && [ -f "$d_iface" ] && live_state="$(cat "$d_iface" 2>/dev/null | tr -d '[:space:]')"
                    bi=0
                    while IFS= read -r line; do
                        case "$line" in
                            STATE\ *) : ;;
                            *) continue ;;
                        esac
                        st_name=$(printf '%s' "$line" | awk -F'|' '{print $2}' | sed 's/^ *//;s/ *$//')
                        st_next=$(printf '%s' "$line" | awk -F'|' '{print $3}' | sed 's/^ *//;s/^NEXT=//;s/ *$//')
                        [ -z "$st_name" ] && continue
                        echo "block_${bi}_state=$st_name"
                        echo "block_${bi}_next=${st_next:-(terminal - no NEXT states)}"
                        echo "block_${bi}_iscurrent=$([ -n "$live_state" ] && [ "$st_name" = "$live_state" ] && echo 1 || echo 0)"
                        bi=$((bi+1))
                    done < "$d_path"
                    n_blocks=$bi
                    echo "fsm_live_state=${live_state:-(not running)}"
                fi
                echo "is_fsm=$is_fsm"
                echo "n_blocks=$n_blocks"
                # two vars, not a negated show= (the renderer's show=
                # attribute has no negation - see proc-mon/h-ai-lab's
                # own earlier no_detail/no_instances precedent).
                # ANDed with the Viewer-tab state right here, not via
                # nested show= (that combinator isn't a thing this
                # renderer's generic vocabulary has) - one flat var per
                # real on-screen condition.
                if [ "$VIEW" = "new_state" ]; then
                    echo "show_blocks=0"; echo "show_raw_text=0"
                else
                    echo "show_blocks=$is_fsm"
                    echo "show_raw_text=$([ "$is_fsm" = 1 ] && echo 0 || echo 1)"
                fi

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
                echo "show_blocks=0"
                echo "show_raw_text=0"
                echo "n_blocks=0"
            fi
        else
            echo "has_detail=0"
            echo "no_detail=1"
            echo "detail_text=(select an instance from the list)"
            echo "show_blocks=0"
            echo "show_raw_text=0"
            echo "n_blocks=0"
        fi
    } > "$tmp"
    mv -f "$tmp" "$OUT"
    ;;

*)
    echo "usage: ai_lab_scan.sh <house_root> [select <name>|publish [out_path]]" >&2
    exit 2
    ;;
esac
