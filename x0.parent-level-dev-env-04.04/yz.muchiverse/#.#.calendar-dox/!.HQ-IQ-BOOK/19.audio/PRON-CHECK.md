# Pronunciation map: validator and override (slice S2, 2026-10-14)

## 1. What exists
- **Bank:** `&.widgits/concept-bank/data/audio/pron.pdl` (`PRON | word | phonemes | source | status`). **It is empty on purpose**: nobody has listened and said which house words are wrong, and we do not guess. Allowed phoneme characters: `data/audio/phonemes.txt` (Kokoro-82M's own vocabulary, 114 characters, extracted once by a converter run).
- **Validator:** `&.widgits/concept-bank/ops/pron_check.c` -> `ops/+x/pron_check.+x <pron.pdl> <phonemes.txt>`; exit 0 valid, 1 invalid (one line per problem), 2 unreadable. Rules: 5 fields; word a-z 0-9 ' - (lower case, 1..40); phonemes 1..60 characters all in phonemes.txt; source in cmudict/espeak/owner/model/llm; status candidate/active; no two active rows for one word.
- **Override in the voice:** `@.apps/pet-trainer/ops/pet_tts_local.py` (the engine behind every pet voice and voice-fx) now reads the bank: active rows replace the engine's own phonemes for those words (other words in the line are phonemized one by one, and the line goes to Kokoro as phonemes). Flags: `--pron FILE` (another map; also uses its candidate rows: a trial, nothing promoted), `--active-only` (with `--pron`: skip candidates), `--no-pron`, `--phonemes-only --text "..." [--lang en-us]` (print the phonemes the engine would use; no audio). A line with no mapped word takes the exact old path.
- **Fixtures:** `data/audio/pron_fixture_good.pdl` (3 active rows + 1 candidate: brixel, tomom, co-lab; NOT the real bank, the phonemes are arbitrary test values) and `data/audio/pron_bad/*.pdl` (10 bad files).

## 2. Proof (2026-10-14; measured, not heard)
| Check | Result |
|---|---|
| real bank (empty) | `VALID: 0 valid rows` |
| good fixture | `VALID: 4 valid rows` |
| 10 bad files (unknown phoneme, upper-case word, duplicate active, `|` in a value, bad status, bad source, empty phonemes, word with a space, not a PRON row, 61 characters) | all exit 1 with the intended message (10 of 10) |
| missing file | exit 2 |
| override changes the phonemes | "the brixel is here": default `ðə bɹˈɪksəl ɪz hˈɪɹ`, active-only override `ðə bɹˈiːksəl ɪz hˈɪɹ`; "tomom and co-lab say hello": `tˈɑːməm ...kˈoʊlˈæb` -> `tˈoʊmɑːm ...kˈoʊlæb`; unmapped line unchanged |
| trial mode | with `--pron` (candidates on) the later candidate row for brixel wins (`bɹˈɪksɛl`); with `--active-only` it does not |
| audio | line "The brixel is here. Tomom says hello." (Thomas voice): no-pron and empty-real-bank files have the **same sha256** (the old path is untouched); the override file differs (sha256 differs; 2.581 s vs 2.432 s) |
Demo for the owner: `XO/25.games-plan/19-audio/demo_pron_default.wav` and `demo_pron_override.wav` (the override phonemes are test values, not a claim about the right pronunciation).
**Not verified:** how any of this sounds; the round-trip check (speak, transcribe, compare) because no listener is wired; candidate-edit types `pron_add` / `pron_change` and the promote step (roadmap A1, not built); non-English voices (ja/zh use other phoneme sets: `--lang` is passed through but the allowed set is Kokoro's whole vocabulary, not per language); words with digits or apostrophes in the middle of a sentence; the 240-character cut applies before mapping.

## 3. Use it
1. Find what the engine says: `python3 @.apps/pet-trainer/ops/pet_tts_local.py --phonemes-only --text "tomom"`.
2. Listen (voice-fx window or `pet_voice.sh`). If wrong, write a candidate row in a scratch file (`status = candidate`, `source = owner`), run `pron_check`, then hear it with `--pron scratch.pdl`.
3. Promote by a human who is not the proposer: today that means adding the row to `pron.pdl` as `active` and committing it (the candidate-edit tool is not built); keep `pron_check` clean.

## 4. Next
`pron_add`/`pron_change` candidate edits through the same validate/overlay/promote tool pattern as `img_edit`; MISS ledger (words the front end spelled out); LLM draft candidates (DESCRIBE style); round trip with the listener; per-language phoneme sets; import a dictionary (CMUdict licence check).
