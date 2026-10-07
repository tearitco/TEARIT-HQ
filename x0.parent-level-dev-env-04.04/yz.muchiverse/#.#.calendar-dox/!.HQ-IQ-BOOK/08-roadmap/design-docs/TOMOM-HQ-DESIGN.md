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

**Known limit:** `chatbot_moe_v1` reads `curriculum/<Subject>/<Subject>.txt`
(and `_train/` models), NOT top-level `vocab_model.txt` / `meta_rl_weights.txt`,
so edits here do not change Ask answers yet. Owner decision: also expose the
curriculum vocab files, or point Ask at a binary that uses the top-level files.

**v2 (not built):** bank view/editing (concept-bank spokes,
words.txt/scores.txt/vars.txt); editing bias/embedding fields; live-target switch.
