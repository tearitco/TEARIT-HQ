# Answers to 2026-09-29 Concerns

**Companion to:** `0.my-concerns.md` (this directory)
**Also filed at:** `#.#.calendar-dox/!.HQ-IQ-BOOK/RUSSIAN_DOLL_HOUSE_DESIGN/00-INDEX.md` (pointer only)
**Method:** four parallel read-only research passes over the actual house
codebase and HQ-IQ-BOOK, then synthesized here. Every claim below is
sourced to a real file. Where something doesn't exist, that's stated
plainly rather than guessed at — this doc distinguishes REAL (built,
verified) from DESIGNED (a doc exists, nothing runs) from ASPIRATIONAL
(neither exists yet).

---

## 0. The pipeline name, and how far "in-house learning" really is

**It already has a name.** `08-roadmap/design-docs/A-TEARIT-IS-ALL-YOU-NEED.md`
(2026-09-22) is exactly this: the combination of Claude (general
reasoning) + Gemma 270M (cheap, DESCRIBE-only) + FSM + GOAP + Concept
Bank + tomom, unified under the house law **"DESCRIBE, never
CLASSIFY."** Call the whole thing **"A TEARIT"** (or "the TEARIT
pipeline") going forward — stop saying "irl/fsm/banks/tomom goap" each
time; that doc is the spec to point at.

**What's REAL today, verified 2026-09-27/28:**
- The constrained-format Gemma DESCRIBE call (`PIPELINE-EMOJI-DIAGRAM-REVISED.md`)
  — proven against the real production LAN model, including a real,
  reproduced failure mode (free-form prompting hallucinated a "thirst"
  field) that justified the constrained format.
- `concept_edit_validate.c` — a real, small, deterministic C validator.
  **Only `type=spoke_weight_delta` is implemented.** The other three
  record types named in the TEARIT spec (`new_concept_node`,
  `fsm_transition_describe`, `goap_action_describe`) are named but
  **not implemented.**
- The `GEMMA_LAN_URL` house-wide config fix (13 sites, done 2026-09-28).

**What's explicitly NOT real yet:**
- **Promotion into live state.** Every validated EDIT record still
  lands in `pending_review.txt` for a human to review — there is no
  auto-promotion path. This is the actual gate between "a model
  proposed something" and "the house believes it." Don't build
  auto-promotion until the validator covers more than one record type
  and there's a real volume of `pending_review.txt` entries to learn
  the failure-rate from.
- **OpenRouter distillation / weight tuning.** Per the 2026-09-29
  addendum to `AI-PUSH-ROADMAP-AND-NUANCES.md` item 3b: OpenRouter's
  role right now is to **produce demonstrations** (tool calls, hand-tuned
  bank edits, event-command placements) that IRL can later learn from —
  it is explicitly NOT the runtime, and nothing currently closes that
  loop (no code reads OpenRouter's demonstrations back into a training
  signal yet).
- **Per-page/book autonomy-level governance** ("book-page owner decides
  how much emergent R&D a meta-IRL should be doing") — grepped
  extensively, genuinely does not exist anywhere. This is a real,
  good idea with zero prior art. See §7 below for a proposed shape.

**Answer to "how soon":** the DESCRIBE step is done. The realistic next
milestone is NOT "the house starts learning" — it's "the other 3 EDIT
record types get implemented and `pending_review.txt` accumulates
enough real entries that promotion criteria can be designed from
data, not guessed." That's weeks, not days, of real engineering, and
it should be sequenced before any OpenRouter-driven autonomy work,
because right now there's no way to even measure whether a promoted
edit was good.

---

## 1. civ-test / asa: survival mechanics (planting, mining, building, eating)

**None of this exists. Not partially — not at all.** This needs to be
said plainly because the question was framed as "how soon," which
implies a foundation to build on:

- civ-test (`civilization/NOTES.md`) is a stub: a castle entity with a
  menu whose every action is `action="void"`. No grid movement, no
  combat, no trading, no tech tree.
- "asa" is not a civ-test survival pal at all — it's half of the
  `asa-&-ava` **chatbot** pair (`@.apps/asa-&-ava/`). A separate `asa`
  pal exists in a user's `livedesk/pals/asa/` but has only standard
  pal/event scaffolding, nothing survival-related.
- Canvas-Craft (the quark→element→compound crafting system the
  planting/mining loop would need) is **"Status: design · Nothing
  built yet"** per its own header.
- Per-entity individual clocks (asked: "do entities have individual
  time?") — **no.** The only per-something clock daemon in the whole
  house is `pc_clock_daemon.c`, and it's per-*world* (piececraft's
  voxel engine), not per-entity. Ordinary desktop pals (castle,
  door_civ, asa) all run on the shared
  `khtpm_desktop_trigger_watcher.c` tick. If asa is ever going to
  "plant a seed and wait 3 days," that needs a new, real per-entity
  elapsed-time primitive — it isn't hiding somewhere unindexed.

**Correction on "phymoji":** the mental model in the question — "we
added phymoji embodiment to every event page retroactively, let's do
that for open-hai too" — conflates two real, separate, unrelated
things:
1. **Phymoji** (`piececraft-hq/phymoji.md`, design-only) is specifically
   about rendering piececraft's 2D emoji objects (trees, cars,
   mountains) as depth-extruded, per-voxel-destructible 3D volumes.
   It has nothing to do with chat, events, or entity embodiment
   generally.
