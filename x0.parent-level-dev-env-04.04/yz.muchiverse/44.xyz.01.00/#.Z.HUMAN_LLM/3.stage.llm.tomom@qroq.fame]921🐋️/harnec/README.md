# Hand-Tuning Harness

## Overview
Uses llama3 via Ollama to suggest weight edits for tomom vocab files, with auto-revert safety.

## Workflow
1. Train baseline: `./+x/trainer.+x curriculum/<Subject>/<Subject>.txt`
2. Ask LLM for edits: `python3 harnec/tune.py <vocab> <prompt> <behavior> <edits_file>`
3. Apply edits: `bash harnec/tune_weights.sh <Subject> <baseline_loss>`
4. Auto-revert: `bash harnec/apply_or_revert.sh <Subject> <baseline_loss>`

## Weight-Only Editing Policy
- Edit ONLY `weight` field (5th column in vocab_model.txt) — NOT embedding/pe/bias1-4
- The embedding field determines the model's initial representation; clustering it hurts
  training convergence. The `weight` field is a secondary input feature that can be
  safely boosted for specific words without disrupting the trained matrices.
- After editing, train 10 epochs (fine-tuning) from the trained baseline, not from scratch.
- Auto-revert if new loss >= baseline loss.

## Files
- `tune.py` — LLM query + edit parser for vocab weight fields
- `tune_weights.sh` — backup, call tune.py, apply edits in-place
- `tune_output.py` — boost output_layer columns for specific words (post-training)
- `validate_weights.sh` — validate vocab structure (9 fields, numeric range)
- `apply_or_revert.sh` — train, compare loss, auto-revert if worse
- `*.bak` — backups of original/trained/tuned states
- `*.results.txt` — loss comparison logs
- `*.edits.txt` — LLM-suggested edits (siloed for rollback)
