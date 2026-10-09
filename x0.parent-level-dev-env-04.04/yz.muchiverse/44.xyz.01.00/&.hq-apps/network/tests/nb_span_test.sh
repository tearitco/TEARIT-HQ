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

# Candidate browser windows from ONE xwininfo call (no per-window
# forks): unnamed, browser-sized, chrome/tiles skipped, the usual
# 784x504 first. Prints one id per line. Keeps X-heavy sections fast
# under a busy shared desktop (2026-10-09: 56 windows, 22 blind
# captures per attempt).
span_candidate_windows() {
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

# Fresh ascii frame, delete-first: existence (not second-resolution
# mtime) proves freshness. The M=$(date +%s)/mtime pattern raced same-
# second stale files mid-suite and misread badges. $1=history file,
# $2=frame file. Returns 0 on a fresh frame.
span_fresh_frame() {
    rm -f "$2"
    sleep 0.5
    printf '112\n' >> "$1"
    for _ in $(seq 1 20); do
        [ -f "$2" ] && { sleep 1; return 0; }
        sleep 0.5
    done
    return 1
}

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
    # Disarm first (2026-10-09: an armed address bar eats every relay
    # key - digits/arrows/Enter get typed, and even the 112 frame probe
    # types 'p' - so nav/click proofs fail with zero code involvement.
    # Bare Esc is a no-op when nothing is armed; twice covers cursor+
    # field stacked. kh_focus_debug.log proved the mechanism.)
    if [ -n "$BPID" ]; then
        printf '27\n27\n' >> "$HR/#.desktop/entity_menu_history/$BPID.txt"; sleep 2
    fi
    if [ -n "$BPID" ] && [ -x "$DUMPOP" ] && [ -f "$BANDPY" ]; then
        # Flake guard (2026-10-09: one full-suite run found no window
        # while a later section did - transient X capture races under
        # suite load must not fail the suite; a real absence fails all
        # attempts loudly). Re-verify the page each attempt: a peer
        # driving the shared live browser can yank it mid-suite.
        for ATT in 1 2 3; do
            [ "$ATT" -gt 1 ] && sleep 5
            grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null || {
                printf 'go:file://%s\n' "$FX" > "$REQ"
                for _ in $(seq 1 40); do
                    grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null \
                        && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
                    sleep 0.5
                done
                sleep 2
            }
            NCAND=0
        for W in $(span_candidate_windows); do
            VW="$(xwininfo -id "$W" 2>/dev/null | grep "Map State" | grep -c IsViewable)"
            [ "$VW" = "1" ] || continue
            NCAND=$((NCAND + 1))
            PNG="$(mktemp /tmp/nb_click_XXXXXX.png)"
            if "$DUMPOP" "$W" "$PNG" >/tmp/nb_dumpop_err.txt 2>&1; then
                # underline band: exactly 1px tall, >=15px wide
                UL="$(python3 "$BANDPY" "$PNG" 2>/dev/null | awk '$2==$1 && ($4-$3)>=15 {print $1, $3, $4}' | head -1)"
                if [ -z "$UL" ]; then
                    echo "click attempt $ATT: $W captured but no underline band"
                    cp "$PNG" /tmp/nb_span_fail.png 2>/dev/null
                fi
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
                        for _ in $(seq 1 150); do
                            if grep -q "^URL|https://example.com" "$PF" 2>/dev/null; then NAVED=1; break; fi
                            sleep 1
                        done
                        if [ "$NAVED" = "1" ]; then
                            echo "PASS: link-span click navigated to example.com"
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
                                CLICKDONE=1
                            else
                                echo "FAIL: text-span click navigated (URL:$(grep '^URL|' "$PF" | tail -1))"; FAIL=1
                                CLICKDONE=1
                            fi
                        else
                            echo "click attempt $ATT: link-span click did not navigate, retrying"
                            rm -f "$PNG"
                            break
                        fi
                    fi
                fi
            fi
            rm -f "$PNG"
            [ "$CLICKDONE" = "1" ] && break
        done
        echo "click attempt $ATT: $NCAND candidate windows"
        [ "$CLICKDONE" = "1" ] && break
        done
    fi
    [ "$CLICKDONE" = "1" ] || { echo "FAIL: span click-through failed after 3 attempts (no window, or no navigation)"; FAIL=1; }
fi

echo "== span keyboard nav (relay digits/arrows/Enter, needs X)"
# Digits focus the rich row, Right steps the segment cursor onto its
# link, Enter dispatches the span's action. Same geometry argument as
# the clicks above: row activation alone navigates nowhere (empty
# onclick), so arrival at example.com proves the span path. Relay
# codes: bare digits focus, 203=Right, 13=Return.
if [ -z "${DISPLAY:-}" ]; then
    echo "SKIP: no X display for the keyboard nav"
else
    BPID="$(pgrep -f 'khtpm_core_render.+x .*network-browser-hq' | head -1)"
    if [ -z "$BPID" ]; then
        echo "FAIL: no network browser window"; FAIL=1
    else
        HF="$HR/#.desktop/entity_menu_history/$BPID.txt"
        FF="$HR/#.desktop/ascii_frames/$BPID.frame.txt"
        # Disarm first - see click section: an armed address bar eats
        # every relay key.
        printf '27\n27\n' >> "$HF"; sleep 2
        grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null || {
            printf 'go:file://%s\n' "$FX" > "$REQ"
            for _ in $(seq 1 40); do
                grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null \
                    && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
                sleep 0.5
            done
            sleep 2
        }
        span_fresh_frame "$HF" "$FF" || {
            echo "FAIL: no fresh frame for badge read"; FAIL=1
        }
        # Relay digits are ASCII codes (50='2'), NOT the digit
        # itself: bare "2"/"0" lines decode to Ctrl+B/NUL (a real
        # committed-test bug that passed spuriously on stale focus).
        # Step away first so the focus assert below proves movement.
        # RN is (re-)parsed every attempt: a reload can renumber rows.
        printf '200\n' >> "$HF"; sleep 2
        # Flake guard: a transient fetch failure looks exactly like
        # a dispatch failure from outside (no nav either way), so a
        # failed attempt reloads and retries once; a real bug fails
        # twice, loudly. Request-file state on failure tells the two
        # apart (go-request written = dispatched, fetch failed).
        KNAVED=0
        for KATT in 1 2; do
                [ "$KATT" -gt 1 ] && {
                    echo "kbd attempt $KATT: reloading and retrying"
                    printf 'go:file://%s\n' "$FX" > "$REQ"
                    for _ in $(seq 1 40); do
                        grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null \
                            && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
                        sleep 0.5
                    done
                    sleep 2
                }
                span_fresh_frame "$HF" "$FF" || {
                    echo "kbd attempt $KATT: no fresh frame for badge read"
                    continue
                }
                RN="$(grep "See the docs for more" "$FF" 2>/dev/null | head -1 | sed -n 's/.*\[.\] \([0-9][0-9]*\)\. See the docs.*/\1/p')"
                PREFOCUS="$(grep -E '\[>\]' "$FF" 2>/dev/null | head -1 | cut -c1-50)"
                if [ -z "$RN" ]; then
                    echo "kbd attempt $KATT: rich row badge not found"
                    continue
                fi
                # fold emits no trailing newline without one on input, and
                # `while read` skips the unterminated tail - the "%s\n"
                # below is load-bearing (a real committed-test bug: only
                # the first digit ever sent, focus landed on row 2).
                printf '%s\n' "$RN" | fold -w1 | while read -r D; do printf '%d\n' "'$D" >> "$HF"; sleep 1; done
                sleep 3
                span_fresh_frame "$HF" "$FF" || {
                    echo "kbd attempt $KATT: no fresh frame for focus check"
                    continue
                }
                if grep -Eq "\[>\] $RN\. See the docs" "$FF" 2>/dev/null; then
                    [ "$KATT" = "1" ] && echo "PASS: digits focused the rich row"
                else
                    echo "kbd attempt $KATT: digits did not focus row $RN (was: $PREFOCUS)"
                    continue
                fi
                printf '203\n' >> "$HF"; sleep 2
                printf '13\n' >> "$HF"
                NAVED=0
                for _ in $(seq 1 150); do
                    if grep -q "^URL|https://example.com" "$PF" 2>/dev/null; then NAVED=1; break; fi
                    sleep 1
                done
                if [ "$NAVED" = "1" ]; then
                    echo "PASS: Right+Enter on the span navigated to example.com"
                    KNAVED=1
                    break
                else
                    # the request file is truncated on consume, so its
                    # state says nothing; the page URL is the signal.
                    # Frame focus + ui rich state pin down WHERE it died.
                    span_fresh_frame "$HF" "$FF"
                    echo "kbd attempt $KATT: no nav (page now: $(grep '^URL|' "$PF" 2>/dev/null | tail -1); focus: $(grep -E '\[>\]' "$FF" 2>/dev/null | head -1 | cut -c1-60); rich rows: $(grep -c 'is_rich=1' "$UI" 2>/dev/null))"
                fi
            done
            if [ "$KNAVED" != "1" ]; then
                echo "FAIL: keyboard span dispatch did not navigate after 2 attempts"; FAIL=1
            fi
            printf 'go:file://%s\n' "$FX" > "$REQ"
            for _ in $(seq 1 40); do
                grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null \
                    && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
                sleep 0.5
            done
            sleep 2
    fi
fi

echo "== span wrapping (longlinks fixture, needs X)"
# A paragraph longer than one visual line must render tinted on EVERY
# line (not just the first), with one underline run per link line, and
# the following rows intact below the box. Proof by band count in a
# captured frame: >=2 tall glyph bands + >=1 single-row underline band.
LONGFX="$HERE/fixtures/longlinks.html"
if [ -z "${DISPLAY:-}" ] || ! command -v xwininfo >/dev/null 2>&1; then
    echo "SKIP: no X display for the wrap proof"
else
    printf 'go:file://%s\n' "$LONGFX" > "$REQ"
    for _ in $(seq 1 40); do
        grep -q "^URL|file://$LONGFX\$" "$PF" 2>/dev/null \
            && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
        sleep 0.5
    done
    sleep 2
    LRN="$(grep -E '^c_[0-9]+_is_rich=1$' "$UI" 2>/dev/null | head -1 | sed 's/^c_\([0-9]*\)_.*/\1/')"
    if [ -n "$LRN" ] && grep -Eq "^c_${LRN}_text=This is a deliberately" "$UI" 2>/dev/null; then
        echo "PASS: long paragraph emitted as a rich row"
    else
        echo "FAIL: no rich row for the long paragraph"; FAIL=1
    fi
    BPID="$(pgrep -f 'khtpm_core_render.+x .*network-browser-hq' | head -1)"
    DUMPOP="$HR/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x"
    BANDPY="$HERE/bluebands.py"
    WRAPDONE=0
    if [ -n "$LRN" ] && [ -n "$BPID" ] && [ -x "$DUMPOP" ] && [ -f "$BANDPY" ]; then
        for W in $(span_candidate_windows); do
            VW="$(xwininfo -id "$W" 2>/dev/null | grep "Map State" | grep -c IsViewable)"
            [ "$VW" = "1" ] || continue
            PNG="$(mktemp /tmp/nb_wrap_XXXXXX.png)"
            if "$DUMPOP" "$W" "$PNG" >/dev/null 2>&1; then
                BANDS="$(python3 "$BANDPY" "$PNG" 2>/dev/null)"
                GLYPHN="$(printf '%s' "$BANDS" | awk '$2-$1>=6' | wc -l)"
                ULN="$(printf '%s' "$BANDS" | awk '$2==$1' | wc -l)"
                # the two kept LINK items add glyph bands without
                # underlines; the rich row must contribute >=2 tinted
                # glyph bands of its own plus its underlines.
                if [ "$GLYPHN" -ge 4 ] && [ "$ULN" -ge 2 ]; then
                    echo "PASS: wrapped tint on multiple lines (glyph bands=$GLYPHN underline bands=$ULN)"
                    WRAPDONE=1
                fi
            fi
            rm -f "$PNG"
            [ "$WRAPDONE" = "1" ] && break
        done
    fi
    [ "$WRAPDONE" = "1" ] || { echo "FAIL: no multi-line span tint found"; FAIL=1; }
fi

echo "== span wrapped click + cursor (longlinks, needs X)"
# The row is wrapped (9 visual lines), so single-line cumulative x can
# never resolve these spans: a click must map py to a line first. The
# LAST underline band is link2 on a late line - clicking its center
# proves the wrapped path (single-line geometry addresses a different
# x universe), and arriving at /second (not /first) proves the right
# segment. Then digits + Right + Right walks idx0->idx1 with the white
# cursor following onto link2's line, and Enter dispatches it.
if [ -z "${DISPLAY:-}" ] || ! command -v xwininfo >/dev/null 2>&1; then
    echo "SKIP: no X display for the wrapped proof"
else
    BPID="$(pgrep -f 'khtpm_core_render.+x .*network-browser-hq' | head -1)"
    DUMPOP="$HR/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x"
    BANDPY="$HERE/bluebands.py"
    HF="$HR/#.desktop/entity_menu_history/$BPID.txt"
    FF="$HR/#.desktop/ascii_frames/$BPID.frame.txt"
    # Disarm first - see click section.
    if [ -n "$BPID" ]; then
        printf '27\n27\n' >> "$HF"; sleep 2
    fi
    grep -q "^URL|file://$LONGFX\$" "$PF" 2>/dev/null || {
        printf 'go:file://%s\n' "$LONGFX" > "$REQ"
        for _ in $(seq 1 40); do
            grep -q "^URL|file://$LONGFX\$" "$PF" 2>/dev/null \
                && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
            sleep 0.5
        done
        sleep 2
    }
    WDONE=0
    SPANWIN=""
    if [ -n "$BPID" ] && [ -x "$DUMPOP" ] && [ -f "$BANDPY" ]; then
        for W in $(span_candidate_windows); do
            VW="$(xwininfo -id "$W" 2>/dev/null | grep "Map State" | grep -c IsViewable)"
            [ "$VW" = "1" ] || continue
            PNG="$(mktemp /tmp/nb_wclick_XXXXXX.png)"
            if "$DUMPOP" "$W" "$PNG" >/dev/null 2>&1; then
                UL2="$(python3 "$BANDPY" "$PNG" 2>/dev/null | awk '$2==$1 && ($4-$3)>=15' | tail -1)"
                if [ -n "$UL2" ]; then
                    SPANWIN="$W"
                    UY="$(printf '%s' "$UL2" | cut -d' ' -f1)"
                    GLYPH="$(python3 "$BANDPY" "$PNG" 2>/dev/null | awk -v u="$UY" '$2<u && u-$2<=5 && ($2-$1)>=6 {print $1, $2, $3, $4}' | tail -1)"
                    if [ -n "$GLYPH" ]; then
                        GY0="$(printf '%s' "$GLYPH" | cut -d' ' -f1)"; GY1="$(printf '%s' "$GLYPH" | cut -d' ' -f2)"
                        GX0="$(printf '%s' "$GLYPH" | cut -d' ' -f3)"; GX1="$(printf '%s' "$GLYPH" | cut -d' ' -f4)"
                        CX=$(( (GX0 + GX1) / 2 )); CY=$(( (GY0 + GY1) / 2 ))
                        printf 'MOUSE_EVENT: 1 %d %d 1\n' "$CX" "$CY" >> "$HF"; sleep 3
                        printf 'MOUSE_EVENT: 1 %d %d 1\n' "$CX" "$CY" >> "$HF"
                        NAVED=0
                        for _ in $(seq 1 150); do
                            if grep -q "example.com/second" "$PF" 2>/dev/null; then NAVED=1; break; fi
                            sleep 1
                        done
                        if [ "$NAVED" = "1" ]; then
                            echo "PASS: wrapped-span click navigated to /second"
                            WDONE=1
                        else
                            echo "wrapped click attempt: no nav at ($CX,$CY)"
                        fi
                    fi
                fi
            fi
            rm -f "$PNG"
            [ "$WDONE" = "1" ] && break
        done
    fi
    if [ "$WDONE" != "1" ]; then
        echo "FAIL: wrapped-span click did not navigate"; FAIL=1
    else
        printf 'go:file://%s\n' "$LONGFX" > "$REQ"
        for _ in $(seq 1 40); do
            grep -q "^URL|file://$LONGFX\$" "$PF" 2>/dev/null \
                && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
            sleep 0.5
        done
        sleep 2
        span_fresh_frame "$HF" "$FF" || echo "WARN: no fresh frame for badge read"
        RN="$(grep "deliberately" "$FF" 2>/dev/null | head -1 | sed -n 's/.*\[.\] \([0-9][0-9]*\)\. This.*/\1/p')"
        if [ -z "$RN" ]; then
            echo "FAIL: wrapped rich row badge not found"; FAIL=1
        else
            printf '%s\n' "$RN" | fold -w1 | while read -r D; do printf '%d\n' "'$D" >> "$HF"; sleep 1; done
            sleep 3
            printf '203\n' >> "$HF"; sleep 2
            printf '203\n' >> "$HF"; sleep 3
            PNG="$(mktemp /tmp/nb_wcur_XXXXXX.png)"
            if [ -n "$SPANWIN" ] && "$DUMPOP" "$SPANWIN" "$PNG" >/dev/null 2>&1; then
                if python3 "$BANDPY" "$PNG" ffffff 2>/dev/null | awk -v u="$UY" '$1<=u+2 && $2>=u-2' | grep -q .; then
                    echo "PASS: cursor underline followed onto link2's line"
                else
                    echo "FAIL: no cursor underline on link2's line"; FAIL=1
                fi
            else
                echo "FAIL: could not capture browser for cursor check"; FAIL=1
            fi
            rm -f "$PNG"
            printf '13\n' >> "$HF"
            NAVED=0
            for _ in $(seq 1 150); do
                if grep -q "example.com/second" "$PF" 2>/dev/null; then NAVED=1; break; fi
                sleep 1
            done
            if [ "$NAVED" = "1" ]; then
                echo "PASS: Right+Right+Enter walked to link2 and dispatched"
            else
                echo "FAIL: wrapped cursor dispatch did not navigate"; FAIL=1
            fi
            printf 'go:file://%s\n' "$LONGFX" > "$REQ"
            for _ in $(seq 1 40); do
                grep -q "^URL|file://$LONGFX\$" "$PF" 2>/dev/null \
                    && grep -q "status=Status: ready" "$UI" 2>/dev/null && break
                sleep 0.5
            done
            sleep 2
        fi
    fi
fi

echo
[ "$FAIL" -eq 0 ] && echo "SPAN PASS" || echo "SPAN FAIL"
exit $FAIL