2. **The retroactive sweep you're remembering** is
   `&.widgits/events-hq/ops/event_retroactive_sweep.sh` — real, built,
   idempotent, dry-run-by-default — which gives every pre-existing
   event page a 🎬️/⚙️ "clacker" widget (`event_auto_clacker.sh`).
   That's the real mechanism. It was never phymoji, and there's no
   evidence it was done specifically for chat-hai.

Use the right name going forward: **"clacker sweep,"** not "phymoji
embodiment," when you mean "give every existing X a new common
widget/capability retroactively." Keep "phymoji" reserved for
piececraft voxel-extrusion.

**Sequencing recommendation:** civ-test survival is genuinely
green-field. Before writing any C: (a) design per-entity elapsed-time
(needed by planting/growing/weather regardless of which game it's for
— this is infrastructure, build it once, house-wide, not civ-test-
specific), (b) finish Canvas-Craft's design-to-code gap, (c) only then
sequence planting → growing/weather → harvest → eat, each as a real
event type, each gated the same way every other house mechanic is
(manager op + ledger, not ad hoc C). Mining/chopping is the same shape
as planting (interact → wait → yield), so build the elapsed-time +
event-yield primitive generically and both mechanics fall out of it —
don't build two bespoke systems.

---

## 2. The per-AI-session "pal" idea (open-hai, and eventually everything)

This is the idea to which the user explicitly said "let's not worry
about implementing this right now, but write it down" — so this
section is the design doc, not a build plan.

**Precedent check, honestly stated:** thinner than assumed. There is
no `ROBOT-CHAT-BLUEPRINT.md` anywhere in the tree. The one prior
mention of "robot pal as chat's own draggable entity with heir/grant
semantics" is a single placeholder line in
`13.agent-coms/GROK/2026-09-17-cursword-file-inventory-chat.md`
("Still no symlink. Still no robot pal.") — i.e. explicitly not built,
not specced elsewhere. The low-level primitive this idea would need
(`kh_inventory_host_dir()` / `MUCHI_TARGET_ENT` in
`khtpm_events_hq_manager.c`) IS real and already used for
host-context resolution — so the plumbing to make "drag entity A into
entity B's inventory, B gains A's capability" work already exists at
the primitive level. What doesn't exist is a chat-specific pal that
uses it.

**The actual proposal, stated cleanly:**
- Every open-hai session gets a real pal entity the moment it's
  created, living under a **BOOK** dedicated to open-hai sessions
  (see naming below).
- That pal's `history.txt` / transcript IS the session — not a
  separate file the pal merely displays. One source of truth.
- Because it's a real pal, it inherits every existing pal capability
  for free: drag-and-drop between desks, the same inventory-hosting
  primitive above (so dropping it into another entity's inventory
  could plausibly grant that entity chat, exactly like the earlier
  robot-chat idea — same mechanism, applied to a *specific*, now-real
  session pal instead of a hypothetical generic robot).
- This generalizes past open-hai: "toys, widgets, the user itself,
  DBs, palettes" per the user's own note. The pattern is: **any
  stateful thing in this house that currently lives in a bespoke
  folder should be representable as a pal**, because pals are the one
  object type that already has drag/drop, inventory, and desk
  placement solved.

