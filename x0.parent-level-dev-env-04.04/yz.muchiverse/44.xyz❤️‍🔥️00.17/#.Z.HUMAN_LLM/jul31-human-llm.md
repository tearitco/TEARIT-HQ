# 🤖️🪤️🏠️ JUL-31 HANDOFF — "NEXT LEVEL" PROGRESS REPORT
# 📛 NAMING (2026-08-01): project formerly "grok@home" is now "tomom@qroq".
#    Aliases: tomokazu / mother tomokazu / mother of iqabella. Short forms
#    normally used: "tomo", "tomokazu", "tomom". Shorthand in docs: T@Q.
#    Local dir: 3.stage.llm.tomom@qroq.fame]921🐋️. Mac dir: tomom-qroq.
# 📌 Topic: Hand-tunable / human-&-agent-tunable LIGHTWEIGHT system, not a
#        strict LLM. Target homes: 🎮 video games (competent NPC behavior +
#        convo), 🔧 harnesses (automation + imitation), 🧠 learning/embedded.
# 🔁 Handoff for future agents. 🧃 Emoji-heavy by design. 2026-08-01.

================================================================
0️⃣ THE ASK (what "next level" actually means)
================================================================
- NOT a strict LLM. A LIGHTWEIGHT system that:
  🎮 lives inside video games for competent NPC behavior/conversation
  🔧 drives harnesses for automation & imitation
  🧠 runs embedded + doubles as a learning tool
- MUST be HAND-TUNABLE + AGENT-TUNABLE: we read the weights as TEXT,
  we (or an LLM harness) can edit them, every change is auditable.
- Realistic about it: not chasing GPT, chasing a controllable,
  explainable, embeddable brain that *does* stuff with tools/BT/FSM.

================================================================
1️⃣ WHERE WE ARE TODAY (the honest scoreboard 🎯)
================================================================
TRACK A — TOMOM@QROQ 🐋️  (the hand-tunable-text-weights track)
  STATUS: 🚧 IN PROGRESS / pipeline rebuilt, training loop STILL BUGGY.
  ✅ DONE:
    - 10 subject corpora built from ship.sip 📚️ (Astronomy, Biology,
      Chemistry, Economics, Geography, History, Literature, Mathematics,
      Physics, Programming) -> curriculum/<Subject>/<Subject>.txt
      (~724 merged vocab words across all 10).
    - curriculum_bank.txt lists all 10 ✅ (fixed dup-Astronomy bug).
    - MoE merge VERIFIED: chatbot_moe_v1 loads all 10 & generates.
    - Root bug #1 FIXED: trainer wrote/read matrices as TEXT, but
      forward/backward loaded them with binary fread -> garbage bytes,
      loss stuck ~4.67 (=uniform). Switched to text loaders.
    - Root bug #2 FIXED: grad serialization (output-layer pointer UB,
      MLP bias grads missing). Added save_grad_output/save_grad_mlp.
    - Root bug #3 FIXED: no matrix initializers existed. Added
      output-init + auto-init inside trainer.
    - Root bug #4 FIXED: attention ctx bug (accumulated wrong dims).
    - Removed forward-only layer_norm (no backward grad -> loss spiked
      to 17.6).
    - Softmax Jacobian step added to backward (grad through softmax).
  ❌ STILL BROKEN (as of this handoff):
    - Loss DOES NOT CONVERGE. On a clean 108-word Mathematics vocab:
      loss starts ~17.9 and RISES to ~18.2 across 10 epochs.
      Uniform baseline = ln(108) ≈ 4.68. So the model is confidently
      predicting the WRONG token — math bug remains.
    - Output layer weights EXPLODE to ±4000, logits to ±2000 (fresh
      init gives ±0.05, so the divergence happens IN the train loop).
    - Suspect: MLP gradient norm blows up (see §4 diagnosis).

TRACK B — IQABOD 🛠️  (the transformer track, real multi-layer)
  STATUS: 🟢 RUNNING SIDE-BY-SIDE, UNCHANGED this session.
  - Loop pid 1282567, lr=0.02, hourly cron, conv store on Linux
    (~/iqabod-store/conv-g1/).
  - Strengths: REAL attention, real scaling, actually learns language.
  - Weakness for "next level": .weights = 60KB opaque float matrices.
    NOT hand-tunable in the human-readable sense. Auditable? No.

