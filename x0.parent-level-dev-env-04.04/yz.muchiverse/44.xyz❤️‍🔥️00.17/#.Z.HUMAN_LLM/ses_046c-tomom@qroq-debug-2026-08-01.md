# 🩺️🔧 SESSION 046c — TOMOM@QROQ TRAINER DIVERGENCE BUG (2026-08-01)
# Handoff report — write this BEFORE touching the Mac. If this box dies,
# a future agent can pick up exactly here.

===============================================================
TL;DR STATUS
===============================================================
- ✅✅ BUG FIXED (2026-08-01, verified on Mac): gradient trainer now converges.
- 🐛 Root cause: ALL savers wrote floats with "%f" (6 decimals). Tiny Adam
  v-state values (< 1e-6) were truncated to "0.000000" on disk and reloaded
  as EXACT 0. Then Adam denominator = sqrt(0)+EPSILON(1e-8) → update
  = lr*mh/1e-8 exploded to ±100-1000 in a single step. Loss was diverging
  because the optimizer state was being destroyed by low-precision I/O.
- 🛠️ Fix: replaced ALL save() fprintf formats with "%.9g" (full float32
  precision, still human-readable text) in optimizer.c, backward_prop.c,
  forward_prop.c, trainer.c. State file lines fixed to "%.9g %.9g %.9g 0/%d".
  IMPORTANT: fscanf loaders KEEP "%f" (fscanf cannot take precision specifiers).
- ✅ Verified on Mac mini corpus (12 words, 10 epochs):
  Loss 2.488 → 2.468, steadily decreasing BELOW uniform baseline (ln(12)
  ≈ 2.485) → genuinely learning. No explosion. Weights all bounded ≤ 0.05.
  Adam update deltas measured = ±0.0010 (max possible = lr) after fix.
- 🧠 Debug sequence that cracked it: (1) single-step fine, sequential blows up;
  (2) DBG instrumentation showed Wmax=258 with M/V/G all bounded — math can't
  do that ⇒ I/O bug; (3) python Adam replicate with file values predicted the
  SAME explosion, idx matched exactly; (4) exploding element had V=0.00000000
  exact zero while M≠0 ⇒ precision-loss on v-state save.


===============================================================
WHAT WE'RE DOING (current work)
===============================================================
1. ✅ DEBUG COMPLETE — bug fixed and verified (see TL;DR).
2. NEXT (in order):
   - Run FULL-corpus verification on Mac: Mathematics (108-word, rebuilt
     vocab) — expect loss → ~4.68 or below.
   - Retrain all 10 subject curricula fresh (each own matrix files) ON THE MAC.
   - Wire chatbot_moe_v1 to read trained matrices (currently scores from vocab
     fields only, so edits don't change generation).
   - Resume hand-tuning harness (Track C): ollama edits vocab_model.txt + diff
     + validate + retrain + eval + keep/revert.
   - gemma→gemma supervision loop: still on hold. Depends on working trainer
     for validate+retrain step. Can resume once full-corpus training verified.

3. MAC THING (per user, right after bug fix):
   - rsync local T@Q → /Users/lfs.master/tomom-qroq/  ✅ (files already pushed)
   - compile all 8 binaries on Mac                        ✅ (done)
   - run instrumented debug on Mac, read DBG output       ✅ (done)
   - cleanup with R6 convention from .MAC-ACCESS.txt      — run at end of session

===============================================================
PROGRESS MADE THIS SESSION (recent debugging)
===============================================================
- Found deterministic smoking gun: weights jumping 0.05 → 169–366 after ONE
  update with a BOUNDED gradient. Different scripts jumped at different steps
  → uninitialized/stateful I/O, not pure math.
- Verified formats all match: weights/m/v/grad_output all = 204 floats
  (16×12 + 12). So it was NOT a row-count mismatch.
- Verified single-step is sane: fresh init ±0.05, one manual update with real
  grad → loss 4.63, max change 0.001.
- Cross-referenced exploding element (python Adam replicate): idx matched
  EXACTLY, V=0.00000000 exact zero while M≠0 ⇒ precision-loss on v-state save.
- Fixed all savers: "%f" → "%.9g". State lines "%f %f %f" → "%.9g %.9g %.9g".
- Verified on Mac: mini corpus loss 2.488 → 2.468 (below uniform 2.485),
  weights ≤ 0.05, Adam deltas = ±0.0010. BUG CONFIRMED FIXED.

===============================================================
FILE MAP (where things live)
===============================================================
- T@Q core: "#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️/"
  - optimizer.c ← FIXED (%.9g savers, no debug line, fscanf stays %f)
  - forward_prop.c, backward_prop.c, trainer.c ← FIXED same way
  - chatbot_moe_v1.c (untouched this session)
  - curriculum/ghmini_corpus/ghmini_corpus.txt (12-word debug vocab)
  - curriculum/<Subject>/<Subject>.txt (10 subject curricula)
- Mac access + cleanup rules: "#.Z.HUMAN_LLM/.MAC-ACCESS.txt" (R6 = cleanup)
- Progress docs: jul31-human-llm.md, ses_046b-tomom@qroq.md, this file
- Plan: "#.Z.HUMAN_LLM/^.2DO.aug01_2026.txt"
- gemma CLI project (separate): "046.open-gema🤖️+1/" (SUPERVISION-HANDOFF.txt
  has the R1–R6 loop rules; builder loop not started)

===============================================================
EXACT COMMANDS FOR PICKUP
===============================================================
Compile on Mac:
  for f in forward_prop backward_prop optimizer trainer vocab_model \
           chatbot_moe_v1 attention mlp_layer; do
    gcc "$f.c" -o "+x/$f.+x" -lm
  done

Run mini verification (12-word):
  rm -rf curriculum/ghmini_train && mkdir -p curriculum/ghmini_train
  cp curriculum/ghmini_corpus/ghmini_corpus.txt curriculum/ghmini_train/ghmini_train.txt
  ./+x/trainer.+x curriculum/ghmini_train/ghmini_train.txt
  → expect loss declining 2.49 → <2.48 (below uniform ln(12)=2.485)

Full-corpus test (Mathematics, 108-word):
  rm -rf curriculum/Mathematics_train && mkdir -p curriculum/Mathematics_train
  cp curriculum/Mathematics/Mathematics.txt curriculum/Mathematics_train/Mathematics_train.txt
  ./+x/trainer.+x curriculum/Mathematics_train/Mathematics_train.txt
  → expect loss declining toward ≤4.68

Cleanup (house rule R6):
  ssh ... "pgrep -fl 'tomom-qroq' || echo clean"
  ssh ... "pkill -f tomom-qroq && echo killed"
  ssh ... "rm -rf /tmp/ghd* /tmp/ghmini* /tmp/ghtrace* /tmp/ghopt* /tmp/gh_dbg*"

===============================================================
LOSS BASELINES
===============================================================
- uniform on 12 words ≈ ln(12) = 2.485
- uniform on 108 words ≈ ln(108) = 4.68
- Anything worse = serialization/math bug still present.