**Naming clarification (BOOK:PAGE):** this convention is real and
shipped (`AI-TRACK-BRAINSTORM-QUESTIONS.md` §7f,
`khtpm_taskbar_manager.c` ~line 1880/1920) — a session is a BOOK, a
desk is a PAGE. But **every BOOK today is tied to a user login.**
There is no house-wide, non-user-owned BOOK (the `/bin`-or-`/sys`
analogy in the concerns doc does not exist yet). If open-hai session
pals are going to live somewhere that isn't any one user's book (so
they can be shared/audited house-wide), that requires a new kind of
BOOK — a system book, unowned by login — which is itself a small,
real, useful primitive to build before the session-pal idea can be
built on top of it correctly. Suggest **`BOOK:SYSTEM`** as the name,
by analogy with the concerns doc's own bin/sys framing.

**Do this next, when the time comes (not now):** write
`08-roadmap/design-docs/ENTITY-EVERYTHING-DESIGN.md` — the
"can anything become a pal" doc — before writing any code, so this
doesn't get built ad hoc for open-hai and then redone differently for
toys/widgets/DBs later.

---

## 3. DSR-test / DSR-TOY / WSR_PAL-PREFERED synergy

**Status: PAUSED, not abandoned.** `10-user-docs/2026-09-18/WHY-DSR-WSR-PAUSED.md`
is explicit: "Both were the morning plan... Not because they are
wrong. Because the container they need was not there yet" — the real
blocker is Cursword needing a working inventory/File-Explorer
substrate before DSR's economy loop (or chat-as-a-pal) can be built on
it. Resume trigger, per that doc: "Inventory has been clicked once for
real."

**Design already answers your synergy question.**
`08-roadmap/design-docs/TEST-GAMES-ROADMAP.md` §6/§6b already
specifies exactly the dual-front-door shape you're asking for: every
economic entity (castle=gov, bank, store, corp) writes intent to a
shared `master_ledger.txt`; a real manager op executes it (same
pattern as `khtpm_desktop_trigger_watcher.c`); **DSR-as-toy**
(`toy.pdl`, nav-numbered `.chtpm` menu window) and **DSR-as-desk**
(walkable entities) both hit the same ledger. This is precisely "DSR-TOY
as a control surface for the physical DSR-desk," already documented —
you don't need to invent this, you need to resume building it once
Cursword's inventory unblocks it.

**What's actually on disk right now, more advanced than the roadmap
doc implies:** `&.hq-apps/dsr/` has a real X11-HQ window
(`dsr.xhtpm`), a real 226-line manager (`ops/dsr_manager.c`), and real
live state (`state/dsr_state.pdl`: turn, active_corp, player_cash,
stock_price, population). This is a working toy shell — but it has
only one corp, no banks/bonds/loans, no multi-entity ledger economy
yet. Don't mistake "a window that opens" for "the economy is real" —
they're two different completion levels.

**WSR_PAL-PREFERED is the more mature sibling, and here's the actual
relationship:** it's a from-scratch rebuild of the *old* wsr-pal
economic sim (bonds/loans/stocks/dividends), not DSR itself — DSR is a
NEW, dystopia-abstraction game with its own front door. WSR_PAL-PREFERED's
Phase 1 (shareholder registry, cent-exact dividend math, verified
2026-09-29) is genuinely done; Phase 2 (calendar/tax/gov-bonds) is in
progress. The synergy is: **DSR's economic mechanics (bonds, loans,
stock trading) are the same math WSR_PAL-PREFERED already built and
verified** — when DSR resumes, its manager op should call into (or
port from) `WSR_PAL-PREFERED/ops/shareholder_registry.c` and
`econ_calendar.c` rather than re-deriving dividend/calendar math from
scratch. That's a real, concrete integration point to write into
whichever design doc picks DSR back up.

**Is OpenRouter capable of building this?** Given the competency
ceiling in §6 below (small-sample-tested, single-file, one-hop only),
no — not the ledger/ecosystem logic itself. It could plausibly help
write individual, narrow, well-specified ops (a single validator, a
single calculation function) once the surrounding architecture is
fixed by a human/Claude/Grok, the same way it's currently scoped for
Concept Bank bootstrap work.

---

## 4. teru-test: the "magic entity" babysitter/dashboard idea

**Zero prior art. This is a new idea, state it as such.** Extensive
grep for "magic entity," "babysitter," "meta agent" across the whole
HQ-IQ-BOOK returns nothing terumon-related. `TERUMON-SPEC.md` mentions
only a single-terumon out-of-game stats dashboard — nothing about
managing multiple terumon at once, nothing about a fairy/babysitter
entity with its own chat history.

