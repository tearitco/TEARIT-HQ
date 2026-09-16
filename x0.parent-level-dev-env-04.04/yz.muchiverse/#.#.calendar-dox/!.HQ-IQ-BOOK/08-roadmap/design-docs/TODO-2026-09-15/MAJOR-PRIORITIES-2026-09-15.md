# Major Priorities — 2026-09-15

Direct live instruction: "id like to discuss plans for completing the
remainder of this house's major-priorities... id like to brainstorm on
these first, and hammer out not just 'what we want done, but how it
should be done'." Written after a live Q&A pass (this same date) that
settled several real, previously-open design forks — those answers are
load-bearing decisions here, not suggestions, and are marked as such.

Grounded against real, existing status, not written blind:
- `12.calendar/2026-09-15/user-report.md` — the event-trigger layer is
  **closed** (player-touch triggers fire real Common Events with zero
  manual Play-button clicks, proven live). The thing it was built to
  unblock, `PLAY-MODE-ENTITY-HARNESS-DESIGN.md`, is genuinely 0%
  started.
- `12.calendar/2026-09-15/notes.md` — real IPC-contract lesson from
  today's dock-bar bug (stale hardcoded constant drifted from a header
  template); AI/networking work explicitly flagged there as "no
  concrete plan yet."
- `08-roadmap/design-docs/TEST-GAMES-ROADMAP.md` — TSOTS (own IP,
  sword-possessing-Bible-characters) and TPMOJIO share a combat
  resolver; both "not started." DSR is next-up per notes.md.
- `#.#.calendar-dox/1-1.HARNECIENT.SMOL/` NIGHT_04–14 — real prior
  conversations that already worked through FSM-first-then-Gemma,
  GOAP+RL+decision-mode, attention-weights-from-Gemma, inverse RL,
  h-ai-lab+registry, and "the picker instinct." Titles alone imply
  real, already-reached conclusions — Track 2 below starts by reading
  and synthesizing these, not re-deriving from scratch.
- `44.xyz.01.00/#.ref/menu/event-guides/mineclonia/` — **already has
  real content** (`mcl_core.pdl`: dirt/stone/ores/water/lava tiles with
  real `trigger=`/`cmds=`/`db=` columns; `interact.pdl`). Track 1 is
  not a blank-page build — it's wiring already-authored reference data
  into a real, loadable desk.

---

## Cross-cutting decisions settled today (apply to every track below)

These came out of direct live Q&A this session and are now the shared
foundation every track below builds on — write code against these
answers, don't re-litigate them per-track.

1. **NPC "move" is a Common-Event scripted command** (direct answer:
   "it should be common event scripted, but so should ai"). NOT a
   hidden built-in verb, NOT AI-opaque — authored in event data the
   same way `mr_show_text`/`mr_move_to_entity` already are.
