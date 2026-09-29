# 🪖⚙️ THE ATTRITION-MODEL — House AI Roadmap, Visualized

> **Companion to:** `AGENT_ROADMAP_ANSWERS.md` (the prose answers this
> diagram summarizes) and `0.my-concerns.md` (the original questions),
> same directory. Also mirrored/pointed-at from
> `#.#.calendar-dox/!.HQ-IQ-BOOK/RUSSIAN_DOLL_HOUSE_DESIGN🪆️/00-INDEX.md`.
> Same format as `2.BOTTLE_PIPE_VISUALIZED/PIPELINE-EMOJI-DIAGRAM-REVISED.md`,
> scaled up from one pipeline to the whole house AI roadmap.
> Also available as interactive HTML: `ATTRITION-INTERACTIVE-DIAGRAM.html`.

**The name, confirmed 2026-09-29:** "the attrition-model" is the
umbrella term for the whole effort — a war of attrition, wearing down
API/OpenRouter dependence until entities have their own small offline
autonomy, sharing trunked intelligence banks. It sounds like both
"TEARIT" and "Harnecient" on purpose. Every box below is a piece
**inside** the attrition-model, not a rival name for it.

Status badges used throughout: 🟢 REAL (built, verified) · 🟡 DESIGNED
(a doc exists, nothing runs) · 🔴 ASPIRATIONAL (neither exists yet).

---

## 🗺️ The Whole Shape, at a Glance

```
                         🪖 THE ATTRITION-MODEL
                    (umbrella strategy — confirmed 2026-09-29)
                                   │
        ┌──────────────┬──────────┼───────────────┬────────────────┐
        │              │          │               │                │
   🧠 A TEARIT     📚 Concept  🐾 tomom      🤝 OpenRouter      🏛️ Agent
   (the learning     Bank      (local        Workers          Hierarchy
    spec/loop)     (data the   learner,       (via open-hai)   (boss →
        │           loop        dormant,          │             managers →
        │           writes      waits on          │             workers →
        │           to/reads    promotion)    🟢 list_dir/          students)
        │           from)           │          read_file            │
        ▼               ▲           │          (auto-run)           ▼
  👁️ Watch Layer         │           │              │          👤 Boss (owner)
        ▼               │           │          🟡 write_file/       — approves
  🤖 Gemma DESCRIBE      │           │             edit_file/            all
   (never CLASSIFY)      │           │             cmd_exec               │
        ▼               │           │           (approval-gated)          ▼
  🧮 Scorer (C)          │           └──────────────┘             🧑‍💼 High-agents
        ▼               │        demonstrations feed IRL          (Claude, Grok)
  🛡️ Validator (C)  ─────┘        NOT the runtime yet                    │
   🟢 1-of-4 record                                                      ▼
   types built                                                  🛠️ OpenRouter
        ▼                                                          (workers)
  📊 Promotion Ledger 🔴                                                 │
   NOT BUILT — stuck                                                     ▼
   at pending_review.txt                                        🎓 Local agents
        ▼                                                       (Kilo/Hai/opencode
  ✅ Live State 🔴                                                = students, NO
   waiting on the                                                tier distinction
   ledger above                                                  built yet)

                                   │
                                   ▼
                    🎮 GAME-EMBODIMENT TESTBEDS
              (where A TEARIT gets real observations —
               2D desk and pc-hq 3D are the SAME test, per Grok's
               addendum, not two separate learning stacks)
        ┌───────────────┬───────────────┬────────────────┐
        │               │               │                │
   🌱 civ-test/asa   🏦 DSR-test    🐣 teru-test      ⛏️ piececraft-hq
   🔴 fully          🟡 paused,     🔴 "magic         🟡 PALCRAFT
   unbuilt           designed,      entity"           designed in
   (no per-entity    real 1-corp    babysitter —      detail, block
   clock exists      toy shell      zero prior art,   placement is
   anywhere)         on disk        new idea          the only gap
```

---

## 1️⃣ 🧠 A TEARIT — the learning spec inside the attrition-model

**Status: 🟢 DESCRIBE step real & verified · 🔴 promotion not built**

The actual DESCRIBE→SCORE→VALIDATE→PROMOTE loop, spec'd in
`08-roadmap/design-docs/A-TEARIT-IS-ALL-YOU-NEED.md`. House law:
**DESCRIBE, never CLASSIFY** — no model ever sets a weight number or
flips live state directly.

### 📥 Input → 📤 Output, stage by stage