**This needs to be designed before it's built**, and it's a good
candidate for the "magic vs real entity" distinction the user raised
in §5 below, because a babysitter fairy is exactly the kind of thing
that should NOT be a physically-embodied pal on a desk (nobody drags a
fairy around) but DOES need its own state (a dashboard, a chat
history, a set of managed goals) — i.e. it's the first concrete use
case that actually needs the "magic entity" category to exist as
something other than a physical pal. Recommend: write
`08-roadmap/design-docs/TERUMON-BABYSITTER-DESIGN.md` as a follow-up
design doc (not in this answer doc) once the "real vs magic entity"
category itself is ratified — see §5.

---

## 5. piececraft-hq: the Minecraft-clone ambition

**Good news: this exact idea is already a real, detailed design doc.**
`08-roadmap/design-docs/PALCRAFT-DESIGN.md` (2026-09-15) is precisely
"a Minecraft-inspired desk where all block/interaction behavior is
scripted as real, auditable RPG-Maker-style Common Events" — which is
the same "made of events" framing in the concerns doc. It already
inventories what's real (GPU raymarch daemon at ~17fps, full camera
model, voxel chunk generation, voxel removal) versus the actual gap:
**block placement is the missing symmetric piece**, and a block
palette has already been cataloged from mineclonia reference data.

**Status: designed in real detail, not implemented.** The path from
here is concrete and narrow — implement placement (removal's mirror
image, and removal already works) and wire the Common-Events catalog.
This is a much shorter remaining distance than civ-test's survival
loop (§1), because the hard infrastructure (raymarch, camera, chunks)
is already built and proven.

**Is OpenRouter capable of doing this?** Placement is exactly the
size of task OpenRouter delegation is currently scoped for (single
op, well-specified, testable) — this is a good real candidate to
actually delegate, more so than anything else raised in this doc,
*provided* it's scoped as "implement `pc_place_voxel.c` mirroring the
existing `pc_menu_input.c` removal path" rather than "build placement."

**"Real vs magic" entity/event, as its own question:** this
distinction does not exist in the house today. The house's current
binary is "real" (backed by actual state/process) vs "stub/placeholder"
— not "physically embodied" vs "magic" (chat events, time events,
fairy babysitters). This is a genuinely new and useful category the
user is introducing, not something already named elsewhere under a
different word. Recommend ratifying it formally: a **real entity** has
a body on a desk/page; a **magic entity/event** has state and behavior
but no body (time ticks, chat sessions before §2 is built, the
proposed terumon babysitter). Writing this down as its own short
architecture doc is worth doing before the terumon babysitter (§4) or
the open-hai session-pal idea (§2) get built, since both are really
asking "does this thing need a body or not," and that question doesn't
have a house-standard answer yet.

---

## 6. What can actually be delegated to OpenRouter right now

Grounded in `13.agent-coms/GROK/2026-09-29/open-hai-api-delegation-guide.md`
and `02-architecture/OPENROUTER-MODEL-COMPETENCY.md`, both real and
current as of today:

- **Can do now:** single-file, self-contained, one-hop tasks.
  `list_dir`/`read_file` run automatically (read-only). `write_file`/
  `edit_file` are offered but every one stops for a human to write
  `APPROVE` — nothing executes silently, ever, and the transcript
  shows a pending line before a confirming line (never assume a write
  happened until the second line appears).
- **Cannot do, deliberately, by design:** `cmd_exec` is *not offered*
  through open-hai's own tool list — the guide states this explicitly
  as "a separate decision for the owner, not something either of us
  should silently add." (Separately, `cmd_exec` DOES now exist as a
  tool on the OpenRouter round-trip path built this session — same
  approval gate, different surface. The two are not the same
  integration point; don't conflate "open-hai's offered tools" with
  "everything OpenRouter can technically call.")
- **Honest competency ceiling:** mechanical tool-call reliability is
  HIGH (verified across 3 models, real round trips). Reasoning
  competence is "tested, encouraging, but on a small sample" — exactly
  one real quiz, 2/2 correct, on one model, one boundary-condition C
  question. The doc's own words: **"do not generalize it to 'the model
  is good at code review' yet."** Multi-file reasoning, real bug-finding,
  and the full write/edit approve→execute→follow-up path remain
  untested.
