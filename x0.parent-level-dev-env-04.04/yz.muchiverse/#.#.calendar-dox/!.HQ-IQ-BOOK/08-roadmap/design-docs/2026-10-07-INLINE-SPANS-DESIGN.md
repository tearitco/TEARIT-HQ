# INLINE SPANS DESIGN — clickable links inside flowing sentences

**Status:** design only (2026-10-07). No code. Needs renderer + xhtpm
contract work, so it waits for lane alignment — do NOT start building
unilaterally.

## Problem

Today a paragraph with links becomes TEXT + LINK + TEXT stacked rows
(split) — every link clickable, but sentences break apart visually. The
old folding kept sentences whole but swallowed hrefs. Neither is what a
browser does: blue underlined spans inside flowing text.

## BLOCKING CONTRACT FLAW found 2026-10-08 — fix before phase 2

`append_rich_rows()` groups a maximal run of consecutive `TEXT`/`LINK`
rows. The extractor emits **one `TEXT` row per paragraph with no boundary
marker**, so the grouping has no way to know where a paragraph ends.

Live proof on `tests/fixtures/mini-article.html`, two separate `<p>`
elements:

```
TEXT|First paragraph of body text. ...
TEXT|Second paragraph with a link inline.
TEXT|See the
LINK|https://example.com|docs
TEXT|for more.
RICH|5                      <-- 5 segments: BOTH paragraphs welded in
```

One span group must be ONE paragraph. As written, the renderer would
receive two unrelated sentences as a single inline run and draw them on
one line — the fidelity bug this whole feature exists to remove, moved
from "links on their own rows" to "sentences run together".

Cheapest fix that costs nothing at render time: have `FLUSH_LINE()` also
emit a `PARA|<n>` marker row. `append_rich_rows()` breaks the group there
and everything else ignores it — the projector matches on row KIND, so an
unknown kind becomes no element and **zero** extra pool elements, unlike
every other addition this session. It does grow page.state, so all
snapshots need re-cutting.

Do phase 2 only after this lands, or the renderer gets built on a wrong
grouping and the mistake surfaces as a layout bug.

## Row schema (manager side, our lane)

New row kind alongside TEXT/LINK:

```
RICH|<nseg>
RICHSEG|<kind>|<text>|<url-or-empty>
```

- `RICH|3` opens a rich paragraph of 3 following segments.
- Each `RICHSEG` is `text|<label>|` or `link|<label>|<url>`.
- Extractor emits these from `<p>` runs today; projector passes them
  through merge untouched (worker never emits RICH — falls back to
  split rows when absent, so old pages keep working).

## Projector → xhtpm (shared contract, needs both lanes' eyes)

Per RICH row, the projector emits:

```
c_N_kind=rich, c_N_is_rich=1, c_N_nseg=3
c_N_s0_kind=text, c_N_s0_text=..., c_N_s0_action=
c_N_s1_kind=link, c_N_s1_text=..., c_N_s1_action='go' 'url'
...
```

The xhtpm repeat body gains one rich candidate. Problem: segment count
varies per row and the template engine has no nested repeat — so the
candidate must be a SINGLE element type the renderer lays inline
(e.g. `<richtext id=... segments="${...}">` with an encoded payload),
not N child elements. Encoding is MANDATORY and was wrong here — see the
encoding fix below. Segments are joined with **\x1E**; the three fields
inside a segment are separated with **\x1F**:

```
segments="text\x1FSee the \x1F\x1Elink\x1Fdocs\x1F'https:\/\/x'\x1F..."
           ^kind  ^text      ^url          ^next segment
```

`\x1E` (record separator) never appears in a label after `uisan()`, so
both bytes are safe as delimiters.

## ENCODING FIX (2026-10-08) — the original spec was ambiguous and cost a day

The doc previously said only *"unit-separator-joined segments
(`kind\x1Ftext\x1Furl`)"*, never naming the joiner. Implementing that
literally uses `\x1F` for BOTH the field separator and the segment join,
so a 3-segment group emits 8 separators with no way to tell where a
segment ends. The reader walks it field-by-field, produces nothing, and
the encoder silently emits no row at all — which looked exactly like the
branch never running, and cost two wrong diagnoses.

**Encoder and decoder must agree on two distinct bytes.** If you are
changing either, change this paragraph too.

## Two bugs found in the manager-side encoder (2026-10-08)

Both in code with no consumer yet, both reverted rather than shipped:

1. **Ambiguous separators** (above) — fixed by the `\x1E`/`\x1F` split.
2. **Probable stack pressure.** The branch added `char payload[8192]` and
   `char flat[4096]` as locals inside the projector's row loop, which
   already carries per-iteration buffers. Output showed
   `c_-1305130384_text=(null)` — a garbage `%d` and a null `%s`, the
   signature of stack corruption. Unverified: the code was reverted before
   it could be confirmed. Heap-allocate or shrink both. Precedent in the
   same function: an out-of-bounds read once produced
   `free(): invalid size`.

The pairing pre-pass itself was proven correct by probe:
`ngrp=1 first_nseg=3`, branch reached — so neither bug was the grouping
or the row walk.

## REORDERED PHASE 2 (2026-10-08) — build the renderer half FIRST

The original order put the manager encoder before the renderer. Both bugs
above live in an encoder with nothing consuming it, which is the worst
order: a bug there blocks the feature while contributing nothing.

New order:

1. **Renderer primitive, driven by a static `.xhtpm` fixture** with a
   literal `segments="..."` attribute. No manager involved.
2. Regression-check other panes in isolation — the shared renderer is the
   risky part and should be verified without the browser in the loop.
3. **Then** the manager encoder, now written to satisfy a decoder that
   already exists and is tested.

This also stops bug 1 being inherited: the decoder is written first, so
the encoder must match a real, exercised spec instead of a paragraph of
prose.

## Renderer (shared core, coordination required)

- Layout: measure each segment with Xft extents (same font path as
  `wrap_line_count`), flow across `avail_w`, wrap between segments AND
  inside long text segments (word boundaries). Multi-row span math must
  match `scroll_row_span` or the overlap bug from 2026-09-23 returns.
- Draw: text segments in body color, link segments in link color +
  underline. Reuse draw_elem's label path per segment run.
- Hit-test: click maps x/y to a link segment → its action. Text
  segments inert.
- Nav: one nav slot per LINK segment (text segments skipped), visible
  rows only. Relay digits address slots globally as today.
- Clip, never translate; idempotent per-frame; generic scrollbar owns
  scroll. (Per khtpm-house-standards layout rules.)

## Phases — SUPERSEDED 2026-10-08, see REORDERED PHASE 2 above

Original order, kept so the change is visible. It front-loaded the
manager encoder ahead of the renderer, which is how two bugs landed in
code with no consumer.

1. ~~Manager: RICH/RICHSEG emission + merge passthrough + projector vars~~
   — emission and merge ARE done; the projector encoder is **not**, and is
   now step 3.
2. ~~Renderer inline layout/draw/hit-test~~ — now **step 1**, first, against
   a static fixture.
3. xhtpm candidate + end-to-end proof on a real article paragraph.

## Explicitly not in scope

- Bidi, hyphenation, justification, ruby.
- Images inside sentences (block media stays block).
- Selection/copy across segments.
