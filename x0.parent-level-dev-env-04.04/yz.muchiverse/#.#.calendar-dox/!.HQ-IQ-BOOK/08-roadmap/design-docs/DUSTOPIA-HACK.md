# DUSTOPIA HACK — the same DESCRIBE→SCORE→STORE loop, aimed at generating a world instead of replaying an action

**Status: RESEARCH/DESIGN, not started.** Written 2026-09-18, folding
in `XO/LLMUD_CODE/6-qwen-dustopia-hack.md` ("THE SELF-BUILDING WORLD"),
resolving its real gaps the same way `LLMUD-HACK.md` resolved
`4.qwen-harnextend++.txt`'s. Sibling document, not a replacement — the
three hacks form one real stack, see §7.

**Read `HARNECIENT-HACK.md` first, then `LLMUD-HACK.md`.** Everything
here is the identical recipe again, at a third mutation point: instead
of generating a legal argument (Harnecient) or classifying an observed
UI action sequence (LLMUD), Gemma here DESCRIBES what unexplored game
content *should contain* — and the same deterministic harness turns
that description into real, spawned, playable content.

## 0. Why this needed its own document, not a section in LLMUD-HACK.md

The *object* being described is different in a way that matters
architecturally: LLMUD-HACK's Behavior Bank stores and replays
**actions a human already performed**. This hack's banks store and
generate **content nobody has authored yet** — the harness isn't
replaying a known-good sequence, it's inventing new world state and
then deciding whether to keep it. That's a materially different risk
profile (§5) and needs its own document to be honest about it, not a
subsection that undersells the difference.

## 1. The mechanical case for DESCRIBE over CLASSIFY — stated plainly, not just cited

Direct feedback on `NIGHT_16`/`LLMUD-HACK.md`: the *mechanics and
importance* of this weren't actually explained, only the measured
result was cited (my-biotech: 2/3 wrong classify, 6/6 correct
describe). Here is the mechanical "why," stated once, properly, so
every later reference in this document (and future ones) can just
point back here:

A small model asked to CLASSIFY is asked to collapse everything it
"knows" about the input into **one discrete token choice, in a single
forward pass, with no visible intermediate reasoning**. At 270M-1B
parameters, that collapse is measurably dominated by surface-level
token statistics — the my-biotech measurement's own finding was a
literal bias toward the positive-sounding word regardless of real
content, reproduced across three different prompt framings (plain
verdict, SAFE/DANGEROUS relabeling, option-order swap) specifically to
rule out a framing artifact before concluding it was real.

A small model asked to DESCRIBE is asked to do the thing autoregressive
language models are actually trained to be good at: continue natural
text conditioned on context. The output is a full, inspectable SPAN —
orders of magnitude more real information than one classification
token — and critically, **the discrete decision is deferred to a
separate, deterministic step that a human wrote, owns, and can audit
and retune** (keyword/relation scoring, argument-density counting,
whatever the domain calls for). The compression to "one bit" still
happens — it has to, decisions are discrete — it just happens
*after* the information-rich generation, in code, not inside one
opaque forward pass.

This is a real, general instance of a standard systems-design
principle, not a house-specific superstition: **don't ask one
component to do, in a single unreliable step, what you can split into
"generate a rich, inspectable intermediate representation" (what the
model is actually good at) plus "make the discrete decision from that
representation" (a job a simpler, deterministic, tunable system does
better and more auditably).** Every real hack in this house's own
three-document family — Harnecient's judge, LLMUD's Watch Mode,
Dustopia's content scorer below — is the same split, applied to a
different pair of (rich thing to generate, discrete thing to decide).

## 2. The real mechanism, condensed from the source doc

```
Player explores → triggers a research event
  ↓
Gemma DESCRIBES the new content (never classifies it)
  ↓
Deterministic harness SCORES the description
  (keyword match against a Synonym Bank, relation check against a Relation Bank)
  ↓
Score ≥ threshold → STORE in the appropriate bank for this layer (§3)
  ↓
FSM EXECUTES content generation (spawn entities, wire events, create terrain)
  ↓
Player interacts → reward/punish → weights update (§1's LLMUD-HACK formula applies unchanged)
  ↓
Next exploration → system remembers, scales deeper
```

