# 🖼️ Images, video, and JS in a khtpm-based browser — one engine, not two

**Status: RECOMMENDATION, 2026-09-02.** Answers a real question raised
reviewing the network-browser work-in-progress: does the shared khtpm
renderer already have what a "real browser" needs, or does it need new
tag types (`img`/`video`), or a separate HTML-specific parser? Written
after checking the actual current renderer source, not guessed.

## The short answer

**No new tag types, no second parser.** khtpm already has a generic
image-drawing primitive (`sprite=` attribute + `sprite.csv`, real,
shipping today for emoji atlases and entity sprites) that a fetched
web image should be converted INTO, the same way everything else that
reaches the shared renderer arrives as data a manager already
projected — not a new `img`/`video` Elem type, and not a rival parser.
This is exactly `CENTROID_GOLD_STD.md`'s core rule applied to media:
one real tree, one real renderer, business logic (media conversion,
JS execution) lives in the manager, never in the shared file.

## 1. Images — the primitive already exists, use it

Real, confirmed-live mechanism (`khtpm_core_render.c`, `khtpm_render_
core.c`): any Elem can carry a `sprite=` attribute pointing at a
directory whose `sprite.csv` the renderer loads and blits
(`load_sprite_csv()`/`draw_sprite_rgb()`) — this is how every emoji
tile and entity sprite in the house already renders, real and shipping
today, not a new idea.

**This is precisely what the network-browser work already planned**:
`nb_media_to_sprite.c` — convert fetched JPEG/PNG/GIF/WEBP into a
64px `sprite.csv` via `stb_image`, then let the existing, unmodified
renderer draw it exactly like it draws an emoji. Correct call, keep
doing it that way. No new tag, no new attribute, no renderer change —
the manager does the conversion (real business logic, real place for
it), the renderer stays generic.

**One real design decision to make explicit** (not yet decided as of
this doc): where does a fetched, converted image's `sprite.csv` live
on disk, and how does its lifetime work — cached indefinitely, evicted
on navigation, capped by total size? The house doesn't have an answer
for "web-fetched cache eviction" anywhere yet; worth a short follow-up
note once real usage shows the actual pattern (don't over-design this
now).

## 2. Video — no existing primitive, but a real, adjacent one to copy

There is no motion/animation-over-time concept in khtpm's Elem/CSS
model as it stands. The nearest real house precedent is `TILE-SYSTEM-
DESIGN.md`'s animated-tile mechanism: **a pure function of wall-clock
time selects which frame to show** — no new renderer capability, just
a time-driven frame-index computation feeding the SAME `sprite=`
mechanism §1 already covers.

**Recommendation**: treat video the same way — decode/extract frames
into a sequence of `sprite.csv` entries (or a frame-indexed variant of
the same format), and let either the manager (simplest — it already
owns the `.chtpm` projection tick loop) or the renderer's existing
animated-sprite time function pick the current frame. This needs real
scoping before building (frame rate vs. projection-tick rate, storage
cost of many frames, whether "video" realistically means short
GIF-style clips first rather than long real video) — flagging the
right precedent to build from, not a finished design.

**Not recommended**: a dedicated video-decode renderer path, a new
Elem type, or reaching for a general media/codec library integrated
into the shared renderer file itself — same reasoning as images, keep
the shared renderer generic and push the real work into a manager/op.

## 3. Duktape JS — manager-side only, never renderer-side, and not a default yes

Real, confirmed-correct shape already chosen by the network-browser
work: `nb_js_eval.c` is its own separate op binary, not code added to
`khtpm_core_render.c` — confirmed via diff, the shared renderer has
zero JS-awareness. **Keep it exactly this way.** `CENTROID_GOLD_STD.md`
rule 2 (business logic lives in a manager/op, never the shared
renderer) applies directly: JS execution is business logic, arguably
the single riskiest kind, and belongs entirely inside the network
browser's own manager/op boundary — the renderer should only ever see
the OUTPUT of a JS run (whatever text/state change it produced),
projected into the `.chtpm` the same way every other manager's real
data already is.

**This is also a real, explicit "check in before building" decision**,
not a default yes — `sep-1-events-SOS.md`'s own Tier 4 list already
named the general case ("Script/Plugin Command... a deliberate
architecture change") for event scripting; running arbitrary fetched
page JavaScript is the same category of decision for the browser.
See `07-install-and-ship/SECURITY.md` §4 for the concrete sandbox
questions (file access, network access from within JS, resource
limits, what the JS bridge can observe/mutate) that need real,
verified answers — not assumed ones — before this ships past a
trusted, small-scale test.

## 4. Nav/interaction conventions — reuse khtpm's, don't reinvent

`CENTROID_GOLD_STD.md` rule 5: real nav-index/digit-jump semantics
stay real for every renderer — this already applies to the network
browser today (its content links get real `nav_index` badges, same
system every other khtpm window uses) and should keep applying as
media/JS land. A "back/reload/bookmark" toolbar, image galleries, or
JS-driven page changes should all still resolve to real Elems with
real `nav_index`/`onclick` — not a parallel, HTML-specific focus/click
model. This is the same answer as §1-3 from a different angle: there
is one real interaction system in this house, and a browser is a
consumer of it, not a reason to build a second one.

## 5. Answering "did we give them everything they need"

For a real, working (if modest) browser: **mostly yes, already.** The
shared renderer + `sprite=` mechanism + manager-owned `.chtpm`
projection + `<cli_io>`/nav-index interaction already cover text,
links, and (once `nb_media_to_sprite.c` lands) images. What's
genuinely missing is not renderer capability — it's the manager-side
work: the media-to-sprite conversion op itself, a real frame-time
design for anything video-like (§2), and the JS op's actual sandbox
boundary being verified, not assumed (§3, `SECURITY.md` §4). None of
these need a new parser or a new tag vocabulary.

## Cross-references

- `CENTROID_GOLD_STD.md` — the core rule this whole doc applies to
  media/scripting specifically.
- `07-install-and-ship/SECURITY.md` §4 — the concrete sandbox
  questions for the JS eval op.
- `08-roadmap/design-docs/` (network-browser-scoped notes: `MEDIA-
  WIRE-NOTES.md`, `JS-ENGINE-NOTES.md`, `SPRITE-GRID-NOTES.md`) — the
  network-browser team's own working notes this recommendation reviews
  against; as of this writing, the actual `.c` files those notes
  describe (`nb_media_to_sprite.c`, `nb_js_eval.c`) had not yet
  reached the real repo — see the branch review that prompted this
  doc.
- `08-roadmap/00-INDEX.md` — TILE-SYSTEM-DESIGN.md's animated-tile
  time function, the real precedent §2 builds on.
