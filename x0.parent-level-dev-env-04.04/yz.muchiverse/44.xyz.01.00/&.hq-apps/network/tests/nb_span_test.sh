#!/bin/bash
# nb_span_test.sh - pins the INLINE SPAN grouping contract AND the
# projector encoder (phase 2 step 3).
#
# Grouping (page.state RICH/RICHSEG rows): one span group == ONE paragraph,
# surrounding text kept on both sides of the link, linkless paragraphs
# produce nothing, PARA markers never reach the projection.
#
# Encoder (ui.txt c_* rows): a verified group becomes one is_rich row
# carrying the exact segments= wire payload, the run's TEXT pieces are
# swallowed, and the LINK row stays a clickable item.
#
# Usage: sh nb_span_test.sh    (needs a running network browser)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
REQ="$HR/#.desktop/network_browser_request.txt"
PF="$HR/#.desktop/network_browser_page.state.txt"
UI="$HR/#.desktop/network-browser-hq_ui.txt"
FAIL=0

[ -f "$UI" ] || { echo "FAIL: no browser running"; exit 1; }

FX="$HERE/fixtures/mini-article.html"
printf 'go:file://%s\n' "$FX" > "$REQ"
for _ in $(seq 1 60); do
    grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null && break
    sleep 0.5
done
sleep 2
[ -f "$PF" ] || { echo "FAIL: no page state"; exit 1; }

echo "== span grouping on mini-article.html"
GROUPS="$(grep -c '^RICH|' "$PF" 2>/dev/null)"; [ -n "$GROUPS" ] || GROUPS=0
SEGS="$(grep -c '^RICHSEG|' "$PF" 2>/dev/null)"; [ -n "$SEGS" ] || SEGS=0

# 1 + 2: exactly one group, and it is the link's own paragraph.
if [ "$GROUPS" -eq 1 ]; then
    echo "PASS: exactly one span group (paragraphs are not welded)"
else
    echo "FAIL: expected 1 span group, got $GROUPS - paragraphs welded?"; FAIL=1
fi
if grep -Fxq 'RICHSEG|link|docs|https://example.com' "$PF"; then
    echo "PASS: the link is a link segment with its URL"
else
    echo "FAIL: link segment missing or malformed"; FAIL=1
fi
# whole-line exact match: every segment line ends with the '|' field
# separator, so an anchored regex reading "for more.$" never matches.
# (2026-10-09: boundary spaces are sentence content - the extractor keeps
# exactly one at an inline-<a> split, so the pieces rejoin whole.)
if grep -Fxq 'RICHSEG|text|See the |' "$PF" && grep -Fxq 'RICHSEG|text| for more.|' "$PF"; then
    echo "PASS: text kept on BOTH sides of the link, same group"
else
    echo "FAIL: text around the link was dropped or split"; FAIL=1
fi
# the group is stamped with its paragraph number (projector matches by
# number, not position - the 2026-10-09 RICH|3|7 mis-stamp).
if grep -Eq '^RICH\|[0-9]+\|[0-9]+$' "$PF"; then
    echo "PASS: group carries its paragraph number"
else
    echo "FAIL: group has no paragraph stamp"; FAIL=1
fi

# 3: the two linkless paragraphs must NOT appear as groups.
if grep -q '^RICHSEG|text|First paragraph' "$PF" \
   || grep -q '^RICHSEG|text|Second paragraph' "$PF"; then
    echo "FAIL: a linkless paragraph produced a span group"; FAIL=1
else
    echo "PASS: linkless paragraphs produce no group"
fi

# 4: markers are for the grouper only, never for the window.
if grep -q 'PARA' "$UI" 2>/dev/null; then
    echo "FAIL: PARA marker leaked into the projection (costs an element)"; FAIL=1
else
    echo "PASS: PARA markers cost zero projection elements"
fi