The reward/punish → weight step is not a new mechanism — it's
`LLMUD-HACK.md` §5's own Laplace-smoothed formula
(`weight = (rewardCount+1)/(rewardCount+punishCount+2)`), reused
verbatim. Don't invent a second weight formula for this hack; there is
exactly one in this house, and it lives in the sibling document.

## 3. Four layers, and an honest accounting of what's real vs. what's new

The source document names four "scaling layers" mapped onto four bank
types. Two of those bank types already have a real, defined home in
this house; two do not, and this document says so plainly rather than
implying otherwise:

| Layer | Bank | Real status in this house |
|---|---|---|
| Surface (a word/phrase gets described, scored, filed) | **Synonym Bank** | Named and scoped in `LLMUD-INTEGRATION-DESIGN.md` — real, has a smallest-first-step already. |
| Mid (a causal relationship gets extracted, wired) | **Relation Bank** | **Not yet defined anywhere in this house beyond the external source doc.** `LLMUD-INTEGRATION-DESIGN.md` names it as a real concept from the LLMUD/Bank-Systems proposal but scopes only the Synonym Bank as the smallest-first-step — Relation Bank has zero house-side design work done. Flagged honestly, not assumed built. |
| Deep (a repeated exploration pattern gets recognized, cached) | **Behavior Bank** | Real, fully specified in `LLMUD-HACK.md` §4/§5 — this layer is the SAME mechanism, same schema, same weight formula, just triggered by exploration sequences instead of coding-action sequences. No new design needed here, only reuse. |
| Meta (canonical phrasings get recognized across variations) | **Sentence Bank** | Named in `LLMUD-INTEGRATION-DESIGN.md`'s own read of the source material, zero house-side design work done beyond that naming. |

**Real implication**: this hack is buildable TODAY only as far as the
Behavior Bank layer (reusing LLMUD-HACK's own schema unchanged) and
partially at the Synonym Bank layer (reusing the already-scoped
smallest-first-step). The Relation Bank and Sentence Bank layers are
real, coherent ideas with a described shape (the source doc's own
worked examples are concrete and usable as a starting spec) but
**zero house-side implementation groundwork** — don't let this
document's polish imply otherwise.

## 4. The fractal/scale-bridging framing — read as inspiration, not as verified engineering

The source document's "Dustopia manifesto," "spectral flow parameter
λ," and the specific λ values used in its worked example (2.5, 2.8)
are **imported wholesale from an external source this house has not
independently derived, defined, or verified.** No file anywhere in
this house defines what λ actually IS mathematically, how it's
computed from real game state, or why 2.5 vs. 2.8 in the worked
example. Treat the micro/meso/macro scale-tier framing and the
general idea "the same generative loop should work at every zoom
level, with lower temporal density at larger scales" as a real,
usable, worth-exploring INSPIRATION — but the λ formalism specifically
should not be cited by a future document as if this house has a real
implementation or definition of it. If the ai-research team wants to
give λ a real, computable definition, that is exactly the kind of
granular theory work this document exists to hand off, not something
resolved here by restating the source doc's own unexplained symbol.

## 5. The real, new risk this hack introduces — and why it needs a harder rule than LLMUD-HACK's

LLMUD-HACK's Behavior Bank replays actions a human already, knowingly,
performed once — the risk surface is "did we replay the right cached
thing," bounded by the fact a human already did it once for real.

**This hack's Layer 3/4 "auto-execute" step is different in kind**: a
repeated exploration pattern being recognized can lead straight to
*generating and spawning new world content* with no human having
authored or reviewed that specific content first — the source doc's
own words: *"Player doesn't need to manually trigger each step
anymore."* Applied to a scored description of a crystal cave, that's
low-stakes. Applied to gameplay-affecting mechanics (the worked
example literally wires a "crystal growth feedback loop" as a real
game system) with no player-visible review gate, this crosses the
same line `HARNECIENT-HACK.md`'s own component 5 (real, player-visible
artifacts — never hidden state) and component 6 (fallback, never
silently trust the model) exist to prevent, and does so with
higher stakes than any of the three hacks so far, because the output
is executable game content, not a suggestion.