TRACK C — GEMMA 🧠  (the "most viable path forward")
  STATUS: 🟢 SEPARATE PROJECT (046.open-gema), SEPARATE GOAL.
  - gemma3:270m over LAN (10.0.0.144:11434), no big LLM needed.
  - Route-and-reason design: deterministic keyword tool table + tiny
    RAG windows -> tools are ground truth, gemma makes choices.
  - Is THE most viable for REAL competent NPC convo/automation NOW,
    because it's an actual small LLM with tool grounding.
  - This handoff is about the T@Q hand-tuning track specifically; gemma
    is the reference "this is the bar" (see §2 comparison).

================================================================
2️⃣ THE COMPARISON (for the "next level" goal, who's how close)
================================================================
CRITERION         | T@Q 🐋️ | IQABOD 🛠️ | GEMMA 🧠
------------------|--------|-----------|----------
Weight format     | ✅ TEXT | ❌ floats | ❌ (weights, not editable)
Hand-tunable      | ✅ yes  | ❌ no     | ❌ (prompts only, not weights)
Agent-tunable     | ✅ yes  | ~ (RL on top) | ✅ (prompt harness)
Auditable diffs   | ✅ yes  | ❌ no     | ~ (prompt/versioned)
Per-token meaning | ✅ yes (embedding/weight/bias = readable knobs) | ❌ | ❌
Real language     | ❌ toy  | 🟡 ok-tiny | ✅ 270m small-LLM
Tool/BT/FSM hook  | ✅ dirs+tools | ✅ shared infra | ✅ already doing it
Embeddable size   | ✅ tiny | ✅ small | 🟡 ~150MB quantized
NPC-convo quality | ❌ now  | 🟡 weak  | ✅ viable
Video-game fit    | 🟡 control/steering layer | 🟡 | ✅ convo+brain
Ease of continue  | ✅ pure C, no deps | ✅ | ✅

TL;DR: 🧠 gemma is the viable brain. 🐋️ T@Q is the viable
STEERING/DIAL-able control layer (once it trains). They are not rivals —
they are different layers of the SAME stack. 🛠️ iqabod sits between.

================================================================
3️⃣ CAN HAND-TUNED WEIGHTS MEANINGFULLY PLAY IN VIDEO GAMES? 🎮
================================================================
YES — but NOT as the language brain. As the **NPC control/dial layer**:
- 🎛️ Per-token bias knobs = personality sliders. Edit bias1/bias2 of
  a "greet" token to make an NPC friendlier/hostile by HAND. That's a
  designer tuning a character sheet, not a scientist training a model.
- 🧩 MoE router (already built: meta_rl_weights.txt + curriculum_bank)
  = faction/location knowledge packs. "If NPC is in Blacksmith zone,
  activate Programming+Economics corpora." Human-visible, human-editable.
- ⚙️ BT/FSM on top (the planned Track-E runtime): hand-tuned weights
  pick WHICH behavior tree state to use; trees do the real animation/
  quest logic. Weights = preferences, not knowledge.
- 🔒 No hallucination risk in game logic because tools/trees are ground
  truth — same bet gemma/045 already proved.
- 🧱 Learning tool angle: a kid/student can SEE why a token reacts —
  "this NPC likes you because weight went from 0.3 to 0.8" — that's a
  teachable, inspectable system.
REALITY CHECK: for free-flowing convo you still want gemma (or iqabod)
as the model, with T@Q biases steering the TOP-LEVEL behavior. For a
toy/embedded/NPC-flavor-text system, T@Q ALONE is enough.

================================================================
4️⃣ THE ONE THING BLOCKING US (current debug, pick up HERE) 🔧
================================================================
SYMPTOM (reproduced today, /tmp/ghrun):
  - Fresh init: output weights ±0.05, logits ±0.05 → loss ≈ 4.63 ≈ uniform ✅
  - 1 manual step with real gradient: loss 4.63, update tiny, sane ✅
  - Full trainer run: Output grad norm stays ~1.0 but **MLP grad norm
    EXPLODES: 0.16 → 660 → 1072 → 1388 → 2505** across words. Output
    weights end ±4000. Loss 18.
DIAGNOSIS (strong leads):
  1. MLP gradients exploding = hidden-state values likely blowing up
     through epochs (ReLU + residual ctx). Check hidden_state.txt values
     across epochs; check m.weights not exploding through updates.
  2. Suspect the ReLU derivative path or the missing grad path for
     ctx's residual term (ctx += iv: backward adds g_iv but iv is the
     INPUT; the residual contribution to g_c should pass through
     IDENTITY — verify g_c includes the passthrough, else grads are
     wrong-scaled through the MLP).
  3. Check g_mlp gradient clipping — backward clips MLP grads? The
     output layer gets clipped to 1.0 but verify MLP has the same guard.
  4. Verify attention scores asr == attn_scores_raw.txt is written
     BEFORE causal mask so softmax-Jacobian in backward matches forward.
  5. Confirm causal_attention=1 in config.txt is what the trainer used
     (it shells config from the OUTPUT DIR, not the CWD — see trainer.c).