echo "== rich projection in ui.txt (encoder)"
# the verified group becomes one is_rich row ...
RICHN="$(grep -E '^c_[0-9]+_is_rich=1$' "$UI" 2>/dev/null | head -1 | sed 's/^c_\([0-9]*\)_.*/\1/')"
if [ -n "$RICHN" ] \
    && grep -Fxq "c_${RICHN}_kind=rich" "$UI" \
    && grep -Fxq "c_${RICHN}_text=See the docs for more." "$UI"; then
    echo "PASS: verified group emitted as one rich sentence row"
else
    echo "FAIL: no rich sentence row in the projection"; FAIL=1
fi
# ... carrying the exact wire payload: kind \x1F text \x1F url per
# segment (link segments append their click action as a fourth field -
# the same go-command shape as the LINK item rows), segments joined
# by \x1E (the renderer decodes this).
if [ -n "$RICHN" ]; then
    FS="$(printf '\037')"; RS="$(printf '\036')"
    ACT="'$HR/&.hq-apps/network/ops/nb_write_go.sh' 'go' 'https://example.com'"
    EXPECTED="text${FS}See the ${FS}${RS}link${FS}docs${FS}https://example.com${FS}${ACT}${RS}text${FS} for more.${FS}"
    SEGVAL="$(grep -E "^c_${RICHN}_segments=" "$UI" | head -1 | sed "s/^c_${RICHN}_segments=//")"
    if [ "$SEGVAL" = "$EXPECTED" ]; then
        echo "PASS: segments payload is byte-exact"
    else
        echo "FAIL: segments payload mismatch (got [$SEGVAL])"; FAIL=1
    fi
fi
# ... the run's TEXT pieces are swallowed (no standalone piece rows) ...
if grep -Eq '^c_[0-9]+_text=See the $' "$UI" 2>/dev/null \
    || grep -Eq '^c_[0-9]+_text= for more\.$' "$UI" 2>/dev/null; then
    echo "FAIL: run TEXT pieces still projected alongside the rich row"; FAIL=1
else
    echo "PASS: run TEXT pieces swallowed by the rich row"
fi
# ... and the LINK row stays a clickable item (clicks live until
# per-segment hit-testing lands).
LINKN="$(grep -E '^c_[0-9]+_text=docs$' "$UI" 2>/dev/null | head -1 | sed 's/^c_\([0-9]*\)_.*/\1/')"
if [ -n "$LINKN" ] \
    && grep -Fxq "c_${LINKN}_kind=link" "$UI" \
    && grep -Eq "^c_${LINKN}_action=.*nb_write_go\.sh" "$UI"; then
    echo "PASS: link row kept as a clickable item"
else
    echo "FAIL: link item row missing or not clickable"; FAIL=1
fi

echo "== span click-through (relay MOUSE_EVENT, needs X)"
# Two-step clicks are the house default: the first click focuses the
# row, the second dispatches. A click on the LINK span must navigate
# (same go-command as the item rows); a click on the TEXT span must
# not. Coordinates come from a real captured frame: the underline band
# (a single 1px-tall blue run under the link glyphs) identifies the
# rich row without trusting any layout guess, and the click point sits
# inside the sentence row's own bounds so the item row below cannot
# receive it - geometry, not hope.
if [ -z "${DISPLAY:-}" ] || ! command -v xwininfo >/dev/null 2>&1; then
    echo "SKIP: no X display for the click-through"