**Resolved, explicit, and non-negotiable for this hack specifically**:
auto-execute (spawning content with no confirmation step) is
**only** permitted once a bank entry has crossed a real, meaningfully
high `observationCount`/`weight` bar — the same "graduation threshold"
question `LLMUD-HACK.md` §7 already named as open for Behavior Bank
entries in general, but for THIS hack the default posture must be
**stricter**, not the same bar. A newly-generated Relation Bank or
Sentence Bank entry should default to "propose, show the player what
would spawn, wait for one explicit confirmation" — the same
"Gemma-suggests-never-auto-writes" discipline
`AI-FUNCTION-CRAFTING-DB-HQ-DESIGN.md` already established for
authoring `event_commands.registry.pdl` rows — until real, accumulated
reward history earns auto-execute for that specific cached pattern.
This is a real design decision this document is making, not
deferring, because the alternative (silent world-mutation with no
review gate) is exactly the failure mode the whole three-hack family
exists to prevent.

## 6. Where this is real, buildable, and testable today: PALCRAFT/mineclonia

This is not abstract. `08-roadmap/design-docs/PALCRAFT-DESIGN.md` /
`13.agent-coms/2026-09-15/GROK.md` (the real, live handoff — see
`00-compact/compact-mineclonia-grok-handoff.md` for the compact
version) is building a real Minecraft-inspired voxel desk on
piececraft-hq's existing world, block behavior authored as real,
auditable Common Events. This is close to the ideal first testbed for
this hack's Behavior-Bank-only layer (§3): a bounded, tile-based,
already-instrumented world where "player explores a new region,
Gemma describes what it might contain, a deterministic scorer checks
it against the real mineclonia tile catalog (`#.ref/menu/event-guides/
mineclonia/mcl_core.pdl`), and a Common Event gets authored" is a real,
scoped, buildable slice — not the full fractal micro-to-macro vision,
just Layer 3 (Behavior Bank) against a real, bounded, already-live
game.

## 7. The real, new sequencing insight this document adds: build the watch layer once, shared

