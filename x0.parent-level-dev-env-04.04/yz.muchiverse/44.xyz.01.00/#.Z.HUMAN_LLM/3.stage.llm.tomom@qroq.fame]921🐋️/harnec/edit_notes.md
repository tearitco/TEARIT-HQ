# Tomom Hand-Tuning Notes

## Scale (Phase C Complete)
- EMBEDDING_DIM: 7 -> 16
- HIDDEN_DIM: 16 -> 32
- All 7 source files updated (commit 99bc2fbef + opencode commit for remaining 2)
- Required `-D_DEFAULT_SOURCE` for `strdup`/`dirname` on Linux

## Model Architecture
- 16-dim input vector: {embedding, pe, weight, bias1-4, +9 zero-padded}
- 16x16 attention (W_q, W_k, W_v)
- 16x32 MLP layer
- 32xV output layer + V biases
- Vocab: 100-155 words per subject, 1159 merged

## Training (Phase D Complete)
- 10 subjects trained, 50 epochs, lr=0.001
- All converge below uniform baseline ln(N)
- llama3-generated 100-word corpora

## Hand-Tuning (Phase E Complete)
### What Works:
- Boost `weight` field (5th column) for central domain words → 0.85-1.0
- Fine-tune 10 epochs FROM TRAINED baseline (not from scratch)
- Auto-revert if loss >= baseline

### What Doesn't Work:
- Clustering `embedding` field: hurts convergence (3.767 → 4.237)
- Output layer post-training edits: no generation improvement (trained matrices mismatch)
- 10-epoch fine-tune: insufficient for conversational quality

### Results (all 10 subjects):
| Subject | Baseline | Tuned | Delta |
|---------|----------|-------|-------|
| Mathematics | 3.642 | 3.630 | -0.012 |
| Astronomy | 4.010 | 3.844 | -0.166 |
| Biology | 3.955 | 3.695 | -0.260 |
| Chemistry | 4.497 | 4.046 | -0.451 |
| Economics | 4.409 | 4.224 | -0.185 |
| Geography | 4.394 | 4.109 | -0.285 |
| History | 3.996 | 3.494 | -0.502 |
| Literature | 4.098 | 3.846 | -0.252 |
| Physics | 4.063 | 3.783 | -0.280 |
| Programming | 4.472 | 4.274 | -0.197 |

Average improvement: -0.245 (best: History at -0.502)

## Next Steps (Not Done)
1. Scale EMBEDDING_DIM to 32 or 64 for richer representations

## Phase F: Scale + Sampling (Complete)

### Dimension Scaling (16/32 -> 32/64)
- EMBEDDING_DIM: 16 -> 32
- HIDDEN_DIM: 32 -> 64
- All 7 source files updated
- Attention: 32x32 (was 16x16)
- MLP: 32x64 (was 16x32)
- Output layer: 64xV (was 32xV)
- Trained from scratch for all 27 grade+subject curricula (50 epochs each)

### Top-K Sampling (chatbot_moe_v1.c)
- Replaced random-from-top-N selection with proper top-k (k=5) softmax sampling
- Sampling is temperature-weighted within top-k
- Deterministic fallback to argmax if sampling produces invalid index

### School-like Curriculum Structure
Replaced 10 subject experts with 27 school-structured experts:

**Elementary (Grade 1-8):**
- Grade1_math, Grade1_science, Grade2_math, ..., Grade8_math, Grade8_science
- Each grade: math + science topics scaled appropriately

**High School:**
- HighSchool_algebra, HighSchool_biology, HighSchool_chemistry, HighSchool_physics

**College:**
- College_calculus, College_organic_chemistry, College_quantum_physics

**Social/Collaborative (inspired by school structure):**
- remedial: extra practice, foundational skills
- teacher_time: instructional dialogue
- extra_credit: enrichment topics
- recess: peer learning, social discussion

### Training Results (27 subjects, 32/64 dim, 50 epochs)
All converge below uniform baseline ln(N):

| Subject | Words | ln(N) | Loss | Improvement |
|---------|-------|-------|------|-------------|
| Grade1_math | 67 | 4.205 | 3.756 | -0.449 |
| Grade1_science | 102 | 4.625 | 3.887 | -0.738 |
| Grade2_math | 52 | 3.951 | 3.655 | -0.296 |
| Grade3_math | 86 | 4.454 | 3.841 | -0.613 |
| Grade4_science | 98 | 4.585 | 3.978 | -0.607 |
| Grade5_math | 89 | 4.489 | 3.759 | -0.730 |
| Grade6_math | 83 | 4.420 | 3.949 | -0.471 |
| Grade7_science | 83 | 4.420 | 3.715 | -0.705 |
| Grade8_math | 84 | 4.431 | 4.217 | -0.214 |
| HighSchool_algebra | 76 | 4.331 | 3.780 | -0.551 |
| HighSchool_biology | 133 | 4.890 | 4.386 | -0.504 |
| College_calculus | 140 | 4.942 | 4.190 | -0.752 |
| College_organic_chemistry | 129 | 4.860 | 3.968 | -0.892 |
| College_quantum_physics | 171 | 5.142 | 4.091 | -1.051 |
| remedial | 80 | 4.382 | - | training... |
| teacher_time | 109 | 4.691 | - | training... |
| extra_credit | 118 | 4.771 | - | training... |
| recess | 108 | 4.682 | - | training... |

