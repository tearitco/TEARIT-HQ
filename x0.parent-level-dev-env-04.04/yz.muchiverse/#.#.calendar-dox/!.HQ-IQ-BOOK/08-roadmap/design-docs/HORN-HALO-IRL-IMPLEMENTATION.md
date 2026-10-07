# HORN_CHAT / HALO_CHAT / IRL Harness — Sprint 1-3 Implementation

**Status: IMPLEMENTED (2026-10-06)** — all three sprints complete, tested, committed.

## Summary

Built a complete attrition-model pipeline per A-TEARIT-IS-ALL-YOU-NEED.md §2-4:

```
HORN_CHAT (Sprint 1) → HALO_CHAT (Sprint 2) → IRL Harness (Sprint 3)
    │                       │                        │
    ▼                       ▼                        ▼
OpenRouter             Concept Bank            HORN vs HALO
+ chtpm                pipeline                grading loop
+ bank context         (DESCRIBE +            → weight deltas
                       validate +              → curriculum regen
                       tiered promotion)
```

## Sprint 1: HORN_CHAT v0.1

**Files:** `^.hai-horn/ops/horn_chat_openrouter.c`, `^.hai-horn/horn_chat.sh`, `^.hai-horn/layouts/horn_chat.chtpm`

- Terminal OpenRouter chat harness with `@` completion
- **Concept Bank context injection**: top-weighted spokes injected into prompt
- Verified: "gravity constant" referenced from bank (weight 1.0)

## Sprint 2: HALO_CHAT v0.1

**Files:** `^.hai-horn/ops/halo_chat_describe.c`, `^.hai-horn/ops/halo_chat_validate.c`, `^.hai-horn/halo_chat.sh`

- Inherits HORN_CHAT UI + adds Concept Bank pipeline per turn:
  1. **DESCRIBE** (Gemma LAN): reads chat history, picks applicable spokes
  2. **VALIDATE** (hub-and-spoke, delta bounds): tiered promotion per AUTO-PROMOTION-RULE.md
     - `preschool`/`elementary_hs` → queue for review
     - `associate_bachelor`/`master_phd` → auto-promote if score ≥ 0.90, N ≥ 20
- **OBS/FEEDBACK records** (A-TEARIT §2.1): unified format for all subsystems
- **Promotion ledger** (A-TEARIT §2.5): replay/simulate, Laplace score `(reward+1)/(reward+punish+2)`
- Verified: `gravity_constant → force` weight 0.60 → 1.0 via chat pipeline

## Sprint 3: IRL Harness

**Files:** `^.hai-horn/ops/irl_harness.c`, `^.hai-horn/ops/irl_signal.c`, `^.hai-horn/irl_pipeline.sh`

- Runs prompts through both HORN_CHAT and HALO_CHAT
- Grades via OpenRouter on 5 criteria: accuracy, clarity, completeness, usefulness, concept awareness
- Aggregates: win rates, avg scores, delta = (halo_wins - horn_wins) / total × 0.1
- Applies delta to Concept Bank spokes → curriculum regenerates
- Verified: HALO 60% win rate vs HORN 20%, delta +0.04 → spoke weights updated

## Concept Bank Curricula Engine

**Files:** `^.hai-horn/ops/curricula_engine.c`, `^.hai-horn/curricula_gen.sh`

- Reads spokes/weights/grades/dependencies → ranked curriculum (JSON + text)
- Grade filtering: 0=preschool..5=grad
- `/curriculum` and `/curriculum-json` commands in HALO_CHAT

## Test Harness

**File:** `^.hai-horn/halo_test_harness.sh` — 8 automated tests:
1. HORN_CHAT round-trip
2. Bank context injection
3. DESCRIBE produces candidate
4. VALIDATE promotes
5. Promotion ledger Laplace score
6. Curricula engine generates
7. Curricula JSON format
8. Grade filtering

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| C ops + shell launcher | Matches house precedent (gem-dev, robot-chat) |
| Gemma LAN for DESCRIBE | Free, local, always-on; house law: DESCRIBE never CLASSIFY |
| Tiered promotion | Matches A-TEARIT §5 bootstrap tiers |
| Laplace scoring | Reuses Bank Layer math, no new stats |
| HORN vs HALO grading | Same model judges both → fair comparison |
| Delta applied to spokes | Concept Bank is the shared substrate |

## Open Questions (per A-TEARIT §6)

- Corpus-level meta-weights: hand-authored vs overlap-derived
- Promotion thresholds: exact score × replay window size
- Validator bounds: grounded in tomom's MLP scale
- `trainer_bp` proposer: loss function + replay-batching

## Files in `^.hai-horn/`

```
ops/
  horn_chat_openrouter.c    # Sprint 1: OpenRouter round-trip + bank context
  halo_chat_describe.c      # Sprint 2: Gemma DESCRIBE + bank context
  halo_chat_validate.c      # Sprint 2: validate + tiered promotion
  obs_feedback_write.c      # Sprint 2: OBS/FEEDBACK writer
  promotion_ledger.c        # Sprint 2: replay + Laplace scoring
  concept_bank_ctx.c        # Shared: load top-weighted spokes
  curricula_engine.c        # Sprint 2/3: curriculum generation
  irl_harness.c             # Sprint 3: HORN vs HALO comparison
  irl_signal.c              # Sprint 3: aggregate grades → delta

halo_chat.sh                # Sprint 2: launcher with pipeline
halo_chat.pal               # PAL orchestration sketch
curricula_gen.sh            # CLI for curriculum display
halo_test_harness.sh        # 8 automated tests
irl_test_prompts.txt        # Test prompts
irl_pipeline.sh             # Complete IRL loop
irl_apply_signal.sh         # Apply delta to Concept Bank
learning_limits.pdl         # master_phd tier for auto-promotion
```

## Running

```bash
# Build all
cd ^.hai-horn/ops && gcc -o horn_chat_openrouter.+x horn_chat_openrouter.c && \
  gcc -o halo_chat_describe.+x halo_chat_describe.c -luuid && \
  gcc -o halo_chat_validate.+x halo_chat_validate.c && \
  gcc -o concept_bank_ctx.+x concept_bank_ctx.c -lm && \
  gcc -o obs_feedback_write.+x obs_feedback_write.c -luuid && \
  gcc -o promotion_ledger.+x promotion_ledger.c -luuid && \
  gcc -o curricula_engine.+x curricula_engine.c -lm && \
  gcc -o irl_harness.+x irl_harness.c && \
  gcc -o irl_signal.+x irl_signal.c

# Run HALO_CHAT
cd ^.hai-horn && ./halo_chat.sh

# Run test harness
cd ^.hai-horn && ./halo_test_harness.sh

# Run IRL pipeline (1 cycle)
cd ^.hai-horn && ./irl_pipeline.sh irl_single_prompt.txt 1
```

## Related Docs

- A-TEARIT-IS-ALL-YOU-NEED.md — master spec (§2-4 loop)
- AUTO-PROMOTION-RULE.md — tiered promotion rules
- IRL-BOOTSTRAP-RECURSION-SPEC.md — recursive bootstrapping (future)
- HORN_CHAT-HANDOFF.md — original handoff for Sprints 1-3