| Stage | Who | Status | Real file |
|---|---|---|---|
| 👁️ Watch Layer | sensors/events | 🟢 real | observation records |
| 🤖 Gemma DESCRIBE | `gemma3:270m` (LAN) | 🟢 verified 2026-09-27 | constrained `TARGET\|STRENGTH\|REASON` format — proven better than free-form, which hallucinated a "thirst" field never in the input |
| 🧮 Scorer | C code | 🟢 real | splits fixed-format line, no NLP |
| 🛡️ Validator | `concept_edit_validate.c` | 🟢 real, but only 1 of 4 record types (`spoke_weight_delta`) | rejects whole-record, never partial-applies |
| 📊 Promotion Ledger | — | 🔴 **not built** | everything currently dead-ends at `pending_review.txt` for a human to read |
| ✅ Live State | Concept Bank + FSM/GOAP | 🔴 waiting on the ledger above | — |

**The honest bottleneck, stated plainly:** the house does not currently
have a way to go from "a model proposed an edit" to "the house
believes it" — that gate is entirely human-review-shaped right now.
Building the ledger before the other 3 record types exist and before
`pending_review.txt` has real volume would mean designing promotion
criteria from guesses, not data. **Sequence: more record types →
real pending-review volume → THEN promotion automation.**

---

## 2️⃣ 📚 Concept Bank — the data layer A TEARIT reads/writes

**Status: 🟢 real, narrow**

