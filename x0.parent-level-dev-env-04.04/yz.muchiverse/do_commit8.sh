#!/bin/bash
set -e
cd "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17"

git add -A
git restore --staged "x0.parent-level-dev-env-04.04/yz.muchiverse^v25.5.05.7z" 2>/dev/null || true
git status --short

git commit -F - <<'COMMIT_MSG_EOF'
Fix palette scroll to actually move content; add real numbered scroll arrows

Follow-up to the earlier scroll port, from live testing reports:

Real bug found and fixed: "thumb moves but doesn't change display." The
row-shift code only moved each row container's own y - the shared
css_layout_pass() engine already gives every tile inside it an
independent absolute y during its own recursion, and draw_elem() reads
that directly, never parent-relative. So scrolling moved the thumb and
the row's own invisible box, but not a single visible tile. This is a
real divergence from the deleted khtpm_hq_render.c's own hand-rolled
layout engine, which didn't have this problem - porting its scroll
snippet verbatim wasn't actually equivalent against this shared engine's
different children-positioning model. Fixed with a new
dbhq_pal_shift_subtree() that shifts the whole row subtree. Verified
with real before/after screenshots of a live drag, not just a debug
print of internal state (which is what missed this bug the first time).

Added real up/down scroll-arrow buttons - a standard scrollbar
affordance that didn't exist before (only track click/drag/wheel/Page
keys did). Per direct instruction, the arrows are real, numbered (up is
always nav 1, down always nav 2, regardless of enabled state - disabled
is a separate concept from unnumbered, so a disabled control keeps its
identity rather than vanishing from the tab order and reshuffling every
other number), keyboard-accessible Elems dispatched through the same
generic onclick path every other tile uses, not raw mouse-only pixels.

Also added a real, generic badge_align_left field to the shared Elem
struct (khtpm_render_core.c) and wired it into draw_elem()
(khtpm_draw_core.c), fixing a live report that the arrow badges ran off
the right edge of the screen - a reusable capability for any edge-pinned
element, not a palettes-only hack, default-off for every existing
consumer.

Confirmed the whole mechanism already applies to the emojis category
automatically (gated on the generic palettes+has-grid flags, never
category-specific) - no separate implementation needed.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
COMMIT_MSG_EOF

echo "--- git log ---"
git log --oneline -5

echo "--- pushing ---"
git push origin main

echo "--- done ---"
