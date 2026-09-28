# AI Push — Roadmap, Sequencing, and Forward-Looking Nuances

**Date:** 2026-09-27
**Status:** Planning document. Two items in this doc are ready to build; everything else is explicitly document-only, not started.
**Also filed at:** `/home/no/Desktop/github/work/XO/5.Recent_roadmap/AI-PUSH-ROADMAP-AND-NUANCES.md`

---

## Start Here: The Two Buildable Tracks

### Track 1 — `ai_describe` as a real registered event

The concrete first step, and it's small on purpose:

1. ✅ **Done, 2026-09-28.** Fixed `GEMMA_LAN_URL` into a shared
   `#.desktop/ai_backend.pdl` config (`gemma_lan_url`/`gemma_lan_model`
   keys, plain `key=value`, same convention `hq_ui.pdl` already uses),
   read once at startup, falling back to the same default if the file
   or key is missing. The original "four files" count (from
   `PIPELINE-EMOJI-DIAGRAM-REVISED.md`) was itself incomplete — a full
   house-wide grep after the first four were fixed found **9 more**
   hardcoded sites. All 9 fixed the same way: `mylawyer_case_worker.c`,
   `mylawyer_judge_worker.c`, `mybiotech_research_worker.c`,
   `mybiotech_fda_verdict.c` (the original four), plus `gov_decide.c`,
   `corp_decide.c`, `khtpm_open_hai_manager.c` (`g_ollama_host`, bare
   `host:port` form — stripped from the shared config's `http://`
   value at load time), `run_native_tools.c` (was a `#define` macro,
   converted to a loaded variable), `verify_cell.c` (same). Each
   duplicates the loader rather than sharing a header, matching this
   house's own explicit per-worker-file convention (several of these
   files' own header comments already say so). Deliberately left
   alone: `tsc_answer.c` (already has an adequate `GEMMA_LAN_URL`/
   `GEMMA_LAN_MODEL` env-var override, a test harness's normal
   convention, not worth replacing) and `connect_op.c` (×2 — URL is
   passed as `argv[1]`, never hardcoded, only a comment mentions the
   IP). All rebuilt, all compile clean.
2. ✅ **Done, 2026-09-28.** Registered `ai_describe` as a real
   `COMMAND` in `#.ref/menu/event_commands.registry.pdl` — zero
   recompile of the renderer/manager, same live-reload proof this
   registry already documented for `take_gold`.
3. ✅ **Done, 2026-09-28.** Wired to the constrained-prompt pattern
   already tested against production `gemma3:270m`: real candidate
   node list read LIVE from `&.widgits/concept-bank/data/masters/`
   (real files: `energy`, `force`, `motion` — NOT the illustrative
   `hunger`/`satiation` example from the design docs, which was never
   actually created as real files; the roadmap's own earlier "no
   Concept Bank file exists on disk" claim was stale — `force.pdl`,
   `energy.pdl`, `motion.pdl`, and one real spoke `gravity_constant.pdl`
   already exist). New op `ai_describe.+x`
   (`&.widgits/entity-cli/ops/`), plus its own duplicated
   `connect_op.c`/`json_parser.c` (matching this house's established
   per-project duplication convention, same shape `mylawyer_case_worker.c`'s
   own `gemma_ask()` already uses).