- **What this means for delegation policy going forward:** keep every
  delegated task "small, single-file, and — crucially — verifiable,"
  per the competency doc's own recommendation. The piececraft-hq
  placement op (§5) fits this shape well. Terumon babysitter logic,
  DSR's ledger economy, and civ-test survival mechanics do not — those
  need architecture decided by a human/Claude/Grok first, with
  OpenRouter doing narrow, already-scoped pieces only, same as the
  Concept Bank bootstrap framing in §0.

---

## 7. The boss/manager/worker/student hierarchy, and where it actually lives

The pieces for this already exist in embryonic form, unlabeled:

- **Co-lab-hai** (`&.hq-apps/co-lab-hai/`) is already, functionally,
  the boss + high-agent layer: every message is human-approval-gated,
  fanned out per named agent (`feed_<agent_id>.txt`).
- **open-hai's delegation channel** (§6) is already, functionally, the
  high-agent → OpenRouter-worker layer, gated by the same
  approve-before-execute philosophy.
- **Per-tool git branches + per-agent worktrees**
  (`01-orientation/BRANCH-STRATEGY.md`, fixed 2026-09-06) are already
  close to "each agent/student owns a scoped, isolated workspace."

**What's genuinely missing:** no tier distinction between "manager"
agents (Claude, Grok) and "student" local agents (Kilo, Hai, opencode)
anywhere in either colab-hai's agent-id scheme or the branch rules —
today every named agent is a peer with equal branch and room
privileges. Building the hierarchy the user describes (boss / managers
/ workers / students, with students eventually promotable to workers
or managers) needs either a permissions layer inside colab-hai (who
can approve/task whom) or a house convention document — neither exists
today. This is worth a short, focused design doc of its own
(`08-roadmap/design-docs/AGENT-HIERARCHY-AND-PERMISSIONS.md`) rather
than folding into this answer doc, because it's cross-cutting and will
need real buy-in from whichever agent implements the permissions
layer.

**Claude/Grok's own persistent "embodied" self-storage** (chat-history
compactions, something to read from after a disconnect): confirmed,
**does not exist anywhere.** No file or directory represents Claude or
Grok as an in-house entity with its own state beyond git branches and
ad hoc `.md`/`.txt` handoff notes. This is a real, clean gap, and
fits naturally under the same BOOK:SYSTEM idea proposed in §2 — a
Claude-pal and a Grok-pal, each with their own `history.txt`, living
under a system-owned book rather than any one user's book, is the
same primitive the open-hai session-pal idea needs, applied to the
agents themselves instead of to chat sessions. Design these together,
not separately — they're the same feature wearing two names.

---

## 8. Roadmap-of-roadmaps and the "RUSSIAN_DOLL" doc convention

`08-roadmap/00-INDEX.md` **already is** a roadmap-of-roadmaps — a flat,
annotated list pointing at `OPEN-ITEMS.md`, `FORWARD-ROADMAP-2026-09-02.md`,
and the various `design-docs/`. Point agents there first; it already
does most of what "a roadmap pointing at design docs that agents can
read and synergize from" is asking for. `12.calendar/00-INDEX.md` is a
different, narrower thing (a dated day-log index) — don't conflate the
two when telling an agent where to start.

**"RUSSIAN_DOLL" as a name is already taken, and means something
else** — it's an existing filesystem-nesting metaphor
(drag-drop-as-`mv`, file-as-container hierarchy) in
`EVENT-MODULARITY-AND-BUILD-SPEED.md` and the DSR-pause doc, describing
in-game data structure, not documentation. **A formal
recursive-doc-index convention under that name would be genuinely
new** — reusing "Russian doll" for a documentation convention risks
real confusion with the existing, different, already-real meaning.

Given that collision, this answer doc's own home
(`RUSSIAN_DOLL_HOUSE_DESIGN/`) should be understood as **the specific
container for this brainstorming thread and its descendants** — not a
general house-wide doc-nesting convention. If a general nested-index
convention is wanted later, it should get its own, non-colliding name.

---

## Summary table

