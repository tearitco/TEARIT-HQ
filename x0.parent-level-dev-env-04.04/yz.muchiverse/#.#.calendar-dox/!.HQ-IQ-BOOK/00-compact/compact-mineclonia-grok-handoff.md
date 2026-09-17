# compact-mineclonia-grok-handoff.md — PALCRAFT, delegated to Grok, live as of 2026-09-15

Quick orientation for anyone picking up PALCRAFT (the Minecraft-
inspired desk) mid-flight. Real handoff, real spec, real open
questions — this is a pointer doc, not a restatement. **Confirmed
still active as of 2026-09-18**, not stale — don't quietly resume or
overwrite it without checking the co-lab room state first.

## What PALCRAFT actually is, technically

**Not a new engine.** PALCRAFT is a new map + a real block-event
catalog layered on piececraft-hq's **existing, already-live** voxel
world (`@.apps/piececraft-hq/pieces/world_01/`, proven live by the
`chicken,6,10,17` test entity in `world_01/animals.txt`).

- **Camera/movement/possession stay piececraft-hq's own** — owned by
  board-viewer's `bv_menu_input.c`, shared in via
  `pchq_board_projector.c`, driven through the xelector entity. No new
  camera/movement/possession code.
- **All block/interaction behavior is scripted as real, auditable
  Common Events** (RPG-Maker-style, on-disk, openable in the existing
  events-hq editor) — never hardcoded into PALCRAFT's own C or
  renderer. This is the whole point: agent relay can audit and test
  block behavior by reading `event.ir.pdl` text, without rendering a
  single pixel.
- **The one genuinely new piece of C, confirmed in `GROK.md`**: voxel
  *placement*, in `pc_menu_input.c`/`pc_generate_chunk.c`. Voxel
  *removal* already exists (`pc_menu_input.c` ~line 806); placement is
  the real, symmetric gap. `GROK.md` explicitly flags this diff for
  review before it lands, same coordination standard as any shared
  piececraft-hq ops file (not `khtpm_core_render.c` itself, but same
  reasoning).

## Real current status

- Delegated to Grok by the user, via the real co-lab-hai multi-agent
  channel (not chat-hai) — task started **2026-09-15**, session
  `1788873184`.
- Handoff doc: `13.agent-coms/2026-09-15/GROK.md`. Spec:
  `08-roadmap/design-docs/PALCRAFT-DESIGN.md`.
- One real proof-of-concept already landed before/at handoff:
  `common_events/palcraft_sign_onclick/` — a hand-compiled two-node
  event package (`show_text` → `scrolling_text`) matching
  `common_events/cdda_beartrap_touch/`'s accepted shape exactly. Copy
  its directory shape for every other block, don't re-derive.
- **Three open questions, from PALCRAFT-DESIGN.md's own end** (Grok
  was asked to answer these in its first co-lab post):
  1. Propose the exact edit/play-mode toggle mechanism (new key name,
     which input flips it, what happens to in-flight possession) —
     none of this exists yet, it's a real design proposal, not a
     lookup.
  2. Which mineclonia rows (by `id=`) to include in the v1 Common
     Event port batch beyond `sign` (already landed) — all remaining
     `need=-` rows, or a smaller next slice?
  3. Any real gap in `pc_menu_input.c`'s placement-side voxel API that
     only shows up once actually in the code (the spec was written
     from a read of the removal side only).

## How this should be undertaken (per the real docs, not invented here)

- **Events-driven, not engine-driven.** Every block type gets its own
  `common_events/palcraft_<block_id>_<trigger>/` package, compiled
  through the existing chain
  (`event.ir.pdl → event.pal → cmd_N.sh` via
  `&.widgits/events-hq/ops/khtpm_events_hq_manager.c`). `<block_id>`
  comes from mineclonia's own `TILE | id=...`; `<trigger>` from its
  `trigger=` column (`on-click` | `parallel` | `player-touch`).
- **Reuse real precedent, don't re-derive.** `PALCRAFT-DESIGN.md` §0
  names five real files Grok was told to read before writing anything:
  - `@.apps/piececraft-hq/pieces/world_01/` (voxel world + the
    `chicken` test entity)
  - `ops/pc_menu_input.c` (existing voxel removal, the pattern
    placement must mirror)
  - `@.apps/piececraft-hq/open_pchq_board.sh` (the real x11-hq
    launcher PALCRAFT reuses, not a new launcher family)
  - `#.ref/menu/event-guides/mineclonia/` — `mcl_core.pdl` +
    `interact.pdl` — the real, pipe-delimited block/interactable
    catalog (id, texture, trigger type, RMMV DB, command list,
    missing-feature `need=` notes). Confirmed present in this repo;
    verified content includes real rows like `sign`, `chest`,
    `furnace`, `tnt`, `crafting_table`, `water`, `lava`.
  - `#.ref/menu/event-guides/examples/mcl_chest/` — a worked full
    example event package.
  - A sample map already wired to these tiles:
    `pieces/system/maps/mineclonia_sample/events.pdl`.
