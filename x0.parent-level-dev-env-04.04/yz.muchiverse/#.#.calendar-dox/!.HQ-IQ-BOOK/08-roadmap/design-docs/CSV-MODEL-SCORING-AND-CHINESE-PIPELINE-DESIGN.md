# CSV model scoring and the Chinese (+pinyin) pipeline - design and status (2026-10-07)

A reusable CSV-in / CSV-out pipeline: take the translatable cells of any bank (a CSV, or the house `KEY | value | ...` pdl rows), have one model translate them in token-budgeted batches, run deterministic checks, have a second model score what passed, and write the result to a sidecar file. First use: Simplified Chinese (zh-Hans) plus Hanyu Pinyin for the concept banks. Everything model-made is advisory: low scores go to a review queue and a person decides.

Code: `&.widgits/concept-bank/ops/csv_lab.c` (one self-contained C file, many verbs; build with `ops/build_csv_lab.sh`). Manifest: `&.widgits/concept-bank/banks.pdl`. Harnesses: `&.widgits/_shared-lib/harness/csv_{jobs,batch,lint,call,merge,pipeline}.pal` + `cases/`. Feed and review files: `&.widgits/concept-bank/state/`.

## 1. Hard rules (house)

- Free models only. Translator: Groq `openai/gpt-oss-120b` through the existing transport `^.hai-horn/ops/+x/horn_chat_backend.+x` (key file `&.widgits/open-hai/state/raw_groq.txt`, read only by that backend; csv_lab never reads, prints or logs it). Judge: the Mac Ollama `qwen2.5-coder:7b` (a different provider AND model). Judge alternative that also scored well: `llama3-groq-tool-use:8b`. `gemma3:1b` translates poorly; do not use it as translator.
- All Ollama calls go to the Mac URL read from `#.desktop/ai_backend.pdl` key `gemma_lan_url` (10.0.0.144:11434). The wrapper REFUSES `localhost`/`127.*` on port 11434 and any host other than the one in that file. Loopback on other ports is accepted for test doubles only.
- Source banks are never written. Results go to sidecars (`*.zh.tsv`). Formulas, symbols, ids, numbers and emoji are never translated: the manifest names the text columns, and a column listed in both `text=` and `never=` is a hard error (exit 7).
- Ops are compiled C, paths from `realpath(argv[0])` or the environment (`CSV_LAB_HOUSE`), no hardcoded absolute path, no header+link split.
- Harnesses are pal (`harness_case_op` + `harness_verdict_op`), never `.sh`, and each one has mutants that must be detected.

## 2. The manifest (`banks.pdl`)

One row per bank: `BANK | name | file= | format=pdl|csv | rowtag= | key= | text= | context= | never= | add=zh,pinyin | sidecar=`.

- `file` may contain `*`: several files form one bank and job ids carry the file name (`g1.pdl:L1`).
- pdl rows: field 0 is the tag, fields are numbered from 1 and a field written `name=value` can be addressed by `name`. csv: header names, or 0-based numbers. `key=line` means the ordinal of the data row (counting every non-comment row, so it is stable under tag filtering).
- `context` columns are sent only as a sense hint (`formula=CH3COOH`) and the prompt tells the model not to translate or copy them.
- A text cell with no letters (a number, a symbol) is skipped.

| bank | file | text | never | notes |
|---|---|---|---|---|
| concept_masters / concept_spokes | `concept-bank/data/{masters,spokes}/*.pdl` | `NODE` name (col 1) | - | 3 + 5 concept names |
| harness_behavior | `_shared-lib/harness/bank/*.behavior.pdl` | SYNONYM word, SENTENCE prose, KEYWORDS list (col 1) | weights (col 2); BEHAVIOR/SLOT/CORPUS/SEQUENCE/SOURCE/COUNTS/WEIGHT rows untouched | 84 cells (before the six csv_* seeds were added) |
| chem_elements | `data/chem/elements.pdl` | name (col 3) | Z, symbol, protons, neutrons, mass number | symbol as context; locked file, hash-protected |
| chem_compound_names / chem_compound_hints | `data/chem/compounds_*.pdl` | `name` / `hint` | emoji, formula, formula_ok, atoms, mass_number, elements, kind, state, mp, bp, density | formula as context; 86 rows |
| chem_phrases | `data/chem/chem_phrases.pdl` | PHRASE (col 1) | - | formulas inside the text must come back verbatim (lint) |
| eden_phrases | `eden/conductor/phrases.pdl` | PHRASE (col 1) | - | 46 rows |
| skillbook_skills | `^.hai-horn/skillbook.pdl` | SKILL name | tier, level, mp, exp | identifiers like `propose_edit` |