2. **NPC movement fires when Play Mode is ON, inside a loaded
   file:desk** (direct answer: "npc's are meant to move when 'play =
   on' is triggered in a 'file:desk' environment"). This is the real
   trigger condition — `PLAY-MODE-ENTITY-HARNESS-DESIGN.md`'s own
   `khtpm_play_mode.state.txt` flag, gated on a real map being loaded
   (the same `map_id`/`desk_id` pair this session's desk-switching work
   made real).
3. **Rendering a moving NPC reuses board-viewer's existing per-frame
   entity read** (direct answer: "1. its like u said" — no new render
   path). Same mechanism already proven for `hero_01`/`xelector_01`:
   `bv_render_3d.c` reads entity state files live every frame; an NPC
   is just another entity state file.
4. **Position/action updates flow through the real master_ledger
   standard**, not a new mechanism (direct answer: "theres probably a
   master-ledger std that we use or should use"). Confirmed real:
   `pc_menu_input.c`'s `ledger_append()` → `data/master_ledger.txt`,
   already the exact channel `pc_trigger_watcher.c` watches for
   `touched_npc` lines to fire player-touch events. NPC move should
   append through the same function, not a parallel path.
5. **A new, reusable "AI-framework Common Event" vocabulary is real,
   shared infrastructure, not per-game** (direct answer: "we may have
   another set of events just for commonly used ai frameworks where we
   can slot stuff in (fsm/goap/llm/rl/irl)... reusable, readable, user
   modifiable for users who aren't ml phd's, we want to make it just as
   easy for them as events are for users who arent coders"). This is
   the literal connective tissue between Track 2 (research) and
   Track 1/3 (usage) — **build it once**, as event-page infrastructure,
   and every track below authors against it rather than inventing its
   own AI hook.

---

## Track 1 — Mineclonia as a real, loadable file:desk

**Owner: Grok, working directly in this live house** (direct answer:
"Direct in this live house" — real files, real build/test loop, not a
disconnected exported spec).

**Goal, in the user's own words**: not just "play Minecraft" — "a
working example of many different events and db-hq items coming
together to form a game, that can be re-referenced." The deliverable
IS the reference pattern future games copy, not just a fun map.

**Real starting material** (not from zero):
- `#.ref/menu/event-guides/mineclonia/mcl_core.pdl` — real tiles
  already declared with `trigger=`/`cmds=`/`db=`/`need=` columns
  (dirt/stone/ore/water/lava, on-click vs player-touch already
  distinguished).
- `#.ref/menu/event-guides/mineclonia/interact.pdl`.
- The desk hierarchy this session built (`file = dir, desk = map`,
  `pieces/system/maps/<project>/<desk_id>/{map.txt,extrusion.pdl}`,
  `game.pdl`'s `n_desks`/`desk_N_id`/`desk_N_label` rows) — mineclonia
  becomes a real project dir under `pieces/system/maps/mineclonia/`,
  loadable through the File dropdown fixed today, no new load mechanism
  needed.
- The generalized extrusion table (`pc_generate_chunk.c`'s
  `load_extrusion_table()`) — mineclonia's block heights (stone/ore
  columns vs. flat dirt/sand) are a real per-glyph table, same
  mechanism as this session's own `test_terraces` proof.

**Build order** (borrows `PLAY-MODE-ENTITY-HARNESS-DESIGN.md`'s own
staged order, now unblocked by today's settled decisions above):
1. **Wander** — smallest provable "move" Common Event. One NPC, one
   `mr_move_to_entity`-style command, fired on a tick while
   `play=on`, writing through `ledger_append()`, picked up automatically
   by board-viewer's existing render read. Prove this end-to-end before
   anything else.
2. **Real db-hq categories for mineclonia** — items/blocks/mobs
   authored as real db-hq entries (Items/Tilesets per `mcl_core.pdl`'s
   own `db=` column), not hardcoded C. This is the "db-hq categories
   only" constraint from the user's own framing.
3. **Mineclonia desk authored** on the real hierarchy above, using
   `mcl_core.pdl`'s tile vocabulary as the glyph/extrusion source.
4. **Event pages wired per-tile**, using `interact.pdl` +
   `mcl_core.pdl`'s own `trigger=`/`cmds=` columns as the real spec —
   translate declared triggers into real Common Events via the
   player-touch/on-click trigger mechanism this session proved live.
5. **NPC AI, once wander is proven**, authored via Track 2/3's shared
   AI-framework event vocabulary (see below) — not a mineclonia-specific
   AI hack.

**Real, must-fix-first blocker** (flagged, not optional): `user-report.md`
already flags **no per-entity trigger scoping** — today there's exactly
one `player-touch` Common Event house-wide, so it doesn't matter which
one fires. Mineclonia needs MANY distinct triggers (water, lava, ore
blocks, each mob). This breaks the moment a second trigger exists, so
it must land before step 3 above, not after.

**Encapsulated task-list framing for Grok**: real RPG-Maker-style
events + db-hq categories only + house nav/cam/control standards
(already fully ported per `pc-hq camera/POV keys` memory) — no new
subsystems invented, every piece above already has a real, proven house
mechanism to build on.

---

## Track 2 — AI-ARC (hai-studio)

**Sequenced discovery-then-iteration** (direct answer: "both, in
sequence thru discovery and iteration as we go").

**Phase A — discovery (do this first, real, not skippable)**: read and
synthesize `#.#.calendar-dox/1-1.HARNECIENT.SMOL/NIGHT_07` through
`NIGHT_14` into one real architecture-synthesis doc. These titles
(`FSM_FIRST_GEMMA_SECOND`, `GOAP_RL_AND_THE_DECISION_MODE`,
`THE_FINAL_BOSS_ATTENTION_WEIGHTS_FROM_GEMMA`, `INVERSE_RL_AND_FAMOUS_LLM`,
`H_AI_LAB_AND_THE_REAL_REGISTRY`, `THE_PICKER_INSTINCT`) imply real,
already-reached conclusions from a past conversation — extract them,
don't re-derive. Concrete Phase A output: a synthesis doc stating what
was already decided (sequencing: FSM before Gemma/LLM; GOAP+RL as the
"decision mode"; attention-weights-from-Gemma as the eventual
capstone) and what's still genuinely open.

**Phase B — iteration**, once Phase A exists: pick the smallest
provable first experiment. Per the user's own scope — "rl/irl, in
house or on lan llms, train local weights and models using diffusion
and handpicked weights, using harnecient hacks, or other light weight
models, testing chat and meta llms, fsms, goaps" — the FSM-first
sequencing from NIGHT_07's own title suggests the real first provable
step is an FSM-driven experiment, not jumping straight to local-model
training. Confirm against the Phase A synthesis before committing.

**Real connective tissue to Track 1/3**: the "AI-framework Common
Event" vocabulary from the cross-cutting decisions above (FSM/GOAP/
LLM/RL/IRL as reusable, user-modifiable event-page slots, exportable
"directly into entities/tiles/pc-hq worlds") is Track 2's actual
deliverable surface — hai-studio experiments should produce THESE
event pages, not a separate, disconnected research artifact.

---

## Track 3 — Minigames (DSR / Dwarf Fortress / TSOTS / TPMOJIO) + AI direction

**NPC move/render/trigger foundation**: fully covered by the
cross-cutting decisions above — Common-Event scripted move, board-viewer's
existing render read, master_ledger as the update channel. Nothing new
needed here beyond what Track 1 builds first (mineclonia proves the
same mechanism these minigames will reuse).

**Real, new shared infrastructure needed**: the same AI-framework
event-page vocabulary from Track 2 — in-game NPC AI and "game manager
AI" for these minigames should be authored through it, not built
per-game. This is explicitly the user's own ask ("how we may use ai
for the in game ai/ game manager ai as well as for the research and
development and 'game building aspects'").

**Per-game real status** (from `TEST-GAMES-ROADMAP.md`, not guessed):
- **TSOTS** (own IP, sword-possessing-Bible-characters) — shares a
  combat resolver with TPMOJIO; PVP/battle resolution "not started";
  accuracy-based damage mechanic "not started, leans on PVP/battle
  resolution." High-motivation per that doc's own framing.
- **TPMOJIO** — same combat resolver dependency as TSOTS; grid
  movement/tactics-range partial (`mr_move_to_entity` exists, no
  combat resolver yet).
- **DSR** — flagged in `notes.md` as literal next-up ("Back to game
  mechanics (DSR / the games roadmap)"), has its own real toy.pdl/app
  already (`&.hq-apps/dsr/`).
- **Dwarf Fortress-test** — only appears as a test-toggle name in
  `TEST-GAMES-ROADMAP.md`; no real design doc found this session.
  **Flagged as needing its own follow-up doc** — not written here,
  don't assume scope for it yet.

**Recommended real next step for this track**: since TSOTS/TPMOJIO both
block on "PVP/battle resolution, not started," and DSR is already
next-up per notes.md, the practical sequencing is DSR first (real,
already has an app shell) while Track 1's mineclonia work proves the
NPC-move/AI-event foundation, THEN tackle the shared TSOTS/TPMOJIO
combat resolver once that foundation is real and tested.

---

## Cross-cutting blocker (must land before Track 1 completes)

**Per-entity trigger scoping** — `user-report.md`'s own flagged gap.
Today: exactly one `player-touch` Common Event exists house-wide, so a
touch fires whatever's there regardless of which tile/entity triggered
it. The moment a second trigger exists (mineclonia needs many), this
breaks — every matching-trigger event fires, not just the intended one.
Real, contained fix (add real entity/tile scoping to
`pc_trigger_watcher.c`'s match logic), but must land as part of Track 1,
not deferred past it.

---

## Separately tracked, not part of this doc

- **export-hq** (real picker GUI window around `create-package.sh`,
  chosen over the thin-wrapper option) — scoped as its own follow-up
  build, tracked separately, not detailed here.
- **Dock Bar Migration Phase 2/3** (generic scrollbar) — already
  tracked in `DOCK-BAR-GENERIC-LAYOUT-MIGRATION.md`, unrelated to the
  three tracks above.
