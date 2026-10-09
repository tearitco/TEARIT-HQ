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
not N child elements. Encoding: unit-separator-joined segments
(`kind\x1Ftext\x1Furl`), since labels never contain \x1F after uisan.

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

## Phases

1. Manager: RICH/RICHSEG emission + merge passthrough + projector vars
   (our lane, safe alone — renders as nothing until xhtpm/renderer land,
   so gate behind a `rich=1` marker the template ignores meanwhile).
2. Renderer inline layout/draw/hit-test (shared — needs review).
3. xhtpm candidate + end-to-end proof on Blockly paragraphs.

## Explicitly not in scope

- Bidi, hyphenation, justification, ruby.
- Images inside sentences (block media stays block).
- Selection/copy across segments.