| Topic | Status | Where |
|---|---|---|
| Pipeline name | OWNER NAME: attrition-model. A TEARIT is the spec inside it | `A-TEARIT-IS-ALL-YOU-NEED.md`, §9 |
| Gemma DESCRIBE step | REAL, verified | `PIPELINE-EMOJI-DIAGRAM-REVISED.md` |
| Concept-bank validator | REAL but 1 of 4 record types | `concept_edit_validate.c` |
| Promotion-into-live-state | NOT BUILT | — |
| Per-page AI autonomy governance | NOT DOCUMENTED anywhere | new, see §0 |
| civ-test / asa survival mechanics | NOT BUILT (stub only) | `civilization/NOTES.md` |
| Per-entity individual clocks | NOT BUILT (world-clock only) | `pc_clock_daemon.c` |
| Canvas-Craft | DESIGN ONLY | `CANVAS-CRAFT-DESIGN.md` |
| "Phymoji" event embodiment | MISREMEMBERED — real mechanism is the clacker sweep | `event_retroactive_sweep.sh` |
| Open-hai session pal / BOOK:user-openhai | ASPIRATIONAL, thin precedent | see §2 |
| BOOK:PAGE convention | REAL, but per-user only | `khtpm_taskbar_manager.c` |
| DSR-test | PAUSED (blocked on Cursword inventory) | `WHY-DSR-WSR-PAUSED.md` |
| DSR event/ledger design | DESIGNED, matches ask exactly | `TEST-GAMES-ROADMAP.md` §6/6b |
| DSR real code today | PARTIAL (1-corp toy shell) | `&.hq-apps/dsr/` |
| WSR_PAL-PREFERED | REAL, Phase 1 done | `WSR_PAL-PREFERED/docs/ROADMAP.md` |
| Terumon babysitter/meta-agent | NOT DESIGNED — new idea | see §4 |
| Piececraft Minecraft clone | DESIGNED IN DETAIL, placement is the gap | `PALCRAFT-DESIGN.md` |
| Real vs magic entity/event | NOT AN EXISTING CATEGORY — new; means "simple vs complex body," NOT "no body" (corrected §10) | see §5, §10 |
| Terumon babysitter's body | 👻 ghost glyph, real pal, custom user-editable GUI | see §10 |
| HQ-LAYOUT-STUDIO (new idea) | NOT DESIGNED — visual widget-layout editor, lives as sub-entity in the ☁️ | see §10 |
| OpenRouter delegation scope | REAL, narrow, honestly limited | delegation guide + competency doc |
| Agent hierarchy (boss/manager/worker/student) | Pieces exist, no tier distinction | see §7 |
| Claude/Grok persistent self-storage | DOES NOT EXIST | see §7 |
| Roadmap-of-roadmaps | ALREADY EXISTS | `08-roadmap/00-INDEX.md` |
| "Russian doll" doc convention | NAME COLLISION — means something else already | see §8 |

---

*Prepared by Claude (Sonnet 5) on 2026-09-29, via four parallel
read-only research passes over the real codebase. Handed to Grok next
for review/edit; a "night class" script covering this material follows
once Grok's pass is in.*

---

## 10. Owner correction (2026-09-29, after NIGHT_30): the babysitter keeps a body — the "real vs magic" framing was a communication gap, not a design decision

**What went wrong in §4/§5 and in NIGHT_30:** this doc's "magic
entity/event" language was read as "the terumon babysitter shouldn't
have a physical form." **That was never the intent, and it's not what
the owner wants.** The actual need was just vocabulary for entities
that hold real, persistent state without needing a *complex* body
(walking, colliding, occupying desk space the way a terumon does) — not
license to skip embodiment altogether. Every entity in this house still
gets a body. The real design question was only ever about how simple
or elaborate that body needs to be, never whether one exists.

**The actual answer, direct from the owner:** the babysitter gets a
**👻 ghost glyph** as its body — a simple, real pal, same as every other
entity, just with a form that fits a caretaker rather than a
creature. It can also have its **own custom GUI**, and that GUI should
be **user-customizable** — which is a bigger idea than just this one
babysitter (see below).