`LLMUD-HACK.md` §2 names the real, biggest gap in THAT hack: nothing
watches Claude Code's own actions yet, though the real watch surfaces
(`entity_menu_history/<pid>.txt`, Claude Code's own tool transcript)
already exist. **This hack's Layer 3 needs the identical class of
watch layer — observing a PLAYER's repeated exploration actions,
not a coder's actions — but it is the same mechanism**: bounded
window of recent real actions → DESCRIBE → deterministic score →
store. There is no real reason to build two separate watch layers for
two separate hacks.

**Real, concrete recommendation**: the watch/observe layer is shared
infrastructure across all three hacks in this family (Harnecient's
own live API-call logging, LLMUD's action-sequence watcher, this
hack's exploration-pattern watcher) and should be built ONCE, generic
over "what kind of action stream am I watching" — not three times.
Given PALCRAFT is real, live, in-house work happening right now, and
its own Common-Event-authoring process is itself a real action
sequence worth watching (which blocks get which event types, what
sequences repeat across a Grok-authored batch) — **building the
shared watch layer first, informed by real PALCRAFT authoring
activity as its first real data source, is a genuinely defensible
sequencing choice**, ahead of either hack's own bank-scoring work,
rather than an afterthought bolted on later. This is a real, new
observation this document is making, not previously written down
anywhere — flagged as a recommendation for whoever sequences the next
phase of work, not a unilateral decision.

## 8. Real open questions for the ai-research/ML-PhD side, not resolved here on purpose

1. **Relation Bank / Sentence Bank real design** — §3's honest gap.
   The source doc's worked examples are a real, usable starting spec;
   turning them into an actual file format + scoring rule is real,
   unstarted work.
2. **λ, given a real definition** — §4's honest gap. Is there a real,
   computable quantity in this house's own existing systems (entity
   density? tick rate? chunk-load radius?) that could stand in for
   the source doc's unexplained spectral-flow parameter, or is the
   whole scale-bridging idea better served by a simpler, house-native
   mechanism with no borrowed cosmology at all?
3. **Cross-hack bank format unification** — does a Behavior Bank
   entry generated by THIS hack (exploration → content) need to be
   structurally distinguishable from one generated by LLMUD-HACK
   (coding action → replay), given they'll likely share one physical
   registry per `LLMUD-HACK.md` §4's own open question, or does the
   `source`/`keywords` fields already make that distinction naturally?
4. **The graduation-threshold number itself** — §5 sets the *policy*
   (stricter default posture for this hack), not a real number. What
   real observation count, for what real reward ratio, should be the
   actual bar before this hack's own auto-execute unlocks?
5. **Runaway generation bounding** — even with a review gate (§5), is
   there a real risk of exploration-triggered content generation
   creating an unbounded, ever-growing bank with no pruning/decay
   mechanism? `LLMUD-HACK.md` §7 already named time-decay as an open
   question for Behavior Bank weights generally — does THIS hack need
   a harder cap (a real, finite content budget per session/world), not
   just a softer decay curve?

## 9. Grounding / further reading

- `HARNECIENT-HACK.md` — the base recipe.
- `LLMUD-HACK.md` — the sibling mutation this doc reuses verbatim for
  its Behavior Bank layer and weight formula; §2's watch-surface
  finding and §7's open questions both directly inform this document.
- `6-qwen-dustopia-hack.md` (`XO/LLMUD_CODE/`) — the external proposal
  this document verifies, resolves the gaps in (§3-5), and extends
  (§6-7).
- `PALCRAFT-DESIGN.md` / `13.agent-coms/2026-09-15/GROK.md` /
  `00-compact/compact-mineclonia-grok-handoff.md` — the real, live,
  concrete testbed named in §6.
- `AI-FUNCTION-CRAFTING-DB-HQ-DESIGN.md` — the propose-never-auto-write
  discipline §5 borrows explicitly.
- `LLMUD-INTEGRATION-DESIGN.md` — where Synonym/Relation/Sentence Bank
  were first named in this house, real status per bank in §3's table.

## Addendum 2026-09-23 — two representations, one weight, and the step that is actually done

The product this hack serves is not "Gemma writes a cave." It is
Dustopia: the same chemistry, astronomy, and mechanics fact held in
two representations, and a weight that says which one is in force.

- **RPG representation.** An event page. Discrete. An actor row, a
  switch, a turn. This is what livedesk can run today.
- **3D representation.** A numeric step. Positions, rates, a clock
  that is fast for small things and slow for large things. This is
  what PC-HQ is for. It is not built as a second game with a second
  rulebook.
- **The switch.** A bank weight, not a mode menu buried in C. The
  event page reads the weight and either plays the discrete command
  or asks the numeric step to advance. Describe, then score, then
  store, from §2, is how a new weight gets into that bank. The model
  describes. The harness scores. A person can read both.

What is done, and what it is allowed to count as:

- `read_receipt` plus `if` plus `send_input` on
  `read-receipt-ent` (commits `91051f70`, `4b10b6e7`, `76cca6cd` on
  `grok`). A page looked at `focus_nav` and pressed Down only when
  the switch was off. That is KPI 0 of the ladder in
  `GS-23-HQ-TECH.md` next to the short customer report. It is not
  chemistry and it is not 3D.
- `send_input` wrote the bare code `201`. The live window wants
  `KEY_PRESSED: 201`. Do not call the desk "driven by the page"
  until those match.
- λ is still undefined, as §4 says. Do not let a later doc treat the
  receipt work as a definition of λ.
- This pass did not re-open `xo-pets` or `Mar$.$treetRace` and did
  not re-verify those binaries. The fable's claim that they exist
  is not a 2026-09-23 test result.

Accountability ladder, abstract and checkable, full writeup in the
tech companion:

1. A page branches on a receipt. **Met.**
2. The key line matches the window mailbox. **Not met.**
3. One live window, one key, stop. **Not met.**
4. One material id has both an event-page fact and a numeric fact.
   **Not met.**
5. A stored weight chooses which fact advances the tick. **Not met.**
6. The same weight is readable on the desk and in a 3D view.
   **Not met.**
7. A local model describes, the harness scores, a person can reject
   the store. **Not met.** `ai_describe` is not in the registry.