Hub-and-spoke weighted concept nodes
(`&.widgits/concept-bank/data/masters/` + `spokes/`). This is what
`spoke_weight_delta` edits target. Every other consumer in the house
(tomom's curriculum, FSM transitions, GOAP preconditions) is meant to
read from this same bank — one shared vocabulary, not per-feature
silos.

---

## 3️⃣ 🐾 tomom — the local learner, currently dormant

**Status: 🟡 exists as real code, waiting on real training signal**

`#.Z.HUMAN_LLM/3.stage.llm.tomom` — a real, hand-built LLM/weight
system. It's the thing that's eventually supposed to attend to
curriculum and the growing Concept Bank so routine work can be
proposed by a local model instead of a paid agent. **Blocked on the
same thing A TEARIT is blocked on:** there's no live promotion loop
yet for it to learn from. Until that loop is boring and proven, paid
agents (Claude, Grok) and humans remain the employees.

---

## 4️⃣ 🔀 FSM + GOAP — the decision layer

**Status: 🟢 FSM/`decision_mode` real and in production use · 🔴 GOAP-describe record type not built**

The model never writes a transition table directly. It only
*describes* a desired transition or action in natural language; the
real, deterministic events-hq compiler turns that description into an
actual event page. `fsm_transition_describe` and
`goap_action_describe` are named in the A TEARIT spec as future
Concept Bank record types — neither is implemented yet (see the
validator row above).

---

## 5️⃣ 🤝 OpenRouter Workers — demonstrations, not the runtime

**Status: 🟢 real, narrow, honestly-limited competency**

Reached through open-hai
(`13.agent-coms/GROK/2026-09-29/open-hai-api-delegation-guide.md`).

| Tool | Gate | Status |
|---|---|---|
| `list_dir` / `read_file` | auto-run, read-only | 🟢 real |
| `write_file` / `edit_file` | **stops for human `APPROVE`** every time | 🟢 real, gated |
| `cmd_exec` | not offered in open-hai's own tool list by design; exists separately on the OpenRouter round-trip path, same approval gate | 🟢 real, gated, separate surface |

**Honest competency ceiling** (`OPENROUTER-MODEL-COMPETENCY.md`):
mechanical tool-call reliability is HIGH (verified, 3 models). Reasoning
competence is real but *lightly* tested — exactly one quiz, 2/2 on one
model, one question. **"Do not generalize it to 'the model is good at
code review' yet"** — the doc's own words. Every OpenRouter task should
stay small, single-file, and verifiable.

**Role in the attrition-model:** OpenRouter's job right now is to
**produce demonstrations** — tool traces, hand-tuned bank edits,
event-command placements — that IRL can later learn from. It is
explicitly **not the runtime**. Nothing currently reads those
demonstrations back into a training signal; that loop doesn't exist
yet either.

---

## 6️⃣ 🏛️ Agent Hierarchy — boss / managers / workers / students

**Status: 🟡 pieces exist, unlabeled; no tier distinction built**

```
👤 BOSS (the owner)
   │  approves every message in co-lab-hai, every write/cmd_exec
   ▼
🧑‍💼 HIGH-AGENTS / MANAGERS (Claude, Grok)
   │  own branches, decide architecture, delegate narrow slices down
   ▼
🛠️ WORKERS (OpenRouter, via open-hai)
   │  single-file, self-contained, one-hop, verifiable tasks only
   ▼
🎓 STUDENTS (local agents: Kilo, Hai, opencode)
   promotable to worker/manager — mechanism doesn't exist yet
```

**What's real:** Co-lab-hai is already, functionally, the boss +
high-agent layer (human-approval-gated, per-agent feed files).
Open-hai's delegation channel is already, functionally, the
high-agent → worker layer, same approve-before-execute philosophy.
Per-tool git branches + worktrees are close to "each agent owns a
scoped workspace."

**What's missing:** no permissions layer distinguishes "manager" from
"student" anywhere — today every named agent (claude/grok/kilo/hai/
opencode) is a peer with equal branch and room privileges. Worth a
dedicated design doc (`AGENT-HIERARCHY-AND-PERMISSIONS.md`) rather than
retrofitting colab-hai ad hoc.

**Also missing, same shape:** Claude and Grok have **no persistent
"embodied" self-storage** — no pal/entity holds their chat-history
compactions the way a game pal holds `history.txt`. This is the same
primitive the open-hai session-pal idea (§7 below) needs, applied to
the agents themselves.

---

## 7️⃣ 🎮 Game-Embodiment Testbeds — where A TEARIT gets real observations

**Grok's addendum, confirmed:** the 2D desk and pc-hq 3D worlds are
**the same test, not two AIs** — a mechanic is an event-command brick
either way; playing a 2D page or a pc-hq scene is just how A TEARIT
gets a real observation. Survival on the desk and survival in pc-hq
both wait on the same unbuilt primitive: **per-entity elapsed time**,
then an event that yields something after that time passes.

| Testbed | Status | Real blocker |
|---|---|---|
| 🌱 civ-test / asa | 🔴 fully unbuilt | No survival mechanic of any kind exists; no per-entity clock exists anywhere in the house (only a per-*world* clock daemon in piececraft) |
| 🏦 DSR-test | 🟡 paused, not abandoned | Blocked on Cursword's inventory substrate; real 1-corp toy shell already on disk (`&.hq-apps/dsr/`), ledger-driven multi-entity economy design already matches the DSR-TOY/DSR-desk synergy question exactly (`TEST-GAMES-ROADMAP.md` §6/6b); `WSR_PAL-PREFERED` already has the dividend/calendar math DSR should port from rather than re-derive |
| 🐣 teru-test | 🔴 "magic entity" babysitter is a brand-new idea | Zero prior art for a multi-terumon dashboard/babysitter entity; needs a design doc before any code |
| ⛏️ piececraft-hq | 🟡 designed in real detail (`PALCRAFT-DESIGN.md`) | Raymarch/camera/voxel-removal already work; **block placement is the only missing symmetric piece** — closest thing to "shovel-ready" in this whole diagram |

---

## 🆕 Two categories this thread introduced, not yet ratified

### "Real entity" vs "magic entity/event"
The house's existing binary is "real" (backed by actual state/process)
vs "stub/placeholder" — **not** "physically embodied" vs "magic" (time
ticks, chat sessions, a fairy babysitter). This is new, and useful: a
**real entity** has a body on a desk/page; a **magic entity/event** has
state and behavior but no body. Recommend ratifying this formally
before building the teru-test babysitter or the open-hai session-pal
idea, since both are really asking "does this need a body?"

### BOOK:SYSTEM (proposed, not built)
`BOOK:PAGE` is real and shipped, but every BOOK today is tied to a user
login. There's no house-wide, unowned book (the `/bin`-or-`/sys`
analogy). Proposed name: **`BOOK:SYSTEM`** — would hold open-hai
session pals, and Claude/Grok's own self-storage pals, under one
system-owned book rather than any one user's.

---

## 🧭 One-sentence summary

**The attrition-model is a war of attrition against API dependence:
A TEARIT's DESCRIBE step already works, but the promotion ledger that
would let anything become "believed" by the house doesn't exist yet —
so OpenRouter, tomom, and every game-embodiment testbed are all,
right now, feeding evidence toward a loop that isn't closed, not
already running inside a closed one.**

---

*Sources: `A-TEARIT-IS-ALL-YOU-NEED.md`, `PIPELINE-EMOJI-DIAGRAM-REVISED.md`,
`concept_edit_validate.c`, `AI-PUSH-ROADMAP-AND-NUANCES.md`,
`OPENROUTER-MODEL-COMPETENCY.md`, `open-hai-api-delegation-guide.md`,
`BRANCH-STRATEGY.md`, `WHY-DSR-WSR-PAUSED.md`, `TEST-GAMES-ROADMAP.md`,
`WSR_PAL-PREFERED/docs/ROADMAP.md`, `PALCRAFT-DESIGN.md`,
`ROBOT-CHAT-BLUEPRINT.md`, and `AGENT_ROADMAP_ANSWERS.md` §0–9a
(this document's direct prose source).*
