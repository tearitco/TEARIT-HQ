AGENT PROGRESS REPORT — j31 (2026-07-31)
=========================================
Working agent: muchi-pal-agent (045.muchi-pal-agent🤖️+1)
Report: session 1 of the "run-the-agent + build-the-team" pass.
Companions: dev-agent-convo/00.session-001🌌.md, 01.assessment💼️.md,
02.LAN-nodes🌐.md, 03.IQABOD-test-report🔬.md, 04.arch-strategy🧠.md,
05.genius-idea💡.md, agent-onbording.txt.

PART 1 — WHAT WAS DONE THIS SESSION (verified, with evidence)
--------------------------------------------------------------
1. Agent harness baseline: 5/5 scenario harnesses PASS
   (proof/ under the agent). list_dir, H1 write+run python, H2 edit-book,
   iqabod-chat (PITFALL-60 fix confirmed), model-remember dual-boot.
2. Code-gen probe (on LAN nodes, NOT this box — compute constraint):
   - gemma3:270m → cannot write code on ANY node (chat/tool-router only)
   - stable-code (Mac) → syntax fail
   - llama3:latest (Mac) → correct, playable game (deployed+verified as
     ~/games/number_game.py on the Mac)
   - qwen2.5-coder:7b is on the Linux box; proven valid code locally earlier.
3. LAN nodes online + documented:
   - MAC lfs.master@10.0.0.144 (ollama LAN-open :11434) — heavy lifting
   - LINUX jb@10.0.0.187 (I exposed ollama host=0.0.0.0, pulled gemma3:270m)
     — reliable, slower, gemma-only
4. IQABOD tested on the Linux node:
   - pipeline works: vocab_only 0.02s, train 10 epochs 0.11s
   - pre-trained 32-token curriculum: semi-coherent word order + <UNK> leaks
   - loss descends but plateaus at random floor (2.6225→2.6211, ln14≈2.64)
     → LR (1e-5) too low is the prime suspect — mechanism OK, optimizer weak
   - binaries are plain libm/libc; easy to compile on any node
5. Docs created: agent-onbording.txt + dev-agent-convo/00-05 (session,
   assessment, LAN nodes, IQABOD report, arch strategy, genius-idea).

PART 2 — LOCKED DECISIONS (user, 2026-07-31)
---------------------------------------------
D1 Storage: this box = tiny manifest (curricula.pdl + reports) ONLY.
   Nodes hold heavy data: ~/iqabod-store/ on Linux and Mac.
   Pointer, not payload (like pointers.pdl). Zip-and-ship safe.
D2 iqabod = BANK OF SMALL EXPERTS + ROUTER (per-curriculum weights, never
   merged, never all-in-one-place). NOT full MoE. Multiple labeled scale
   systems per machine; prove toy-scale loop first.
D3 Genius idea = ON-DEMAND DISTILLATION with gap-driven teacher prompting:
   gap report (<UNK>/weak spots) → prompt llama3/gemma "fill these gaps" →
   retrain (0.1s) → expanded brain. Buildable, no new theory.
D4 Supervisor = reusable OPS + a harness that calls them (house pattern).
D5 Teacher = llama3 primary, gemma3:270m fallback.
D6 First goal = reasonable CONVERSATION (grade curriculums); story writing +
   basic pal coding = documented adjacent goals.
D7 Agent tool pipeline (write/edit/read/run/search) is the shared toolset;
   gemma+iqabod are provider_kinds behind one router.

PART 3 — WHAT I'M STARTING NOW (supervisor v1)
----------------------------------------------
A. Storage layout on nodes (do first — everything stores there):
   Linux 10.0.0.187: ~/iqabod-store/<curriculum>/{corpus.txt,vocab,weights}
   Mac 10.0.0.144:   ~/iqabod-store/ (for big-scale runs later)
B. curricula.pdl manifest on THIS box (local, tiny):
   name|topic|rung|expert_location|weights_size|last_report|verdict
C. New ops (ops/, self-contained C, house style, reusable):
   - train_step.c     queue entry → teacher generate → vocab_only+train on
                      the target node → eval numbers → verdict string
   - eval_curriculum.c held-out prompts → loss + <UNK>% + optional
                      llama-as-judge → report row (gap report for D3)
   - list_curricula.c read curricula.pdl manifest
D. Harness: %.harnesses/iqabod-loop/ (or agent scripts/) — reads
   curriculum-queue.txt, calls the ops in order, appends training-report.txt,
   prints one-line summary, sleeps (set-and-forget capable).
E. First queue: conversation curriculum (grade1: simple chat vocab).

PART 4 — NEXT AFTER SUPERVISOR v1 (in order)
--------------------------------------------
1. Gap-report teacher prompting (D3) — same ops, smarter prompt.
2. Router op: intent/topic → pick curriculum expert.
3. Bridge harness: gemma routes + iqabod expert generates → file lands.
4. Behavior-tree decision engine (replace hardcoded gemma_strategy weights).
5. escalate_to_llama op (management-review rung).

STATUS: all green. Supervisor v1 build is next.

UPDATE (same day, mid-build): IQABOD ROOT-CAUSE BUG FOUND + first-slice result
------------------------------------------------------------------------------
The first vertical slice (conv-g1 conversation curriculum) hit a REAL IQABOD
bug. Diagnosis (from generation_module.c source, confirmed on node):
- Generation caps at seq_len=32 (line 766: `if (pos >= model->seq_len) break;`)
- TRAINING does NOT. Training loop (line 1668-1673) feeds position i over the
  WHOLE corpus (num_corpus_tokens up to 80+), no seq_len guard.
