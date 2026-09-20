# LLMUD integration — folding parser-events and bank systems into h-ai-lab's own plan

**Status:** design · **Date:** 2026-09-17 · **Nothing built yet**
**Source material:** `/home/no/Desktop/github/work/XO/LLMUD_CODE/0.LLMUD.md`
("LLMUD: Local LLM MUD on the LIVE DESK") and `1.BANK_SYSTEMS.md`
("Bank Systems: Beyond Synonyms") — two external architecture proposals,
outside this house, folded in here rather than copy-pasted. Full
narrative walkthrough of this fold-in: `1-1.HARNECIENT.SMOL/
NIGHT_15_THE_BANK_AND_THE_BRICK.txt`.

This doc does not restate LLMUD/Bank-Systems' content — read the
sources directly for the full text. It does three things: (1) says
which pieces are genuinely new territory for this house, (2) says
which pieces are the SAME idea this house already scoped under
different names, and (3) cross-references `H-AI-LAB-DESIGN.md` Part 4
and Part 5 explicitly, since that's where the overlap is real.

---

## 1. The honest mapping, term for term

LLMUD was written independently, with its own vocabulary (RPG-Maker
event primitives, "Banks," inverse RL). Before deciding what's new,
map its terms onto this house's real, already-built or already-scoped
things:

| LLMUD term | This house's real equivalent | Status |
|---|---|---|
| "FSM Sequencer Event" | events-hq's Common Event (trigger + condition + command list), `event_commands.registry.pdl` | **built, live** |
| "Event Executor" | events-hq's own command dispatch (`TEMPLATE exec ...` per command row) | **built, live** |
| "Parser Event" (grep synonym bank → Gemma fallback) | nothing today does this generically. `cursword`'s own NLU is ad hoc procedural C, not table/bank-driven. **Genuinely new.** | not started |
| "Inverse RL Weighting" / "Watch Mode" | `IRL-BOOTSTRAP-RECURSION-SPEC.md` (NIGHT 12) — Layer 1 (IRL reward inference from in-line correction signal), Layer 6 (recursion rule) | **spec'd, not built** — same idea, already named |
| "Reward/Feedback Event" (👍/👎/Rollback/Lock In buttons) | `H-AI-LAB-DESIGN.md` Part 4's Review Queue (Accept/Reject on a Gemma-authored draft) | **spec'd, not built** — same idea, narrower (Part 4 has no Rollback/state-snapshot concept yet — see §3) |
| "Synonym Bank" / "Relation Bank" / "Sentence Bank" | nothing today. The closest existing thing is `ai_instances_registry.txt` (Part 1 of H-AI-LAB-DESIGN.md), but that's a registry of AI *instances*, not a bank of *learned associations*. **Genuinely new**, see §2. |
| "AI bricks... rearrangeable like legos" (LLMUD's own framing, `0.LLMUD.md` §"Custom Plugins/Events Needed") | `H-AI-LAB-DESIGN.md` Part 5, verbatim same framing, already written and scoped before this fold-in | **already scoped, independently converged** |
| `IRL_Weights` / `weights.txt` | `IRL-BOOTSTRAP-RECURSION-SPEC.md`'s own `weights.txt` draft file, NIGHT 9's Gemma-authored-weights tool | same file, same idea |
| "Pricing Tiers" (`1.BANK_SYSTEMS.md` §Storage Strategy) | out of scope for this doc — a monetization framing for an external product concept, not a house feature. Not folded in. |

**The headline finding:** LLMUD's FSM/event/reward-loop layer is not
new — it independently re-derives the exact shape `events-hq` +
`H-AI-LAB-DESIGN.md` Part 4/5 already committed to (structured,
bounded actions, never a freeform chat box that has to be phrased
right). That convergence is worth taking seriously as *validation*,
not as a reason to rebuild anything. What LLMUD adds that is real and
missing is narrower than the whole document: the **Parser layer** (§2)
and the **Bank systems** underneath it (§2), plus one real gap in the
reward loop this house hasn't spec'd yet (§3).

---

## 2. What's genuinely new: Parser + Banks

### 2.1 The gap this fills

Today, "talking to an AI instance" in this house means one of three
things, per `H-AI-LAB-DESIGN.md` Part 3 tier 2: a raw `POST /api/chat`
to an attention-net, a direct `decision_mode` dispatch write for a
decision-pal, or (once built) injecting a message as an FSM trigger
event. In every case, the human's raw text either goes straight to a
model (attention-net, decision-pal in `llm` mode) or has to exactly
match a trigger string (FSM). There is no layer that turns "save my
conversation about networking" into a normalized intent + parameters
*before* it reaches any of those three paths, and no learned,
inspectable, user-expandable store of what phrases mean what.

`cursword_fsm.c` (Part 2 of H-AI-LAB-DESIGN.md, the house's one real
live FSM) comes closest — but its own real transitions are gated on
`wait_for()`/polling side effects, not a clean "phrase arrives, intent
comes out" parser, and it was never designed to be one.

### 2.2 Synonym Bank — the smallest real piece, and where it plugs in

LLMUD's Synonym Bank (`1.BANK_SYSTEMS.md` §"Synonym Bank") is the
smallest, most concretely buildable of the three bank types: `word →
[aliases]`, per-entry weight, O(1) lookup. This maps directly onto an
`ai_fsm_transition`-style event command's FIELD2 (target state /
trigger name, per `H-AI-LAB-DESIGN.md` Part 5's table) — instead of a
human or an upstream caller having to type the FSM's exact trigger
string, a Synonym Bank lookup could resolve "put it away" / "save
that" / "archive this" to the same real trigger name before the
`ai_fsm_transition` op fires. This is a genuinely new small file +
small op, in the same "one writer, real convention" shape as every
other house state file:

```
#.desktop/ai_synonym_bank.txt
CANON=<canonical trigger/intent name>|ALIAS=<phrase>|WEIGHT=<0.0-1.0>|SOURCE=<user|learned>
```

Reader: a new small op, `ai_synonym_lookup.sh <house_root> <phrase>`,
substring/token match against `ALIAS`, return the highest-weight
`CANON` above a threshold, else "no match" (never guesses past the
threshold — same discipline as every other house AI feature that
refuses to auto-merge past a confidence line). This is the FIRST real
consumer that could sit in front of Part 5's `ai_fsm_transition`
command, and it's the smallest real first step for this whole fold-in
— see §5.

### 2.3 Relation Bank and Sentence Bank — real, but not smallest-first

Relation Bank (`entity ↔ entity` weighted edges) and Sentence Bank
(cached full-query → FSM-path + similarity matching) are real ideas
with no house equivalent, but they're a materially bigger lift:
Sentence Bank in particular needs a real similarity function (LLMUD's
own pseudocode reaches for cosine similarity on embeddings — this
house already has one real building block for that, NIGHT 11's cheap
`cosine_similarity.c` router, Layer 3 of the five-layer IRL stack) and
a cache-eviction/pricing-tier-shaped storage policy that LLMUD's own
doc frames around a monetization model this house doesn't have. Both
are real future territory, explicitly NOT the smallest first step —
see §5's honest ordering.

### 2.4 Watch Mode / inverse learning — already spec'd, LLMUD adds one real detail

`IRL-BOOTSTRAP-RECURSION-SPEC.md`'s Layer 1 (in NIGHT 12) already
covers "watch real interaction, extract a reward signal from in-line
correction, without a separate labeling pass." LLMUD's "Watch Mode"
(`0.LLMUD.md` §6) is the same idea, told from the desktop-action side
rather than the transcript side: it watches *what the user actually
does* (spawn entity → move it → add event → test it), not just what
they *say*, and reconstructs an "ideal FSM" from the action sequence
itself. That's a real, new angle the spec doesn't currently cover —
IRL-BOOTSTRAP-RECURSION-SPEC.md's Layer 1 only reads `open-hai`
transcripts (`U|`/`A|` lines), it has no notion of watching desktop
actions. Whether that's worth building is an open question (§6), not
a commitment — it would need a real action-interception hook this
house doesn't have (LLMUD's own doc lists `WatchModeHook — Intercept
desktop actions without code changes` under "Nice-to-Have," honestly
un-designed on the source side too).

---

## 3. One real gap LLMUD's Reward/Feedback Event exposes in Part 4

`H-AI-LAB-DESIGN.md` Part 4's Review Queue table has three real states
today: propose (Gemma writes a draft), accept (human writes it into
the real file), reject (human discards it). LLMUD's own
`ManifestDisplay`/`RewardLoop` events (`0.LLMUD.md` §4/§5) have a
fourth button Part 4 doesn't: **Rollback** — restore a `StateSnapshot`
taken before the action executed, distinct from "reject the draft
before it's ever written." Part 4 as currently spec'd never writes
into a live file except via Accept, so a full Rollback in LLMUD's
sense (undo something already applied) isn't currently needed — but
it's worth naming as a real question for once `ai_fsm_transition`
(Part 5) starts firing state changes directly from live Common Events,
not just from a human's Accept click: an event-fired FSM transition
has no draft/accept gate at all (Part 5's own "Real, honest limits"
section already says so), so if that transition turns out wrong there
is currently no rollback path. Flagged as an open question, §6 — not
solved here.

