# 🤖 Topic 2 — Building out independent AI systems (local LLM / RL / gemma-harness)

> Companion deep-dive to `#.haiku+/30.jul-30-handoff.md`. Everything in
> this file was verified by direct file reads this session, not inferred
> from filenames. Where something is a DESIGN ONLY with zero code, it says
> so explicitly — treating a roadmap doc as if it were working
> infrastructure is exactly the mistake this house's own
> `!.xyzos-standards+1.txt`/`!.xyzos-pitfalls+1.txt` exist to prevent.

## 🗺️ 0. The real state of the world, in one table

| Layer | Location | Status |
|---|---|---|
| Local LLM chat (gemma3:270m via Ollama) | `045.muchi-pal-agent🤖️+1/` | ✅ **WORKING**, one known open bug |
| Multi-provider routing (Gemini/llamacpp/groq/iqabod) | same, `ops/send_message.c`/`check_response.c` | ✅ **WORKING** for the LLM providers, ⚠️ garbage output for `iqabod` |
| Homemade C transformer (IQABOD) | `x0.moke-pet-project-04.04/#.standby/#.IQABOD🪞️]z6]RMS` (outside 44.xyz) | ✅ real, working train/generate CLI, ⚠️ **undertrained → incoherent output** |
| Reinforcement learning | `ROADMAP-models.txt` | ❌ **design document only, self-labeled "nothing built"** |
| Teacher/student distillation loop (IQABELLA) | `$.zest-er-summary` design | ❌ **design only** |
| Shared bot vocabulary for test-bots AND AI players | `fsm_bot_programmer.txt` (TPMOS `#.plans/`) | ❌ **spec only, not implemented anywhere** |

This table is the actual starting line. Any plan that skips straight to
"build the RL loop" or "train IQABOD on real gameplay" without first
closing the ✅-with-a-bug and the ⚠️ rows is building on sand — this
house's own onboarding doc already has a name for that mistake:
**"harness-before-hype."** Apply it here.

## 🔧 1. What `045.muchi-pal-agent🤖️+1/` actually is — read the code, not the name

This is the REAL, currently-working local-LLM integration in this house.
Trace flow, so you can start editing it cold:

```
user types in chat -> pal keyboard history -> chtpm_parser_pal
  -> gemma_strategy.c (op)
       - runs DETERMINISTIC KEYWORD MATCHING FIRST, before any LLM call
         (gemma3:270m is too small to trust with real tool-calling —
         this is a deliberate, documented design choice, not a stopgap)
       - decides: plain chat reply, or a specific in-game tool action
  -> strategy_execute_a.c (op) - dispatches the decided action
  -> connect_op.c - opens the connection to whichever provider is
     selected (pieces/registry/models/model_list.txt + the `/model`
     command choose provider_kind: gemma/gemini/llamacpp/groq/iqabod)
  -> send_message.c - POSTs to the real endpoint
       gemma path: localhost:11434/api/chat (real local Ollama, gemma3:270m)
  -> check_response.c - polls for the reply, writes it back into the
     world's shared state (world_01/state.txt) for the frame to pick up
```

**Known open bug, fix in progress, finish this before building anything
new on top:** `jul-21-gemma-fix.txt` documents TWO issues from the same
session — (1) concurrent unreaped sessions racing on shared
`world_01/state.txt`, causing double-logged replies and a stuck
"THINKING" state — **this one is fixed and confirmed**; (2) after that
fix landed, a NEW render-refresh freeze appeared — **this one is still
open**, and the file itself prescribes the next diagnostic step (add a
debug log inside `check_response.c`, not yet added as of this write-up).
This is your literal first task if you touch this system at all: add
that log, reproduce, read the log, fix it. Do not build a second AI
feature on a daemon with a known freeze bug — you will not be able to
tell your new feature's bugs apart from this one.

**Second thing to check before extending anything**: `agent-summary-
claude.txt` (2026-07-26) documents a REAL regression that already
happened once — `gemma_strategy`/`strategy_execute_a` silently fell out
of `default_op.txt`'s own op-registration list, which means tool
detection silently stopped firing with **no error, no crash, just
quietly-wrong behavior** (the same "silent, not loud" failure shape as
PITFALL 58 in the editor/file-menu work). It was caught and fixed via
`test-harn-same/` (note: yes, this is one of the "test-harn-same" named
harnesses the naming-convention fix earlier this session flagged as a
"dumb name" for the editor's own harness — this one under
`045.muchi-pal-agent🤖️+1/` has the same generic name; worth renaming for
the same greppability reason, low priority, not urgent). **Lesson to
carry forward**: every time you touch `default_op.txt`, re-run this
project's own harness immediately after — a silently-dropped op
registration produces ZERO error output, only a feature that quietly
stopped working.

## 🧠 2. IQABOD — the homemade transformer, and why it's not "an AI system" yet