4. **Deliberately NOT done, matching established precedent**: does not
   call `concept_edit_validate.+x` or touch any real spoke/master file.
   Writes one line to the entity's own `pending_review.txt` — same
   scope `ai_lab_concept_bank_propose.sh`'s own header comment already
   established for this exact step ("prove the Gemma call + the draft
   write... NO accept/merge path yet"). Promotion wiring is a separate,
   later, not-yet-built step.
5. **Real round-trip result, ember, 2026-09-28**: the mechanism is
   proven end-to-end — real call, real fixed-format parse, correct
   reject-and-log on malformed output (never partial-applied). One
   real, reproducible finding: ember's actual `history.txt` right now
   is window-lifecycle noise (`WINDOW_OPEN`/`PHYMOJI_LOADED` from
   today's Move testing), not rich behavioral content, and the model
   consistently echoed the format template instead of filling it in
   against that thin input. This is a **data problem, not a code
   bug** — a real fix caught during a real bug hunt this session
   (`gemma_ask()`'s `popen()`/`remove()` ordering had a genuine race:
   the response file was deleted before `json_parser.+x` could open
   it — fixed by moving the `remove()` to after `pclose()`).
   **Next real step**: test against an entity with genuine behavioral
   history (a real Move, a real interaction), or curate which
   `history.txt` line types count as observation-worthy, before
   drawing any conclusion about the prompt format itself.

### Track 2 — pc-hq automated test-bot harness

Buildable now with existing infrastructure, no new architecture:

- A test-bot harness drives the real taskbar through the relay into
  pc-hq — same mechanism every existing harness already uses (never
  xdotool, per house testing convention).
- It exercises real gameplay: create an event, save a project/book,
  load it back, verify real state changed.
- Pass/fail on each scenario becomes a real feedback event — same
  shape as a thumbs-up/down on a terumon action — feeding the same
  Watch → Gemma → Scorer → Validator → Ledger loop Track 1 wires up,
  except the "actor" is the test bot and the "learning" is tomom/IRL
  noticing which FSM paths reliably produce a successful save/load.
- This is the natural training-data source for the IRL/FSM loop:
  instead of waiting for a human to play and give feedback, the test
  bot's own pass/fail becomes real, high-volume feedback signal from
  day one.

Both tracks are independent and can proceed in parallel. Neither is
blocked on anything below this line.

---

## Track 1's Natural Test Surface: the Drop-In Chatbot Entity

**This is not a third track — it's the user-facing surface Track 1
needs anyway**, and it already has real, separately-documented
foundations to build on rather than starting from nothing:

- `13.agent-coms/GROK/2026-09-17-cursword-file-inventory-chat.md`
  already specs a 🤖️ emoji entity that IS a chat surface: "Robot chat
  is a separate entity, events modified on that pal — not Cursword
  main. Drag the robot into the Cursword folder (or out to desktop /
  another inventory). Chat happens from Inventory, not Cursword main."
- `12.calendar/2026-09-20/2do.md` §8b/8c documents a real,
  **already-built** host-context bridge (`khtpm_events_hq_manager.c`/
  `khtpm_core_render.c`, `MUCHI_TARGET_ENT`) so an entity's own method,
  run from another entity's inventory right-click, correctly resolves
  `$ENT` to itself rather than the host it's sitting inside.

**Why this maps directly onto Track 1, not a new mechanism:**
- The chatbot entity's own state files (glyph, history, whatever it
  accumulates) ARE the Watch Layer's observation input — "consume the
  data of the entity as part of its context" is exactly what an
  `ai_describe` call already needs to be handed.
- Chatting with it and reacting IS the feedback valence signal the
  promotion ledger already expects.
- "Chat command shortcuts" are just new registered COMMANDs — same
  registry, same pattern as `apply_range`/`advance_fact`, nothing new
  to invent.
- "Teaching tomom / building out the Bank in parallel" is the same
  promotion-ledger mechanism Track 1 already wires up, just with a
  human's live chat as one more real observation source alongside the
  automated pc-hq test-bot (Track 2).

### Question 4, resolved by context clues (2026-09-27 design session)

`AI-TRACK-BRAINSTORM-QUESTIONS.md` Question 4 quotes a cut-off
instruction — *"we were gonna give the 🤖️ ..."* — nested directly
under item 2's own plan: "robot/puzzle-piece entities carrying events,
dropped into inventories, methods run from the Inventory right-click."
**Read in that context, the strongest inference is the missing words
were about giving the robot its first real, concrete event/method to
carry** — an MVP action, not personality. This is inference, not
confirmed fact — the house should still say so if the real answer
turns out to be something else. This session's own conversation has
since organically grown that original, narrower idea into something
considerably richer (below) — worth being honest that the richer
version is a 2026-09-27 extension, not what the original sentence
necessarily meant.

### Design resolved this session (not yet built — a design, like every other item in this section)

**Personality mechanism:** the entity's own accumulated data (history,
state files) feeds `ai_describe` (Track 1's own Gemma call) to build
an **instance-scoped Concept Bank** — the entity's personality IS its
own bank, built the identical DESCRIBE→SCORE→VALIDATE→PROMOTE way
everything else in this house's AI track works. Not a separate
free-text personality channel alongside the bank — the bank IS the
personality, same mechanism, no second thing to keep in sync.

**Chat command shortcuts — two real modes, not one:**
- Explicit `/command` syntax — a literal registered COMMAND, same
  registry `apply_range`/`advance_fact` already live in, nothing new
  to invent mechanically.
- Free natural-language chat, dispatched through the exact
  `HARNECIENT-HACK.md` mechanism (`DAY_05_THE_HARNECIENT_HACK.txt`) —
  the model never gets to call a tool directly; it produces plain
  text, and the house's own harness pattern-matches that text into a
  real dispatch, the same illusion-of-tool-use trick already proven
  house-wide. Both modes resolve to the same real COMMAND dispatch
  underneath — `/command` is just the fast path that skips the
  pattern-match step.

**Template/instance split — designed, not punted:** reuses
`NIGHT_26_EVERYTHING_IS_AN_EVENT.txt`'s own bot-DNA template+delta
proposal, applied at the smallest real case (one chatbot "species," N
copies): a shared TEMPLATE bank holds the starting personality: each
dropped-in copy holds only a DELTA bank for what it's genuinely
personalized through its own conversations. Resolution is the same
two-step NIGHT 22 already specified for spoke/master lookup —
template value, overridden by delta where a delta slot is non-empty.
A promoted insight written to the TEMPLATE benefits every copy with no
delta at that slot, same propagation payoff NIGHT 26 already argued
for population-scale bots. Still not built — reasoned through as a
real design in Night 27, same as hub-and-spoke was in Night 22.

**Visibility / debugging — existing shapes first, charts later:**
prefer reusing an existing house UI shape (a db-hq-style real editor
window, or whatever pattern already renders an entity's own state) to
show a chatbot's live bank weights, recent observations, and the
reasoning behind a given response — rather than inventing a new
bespoke visibility surface. Flagged as a real, explicit future
direction, not started: **chart/dashboard generation as a compiled op**
— a real, reusable house primitive for rendering bank/observation data
visually, usable by this chatbot's own debug view and by any other
future dashboard need house-wide, not a one-off built just for this.

---

## Document-Only: Networking Reuse (IRC / Forum / Chain / Multiplayer / Blockchain-Mining)

**Verdict: yes, one substrate — but it doesn't exist yet, so there is
nothing to reuse today.**

Architecturally this is exactly `NIGHT_26_EVERYTHING_IS_AN_EVENT`'s own
thesis: a networked chat message, a multiplayer move, and a mining/
consensus event are all event-shaped, and should converge on the same
P2P layer rather than three parallel stacks. But `palnet_peer.c`
(`044.pal-chat-irc👥️+2/ops/`) hardcodes `127.0.0.1` for both its bind
call and its peer-discovery presence file — confirmed by reading the
source, not assumed. Today's "multi-user" P2P test is real
multi-*process*, same machine only. There is no real cross-machine P2P
layer to point IRC/Forum/Chain/multiplayer/mining events at yet.

**Action:** the rule "reuse this one substrate, never build a second
networking stack for a new event-shaped feature" is now written into
`CROSS-MACHINE-NETWORKING-PLAN.md` as an explicit note. Nothing to
build here until that plan's own first milestone (real bind, real
cross-machine test) lands.

---

## Document-Only: Store, Personal Blockchains, Node-Value-Weighting, CONES

Split deliberately into a cheap-to-decide-now half and an
expensive-to-guess-now half.

### Lock in now (architectural default, not a build item)

**Every game's entity/gold/item creation should default to writing
against a personal, per-game blockchain backend from day one — even
if the user never surfaces or uses it.** This costs nothing to decide
now and is far cheaper to bake into entity/currency creation code from
the start than to retrofit after hundreds of call sites exist without
it. This is a stated default for new code going forward, not a
justification to build the chain itself yet — it can be a no-op/stub
backend until the real chain infrastructure exists, as long as the
call sites are already shaped correctly.

### Document only — genuinely more advanced, no foundation yet

**Valuing a chain by unique distributed node count / how much of the
chain each node holds, with a "corporate-validated, truncated chain"
exception for CONES (or an enterprise customer's own house):** this is
a real proof-of-stake/proof-of-distribution-style economic design
question. It is explicitly **not** ready to implement, stated plainly:
you cannot weight by "unique distributed nodes" when there has only
ever been one node. This entire section is blocked on the same
cross-machine networking milestone as the section above — there is no
real second node to test any weighting formula against yet.

This is not "getting ahead of yourself" to think about — it would be
getting ahead of yourself to build it. Writing it down costs one
section; skipping the writedown risks a redesign once real multi-node
data exists to design against.

---

## Document-Only: The "Meta Bots" Control App

A real, distinct concept from `h-ai-lab` — a control surface for
multiple entity/bot sessions, possibly running on other machines or
VMs, interfaced as **sessions** with save/load **bot-profiles**,
itself controllable by an agent or meta-harness to manage sub-agents
(not just a human operator).

**Blocked on the same thing as the two sections above:** if bots are
meant to run on other machines, this app needs real cross-machine
comms to be more than a local mock of itself. Document the shape now;
don't build until the networking plan's first real milestone lands.

**Naming (unresolved, low-stakes, house's call):** candidates —
`bot-fleet`, `session-yard`, `muster`. Not architecturally load-bearing;
pick whichever fits the house's naming voice when the time comes.

---

## Grounded in Existing Vision Docs (not new ideas — cross-referenced, not duplicated)

Two threads raised are already real, named, existing design/vision
documents in this house — worth citing directly rather than treating
as fresh proposals:

### Farming / animal reproduction / world-content generation

`DUSTOPIA-HACK.md` (`08-roadmap/design-docs/`, status: RESEARCH/DESIGN,
not started, written 2026-09-18) is the real, existing document this
belongs under — the same DESCRIBE→SCORE→STORE loop from
`HARNECIENT-HACK.md`/`LLMUD-HACK.md`, aimed at *generating* unauthored
game content instead of replaying or classifying an observed one. It
does not currently name animal reproduction/farming specifically — that
would be a natural, in-scope extension of its existing framing
("inventing new world state and then deciding whether to keep it"),
using the exact same Concept Bank masters (`chemistry_reaction`, and
whatever biology-flavored masters this door leads to per
`NIGHT_26_EVERYTHING_IS_AN_EVENT`'s own biology section) rather than a
separate mechanism. Worth a short addendum in `DUSTOPIA-HACK.md` itself
naming farming/reproduction as a concrete first content-generation
target, once Track 1 above has a real working `ai_describe` round trip
to generate content candidates *from*.

### Multiplayer shooter / boardgames

Already real, already named, already part of the NIGHT lesson track —
not new ground:
- `NIGHT_02_WAGER_CHESS.txt` — simplified-rules, real-stakes PvP
  boardgame (pre-ordained armies, fast real matches).
- `NIGHT_03_FPS_NIGHTS.txt` — "many shooters, one house," the
  multiplayer FPS vision, explicitly the reason the whole NIGHT track
  is named NIGHTS.

Both are design/vision, not built as running games. Both would ride on
the same networked-event substrate as the Networking section above,
once real — a PvP move or a shot-fired event is exactly the same
event-shape this whole roadmap keeps returning to. No new design work
needed here beyond what those two lessons already contain; the honest
status is "named and designed, blocked on the same networking milestone
as everything else above."

---

## Summary Table

| Item | Status | Blocked on |
|---|---|---|
| `ai_describe` registry + Gemma wiring | **Build now** | Nothing |
| pc-hq automated test-bot harness | **Build now** | Nothing |
| Drop-in chatbot entity (single copy, one inventory) | **Buildable now** as Track 1's test surface | Nothing (host-context bridge already exists) |
| Chatbot entity template/delta bank (shared personality, many copies) | **Designed** (2026-09-27, reuses NIGHT 26's bot-DNA shape) | Nothing technical, but genuinely unbuilt |
| Chart/dashboard-generation op | Document only, flagged future primitive | Nothing, just not started |
| Networking reuse (IRC/Forum/Chain/multiplayer/mining) | Document only | `palnet_peer.c` real cross-machine fix |
| Per-game personal blockchain default | **Lock in now** (as a code convention, not infra) | Nothing |
| Node-value-weighting + CONES exception | Document only | Real multi-node data |
| Meta-bots control app | Document only | Cross-machine networking |
| Dustopia farming/animal reproduction | Already documented (`DUSTOPIA-HACK.md`) | A working `ai_describe` round trip to generate candidates from |
| Multiplayer shooter / boardgames | Already documented (NIGHT 02/03) | Networking reuse milestone |

---

## Related

- `PIPELINE-EMOJI-DIAGRAM-REVISED.md` — the tested Gemma I/O contract Track 1 builds on.
- `CROSS-MACHINE-NETWORKING-PLAN.md` — the real blocker for every networking-dependent item above.
- `NIGHT_26_EVERYTHING_IS_AN_EVENT.txt` — the unifying "everything is an event" framing this whole doc applies.
- `DUSTOPIA-HACK.md`, `NIGHT_02_WAGER_CHESS.txt`, `NIGHT_03_FPS_NIGHTS.txt` — existing vision docs cross-referenced above, not duplicated.