All paths verified to exist before the manifest was written. To add a plain CSV, add one `format=csv` row (section 9).

## 3. Stages (`csv_lab pipeline`, the driver)

`extract -> batch -> translate -> lint -> repair (failed rows only) -> score -> merge`. One command:

```
csv_lab pipeline banks.pdl <bank> --work DIR --answers data/zh/answers.tsv --quota-ledger state/csv_lab/groq_quota.txt \
        [--feedback-dir quest-pilot/delegation-bank/workers/groq-gpt-oss-120b] [--ref shared/CHINESE-VOCAB-SOURCE-LOCATION.pdl] \
        [--rows 25] [--tokens 900] [--retries 2] [--threshold 4] [--no-judge] [--out sidecar] [--ui FILE] [--review FILE]
```

Environment: `CSV_LAB_HOUSE` (house root, else found by walking up from the binary to the folder holding `xyzfs`), `CSV_LAB_KEY_DIR` (folder holding `raw_groq.txt`), `CSV_LAB_GROQ_BACKEND` / `CSV_LAB_MAC_URL` (test doubles only).

Every stage is also a verb, so a stage can be run or tested alone:

| verb | what it does |
|---|---|
| `jobs` | read the manifest row, collect cells, write `all.tsv` (every job), `jobs.tsv` (untranslated) and `hits.tsv` (answered by the answer bank) |
| `batch` | split `jobs.tsv` into `b001.tsv...` capped by rows and an estimated token budget (chars/4, CJK a little more); rows are renumbered 1..n per batch |
| `prompt`, `parse` | build the translate or score prompt; parse the reply tolerantly (code fences, tab / ` | ` / double-space separators, `N.` prefixes, duplicates keep the first, unknown numbers counted) back to job ids |
| `lint` | deterministic checks (section 4); with `--expect jobs.tsv` also the row-numbering check |
| `call groq|mac` | model call wrapper (section 6) |
| `quota` | print used/limit and the wait needed from the ledger (section 7) |
| `merge`, `ui` | sidecar, review queue, answer bank, FEEDBACK rows, live feed (sections 5, 8) |

Repair: after lint, only rows that failed or were never answered go into the next round, with the reason appended to their context (`FIX: no-tone-marks`); 2 retries by default. A row still failing after the last round ends as `lint_fail` and is never scored. Each round is its own folder (`batches/r0`, `r1`, ...), so a person can read exactly what was sent. If the translator reports a rate limit (exit 3) the run stops early, announces it, and merges what exists; a later run resumes because the answer bank skips finished rows.

## 4. Deterministic lint (never a model)

Per row (`id, zh, pinyin, source`), every failing check is listed:

- `empty-zh`, `no-cjk`, `kana-or-hangul`, `traditional-char:X` (a sample list of common traditional-only forms), `latin-in-zh:X` (a Latin run that is not in the source: formulas and names from the source are allowed and must be kept).
- `length:Ncjk-for-Mletters`: Chinese characters must be between letters/14 and letters+4.
- Pinyin: `empty-pinyin`, `tone-number:` (`shui3`), `letter-v-instead-of-u-umlaut`, `bad-char-in-pinyin`, `no-vowel:`/`bad-initial:`/`bad-syllable:` (initial + final must be a legal combination; coarse table, not a dictionary), `two-marks:`, `tone-placement:` (a before o before e, `iu`/`ui` on the second vowel), `no-tone-marks`, `too-few-tone-marks:m/n` (more than one unmarked syllable in three), and `syllable-count:cjk=N,pinyin=M` (erhua `儿` may merge into the previous syllable).
- Row numbering (`--expect jobs.tsv`): `missing-row`, `duplicate-row`, `unexpected-row`.
- Optional advisory cross-check against the reference vocabulary: warnings only, never failures. `ref-gloss:study=学习` when the whole English source equals a reference gloss but the translation lacks that headword; `ref-pinyin:学习` when a multi-character reference headword appears but its pinyin (spaces stripped, tones kept) is not found. Known noise: a neutral tone written without the reference's mark (`xǐ huan` vs `xǐ huān`) warns.
- The reference file `1k.freak.chinese-core-vocab.txt` (Cambridge Pre-U 9778 core list, about 845 distinct headwords; hanzi, pinyin, English gloss separated by 2+ spaces) is exam-board material and stays outside the repo. It is reached through the pointer `shared/CHINESE-VOCAB-SOURCE-LOCATION.pdl` (`SOURCE | vocab_file | <path>`). If the pointer or the file is missing, lint prints `ref=skipped (...)` and every other check still runs. Nothing derived is stored under `#.NNEST_ASSETS`: the file is read each run (milliseconds).

## 5. Scoring, statuses and the review flow

The judge sees only lint-clean rows (`N<TAB>English<TAB>Chinese<TAB>pinyin`, 20 per call) and answers `N<TAB>score 1-5<TAB>reason`. Rows whose score is missing get one retry call, then stay unscored.

Status vocabulary (sidecar, answer bank, feed): `new` (not yet processed) | `translated` (lint-clean, unscored) | `lint_fail` | `low_score` (score below `--threshold`, default 4) | `ok` (score at or above it) | `accepted` / `rejected` (owner decision).

- `ok` and `accepted` are promotable. `low_score`, `translated` and `lint_fail` wait in `review.tsv` of the work folder (id, source, zh, pinyin, score, reason). `rejected` stays out of anything promoted and is retranslated on the next run.
- Owner decisions are read from `state/csv_lab_review.txt` (override `--review` / `CSV_LAB_REVIEW`): append-only rows `REVIEW|<bank>|<id>|accept` or `REVIEW|<bank>|<id>|reject`, last row for a job wins, rows for other banks are ignored. `merge` applies them: the sidecar status becomes `accepted`/`rejected`, the row leaves the review queue, the answer-bank row's status is rewritten (`accepted` stays a hit; `rejected` is no longer a hit, so the next run retranslates it), and the feed shows the new status. Nothing but a person writes that file.
- Model scores are advisory: a score never deletes, overwrites or promotes beyond `ok`.

