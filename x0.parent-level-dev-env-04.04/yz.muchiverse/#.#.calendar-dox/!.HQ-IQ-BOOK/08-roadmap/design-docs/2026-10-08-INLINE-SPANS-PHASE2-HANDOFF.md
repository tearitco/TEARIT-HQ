# INLINE SPANS — PHASE 2 HANDOFF (written 2026-10-08)

> **ADDENDUM 2026-10-09 — read this first, it corrects §2 below.**
> The draw snippet reproduced in §2 has two real bugs; trust the committed
> tree, not the snippet:
> 1. **Field order.** Line 69 tests the SECOND field for `"link"` AND draws
>    that same field as the text — kind is the FIRST field (`sp..f1`), text
>    the second. As written a link is looked for in its own label and can
>    never match. Committed as `327d9f7e8` with kind-first.
> 2. **Placement.** The snippet sits in the single-line `else`, which never
>    runs for real rows: a flat-list row is `ROW_H` tall so it always takes
>    the multiline wrap path. The committed branch runs BEFORE the
>    multiline/single-line split, fit-gated (`extents.width <= avail_w`).
>
> The actual blocker was neither snippet bug but the **frame round-trip**:
> default-mode windows never call `render_tree()`; they serialize each
> `Elem` through `kh_serialize_frame_elem()` and rebuild a stack `tmp` in
> `kh_paint_frame_line()`. `segments` was in neither half, so `draw_elem()`
> could never see it. Both halves landed in `327d9f7e8` (trailing field,
> pipe-escaped; old short lines still honestly skip).
>
> **Encoder also landed 2026-10-09** (this doc's §6, committed separately):
> `write_ui_projection()` pre-passes `RICH|<nseg>|<para>` groups (para
> stamp added; groups that don't cover their whole run, lone links, and
> overlong/hostile payloads get NO group), verifies each run row-by-row,
> and emits one `c_*_is_rich` row (`label` = whole sentence,
> `segments=` = wire payload) via a new show-gated candidate in
> `network-browser-hq.xhtpm`. The run's `TEXT` rows suppress; its `LINK`
> rows still emit as clickable items. Anything unverified renders exactly
> as before. Two live-found bugs fixed in the same block: groups were
> stamped with the FOLLOWING paragraph's number (`RICH|3|7` for a para-6
> run — flush-then-advance, not advance-then-flush), and the extractor
> trimmed sentence spaces at inline-`<a>` splits (`See thedocsfor more.`)
> — boundary spaces are now kept (single, HTML-collapse semantics), checks
> still run on the trimmed form. Pixel proof: two blue bands, sentence
> span + kept item. Suites: `nb_all_tests.sh` ALL PASS (span suite now 10
> assertions incl. byte-exact payload).
>
> Fixture recipe corrections (§4): needs `<page id="main">` (else
> `find_page` misses and the window is 38px tall), `<text>` DIRECT
> children of `<page>` (a `<panel>` wrapper never gets laid out), and NO
> `<!doctype html>` first line — `parse_element` treats any `<!` as a
> comment scanning for `-->`, so a doctype with no later comment parses to
> nothing. That parser bug is RECORDED, NOT fixed (zero real `.xhtpm`
> files use a doctype, so it is latent).

Where the inline-clickable-spans feature stands after a long session, and
exactly what the next person has to do. Read
`2026-10-07-INLINE-SPANS-DESIGN.md` first for the original design; this
document is the state of play and supersedes its phase ordering.

**Bottom line:** the data contract is DONE, CORRECT and TESTED. The renderer
primitive is half-built (parse committed, draw written but uncommitted). The
one thing standing between here and a shipped feature is getting a screenshot
of a specific window — which turned out to be a one-command problem that cost
four failed attempts because the tooling lies to you.

---

## 1. What is landed (safe, committed, inert)

| What | Commit | Notes |
|---|---|---|
| `RICH`/`RICHSEG` row emission | `5caddd377` era | existed since 2026-10-07, inert |
| `PARA\|<n>` paragraph markers | `6b05e0a2f` | fixes the blocking grouping flaw |
| Span grouping contract test | `6b05e0a2f` | `tests/nb_span_test.sh`, 5 assertions |
| `segments=` attribute on `Elem` | `b37d26d56` | `char segments[8192]`, empty by default |
| `segments=` parsing | `b37d26d56` | optional attribute, entities decoded |

**The grouping contract, in one line:** one span group == one paragraph, with
the link's surrounding text kept in the same group on both sides. Verified:

```
RICH|3
RICHSEG|text|See the|
RICHSEG|link|docs|https://example.com
RICHSEG|text|for more.|
```

Two linkless paragraphs correctly produce no group. `PARA|` markers cost
**zero** pool elements (nothing matches that row kind), so they are free.

---

## 2. What is written but NOT committed