---

## 4. Cross-reference summary (explicit, for anyone reading H-AI-LAB-DESIGN.md next to this doc)

- **Part 4** (Gemma-driven action rows): LLMUD's Reward/Feedback Event
  independently confirms the "structured buttons, never freeform chat"
  shape already chosen. No change to Part 4's plan. One new open
  question added (Rollback, §3/§6).
- **Part 5** (AI bricks as event commands): LLMUD's own document uses
  the literal word "legos" for the same idea, independently. No change
  to Part 5's command table. This doc adds one new candidate command
  the Synonym Bank could produce: resolving an ambiguous FIELD2 phrase
  into the real trigger name `ai_fsm_transition` expects, before the
  op fires — a possible `ai_synonym_resolve` command, or (cheaper) a
  pre-processing step inside `ai_event_fsm_transition.sh` itself. Not
  decided here, §6.
- **IRL-BOOTSTRAP-RECURSION-SPEC.md** (NIGHT 12): LLMUD's Watch Mode
  is a real, new angle (desktop-action observation, not just
  transcript-text observation) that spec doesn't cover. Flagged as
  future work, not merged into the spec text itself — that spec's own
  smallest-first-step (hand-score 5 exchanges) is unaffected.

---

## 5. Smallest real first step (do exactly this, nothing bigger, first)

Following the same one-real-thing-at-a-time discipline every other
design doc in this chapter uses:

1. **Synonym Bank file + lookup op only** (§2.2) —
   `ai_synonym_bank.txt` + `ai_synonym_lookup.sh`, hand-seeded with a
   handful of real aliases for cursword's own real trigger names
   (`fsm_table.pdl`'s existing `trigger=` values, per
   `H-AI-LAB-DESIGN.md` Part 2). No Relation Bank, no Sentence Bank,
   no Parser Event, no Gemma fallback yet — prove the file format and
   the lookup work standalone before wiring anything to it.
2. Only once (1) is confirmed: wire the lookup into ONE real place —
   the most natural is `ai_event_fsm_transition.sh` (Part 5's own
   smallest-first-step op) checking FIELD2 against the bank before
   falling back to an exact trigger-name match. This is the first real
   point where LLMUD's Parser-layer idea touches a real, already-
   scoped H-AI-LAB-DESIGN.md command.
3. Everything else in this doc — Relation Bank, Sentence Bank, Watch
   Mode, Gemma-as-fallback-parser, Rollback/StateSnapshot — waits
   until (1) and (2) are built and checked, same discipline as every
   prior NIGHT.

---

## 6. Open questions this doc does not resolve

- Does a Synonym Bank entry's `WEIGHT` get updated by the same IRL
  correction signal `IRL-BOOTSTRAP-RECURSION-SPEC.md` Layer 1 already
  reads (`U|`/`A|` in-line correction) — i.e. is this one more reader
  of that same signal, or does it need its own? Leaning "same signal,"
  not decided.
- Relation Bank / Sentence Bank: real, but not scheduled. Whether
  Sentence Bank's similarity matching reuses NIGHT 11's
  `cosine_similarity.c` (Layer 3) directly, or needs its own smaller
  string-similarity function first (LLMUD's own pseudocode mixes both
  canonical-form substring matching and cosine similarity) — decide
  when this actually starts.
- Watch Mode / desktop-action interception (§2.4): no real hook exists
  in this house today. Whether that hook is worth building at all, or
  whether the transcript-only signal `IRL-BOOTSTRAP-RECURSION-SPEC.md`
  already reads is sufficient, is unresolved.
- Rollback (§3): whether `ai_fsm_transition`-fired transitions need a
  `StateSnapshot`-style undo before they're allowed to fire
  unattended from a live Common Event, or whether the existing
  draft/Accept gate (Part 4) already covers every case that matters —
  unresolved, worth revisiting once Part 5's smallest-first-step
  (`ai_fsm_transition`) is actually live.
- LLMUD's own "pricing tiers" storage strategy is explicitly NOT
  folded into this house's plan (no monetization model here for bank
  storage) — noted so nobody re-derives it later thinking it was
  dropped by accident.