- **Rows with a `need=` gap are real, flagged follow-up, not silently
  stubbed** — `shop_processing`, `transfer_player`, `set_move_route`,
  `fadeout`/`fadein`, `play_se` — tracked in a `## Deferred (need= gaps)`
  section Grok appends to `PALCRAFT-DESIGN.md` itself as it works
  through the catalog.
- **Coordinated through the real co-lab-hai relay.** Grok posts via
  `&.hq-apps/co-lab-hai/ops/colab_hai_post.sh`, human-approved each
  time; Sonnet reads/tests live through the same
  `#.desktop/entity_menu_history/<pid>.txt` mechanism every
  khtpm-family window already uses for relay-driven testing. If a
  Grok PR would require *only* a screenshot to verify what a block
  does, that PR is out of spec per the design doc itself.
- **Track 1 cross-reference**:
  `08-roadmap/design-docs/TODO-2026-09-15/MAJOR-PRIORITIES-2026-09-15.md`
  covers the same initiative from a sibling angle, written the same
  day — "mineclonia as a real, loadable file:desk," also owned by
  Grok. Its build order (wander → real db-hq categories → desk
  authored → event pages wired per-tile → NPC AI) and its flagged
  must-fix-first blocker (no per-entity trigger scoping — today
  exactly one `player-touch` Common Event exists house-wide, which
  breaks the moment mineclonia needs multiple distinct triggers) both
  bear directly on PALCRAFT's own event-authoring work and should be
  read together with `GROK.md`/`PALCRAFT-DESIGN.md`, not separately.

## A new architectural note — open sequencing question, not a decision

**Flagging this here as a genuinely new observation, not something
already written down anywhere else — an open question for whoever
picks up either PALCRAFT or the watch-layer thread next, not a
unilateral call.**

`08-roadmap/design-docs/LLMUD-HACK.md` §2 names a real, currently
unfilled infrastructure gap: this house already has two real
"watching" surfaces — the nav/interact relay
(`entity_menu_history/<pid>.txt`) and Claude Code's own tool-call
transcript — but nothing yet *consumes* either as an observation
stream to build a Behavior Bank from (§2's own framing: "wire a
consumer to infrastructure that already exists," not build new
capture). As of this doc, `DUSTOPIA-HACK.md` does not exist yet in
`08-roadmap/design-docs/` — check again if picking this up later, the
brief that prompted this note anticipated it might land as a sibling
of `LLMUD-HACK.md`.

The observation worth surfacing: PALCRAFT's own build convention —
one Common Event package per block type, authored, compiled, and
tested through the same relay mechanism §2 already names as a real
watch surface — means every block Grok authors during this handoff
*is itself* a live, real instance of the exact authoring process a
watch layer would want to learn from (which `trigger=` type gets
paired with which command shape, what sequences repeat across
`sign`/`chest`/`furnace`/etc., how a `need=` gap gets deferred vs.
stubbed). That makes a case that building even a minimal watch/observe
consumer **before or alongside** PALCRAFT's event-authoring batch —
not after it ships — could turn this handoff into real, in-house
training data for the Behavior Bank idea, rather than a hypothetical
qwen-doc example. This is the same self-building/behavior-bank
direction `LLMUD-HACK.md` describes (and `DUSTOPIA-HACK.md` may also
describe, once it exists) — PALCRAFT would just be a real, live
source rather than a theorized one. Left open on purpose: whether
that sequencing is worth the added surface area against an
already-active, human-approved delegation, or whether it's cleaner to
let PALCRAFT ship first and mine its `master_ledger.txt`/Common Event
history after the fact.

## Pointers, not restatement

- `13.agent-coms/2026-09-15/GROK.md` — the real, live handoff.
- `08-roadmap/design-docs/PALCRAFT-DESIGN.md` — the full technical
  spec, §0 precedent list, and the three open questions in full.
- `08-roadmap/design-docs/TODO-2026-09-15/MAJOR-PRIORITIES-2026-09-15.md`
  — Track 1, the sibling "mineclonia as a real file:desk" angle.
- `08-roadmap/design-docs/LLMUD-HACK.md` — §2 specifically, for the
  real watch-surface gap this doc's new note builds on.
- `44.xyz.01.00/#.ref/menu/event-guides/mineclonia/` — `mcl_core.pdl`
  + `interact.pdl`, the real block/tile catalog PALCRAFT builds from.
