#!/bin/bash
# nb_table_test.sh - pins the INLINE TABLE COLUMNS contract.
#
# A TROW row carries its cells positionally (c_*_cells, \x1F-joined,
# empties significant); the renderer draws equal columns from it while
# the joined label stays as the fallback/copy text. Ragged rows grid
# differently per row - honest, not a bug.
#
# Checks on tests/fixtures/table.html (two tables: headers, an empty
# cell, an entity, a caption):
#   1. every TROW row emits a cells payload with the right arity
#      (header 2, body 2 incl. the empty cell, second table 2)
#   2. payloads are byte-exact (entity decoded, no pipes)
#   3. column 2 starts at the same x on every body row (captured frame)
#
# Usage: sh nb_table_test.sh    (needs a running network browser)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
REQ="$HR/#.desktop/network_browser_request.txt"
PF="$HR/#.desktop/network_browser_page.state.txt"
UI="$HR/#.desktop/network-browser-hq_ui.txt"
FAIL=0

[ -f "$UI" ] || { echo "FAIL: no browser running"; exit 1; }

table_candidate_windows() {
    xwininfo -root -tree 2>/dev/null | grep -E '\(has no name\)' | while read -r tline; do
        W="$(printf '%s' "$tline" | awk '{print $1}')"
        G="$(printf '%s' "$tline" | grep -oE '[0-9]+x[0-9]+\+' | head -1 | tr -d '+')"
        WW="$(printf '%s' "$G" | cut -dx -f1)"; HH="$(printf '%s' "$G" | cut -dx -f2)"
        case "${WW:-0}x${HH:-0}" in
            724x304|51x51|1x1|10x10|1520x29|1205x29|100x100|200x200|99x46|519x46|229x46) continue;;
        esac
        [ "${WW:-0}" -ge 640 ] 2>/dev/null || continue
        [ "${HH:-0}" -ge 480 ] 2>/dev/null || continue
        if [ "$WW"x"$HH" = "784x504" ]; then printf '1 %s\n' "$W"; else printf '2 %s\n' "$W"; fi
    done | sort -n | awk '{print $2}'
}

FX="$HERE/fixtures/table.html"
printf 'go:file://%s\n' "$FX" > "$REQ"
for _ in $(seq 1 40); do
    grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null \
        && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
    sleep 0.5
done
sleep 2

echo "== table cell payloads in ui.txt"
FS="$(printf '\037')"
# header, both bodies incl. the empty cell, second table incl. entity.
# -F (literal): payloads carry regex-active chars like the dots in 9.99.
for WANT in "Plan${FS}Price" "Free${FS}0" "Pro${FS}9.99" "Team${FS}" "Key${FS}Value" "a & b${FS}1"; do
    if grep -Fq "_cells=${WANT}" "$UI" 2>/dev/null; then
        echo "PASS: cells payload [$(printf '%s' "$WANT" | tr '\037' '|')]"
    else
        echo "FAIL: cells payload missing [$(printf '%s' "$WANT" | tr '\037' '|')]"; FAIL=1
    fi
done
# arity: every emitted payload on this fixture has exactly 2 columns
# (exactly one \x1F).
if grep -E '^c_[0-9]+_cells=' "$UI" 2>/dev/null | awk -F'\037' 'NF!=2{bad=1} END{exit bad+0}'; then
    echo "PASS: all payloads have 2 columns"
else
    echo "FAIL: a cells payload does not have 2 columns"; FAIL=1
fi