else
    BPID="$(pgrep -f 'khtpm_core_render.+x .*network-browser-hq' | head -1)"
    DUMPOP="$HR/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x"
    BANDPY="$HERE/bluebands.py"
    CLICKDONE=0
    if [ -n "$BPID" ] && [ -x "$DUMPOP" ] && [ -f "$BANDPY" ]; then
        for W in $(xwininfo -root -tree 2>/dev/null | grep -E "^\s+0x[0-9a-f]+ " | awk '{print $1}' | sort -u); do
            G="$(xwininfo -id "$W" 2>/dev/null | grep -E "Width|Height" | awk '{print $2}' | tr '\n' 'x')"
            case "$G" in
                724x304x|51x51x|1x1x|1520x29x|1205x29x) continue;;  # fixture/chrome/tiles, never the browser
            esac
            VW="$(xwininfo -id "$W" 2>/dev/null | grep "Map State" | grep -c IsViewable)"
            [ "$VW" = "1" ] || continue
            PNG="$(mktemp /tmp/nb_click_XXXXXX.png)"
            if "$DUMPOP" "$W" "$PNG" >/dev/null 2>&1; then
                # underline band: exactly 1px tall, >=15px wide
                UL="$(python3 "$BANDPY" "$PNG" 2>/dev/null | awk '$2==$1 && ($4-$3)>=15 {print $1, $3, $4}' | head -1)"
                if [ -n "$UL" ]; then
                    UY="$(printf '%s' "$UL" | cut -d' ' -f1)"
                    # glyph band directly above (within 5px), >=10px tall
                    GLYPH="$(python3 "$BANDPY" "$PNG" 2>/dev/null | awk -v u="$UY" '$2<u && u-$2<=5 && ($2-$1)>=6 {print $1, $2, $3, $4}' | tail -1)"
                    if [ -n "$GLYPH" ]; then
                        GY0="$(printf '%s' "$GLYPH" | cut -d' ' -f1)"; GY1="$(printf '%s' "$GLYPH" | cut -d' ' -f2)"
                        GX0="$(printf '%s' "$GLYPH" | cut -d' ' -f3)"; GX1="$(printf '%s' "$GLYPH" | cut -d' ' -f4)"
                        CX=$(( (GX0 + GX1) / 2 )); CY=$(( (GY0 + GY1) / 2 ))
                        TX=$(( GX0 - 30 ))
                        if [ "$TX" -lt 0 ]; then
                            echo "FAIL: span too close to the left edge for a text-span probe"; FAIL=1
                            rm -f "$PNG"
                            break
                        fi
                        HF="$HR/#.desktop/entity_menu_history/$BPID.txt"
                        printf 'MOUSE_EVENT: 1 %d %d 1\n' "$CX" "$CY" >> "$HF"; sleep 3
                        printf 'MOUSE_EVENT: 1 %d %d 1\n' "$CX" "$CY" >> "$HF"
                        NAVED=0
                        for _ in $(seq 1 60); do
                            if grep -q "^URL|https://example.com" "$PF" 2>/dev/null; then NAVED=1; break; fi
                            sleep 1
                        done
                        if [ "$NAVED" = "1" ]; then
                            echo "PASS: link-span click navigated to example.com"
                        else
                            echo "FAIL: link-span click did not navigate"; FAIL=1
                        fi
                        # back to the fixture, then the TEXT span must NOT navigate
                        printf 'go:file://%s\n' "$FX" > "$REQ"
                        for _ in $(seq 1 40); do
                            grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null \
                                && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
                            sleep 0.5
                        done
                        sleep 2
                        printf 'MOUSE_EVENT: 1 %d %d 1\n' "$TX" "$CY" >> "$HF"; sleep 3
                        printf 'MOUSE_EVENT: 1 %d %d 1\n' "$TX" "$CY" >> "$HF"; sleep 8
                        if grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null; then
                            echo "PASS: text-span click did not navigate"
                        else
                            echo "FAIL: text-span click navigated (URL:$(grep '^URL|' "$PF" | tail -1))"; FAIL=1
                        fi
                        CLICKDONE=1
                    fi
                fi
            fi
            rm -f "$PNG"
            [ "$CLICKDONE" = "1" ] && break
        done
    fi
    [ "$CLICKDONE" = "1" ] || { echo "FAIL: browser window with span underline not found"; FAIL=1; }
fi

echo
[ "$FAIL" -eq 0 ] && echo "SPAN PASS" || echo "SPAN FAIL"
exit $FAIL