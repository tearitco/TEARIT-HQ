# IRL bootstrap recursion — spec for a self-teaching famous-llm chain

**Status: RESEARCH / DESIGN ONLY, not started.** Direct live request
(2026-09-12): watch real open-hai chats, distill curriculums from them,
weight them (hand + IRL), write FSM/GOAP "joints" for the "famous"
sub-agent, and do all of this recursively — Claude bootstraps a process
that teaches Gemma to bootstrap its own smaller "famous-llm," to the
point where two real hops of recursion are demonstrated (Claude→Gemma's
process, Gemma's process→a famous-llm it produced). A third hop
(Claude bootstraps Gemma to bootstrap its own famous to bootstrap its
own famous) is explicitly named as real future work, NOT part of this
spec's proof bar. Full narrative walkthrough:
`1-1.HARNECIENT.SMOL/NIGHT_12_THE_BOOTSTRAP_RECURSION.txt` (companion
to `NIGHT_11_INVERSE_RL_AND_FAMOUS_LLM.txt`, which this spec extends —
read NIGHT 11's five layers first, this doc assumes them).

This doc is the terse, buildable version of NIGHT 12's narrative. No
re-narration — read the NIGHT file for the "why," this file for the
"what, in order."

## Grounding: what's real, today, zero new code

- `&.widgits/open-hai/ops/khtpm_open_hai_manager.c` `persist_msg()`
  (~line 191) already appends every real open-hai exchange, forever,
  to `<session_dir>/transcript.txt` — one line per turn, `U|<text>`
  (human) / `A|<text>` (Gemma via Ollama). This is Layer 0's real data
  source. No new logging needed.
- `*.monads/*.cursword/ops/cursword_fsm.c` already documents the real
  FSM-first/Gemma-optional pattern this spec's FSM reuses (model
  optional, canned fallback always present) — `cursword_say()` itself
  is still an open stub (horizon item 3), a separate, smaller gap from
  this spec.
- `014.wsr-pal💸️📌️+2/ops/corp_decide.c` `decision_mode=3` (llm via
  gemma3:270m) is the real, working, bounded-call shape this spec's
  "JUDGING" state reuses — one short call, real output, no fine-tuning.
- `047.scm🎓️+1/corpuses/*/weights.txt` are the real, existing,
  hand-tuned per-corpus weight files NIGHT 9/11's Gemma-authored-draft
  proposal already targets — this spec's Layer 2 output lands in the
  same shape.
- `#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️/attention.c`'s
  `struct VocabEntry` (real `weight` field, `W_q`/`W_k`/`W_v`) is the
  real, small, from-scratch net shape this spec's hop-two Layer 4
  target reuses at a smaller scale — never a call to a bigger hosted
  model.

## The five layers (NIGHT 11, unchanged) + one new layer

0. Corpus — real history (here: `open-hai` transcripts).
1. IRL reward inference — per-exchange judgment, "corrected" vs
   "accepted," inferred from the NEXT line in the same transcript
   (see "the correction signal," below) — not a separate labeling pass.
2. Weight authoring — Gemma proposes, Layer-1 rewards correct, a human
   reviews the diff before merge (unchanged discipline from NIGHT 9).
3. Meta-curriculum router — cheap cosine similarity over each
   curriculum's own attention profile, `cosine_similarity.c` (qroq),
   no full-corpus compute.
4. The real small attention net — `attention.c`-shaped, running only
   on the curriculum Layer 3 selected, at whatever size this hop calls
   for (hop one: unchanged; hop two: real, from-scratch, deliberately
   smaller than qroq's original).
5. The FSM wrapper — NIGHT 7's shape, keeps the system around Layers
   0-4 in a defined state at all times.
6. **NEW — the recursion rule.** "The same five layers, run again, by
   whoever Layers 1-2 just produced enough trust to run unsupervised."
   Hop count is a parameter of Layer 6, not a fixed number. This spec's
   proof bar is 2 hops (see "Hops," below); hop 3+ is real future work,
   explicitly out of scope here.

## The correction signal (the one piece that makes Layer 1 cheap)

`open-hai` transcripts already carry the reward signal inline — no new
sensor, no separate labeling UI. If the `U|` line immediately following
an `A|` line restates the question, says "no I meant," or otherwise
re-asks, that's a real, cheap, textual NEGATIVE signal. If the next
`U|` line moves on to a new topic, that's a real POSITIVE signal (the
answer was accepted). This is what both the hand-scoring step (Step 0
below) and the FSM's own `JUDGING` state (Step 2) actually compute —
plain text pattern/semantic comparison across two adjacent lines, never
a new metric invented from scratch.

## Hops

**Hop 0 (not really a hop — today's real starting point):** Claude
reads real `open-hai` transcripts and does Layer 1's job by hand,
using the correction signal above.

**Hop 1 (Claude bootstraps Gemma's own process):** Claude builds a
real, small, standalone FSM (working name: `irl_bootstrap_fsm`, kept
deliberately distinct from `cursword_fsm.c` and from "famous-llm," the
Layer-4 OUTPUT — a process and its product need different names).
States: `IDLE -> WATCHING (tail a transcript.txt) -> JUDGING (one
gemma3:270m call, decision_mode=3-shaped: "did the last exchange look
corrected or accepted") -> PROPOSING (draft weights.txt line, human-
reviewed, NEVER auto-merged) -> IDLE`. Gemma's weights are never
touched — the FSM sits around it exactly as `cursword_fsm.c` sits
around its own model call. Once built, this FSM runs Layers 1-2
unattended, on Gemma's own conversation history, with zero further
Claude involvement per-run.

**Hop 2 (Gemma's process bootstraps a smaller famous-llm):** The SAME
`irl_bootstrap_fsm`, now pointed at Gemma's own judged corpus slice,
runs all five layers: Layer 0 = Gemma's judged transcript slice,
Layer 1/2 = hop 1's already-proven FSM+Gemma loop, Layer 3 = the same
cheap cosine router, Layer 4 = a real, small, from-scratch
`attention.c`-shaped net trained on that one narrow slice (fewer
params than qroq's original — "smaller" is literal here, not
marketing), Layer 5 = the same FSM shape wrapping it. The output is a
real "famous-llm" instance — not a "Gemma," a small net Gemma's own
process produced.

**Hop 3+ (explicitly NOT this spec's job):** Identical claim, one level
further down (the produced famous-llm bootstraps its own smaller
famous-llm). Real future work — proving it a third time tests
durability/compute budget, not the idea. Do not start this until hops
1-2 are built, proven, and reviewed.

## Proof bar per hop (the actual acceptance test, not "should work")

- **Hop 1 proof:** the FSM ran unattended for a real stretch against
  real transcript files, produced at least one `weights.txt` draft,
  and a human — blind-reading a handful of its per-exchange judgments,
  not told which the FSM flagged — agrees with the FSM's call more
  often than chance.
- **Hop 2 proof:** the resulting famous-llm answers a held-out real
  question from its OWN judged curriculum better than a `rand()`-init
  version of the identical net architecture trained on the identical
  data with NO IRL weighting. This isolates whether the recursion
  added anything — "better than Gemma" or "better than Claude" is the
  wrong, uninformative comparison; "better than its own unweighted
  baseline" is the honest one.

## Four real gaps this spec adds beyond NIGHT 11's own list

1. **A real stopping condition for `JUDGING`**, or weights drift
   forever with no re-review window. `PROPOSING` must be a real
   `PENDING_REVIEW` queue state, never an auto-merge, at every hop,
   however deep the recursion goes — NIGHT 9's review discipline must
   survive unattended operation, not just the first hand-run.
2. **An external ground-truth anchor at every hop.** Hop 2 in
   particular is a closed loop (Gemma picks its own curriculum slice
   AND judges its own correctness) — a textbook reward-hacking
   condition with nothing watching from outside. The fix: keep the
   SAME transcript correction signal (a real human's next line, via
   open-hai) as the anchor at every hop, never let a hop judge itself
   with no outside signal at all.
3. **Resource honesty.** Hop 2's from-scratch training run is a
   multi-minute-plus background job on this house's documented
   weak-CPU machine — run it under `nice -n 15 ionice -c3`
   ([[nice-heavy-background-work]]), as a real background job with its
   own progress marker file, never something a human sits and watches.
4. **Naming discipline.** `irl_bootstrap_fsm` (the process, hop 1) vs.
   "famous-llm" (the product, hop 2's Layer-4 output) vs.
   `cursword_fsm.c` (a real, different, still-unfinished stub) must
   stay three distinct names in every doc/commit that touches this, or
   the design rots into ambiguity the moment someone reads it cold.

## Smallest real first step (do exactly this, nothing bigger, first)

1. Point a read-only script at ONE real `open-hai` session's
   `transcript.txt`. By hand, hand-score 5 real `U|`/`A|` exchanges as
   corrected-or-accepted using the correction signal above. No FSM, no
   Gemma call yet — this step only proves the file format and the
   correction signal are as clean on real data as they look on paper.
2. Only once step 1 is confirmed: build `irl_bootstrap_fsm`'s
   `WATCHING`/`JUDGING` states around it, with Gemma doing the judging
   call (`decision_mode=3`-shaped, one bounded call).
3. Everything past that — `PROPOSING`/`PENDING_REVIEW`, hop 2's
   from-scratch net, Layer 6 as a general rule, hop 3+ — waits until
   steps 1-2 are built and independently checked. Do not skip ahead.

## Open questions this spec does not resolve

- Which real open-hai session(s) to pull the first 5 hand-scored
  exchanges from — pick a real, already-existing one when starting
  step 1, don't manufacture a synthetic conversation for it.
- Whether "Agent 45"/"Iqabella" (named in NIGHT 10's own request, per
  `HARNECIENT-NIGHT-TRACK-HORIZON-ITEMS.md`'s "not resolved" section)
  are a better hop-2 Layer-4 target than a brand-new qroq-shaped net —
  still open, not decided by this doc.