echo "== word bank viewer table payloads"
# Generate HTML via wordbank_view_op from the wb_entity fixture,
# then reload: proves the pipeline (view op -> browser table columns).
WBOP="$HR/&.widgits/_shared-lib/ops/+x/wordbank_view_op.+x"
WBFX="$HERE/fixtures/wb_entity"
if [ -x "$WBOP" ] && [ -d "$WBFX" ]; then
    "$WBOP" --render "$WBFX" > "$HERE/fixtures/wb_viewer.html" 2>/dev/null || true
    WBFX2="$HERE/fixtures/wb_viewer.html"
    printf 'go:file://%s\n' "$WBFX2" > "$REQ"
    for _ in $(seq 1 40); do
        grep -q "^URL|file://$WBFX2\$" "$PF" 2>/dev/null \
            && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
        sleep 0.5
    done
    sleep 2
    # the word bank table has 3 data rows + 1 header = 4 cells payloads
    # columns: Canon|Alias|WEIGHT|SOURCE -> 4 columns (\x1F x 3)
    WB_FS="$(printf '\037')"
    for WANT in "name:demo${WB_FS}demo${WB_FS}0.50${WB_FS}seed" \
                "action:follow${WB_FS}follow${WB_FS}1.00${WB_FS}user" \
                "action:stay${WB_FS}wait${WB_FS}0.00${WB_FS}user"; do
        if grep -Fq "_cells=${WANT}" "$UI" 2>/dev/null; then
            echo "PASS: wordbank cells [$(printf '%s' "$WANT" | tr '\037' '|')]"
        else
            echo "FAIL: wordbank cells missing [$(printf '%s' "$WANT" | tr '\037' '|')"; FAIL=1
        fi
    done
    # arity: wordbank rows have 4 columns (Canon|Alias|Weight|Source)
    # filter to wordbank payloads (Canon/weight rows) - table.html ones
    # still lurk in $UI with 2 columns, so scope the check.
    if grep -E '^c_[0-9]+_cells=' "$UI" 2>/dev/null | grep -E '(name:|action:)' | awk -F'\037' 'NF!=4{bad=1} END{exit bad+0}' 2>/dev/null; then
        echo "PASS: wordbank payloads have 4 columns"
    else
        echo "FAIL: a wordbank cells payload does not have 4 columns"; FAIL=1
    fi
else
    echo "SKIP: wordbank_view_op not compiled or fixture missing"
fi

echo "== column alignment in a captured frame (needs X)"
if [ -z "${DISPLAY:-}" ] || ! command -v xwininfo >/dev/null 2>&1; then
    echo "SKIP: no X display for the alignment proof"
else
    BPID="$(pgrep -f 'khtpm_core_render.+x .*network-browser-hq' | head -1)"
    DUMPOP="$HR/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x"
    ALIGNED=0
    if [ -n "$BPID" ] && [ -x "$DUMPOP" ]; then
        for W in $(table_candidate_windows); do
            VW="$(xwininfo -id "$W" 2>/dev/null | grep "Map State" | grep -c IsViewable)"
            [ "$VW" = "1" ] || continue
            PNG="$(mktemp /tmp/nb_table_XXXXXX.png)"
            if "$DUMPOP" "$W" "$PNG" >/dev/null 2>&1; then
                EDGES="$(python3 "$HERE/colscan.py" "$PNG" 330 2>/dev/null)"
                # dominant column-2 x: rows agreeing within +-2px
                # (sorted cluster count - antialiasing jitters exact
                # matches). Link-blue thead rows are invisible to the
                # grey scan, so this counts body rows; >=6 agreeing
                # samples proves the grid (each body row contributes
                # several scan rows).
                BEST="$(printf '%s' "$EDGES" | awk '{print $2}' | sort -n | awk 'NR==1{prev=$1;cur=1;best=1;next} {if ($1-prev<=4) {cur++; if(cur>best)best=cur} else {cur=1} prev=$1} END{print best+0}')"
                if [ -n "$BEST" ] && [ "$BEST" -ge 6 ]; then
                    echo "PASS: column 2 starts together on $BEST sampled rows"
                    ALIGNED=1
                fi
            fi
            rm -f "$PNG"
            [ "$ALIGNED" = "1" ] && break
        done
    fi
    [ "$ALIGNED" = "1" ] || { echo "FAIL: no aligned column 2 found"; FAIL=1; }
fi

echo
[ "$FAIL" -eq 0 ] && echo "TABLE PASS" || echo "TABLE FAIL"
exit $FAIL