`IQABOD` (source outside the `44.xyz` tree, at
`x0.parent-level-dev-env-04.04/x0.moke-pet-project-04.04/#.standby/
#.IQABOD🪞️]z6]RMS`, originally at
`x0.parent-level-dev-env-03.00/#.ref/^.IQABOD-llm-06.00/`) is a real,
hand-written C transformer — attention, RoPE, RMSNorm, SwiGLU, real
backprop (`main_orchestrator.c`, `train_module.c`,
`feedback_tx_module.c`), with a real working train/generate CLI. It is
already wired as a live `provider_kind` option inside
`send_message.c`/`check_response.c` — meaning `/model iqabod` inside
muchi-pal-agent's own chat actually calls it, right now, no additional
wiring needed. **The gap is not code, it's training data.** Per
`TODO.txt`, Phase A5 (2026-07-16) already produced a trained checkpoint —
explicitly described in that same file as "word-salad," not coherent.

**This is the highest-leverage next step in this whole area**, because
the plumbing is done and the gap is purely "more/better training data +
more training runs," which is grindable, parallelizable work rather than
new architecture:

1. Read `TODO.txt` and `ROADMAP-models.txt` in full FIRST — they already
   define the curriculum bank concept (§5 of the roadmap) and the phase
   numbering (A5 is done, what comes after A5 is the real next task, not
   invented here since it's already someone else's living plan).
2. A cheap, real, grounded source of training data already exists that
   nobody has piped into IQABOD yet: **the real chat transcripts
   `045.muchi-pal-agent🤖️+1/` produces from GEMMA** (a small but genuinely
   coherent model) are a legitimate supervised-fine-tuning corpus for
   IQABOD — this is literally the "teacher/student" shape the
   `$.zest-er-summary` doc's own IQABELLA design already sketches (gemma
   as teacher, IQABOD as student, harvest real conversation logs, train).
   That design doc exists; building the actual harvest-and-train script
   that realizes it does not yet exist. That's the concrete next step,
   not a new design.

## 🎯 3. Reinforcement learning — do not start here

`ROADMAP-models.txt` labels itself, in its own text, **"GOAL/DESIGN
DOCUMENT ONLY. Nothing described here is built."** It lays out real
substance worth reading (a curricula bank in §5, `feedback_tx_module.c`'s
signed-learning-rate reward mechanism, an LLM-as-judge auto-training FSM
in §11.2) — but §11.2 auto-training loop cannot mean anything until IQABOD
has a coherent SUPERVISED baseline to reinforce FROM. `TODO.txt` already
says this explicitly ("NOT NOW," blocked on Phase A/B). **Do not let
"build out independent AI systems" get read as "go build the RL loop
first"** — the existing plan already puts it last for a real reason
(reinforcement learning without a working base policy just amplifies
noise), and nothing found this session contradicts that ordering.

## 🕵️ 4. The missing piece: a shared "agent acts in the game" vocabulary

`fsm_bot_programmer.txt` (TPMOS `#.dev-storage/#.plans/`, outside this
house, cited but not present here) describes something genuinely useful
that doesn't exist yet ANYWHERE in this house: a shared `bot::*` op
vocabulary meant to drive BOTH test bots (topic 3's continuous harness)
AND real AI game-players (this topic) through the identical interface —
i.e., "move north," "open inventory," "talk to NPC" would be the SAME
primitive whether a deterministic test script or an LLM-driven agent
issues it. **This is worth building for real, and it's the connective
tissue between topic 2 and topic 3** — right now `gemma_strategy.c`'s own
tool-dispatch and topic 3's future "play like a real user" harness would
otherwise be built as two unrelated, duplicate action-dispatch systems.
Building the shared vocabulary FIRST (even a minimal one — a handful of
`bot::move`/`bot::interact`/`bot::chat` ops that both `gemma_strategy.c`
and a new autonomous test-harness agent both call into) avoids that
duplication and is a real, scoped, buildable next step — unlike the RL
loop, which has a hard prerequisite that isn't met yet.

## ✅ 5. Recommended order (applying "harness-before-hype" literally)

1. **Fix the open render-refresh freeze** in `045.muchi-pal-agent🤖️+1/`
   (§1) — a few hours, unblocks trusting the system at all.
2. **Re-verify `default_op.txt` registration** hasn't drifted again since
   `agent-summary-claude.txt`'s fix — five minutes, cheap insurance.
3. **Build the gemma-transcript → IQABOD training-data harvester** (§2) —
   this is real, scoped, and turns an existing "word-salad" model into
   the first genuinely useful checkpoint, without touching RL at all.
4. **Build a minimal shared `bot::*` vocabulary** (§4) — scoped small,
   directly enables both this topic's future autonomous game-players AND
   topic 3's continuous test-harness cycle, so it should be built once,
   shared, not duplicated.
5. **Only then**, revisit `ROADMAP-models.txt` §11.2's RL auto-training
   loop — by this point IQABOD has a real supervised baseline to reward
   against, which is the actual prerequisite the roadmap itself already
   names.

Do not skip to step 5. The existing docs already agree on this ordering;
this section just makes it explicit and actionable.