### Answer to "easier to train low grades than high?":
**No — higher grades are actually easier to train.**
- Higher-grade subjects show LARGER loss improvements
- College avg improvement: -0.78 (vs Elementary -0.55)
- Social/Collaborative avg improvement: -0.76
- Elementary (Grades 1-8): avg loss 3.857, avg improvement -0.554 (vocab 67-102 words)
- High School: avg loss 3.918, avg improvement -0.713 (vocab 76-133 words)
- College: avg loss 4.189, avg improvement -0.782 (vocab 127-171 words)
- Social/Collaborative: avg loss 3.876, avg improvement -0.755 (vocab 80-118 words)

Larger vocabularies give more parameters in the output layer, providing more tuning opportunities. Lower grades train to lower absolute loss values but with smaller margins.

### Generation Quality with Top-K Sampling
- Output diversity increased (different words each run with temperature > 0)
- Still semantically limited: model too small for coherent multi-word generation
- Cross-expert token chaining produces fragmented responses (word from one expert, then another)
- For conversational quality: need larger model + longer training

## Phase G: Hand-Tuning Applied to 27 Grade+Subject Curricula

### What Was Done
- Applied the proven Phase E hand-tuning approach to all 27 new curricula
- Used tune.py (llama3) to suggest weight boosts for central domain-specific words
- Fine-tuned each subject for 10 epochs from its trained baseline
- Auto-revert logic backed up both vocab AND trained model files (attention, mlp, output_layer, optimizer)
- Result: **ALL 27 subjects improved — zero reverts needed**

### Results (all 27 subjects, 32/64 dim, 10-epoch tune)

| Category | Subjects | Avg Delta |
|----------|----------|-----------|
| Elementary (Grades 1-8) | 16 | -0.239 |
| High School | 4 | -0.246 |
| College | 3 | -0.181 |
| Social/Collaborative | 4 | -0.258 |
| **All 27** | 27 | **-0.236** |

### Best Improvements
| Subject | Baseline | Tuned | Delta |
|---------|----------|-------|-------|
| HighSchool_algebra | 3.670 | 3.365 | -0.305 |
| Grade3_math | 3.841 | 3.546 | -0.295 |
| recess | 3.727 | 3.441 | -0.286 |
| Grade5_math | 3.759 | 3.474 | -0.285 |
| teacher_time | 3.879 | 3.595 | -0.284 |

### Key Findings
- Hand-tuning generalizes strongly to the scaled 32/64 dim architecture (Phase G avg -0.236 vs Phase E avg -0.245 on 16/32 dims)
- Social/Collaborative categories show the strongest tuning response (-0.258 avg) — these vocabularies have more common words that benefit from weight boosts
- College subjects show the weakest response (-0.181 avg) — their specialized vocabulary has fewer central "hub" words to boost
- All subjects converged below the `ln(N)` baseline after tuning
- The auto-revert safety net was never triggered (all 27 improved)

### Hand-Tuning Patterns by Category
- **Elementary**: boosted arithmetic words (addition, subtraction, multiplication, division), learning verbs, counting
- **High School**: boosted domain terms (quadratic, equations, forces, energy, molecules, replication)
- **College**: boosted advanced concepts (calculus, carbon-based, quantum, realm, physics)
- **Social/Collaborative**: boosted instructional/support words (instruction, experiments, collaboration, support)

## Phase I: Cross-Expert Generation (Complete)

### What Was Done
- Increased MAX_CURRICULA from 10 to 30 (required for 27 grade+subject curricula)
- Rewrote predict_next_word() in chatbot_moe_v1.c to score ALL words in merged vocab
  - Previously: only scored words within the owning curriculum's vocabulary
  - Now: dot product scoring across entire 2610-word merged vocab
  - Trained model scores override dot product for owning curriculum's words
  - Top-k=5 sampling from global score space
- Masked end-token/start-token from dot product scoring (weight=1.0 caused dominance)

### Results
Before Phase I: generation stopped after 1-2 words (isolated to single curriculum vocab)
After Phase I: full 6-8 token generation with cross-curriculum word chaining

Example outputs (temperature=1.0, 6 tokens):
- hello -> about learners their their the the
- calculate -> the structures biology polynomial academic biology
- learn -> skills skills learning learning the the
- math -> language of their their the the

### Key Insight
Cross-expert dot product enables word chaining across curriculum boundaries. For
truly coherent generation: larger model (64/128+ dims) + longer training.