- Any corpus longer than 32 tokens writes KV cache out of bounds → heap
  corruption → "free(): invalid pointer" crash + "Position N out of bounds".
- This explains BOTH the "loss giving exact same for both times" (#.how2.3)
  AND the loss plateauing at random floor: iqabod has only ever been trained
  on ≤32-token toy corpora. 80-token conversation corpus = crash.
- Our toy 14-token corpus worked precisely because it fit under 32.

Impact: iqabod CANNOT currently be trained on any real curriculum (>32 tokens
= one short sentence). This is the single highest-value fix in the whole
project — it unblocks conversation training entirely.

Proposed fix (house-style, small): sliding-window training — wrap position as
`pos = i % model->seq_len` and zero the KV cache at each window boundary, so
training matches generation's 32-token context. One targeted change to the
training loop in generation_module.c. Then recompile on the Linux node, rerun
conv-g1, verify loss genuinely descends below the random floor.

Files touched: #.z.mirror_llm]z5]IQABOD🪞️+4/generation_module.c (training loop only).

UPDATE 2 (same day): FIX DONE + VERIFIED — iqabod now learns for real
----------------------------------------------------------------------
1. Sliding-window training fix applied to generation_module.c (pos = i % seq_len,
   zero KV cache at window boundary). Recompiled on the Linux node. The
   >32-token corpus crash ("free(): invalid pointer") is GONE — training now
   completes on the full 80-token conv-g1 curriculum.
2. LR was the second bottleneck. config.txt lr=1e-5 → loss sat on the random
   floor (4.38 = ln 80). At lr=0.1: loss fell 4.38 -> 0.0013 in 100 epochs,
   and generation became genuinely coherent:
     "hello is a common greeting Good morning used in the early hours day
      suitable any time after breakfast Hi casual way to greet someone How
      are you"
   Real learned word order, no <UNK> leak. iqabod can now actually learn a
   real conversation curriculum.
3. Confirmed end-to-end on the Linux node (conv-g1, ~/iqabod-store/). This
   box only holds the curricula.pdl manifest + this report.

NEXT (supervisor v1 continues): set the node-side default LR to ~0.1
(config.txt in each store dir or via the train call), rebuild the conv-g1
weights with the default, wire the generate->clean->train->eval->report loop
into the train_step/eval_curriculum ops, then iterate the curriculum queue.

UPDATE 3 (same day): supervisor v1 LOCKED IN on the Linux node
---------------------------------------------------------------
- config.txt written to ~/iqabod-store/conv-g1/ with learning_rate=0.1,
  epochs=100. The DEFAULT `main_orchestrator.+x train corpus.txt` path now
  trains clean with no args: loss 0.00056 (100 epochs).
- conv-g1 eval (held-out prompts "good morning" / "see you" / "how are you
  today"): output is a coherent recitation of the curriculum, essentially
  memorized — expected at this scale (15-line corpus, near-zero loss).
  <UNK> appears exactly once at generation start (special-token artifact,
  prompt-priming quirk), not mid-text. Verdict recorded: PASS (proof of
  learning). curricula.pdl updated to status=training + verdict.
- The two bottlenecks are now BOTH proven + fixed:
  (1) seq_len crash — sliding-window fix (UPDATE 2);
  (2) LR 1e-5 too low — store config.txt now lr=0.1.
- Everything above lives on the Linux node (jb@10.0.0.187:~/iqabod-store/).
  This box: curricula.pdl + this report only.

NEXT (supervisor ops build): train_step.c / eval_curriculum.c /
list_curricula.c + queue harness (curriculum-queue.txt, one-line summary,
set-and-forget). Then curriculum g1 iteration → g2.

UPDATE 4 (same day): SUPERVISOR v1 BUILT + VERIFIED END-TO-END
----------------------------------------------------------------
- ops/train_step.c -> ops/+x/train_step.+x   (train one curriculum on a node)
- ops/eval_curriculum.c -> ops/+x/eval_curriculum.+x (held-out probes, <UNK>%)
- ops/list_curricula.c -> ops/+x/list_curricula.+x  (reads curricula.pdl)
- %.harnesses/iqabod-loop/button.sh          (queue loop: once / loop / status /
                                              enqueue / dequeue; report + queue
                                              files live in the harness dir)
- Real run through the harness (conv-g1@linux):
    TRAIN|conv-g1|linux|epochs=100|final_loss=0.0002
    EVAL_SUM|conv-g1|linux|probes=3|ssh_fail=0|max_unk=0
  - <UNK> leaks are GONE across all three held-out probes (was 1 at start
    before the config.txt retrain). All output coherent recitations.
- The full generate->train->eval->report->sleep loop is now set-and-forget:
    PRISC_PROJECT_ROOT=<agent> %.harnesses/iqabod-loop/button.sh loop
  cycles every IQABOD_LOOP_DELAY (default 300s).
- Queue holds conv-g1@linux; curricula.pdl + queue + report are the only
  payloads on THIS box (storage rule intact).

NEXT (Part 4 order): 1) curriculum g2 enqueue + gap-report teacher prompting
(D3), 2) router op (intent/topic -> expert), 3) bridge harness (gemma routes +
iqabod generates), 4) BT decision engine, 5) escalate_to_llama op.