The draw branch. It compiled cleanly but was **reverted rather than shipped**
because no screenshot ever proved it, and it changes pixels in a file every
pane in the house draws through. It is reproduced here so it is not lost.

**File:** `&.widgits/_shared-lib/khtpm_draw_core.c`
**Site:** `draw_elem()`, the single-line (non-multiline) branch, immediately
before the existing `draw_text_emoji(font, &col, badge_label_x, ty, draw_label);`

```c
int drew_segments = 0;
if (e->segments[0] && draw_label == shown_label && avail_w > 0) {
    /* only when it fits un-clipped: a clipped row keeps the
     * plain path so the "..." ellipsis logic stays the single
     * owner of overflow */
    if (extents.width <= avail_w) {
        const char *sp = e->segments;
        XftColor link_col = xft_color("#8fb8ff");
        int sx = badge_label_x;
        while (*sp) {
            const char *seg_end = strchr(sp, '\x1e');
            size_t seg_len = seg_end ? (size_t)(seg_end - sp) : strlen(sp);
            /* fields: kind \x1F text \x1F url */
            const char *f1 = memchr(sp, '\x1f', seg_len);
            const char *f2 = f1 ? memchr(f1 + 1, '\x1f',
                                  seg_len - (size_t)(f1 + 1 - sp)) : NULL;
            if (f1 && f2) {
                int is_link = (f2 - f1 - 1) == 4 && strncmp(f1 + 1, "link", 4) == 0;
                size_t tlen = (size_t)(f2 - (f1 + 1));
                char seg_text[1024];
                if (tlen >= sizeof(seg_text)) tlen = sizeof(seg_text) - 1;
                memcpy(seg_text, f1 + 1, tlen);
                seg_text[tlen] = 0;
                if (tlen > 0) {
                    XftColor sc = is_link ? link_col : col;
                    draw_text_emoji(font, &sc, sx, ty, seg_text);
                    XGlyphInfo sx_ext;
                    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)seg_text,
                                       (int)tlen, &sx_ext);
                    sx += sx_ext.xOff;
                }
            }
            if (!seg_end) break;
            sp = seg_end + 1;
        }
        drew_segments = 1;
    }
}
if (!drew_segments)
    draw_text_emoji(font, &col, badge_label_x, ty, draw_label);
```

Two things deliberately left out:

- **Underline.** The house draws rects via `XFillRectangle` + a GC, not
  `XRenderFillRectangle` + `XftColor` (that call does not compile here). Needs
  the correct GC, not a guess. Colour alone distinguishes segments for now —
  underline and per-segment hit-testing land together.
- **Wrapping.** See §5 — this is the dangerous one, not a shortcut.

---

## 3. The one remaining blocker - SCREENSHOTTING (now VERIFIED WORKING)

**This is the whole thing standing between you and a committed feature.**

`&.widgits/_shared-lib/ops/dump_frame_png_op.c`:

```
usage: dump_frame_png_op <window_id_hex> <out_png_path>
```

It takes an **explicit window id**. A `112` relay dump does NOT reliably
capture the window you are testing - that is what made four attempts fail.
The squashed screenshots were **other windows entirely**, not a geometry bug.

### VERIFIED 2026-10-08 - this exact sequence works

```bash
R=<house>/44.xyz.01.00
# 1. launch
setsid $R/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x \
    "$R" "$R/&.widgits/span-fixture/span-fixture.xhtpm" \
    "$R/&.widgits/span-fixture" &
sleep 8

# 2. find the window BY GEOMETRY, not by name.
#    The fixture window has NO WM_NAME ("has no name" in xwininfo), so
#    grepping for "spans" finds nothing. Its unique 720px width does.
xwininfo -root -tree | grep -E "^\s+0x[0-9a-f]+" | grep -vE "51x51|1x1"

# 3. capture it directly (verified: wrote a 720x38 png showing the title)
$R/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x 0x1a00001 /tmp/span.png
```

Step 2 matters: the window is **unnamed**, so any name-based lookup fails.
Geometry is the only reliable handle.

### Known fixture defect, still open

The fixture window came up **720x38** - CSS width applied, height collapsed,
background stayed light. So the fixture's `.css` is at best partially
applied. Unresolved. Possible causes to check in order:

1. the same-stem merge not firing for a non-`.chtpm` file (the code comment
   says "same stem as network-browser-hq.**chtpm**" - this fixture is `.xhtpm`)
2. `height:` needing a different unit or a class-qualified selector
3. the merge loading but a later default overriding height/background

Verify by capturing and checking whether `window { width: 720px }` is what
produced the 720 - that at least proves the file IS being read.

---

## 4. The fixture (recreate it; it is not committed)

`&.widgits/span-fixture/span-fixture.{xhtpm,css}`

`.xhtpm` — window must carry `class="span-fixture database-window"`:

```html
<window label="Spans" class="span-fixture database-window">
  <page>
    <row>
      <text class="quiet" label="inline spans"/>
      <item class="quiet" label="Back"/>
    </row>
    <panel>
      <text class="plain" label="See the docs for more."/>
      <text class="seg"  label="See the docs for more."
            segments="text\x1fSee the \x1f\x1elink\x1fdocs\x1f\x1etext\x1ffor more.\x1f"/>
      <text class="seg2" label="plain fallback row"/>
      <text class="seg3" label="two links"
            segments="text\x1fGo to \x1f\x1elink\x1falpha\x1f\x1etext\x1fthen \x1f\x1elink\x1fbeta\x1f\x1etext\x1fnow.\x1f"/>
    </panel>
  </page>
</window>
```

**The `\x1f`/`\x1e` bytes must be literal control characters**, not `&#31;`
entities — write them with python or `printf`. This is what made the first
fixture attempt silently wrong.

`.css` — geometry is CSS, merged by same stem (`khtpm_core_render.c` replaces
the last `.` in the `.xhtpm` path with `.css`):

```css
window { width: 720px; height: 300px; }
window.span-fixture { width: 720px; height: 300px; background-color: #141414; color: #dddddd; }
.plain, .seg, .seg2, .seg3 { color: #cccccc; }
```

The fixture's **ASCII frame is reliable** and is a good cheap check
(`#.desktop/ascii_frames/<pid>.frame.txt`, pid-scoped) — it proves parse and
layout even when the PNG path is misbehaving.

---

## 5. Do NOT implement wrapping yet

`khtpm_draw_core.c:1545`, in the file's own words:

> *"this MUST match `scroll_row_span()`'s own `avail_w` exactly — that
> function decides how many ROW_H units this row gets laid out with, this is
> what actually draws into that space."*

That is the 2026-09-03 overlap incident. The layout pass
(`scroll_row_span()` in `khtpm_core_render.c`) measures a row from its plain
`label`; the draw pass measures what it paints. If segments are measured
differently in one and not the other, the overlap bug returns.

That is why the shipped branch is single-line-and-fits only: a one-line row
already has height one, which the layout pass already reserved. Wrapping
segments needs `scroll_row_span()` taught about segments **first**, or the
two passes taught to share one measurement.

---

## 6. After the draw is committed — the manager encoder

Do this **second**, not first. It is what both original bugs lived in.

1. Pre-pass over page.state pairing `RICHSEG` rows to their `RICH` group.
   They live at the **END** of page.state, so the single forward pass cannot
   see them. Probed and proven: `ngrp=1 first_nseg=3`, branch reached.
2. Projector emits `c_N_segments=<payload>` plus the flat `c_N_text=`.

**Two bugs to not repeat:**

- **Ambiguous separators.** Segments joined by `\x1E`, the three fields
  inside a segment by `\x1F`. Using `\x1F` for both produces 8 separators for
  3 segments with no findable boundary; the parse yields nothing and the
  encoder *silently emits no row*, which looks exactly like the branch never
  ran. Cost two wrong diagnoses.
- **Probable stack pressure (UNVERIFIED).** `char payload[8192]` +
  `char flat[4096]` as locals in the projector's row loop produced
  `c_-1305130384_text=(null)` — garbage `%d`, null `%s`. Heap-allocate or
  shrink. Precedent in the same function: an out-of-bounds read once produced
  `free(): invalid size`.

---

## 7. Hit-testing (not started)

Map click x-offset → segment → its action; text segments inert. One nav slot
per LINK segment. Scoped the same way as the draw: single-line rows first.

---

## 8. Verify with

```
sh <network>/tests/nb_span_test.sh      # grouping contract, 5 assertions
sh <network>/tests/nb_all_tests.sh      # projection + layout + span + form
```

Current state: span 5/5, layout 12/12, projection 6/6, form gate 30/30.

---

## 9. Traps, all of which cost real time

| Trap | Reality |
|---|---|
| Launch argv | `<house_root> <xhtpm> <package_dir>` — not the shape the usage comment suggests |
| Window geometry | CSS only, same-stem merge. `width=` on `<window>` does nothing |
| Screenshots | `dump_frame_png_op` needs an explicit window id; `112` may capture a different window |
| Fixture `\x1f` | literal control bytes; `&#31;` does not survive |
| `\x1E` vs `\x1F` | two distinct bytes, or the payload is unparseable |
| `grep -c` on a missing file | prints nothing, which reads as `0`. See AGENTS.md |

The screenshot one is the general lesson: **confirm the artifact is the
artifact** before reading anything into it. Same family as the stale counter
and the missing frame, both now guarded in the test harness.

---

## 10. Sequence

1. Launch fixture → get window id → screenshot (baseline, no draw branch)
2. Re-apply §2 draw branch → rebuild → screenshot → **commit if the link
   segment is visibly tinted**
3. Regression-check the browser window and one other pane
4. Manager encoder (§6), then hit-testing (§7)
5. Wrapping only once `scroll_row_span()` agrees (§5)