**New idea, needs its own document: "HQ-LAYOUT-STUDIO"** (working
name, short on purpose — pick something shorter still if one comes up
naturally). A toy/widget that lets a user **visually create and edit
layouts** for their own custom widgets — the babysitter's dashboard
being the first real use case, not the only one. Two real integration
points, both given directly by the owner:
- **Storage:** it should live in the **☁️ (cloud) entity** already
  proposed in the DSR-test section of `0.my-concerns.md` ("maybe 1 ☁️
  or 📦️ thing holding things like that for game can sit in upper right
  corner of screen") — as a **sub-entity inside that one cloud entity**,
  not a new top-level thing.
- **Authoring:** a layout built in HQ-LAYOUT-STUDIO should be able to
  **read an existing event page**, or have an **event drag-and-dropped
  into it as usual**, using the exact same drag-and-drop convention
  every other event placement in this house already uses. No new
  interaction model — the novelty is the visual layout editor itself,
  not how events get attached to it. (Confirmed directly by the owner
  as a follow-up to this same correction.)

**Correction to the summary table's "Real vs magic entity/event" row
and to §5's framing:** the category itself (state-and-behavior without
a *complex* physical presence) is still a real, useful distinction
worth keeping — it correctly separates "a terumon walking around a
desk" from "an elapsed-time tick" or "a chat session's turn history."
It should NOT be read as "no body at all." Every future design doc or
night class touching this should say so explicitly: **a simple body
(a glyph, a small custom GUI) is still a body.** The terumon babysitter
is a real pal with a 👻 glyph, not a bodyless process.

**Follow-up work this correction opens, not yet done:**
- Write `08-roadmap/design-docs/HQ-LAYOUT-STUDIO-DESIGN.md` — the
  visual widget-layout editor, its ☁️-entity storage, and its
  event-page read/drag-drop integration. Not started; this section is
  the seed for that doc, not the doc itself.
- Revisit `08-roadmap/design-docs/TERUMON-BABYSITTER-DESIGN.md` (still
  not written, per §4) to specify the 👻 glyph and its HQ-LAYOUT-STUDIO
  dashboard together, once that doc exists.
- `NIGHT_30_THE_ATTRITION_MODEL.txt`'s "real vs magic entity" section
  should be understood with this correction in mind — nothing in it
  needs to be re-recorded as wrong, since the underlying distinction
  still holds, but a future class or reader should not conclude
  "magic" meant "bodyless." This paragraph is the fix for that reading.

---

## 9a. Owner confirmation (2026-09-29, after §9)

Confirmed directly by the owner: **"attrition-model" is the umbrella
term**, standing in for the whole combination — A TEARIT, tomom,
IRL/FSM/GOAP, Concept Bank — so it doesn't need spelling out every
time. Use "the attrition-model" as the general term going forward;
"A TEARIT" stays the name of the specific DESCRIBE-pipeline spec
document/mechanism inside it, not a rival name for the whole thing.
This is now settled, not a proposal.

## 9. Grok addendum (2026-09-29, after the co-lab handoff)

Read against the live tree and the XO packet. Sonnet's status table
stands, with these corrections and one integration answer.

**The name.** Owner decision, 2026-09-29, after §0: the official
name going forward is the **attrition-model**. It is the umbrella.
A TEARIT (`A-TEARIT-IS-ALL-YOU-NEED.md`), the Concept Bank, the
Harnecient hack, IRL, RL, FSM, and GOAP are pieces inside it, not
rival names. The word is the work: a war of attrition, wearing
API dependence down until an entity has a small offline autonomy,
with entities sharing trunked intelligence banks. It is meant to
sound like both TEARIT and Harnecient. §0's "call it A TEARIT" was
right as the spec title and is superseded as the spoken name of
the whole effort.

**2D desk and pc-hq 3D are the same test, not two AIs.** A mechanic
is an event-command brick. The desk page and a piececraft world are
two faces of one fact (Dustopia). Playing a 2D page or a pc-hq scene
is how A TEARIT gets a real observation. It is not a separate
learning stack. Survival on the desk and survival in pc-hq both wait
on the same unbuilt primitives Sonnet named: per-entity elapsed time,
then an event that yields something after that time.

**How soon it learns.** Same answer as §0, stated for the hand-tune
question. DESCRIBE is real. A hand-tuned weight, an OpenRouter tool
trace, and a Gemma Harnecient lesson are allowed fuel. Nothing reads
those traces back into a spoke. `ai_describe` stops at
`pending_review.txt`. Live in-house learning starts when an EDIT
record is accepted into a spoke and a second actor can run the
command that spoke changes. That is after the other three EDIT types
and a pile of real pending lines. Weeks of engineering, not the next
afternoon. OpenRouter distillation into a local model is after that
loop exists, not before.

