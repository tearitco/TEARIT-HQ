# concept-bank-hq - read-only view of the Concept Bank (2026-10-07)

`44.xyz.01.00/&.hq-apps/concept-bank-hq/`, same three-part shape as tomom-hq:
xhtpm + css, ONE `ops/concept_bank_manager.c` `<module>` (polls
`concept_bank_action.txt`, publishes `concept_bank_ui.txt`), thin shim
`ops/concept_bank_item.sh`, `open_concept_bank_hq.sh`, config
`concept_bank_hq_config.pdl` (`bank_dir`, default `&.widgits/concept-bank`).
Taskbar: h-ai cell `ai_menu_9` via `ops/open_concept_bank.sh`.

**Pages:** Masters (own master->master slots + a DERIVED mirror computed by
scanning every spoke slot that points at it; nothing is written, the
`MIRROR_*` lines in master files are ignored), Spokes (slots with a
`[######----] +0.60` bar), Candidates (id/type/target/slot/delta/status per
`data/candidates/*.txt`, plus a `validate` button). Header shows bank dir and
counts. Empty or malformed files/lines are shown and marked, never dropped.

**Read-only:** the manager never opens a bank file for writing. `validate`
runs `ops/+x/concept_edit_validate.+x <bank_dir> <file>` (fork+exec+waitpid,
10 s watchdog, stdout to `state/validate_out.txt`); that binary only
validates. Checked: sha256 of every file under `data/` identical before and
after the session. No promote button. The window shows the four-tier note from
AUTO-PROMOTION-RULE.md as static text, plus: current code (`^.hai-horn
halo_chat_validate`) promotes at tier >= 2 without checking the ledger; the
rule file is a stub.

**v2:** edit via a candidate EDIT line + the validator (never direct writes);
entity word banks (`zz.wordbank`, words/scores/vars); corpus tags;
`pending_review.txt` paths per entity.
