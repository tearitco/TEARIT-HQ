# The pronunciation map (design, 2026-10-14)

One file, `pron.pdl`, says how every word the house knows is pronounced. Every voice engine (Kokoro through its own phonemizer, v0/v1/v2 through ours) reads it, so one fix improves all voices. **Not built.**

## 1. Rows
`PRON | word | phonemes | source | status` (see the design doc section 4). Words are lower case; multi-word entries are allowed (`hai`, `tomom`, `brixel`). `status = active` is used; `candidate` is only used in an overlay trial. Names, invented words and abbreviations (the house has many: `khtpm`, `pdl`, `brixel`, `debil`) are the value of this file; common English can fall back to the engine's own dictionary.

## 2. Sources and the lookup order
1. `PRON` active row (house override) -> 2. CMUdict-style dictionary row if we import one (licence to check) -> 3. espeak-ng's own rules -> 4. unknown: spell it, log a miss.
Misses append `MISS | ts | word | where` to a marker ledger; a daily review file lists them; LLM look-ups may **draft** `PRON` candidates for the misses (DESCRIBE style: the model proposes a spelling-like pronunciation, real code converts it to phonemes and validates it).

## 3. How an engine uses it
- **v0/v1/v2 (ours):** text -> tokens -> map lookup -> phonemes -> synthesis.
- **Kokoro (outside engine):** its phonemizer is espeak-ng. Bridge: replace the word in the input text with a respelling that produces the wanted phonemes, or pass phonemes directly if the API allows (check `kokoro-onnx`; not verified). The test below tells which works.

## 4. The checks (what makes it a bot, not a list)
- **Validator** (`pron_check`, C): word is lower case, phonemes belong to the allowed set, no duplicate active word, no `|` in values.
- **Round trip** (needs the listener): synthesise the word, transcribe with the speech-to-text op, compare; the pass rate per voice is a number in the ledger (`HEAR`).
- **Judges:** two judges on a short sentence set per voice; a pronunciation change needs both yes.
- Promotion by a human who is not the proposer, via the same `edit promote` pattern as `img_edit`.

## 5. Proof for the first slice (S2)
1. Pick a house word the current engine mispronounces (to be found by listening; do not assume which). 2. Record the engine's phonemes for it. 3. Add a `PRON` override. 4. Show the phoneme string changed and the audio differs (hash and a visible waveform/spectrogram). 5. A bad row (unknown phoneme, duplicate) must fail validation. Report what was *heard*, by the owner or claude, separately from what was measured.

## 6. Knobs and renaming
Pitch and speed are voice properties (`VOICE2`), not pronunciation properties; the map says *what* is said, the voice row says *who* says it. Renaming a voice changes `display name` only; the `id` never changes, so old ledger rows and cached wavs stay valid.