NEXT ACTION when picked up:
  - Rebuild fresh, run trainer 2 epochs with MLP grad-norm + hidden-norm
    prints each step. Find first word where MLP norm jumps. That word's
    gradient math is the bug.
  - Once loss < 4.68 and decreasing → training is REAL → then re-run all
    10 subjects, re-verify MoE, then proceed to hand-tuning harness.

================================================================
5️⃣ ROADMAP TO "NEXT LEVEL" (what's left, in order) 🗺️
================================================================
[ ] FIX the MLP-grad explosion (the blocker, §4) -> loss converges
[ ] Retrain all 10 subject corpora fresh (each own matrices)
[ ] Wire chatbot_moe_v1 to READ trained matrices (currently it scores
    from vocab fields only — so generation stays end-token-heavy even
    after training!)
[ ] Build corpus→vocab converter + IQA converter (2DO Track B/D)
[ ] Hand-tuning harness (Track C): ollama edits vocab_model.txt +
    diff + validate + retrain + eval + keep/revert (git-style)
[ ] meta_rl_weights.txt tuning same way (steer corpus selection)
[ ] BT/FSM runtime skeleton shared by T@Q + IQA (Track E)
[ ] BT chaining: choose corpus → generate → edit → verify
[ ] Side-by-side compare harness: T@Q vs IQA on same prompts
[ ] 💎 THE DEMO: hand-tune a "greeting" token's bias, watch an NPC
    (in the web-interface or CLI) get friendlier — IN 5 MINUTES, by
    hand, with a git diff. That is the pitch.

================================================================
6️⃣ THE RULE (this project's spine) 🦴
================================================================
THE RULE: if the current machinery can't do it, we BUILD the machinery.
No cop-outs. Push forward.

The hand-tuned weights are not a competitor to gemma — they are the
READABLE, AUDITABLE, EMBEDDABLE CONTROL LAYER that makes the whole
stack trustworthy in a video game. That's the story. Tell it to the
next agent loudly.

================================================================
7️⃣ FILES THAT MATTER (quick map) 🗃️
================================================================
- #.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️/  → T@Q core:
    trainer.c, forward_prop.c, backward_prop.c, optimizer.c,
    vocab_model.c, chatbot_moe_v1.c, curriculum_bank.txt,
    curriculum/<Subject>/<Subject>.txt (10), +x/*.+x (built)
- #.Z.HUMAN_LLM/ship.sip-📚️-corpus-bank/curricula/*/curriculum.txt
  → source corpora (10 subjects, 624 words)
- #.Z.HUMAN_LLM/^.2DO.aug01_2026.txt → 5-track integration plan
- #.Z.HUMAN_LLM/^.feasibility_handtune_aug01_2026.txt → candidate verdict
- #.Z.HUMAN_LLM/ses_046b-tomom@qroq.md → this session's full log
- 046.open-gema🤖️+1/SUPERVISION-HANDOFF.txt → gemma-CLI supervision
  instructions for future agents (separate loop)