**Correction to §2.** `ROBOT-CHAT-BLUEPRINT.md` does exist. It is
outside the NNEST tree, at
`/home/no/Desktop/github/work/XO/6.robot-blue-print/ROBOT-CHAT-BLUEPRINT.md`.
`robot_chat_001` is a real pal. `&.widgits/robot-chat/robot-chat.xhtpm`
is a typed multi-turn window that calls `ai_chat.+x`. It does not
pattern-match a reply into a COMMAND, and a typed `/command` does not
skip the model. The inventory drop that grants Chat through
`MUCHI_TARGET_ENT` is still unbuilt. The session-pal idea in §2 is
still aspirational. The sentence "no robot pal, not specced
elsewhere" is the part that is wrong.

**OpenRouter, as of this session.** `cmd_exec` is offered on branch
`grok` (`913678267`), approval-tested with `echo hello-from-cmd-exec`.
The `claude` checkout's running open-hai was built before that commit.
Reads and file edits are in the live window. Shell commands need that
commit merged or the manager rebuilt. The external copies of the
roadmap and the revised pipeline now live in
`/home/no/Desktop/github/work/XO/8.grok-s29/` for the design-agent
handoff. Item 3b there matches §0 of this file: demonstrations, not
the runtime.

**What I am not starting from this pass.** No civ-test C. No
auto-promotion. No night-class script until this addendum has been
seen. Kilo stays deprecated.

---

## 11. A real sequencing mistake, caught by the owner (2026-09-30) —
## don't anchor next-steps on the freshest design docs

**What happened:** after four new design docs landed in one session
(the two attrition-model diagrams, `HQ-LAYOUT-STUDIO-DESIGN.md`,
`PAL-CHAIN-META-AND-SUBCHAINS.md`), the owner asked what should come
next in dev, holistically. The answer given was a three-item list:
gas metering in `prisc+x`, wallet ownership binding, per-entity
elapsed time. **The actual AI leg — finishing the other 3 Concept Bank
EDIT record types in `concept_edit_validate.c`
(`new_concept_node`, `fsm_transition_describe`,
`goap_action_describe`) — was left off the list entirely.** The owner
caught this directly: "the thing that surprises me is u never
mentioned the ai leg."

**Why this was a real mistake, not just a different valid ordering:**
the whole point of the attrition-model, as named and confirmed in §9a,
is winning independence from OpenRouter via a working learning loop.
Gas metering, wallet binding, and per-entity clocks are all
infrastructure **in service of** a richer world for that loop to
eventually learn from. None of them touch the loop itself. Building
world-richness while the promotion gap (§0) stays closed just produces
more manual work for humans/OpenRouter to describe by hand — the
opposite of what "attrition" is supposed to mean. Leaving the actual
named strategic priority off a "what's next" list, right after writing
four docs that don't touch it, is exactly the trap: the freshest work
in context crowds out the oldest, cheapest, most central item.

**Why the AI leg should have been listed first, concretely:**
- Implementing the other 3 EDIT record types is **cheaper** than any
  of the three infra items — additive validator code, same shape as
  the one type that already exists, already fully speced in
  `A-TEARIT-IS-ALL-YOU-NEED.md`. No new infrastructure, no retrofit,
  no policy decision needed.
- It is **fully independent** of gas metering, wallet binding, and
  per-entity clocks — nothing forces it to wait, and nothing about it
  was deliberately sequenced after something else. It was simply not
  considered.
- It directly unblocks the actual named bottleneck from §0: real
  `pending_review.txt` volume across all four record types, which is
  the prerequisite for designing promotion — the single biggest gap in
  the whole roadmap, more central than anything raised today.

**Corrected priority order, as of this correction:**
1. **The other 3 Concept Bank EDIT record types** — cheap, independent,
   directly serves the attrition-model's actual stated purpose.
2. **Gas metering in `prisc+x`** (§11 of `PAL-CHAIN-META-AND-SUBCHAINS.md`)
   — also serves the AI leg indirectly: gas-metered event invocations
   are exactly the kind of structured observation the Watch Layer (§0,
   step 1) wants. Also fixes the already-documented popen-freeze bug
   class.
3. **Wallet ownership binding** (pal-chain §9).
4. **Per-entity elapsed time** (civ-test survival, §1).

**The general lesson, worth carrying into every future "what's next"
conversation in this house, not just this one:** when a pile of fresh
design docs exists in recent context, check explicitly whether the
oldest, cheapest, most strategically-central open item got sequenced
out entirely — not just sequenced later. Recency in a conversation is
not the same as priority in a roadmap, and the two are easy to
conflate exactly when several new docs were just written.
