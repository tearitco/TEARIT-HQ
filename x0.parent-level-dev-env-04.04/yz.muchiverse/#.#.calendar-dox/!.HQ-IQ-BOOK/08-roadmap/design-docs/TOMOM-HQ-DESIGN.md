# tomom-hq - native X11-HQ editor for tomom's text weights (2026-10-07)

**What:** `44.xyz.01.00/&.hq-apps/tomom-hq/` - view and hand-edit the plain-text
weights of tomom (`#.Z.HUMAN_LLM/3.stage.llm.tomom.../`): `vocab_model.txt`
(`number word embedding pe weight bias1..4`, 1362 rows) and
`meta_rl_weights.txt` (55 `key value` rows). Standard three-part shape:
`tomom-hq.xhtpm` + css (layout only, shared renderer, zero renderer C),
ONE compiled `ops/tomom_manager.c` `<module>` (polls `tomom_action.txt`,
publishes `tomom_ui.txt`; the text bars `[#####-----]` are composed there),
thin shims `ops/tomom_{item,filter,ask}.sh`, launcher `open_tomom_hq.sh`.
Taskbar: HQ menu row 14 and ai (h-ai) cell row 8, both via
`_.monads/_.livedesk-taskbar/ops/open_tomom.sh`.

**Target safety:** edits go to the dir in `tomom_hq_config.pdl`
(`target_dir`, default `state/work/tomom`, a scratch copy made by the INIT
action from the live vocab/meta/config; INIT refuses outside `state/work/`).
Live tomom files are only read. `state/work/orig/` is the snapshot for "reset
to original". The window header shows the target. Pointing at the live
tomom is a deliberate later switch (not built). All weight writes are atomic
(temp + rename).

**Audit (append-only `state/edits.txt`):**
`EDIT | id | file | key | old | new | proposer=human | ts [| undo_of=<id>]`.
Vocab key = `<number>:<word>.weight`. `undo last` appends the inverse row
with `undo_of=` (stack rebuilt from the file at start). Reset is a normal
undoable edit. Ask rows go to `state/ask_log.txt`.

**v1 scope:** Meta-RL page (list, select, `-`/`+` step 0.05, reset, undo);
Vocab page (cli_io filter, 150-row pages via prev/next, edit `weight` only,
other fields shown); Ask tab (builds `chatbot_moe_v1` into the scratch dir,
fork+exec, 60 s watchdog, last `Response:` line). One generic scrolllist
serves every tab (clip, not translate).

**What chatbot_moe_v1 really reads (from chatbot_moe_v1.c):** the top-level
`vocab_model.txt` and `meta_rl_weights.txt` are NOT read (meta_rl is not used
by Ask at all; subjects are simply every line of `curriculum_bank.txt`).
1. `curriculum_bank.txt`: one `curriculum/<S>/<S>.txt` per line (max 10) = the MoE experts.
2. `curriculum/<S>/<S>.txt`: that subject's vocab (`number word embedding pe weight bias1-4`), ~60 words; words of all subjects are concatenated into the merged vocab.
3. Prompt: last prompt word found in the merged vocab (first match wins) is the start; unknown => `start-token`.
4. The subject that owns the current word's merged index is the expert; only its vocab and model score the next word.
5. `curriculum/<S>_train/attention_model.txt`: 3 x (7x7) W_q, W_k, W_v (147 floats) turn the current word's 7 numbers into q/k/v.
6. `mlp_model.txt`: 7x16 weights + 16 biases (128 floats), ReLU hidden layer on the attention context.
7. `output_layer.txt`: 16 x vocab weights + vocab biases (1020 floats for 60 words); raw output = next-word scores.
8. Next word = uniform random pick among the top-N scores (N=10; 5 if temperature < 0.5, 20 if > 2.0; Ask uses 0.5 so N=10); temperature only changes the softmax, not the ranking. `end-token` or length ends it.
9. `srand(time(NULL))` => no fixed seed; the editor builds `chatbot_seed7` (the same source with that one call replaced by `srand(7)`, in the scratch dir) so one prompt gives one answer.
10. `*_train/{m,v}.txt`, grads, loss, optimizer_state are training-only, never read by the chatbot.

**Subject tab** edits 5-7 (step 0.05 attention, 0.01 mlp, 0.05 output_layer; chosen from the value ranges +-0.6 / up to 0.2 / +-0.8). Audit file column = path relative to the scratch copy, key = `<flat index>:<label>` (e.g. `962:bias[gazes]`). The scratch orig snapshot is `state/work/orig/curriculum`. An edit splices exactly one token in the file.

**Proof (fixed seed 7, prompt `start-token`, 12 tokens):** original => `shining cosmos millions the`; after +0.05 on Astronomy `bias[gazes]` (E13) => `holes nebulas traps billions gazes cosmos cosmos gazes holes gazes cosmos gazes`; after undo (E14) => original again, file identical to orig. Evidence: `state/evidence/edits.txt`, `state/ask_log.txt`.

**v2 (not built):** editing the subject vocab `<S>.txt` (embedding/pe/weight/bias fields);
bank view/editing (concept-bank spokes,
words.txt/scores.txt/vars.txt); editing bias/embedding fields; live-target switch.
