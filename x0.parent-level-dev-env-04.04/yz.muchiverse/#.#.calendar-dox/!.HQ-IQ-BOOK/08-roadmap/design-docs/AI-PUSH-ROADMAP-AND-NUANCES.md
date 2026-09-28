# AI Push — Roadmap, Sequencing, and Forward-Looking Nuances

**Date:** 2026-09-27
**Status:** Planning document. Two items in this doc are ready to build; everything else is explicitly document-only, not started.
**Also filed at:** `/home/no/Desktop/github/work/XO/5.Recent_roadmap/AI-PUSH-ROADMAP-AND-NUANCES.md`

---

## Start Here: The Two Buildable Tracks

### Track 1 — `ai_describe` as a real registered event

The concrete first step, and it's small on purpose:

1. Fix `GEMMA_LAN_URL` (hardcoded in four C files across `my-lawyer`/
   `my-biotech`, see `PIPELINE-EMOJI-DIAGRAM-REVISED.md`) into a shared
   `.pdl` config, read once at startup. Do this first — the new op
   needs the endpoint anyway, and it's the cheapest moment to kill four
   hardcoded copies instead of adding a fifth.
2. Register `ai_describe` as a real COMMAND in the events registry
   (cap 128, confirmed room — `ai_describe`/`ai_fsm_transition`/
   `ai_goap_plan` are currently named in conversation but absent from
   the real registry).
3. Wire it to the constrained-prompt pattern already tested against
   production `gemma3:270m` (see the revised pipeline doc): real
   candidate node list in, one fixed `TARGET: ... | STRENGTH: ... |
   REASON: ...` line out.
4. Feed that line through a scorer into the *existing*
   `concept_edit_validate.+x` — unchanged, already validates
   `spoke_weight_delta`.
5. **Stop there.** Do not build `ai_fsm_transition`/`ai_goap_plan` in
   the same pass — those depend on `fsm_transition_describe`/
   `goap_action_describe` validator support, which doesn't exist yet.
   One real end-to-end round trip on one real terumon, one real
   observation type, before anything widens.

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