Sidecar `<stem>.zh.tsv` (or the manifest's `sidecar=`): a comment line, then `id, source, zh, pinyin, score, status, translator, judge` per job (including answer-bank hits). Re-running regenerates it from the work folder and the answer bank.

## 6. Model calls

- `call groq`: fork + exec of the house transport with `HORN_PIN_PROVIDER=groq`, `HORN_PIN_MODEL=openai/gpt-oss-120b`, `HORN_TOOLS=off`, its own scratch folder, and a watchdog (`--timeout` + 5 s, then SIGKILL of the process group). Exit codes: 0 ok, 3 rate/quota limit, 4 failed, 5 empty reply, 6 timeout. Usage comes from the raw response (`prompt_tokens`, `completion_tokens`); if absent it is estimated and marked `est=1`. Reasoning tokens of gpt-oss are included, so a 25-row batch costs about 4,000 tokens, not 1,000.
- `call mac`: plain HTTP/1.1 `POST /api/chat` (`stream:false`, `temperature:0`) over a socket with a 10 s connect timeout, a total deadline checked every second, no popen, no read to EOF without a deadline. Counts come from `prompt_eval_count`/`eval_count`.
- The key and the Authorization header never reach csv_lab output, ledgers or feed (the harness runs a backend double that prints the key to its own stderr and asserts it appears nowhere).

## 7. Quota rules

Groq free limits: 1,000 requests/day, 8,000 tokens/minute per model. `state/csv_lab/groq_quota.txt` is an append-only ledger of `REQ|epoch|provider|model|tok_in|tok_out|est|rc` (failed calls count). Before each Groq call: refuse if today's requests reached the limit (exit 3, backend not started); otherwise wait (2 s polls) until the tokens used in the last 60 s plus an estimate for this call (prompt + 2,500) fit under 7,000. `csv_lab quota <ledger> <now> <est>` prints the same arithmetic. Batches are 20-30 rows, answered rows are skipped via the answer bank, and the driver never loops on a rate-limit error. The answer bank (`data/zh/answers.tsv`, one row per `sha256(source text, 0x1f, language)`) is what makes re-runs cheap; only lint-clean rows enter it.

## 8. The live feed for the window

`state/csv_lab_ui.txt` (override `--ui` / `CSV_LAB_UI`), rewritten atomically (temp file + rename) at every stage change and after every batch, so a reader never sees a half file. Plain text, `|` separated, no `|` inside values (replaced by `/`):

```
HEAD|<run_id>|<bank>|<stage>|<done>|<total>     stage = extract|batch|translate|lint|score|merge|done|error
QUOTA|groq_req|<used today>|1000
QUOTA|groq_tok_min|<tokens in the last 60 s>|7000
QUOTA|mac_calls|<today>|0
JOB|<bank>|<id>|<en>|<zh>|<pinyin>|<score 1-5 or ->|<status>      one row per job of the current run
LOG|<HH:MM:SS>|<one short line>                   the last 40 only
```

`done/total` count rows (extract, lint, merge, done) or batches (batch, translate, score). `error` means a model call failed or the translator hit its limit; the run still merges what it has. JOB status is derived from the files of the work folder at every refresh, so rows turn `new -> translated -> lint_fail|ok|low_score` while the run is going, and the owner's `accepted`/`rejected` decisions are applied on top. The LOG ring is the file `csv_lab_ui.txt.log` (a new run starts a new ring). The older per-folder `csv_lab_ui.txt` of the first draft is gone.

## 9. Running it on a new CSV

1. Add a row to `banks.pdl`: `BANK | words | file=path/to/words.csv | format=csv | key=id | text=english | context=note | never=id,symbol | add=zh,pinyin`.
2. `csv_lab pipeline banks.pdl words --work state/csv_lab/words --answers data/zh/answers.tsv --quota-ledger state/csv_lab/groq_quota.txt --out path/to/words.zh.tsv`.
3. Read `review.tsv`; append `REVIEW|words|<id>|accept` or `reject` rows to `state/csv_lab_review.txt`; run `csv_lab merge ...` (or the pipeline again, which is free for answered rows) to apply them.

Another language or model pair: `--lang xx` changes the answer-bank key; the prompts and the pinyin lint are Chinese-specific, so a new language needs its own prompt text and lint rows (the verbs `jobs`, `batch`, `parse`, `call`, `quota`, `merge`, `ui` are language-neutral).

## 10. Verification (all fresh, 2026-10-07)

Six locked pal harnesses, run with a scratch prisc, every result in `harness/results/*.verdict.txt`:

| harness | checks | mutants (a compiled copy with one defect must behave differently) |
|---|---|---|
| csv_jobs | 28 | rejected answers counted as hits; language dropped from the key |
| csv_batch | 33 | row cap ignored; duplicates accepted |
| csv_lint | 50 | no-tone-marks, syllable-count and tone-placement checks removed |
| csv_call | 43 | the port-11434 rule moved to another port (stub port then refused) |
| csv_merge | 66 | threshold off by one; lint failure ignored; FEEDBACK valence flipped |
| csv_pipeline | 58 | repair resends everything; judge sees lint failures; reference table not reset per lint run |

Models are replaced by doubles (`harness/fixtures/csv_lab/stub_groq.c` for the transport, `stub_mac.c` as a loopback HTTP server on ports 184xx); no harness contacts a real model. `harness_case_op` passes at most 15 arguments to a RUN, so the pipeline case supplies house root, key dir and feed path by `ENV`.

Real runs (the only two, free models; sidecars committed, work folders are runtime state and git-ignored):

| run | rows | translated | lint-failed (final) | low-scored (review) | Groq requests | Groq tokens | Mac calls | Mac tokens |
|---|---|---|---|---|---|---|---|---|
| `eden_phrases` (`eden/conductor/phrases.pdl` -> `phrases.zh.tsv`) | 46 | 46 | 0 | 0 | 2 | 7,876 (in 1,120 / out 6,756) | 3 | 3,053 |
| `chem_compound_names` (`data/chem/compounds_*.pdl` -> `compound_names.zh.tsv`) | 86 | 86 | 0 (1 failed the first pass, fixed by one repair round) | 0 | 5 | 11,805 (in 2,603 / out 9,202) | 5 | 2,948 |

Also spent: 5 Groq requests (about 11,300 tokens) by a first chem run that crashed in the second lint round (a bug: the reference vocabulary table was appended to instead of reset on every lint run; fixed, and the pipeline harness now has a case and a mutant for it). Groq requests used today in total: 12 of 1,000; Mac calls: 8. The answer bank (`data/zh/answers.tsv`) holds all 132 lint-clean rows, so re-running either bank costs nothing.

Score distribution: every one of the 132 rows got a 5 from the Mac judge. That is the honest weak point (section 11).

Self-audit by reading (not model output): rows that passed lint AND the judge but are wrong or doubtful, listed so a person can reject them in `state/csv_lab_review.txt`:

- chem `Glycine` (first batch) = `甘氨酸 gān àn suān`: 氨 is `ān`, not `àn` (the later amino-acid batch wrote it right: `gān ān suān`). Same word, two spellings: an inconsistency the answer bank cannot see because the source text of the two rows differs in context only.
- chem `Pyridine` = `吡啶 pī dīng`: 吡 is `bǐ`.
- Eden `Set the hoe aside after you finish the row` = `... 锄头 ... chu tóu`: should be `chú tou`; the unmarked `chu` slipped through because lint tolerates one unmarked syllable in three.
- Eden `...hú luó bǔ` (胡萝卜): `bo` neutral is the usual reading; and `fructose-1` / `Fructose-6-phosphate` are the sources' own odd truncations (mcat17 reads `Fructose-1`), so the translation mirrors a source problem.
- Reference cross-check warnings (advisory): `尽快` and `喜欢` x2 (the model wrote `xǐ huān`, the list has the neutral second syllable); no hard disagreement with the reference list.

The other rows read as correct to the author (common chemical names, standard Mainland terms; amino acids and sugars use the standard `-氨酸` / `-糖` forms).


## 11. Honest limits

- The judge is lenient and can disagree with a person. On Eden phrases `qwen2.5-coder:7b` gave all 46 rows a 5 with Chinese-language reasons; a 7B coder model is not a native speaker. Treat `ok` as "passed machine checks and a weak second opinion", not as "correct". The review queue only fills when the judge or the lint objects; spot-check some `ok` rows.
- Lint proves form, not meaning: pinyin is checked for shape, tone marks, placement and count, not against a dictionary (a legal-looking wrong reading, or a wrong character with matching pinyin, passes). The reference cross-check is only advisory and covers ~845 headwords; polyphones make single-character checks useless, so only multi-character headwords are compared.
- Simplified vs traditional is a small sample list, not a conversion table. Mainland word choice is requested in the prompt, not verified.
- `gemma3:1b` translates poorly and was not used; judges disagree with each other (qwen2.5-coder and llama3-groq both scored well in earlier trials at ~8 s per batch, but they were not run against each other here).
- Token usage for Groq includes hidden reasoning; the per-minute pacing uses the previous calls' real counts and an estimate for the next, so a very large batch can still hit the 8,000 limit once (the driver then stops and resumes later).
- One sidecar format, one answer bank for all banks (keyed by text, so two banks with the same English word share one translation: good for consistency, wrong if the sense differs; use `context` columns and `--lang` variants to separate them).
- Not done here: the other seven banks (concept, behavior, elements, hints, chem phrases, skillbook) were extracted and counted but not translated; no promotion of any translation into live data; no X11 window (the feed is ready for it).
