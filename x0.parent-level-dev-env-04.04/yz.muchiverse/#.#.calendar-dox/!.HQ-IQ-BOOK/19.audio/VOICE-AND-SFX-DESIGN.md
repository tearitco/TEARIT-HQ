# In-house voice, speech-to-text and SFX: the design (draft 1, 2026-10-14)

Design only. Nothing here is built. Read `00-INDEX.md` and `INVENTORY-AND-PRIOR-ART.md` first. Owner direction and rails are in the index. Everything is written in C ops, events and pal (house rule: window logic is C or pal; python only as an off-line stand-in or one-off converter, as with img-hai's draft script), partly for fun and learning, partly so it can become a product.

## 1. Goals (what "done" looks like, in the owner's terms)
1. **Our own voice software**: text to speech that the house can train, tune and rename, without an outside service (edge-tts died; a rate limit threatened the pets).
2. **A voice-fx studio**: an x11-hq window to list voices, change **pitch** (and speed, formant, brightness), **rename** voices, and **make new voices** from an existing one; preview; judge.
3. **A pronunciation bot**: one map of how to say every word the house knows; every voice reads it; a person or model can add a word, nothing changes without the candidate/validate/promote path.
4. **A separate SFX generator toy** for in-game sounds, feeding the DAW.
5. **Sound-to-text in the same pipeline**: speech and effects get words (a transcript, a tag), so stories and narrations can be searched, composed and checked with the same concept banks and LLM look-ups.
6. **Train with attrition**: the same loop as pictures: a drafter proposes, real code validates, two judges (claude, the owner) say yes or no, a different human promotes, every step is a text row.

## 2. Rails (copied from img-hai, proven there on 2026-10-13)
- A model or tool **drafts**; it never sets a number in a bank and never promotes. Real code validates. Proposer is not promoter. No auto-promotion.
- Every output is a file plus a ledger row; **a file is never overwritten** (`_r2`...; name includes the experiment/overlay name).
- Two judges, both must say yes before a candidate is offered for promotion (`JUDGE | label | claude|owner | yes|no | date`, latest wins).
- Nothing is generated at game runtime: sounds and voices are made off-line, cached as files, and played by the game.
- Heavy work (training, long synthesis batches) goes to debil/Mac by ssh, under `nice`; the dev machine only runs small cached jobs (owner's offload rule).
- Free models and free data only; read the licence before any asset ships (INVENTORY section 3).
- Tests drive the real window through `relay_drive` (press by id); a test that cannot fail does not count (keep a negative case).

## 3. The five tools and how they connect
```
 words (word bank, concept bank, LLM look-up)
    |                                   +--------------------+
    v                                   | pronunciation map  |  PRON rows: word -> phonemes
 [1 voice-fx studio] --VOICE2 rows------>  (one file, all voices)
    |  plays                            +---------+----------+
    v                                             |
 [2 voice engine ops]  text -> phonemes -> sound -+--> wav (cached, ledger SAY row)
    ^   V0 espeak-ng front end + our map + our DSP knobs (C)
    |   V1 recorded units (diphones) from our own recordings (C)
    |   V2 small trained model (later; attrition; teacher data licence-checked)
    |
 [3 sfx toy]  recipe rows -> synth op (C) -> wav, tagged -> bank (SFX rows)
    |
 [4 listening ops]  wav -> words:  baseline whisper.cpp; in-house keyword spotter first
    |                                   (same ledger/JUDGE rows; WER is the grader)
    v
 [5 DAW]  `@.apps/media-daw-hq` imports wavs as audio clips (needs the DAW to gain audio clips first)
```
Each tool is a stand-alone op reading files and writing files (house convention: ops, markers, no header-link sharing; shared pure code by text-include).

## 4. Data formats (pipe rows like the rest of the house; `#` comments)
```
voices.pdl   VOICE2 | id | display name | engine | base | pitch_st | speed | formant | bright | note
               engine = kokoro | v0 | v1 | v2        base = a Kokoro voice id, or a v0/v1 voice id
               pitch_st = semitones (-12..+12), speed = 0.5..1.6, formant = -4..+4 semitone-equivalent shift, bright = 0..1
             (the existing VOICE | species | edge voice | rate | pitch and KMAP rows keep working; VOICE2 is the new, richer row)
pron.pdl     PRON | word (lower case) | phonemes (espeak-ng/IPA-like, space separated) | source | status
               source = cmudict | espeak | owner | model | llm ; status = candidate | active
sfx.pdl      SFX | id | tags (comma list) | kind | params (key=value ...)       (the recipe, section 7 of SFX-TOY-DESIGN.md)
ledger       SAY | ts | voice id | text hash | seconds of audio | ms to make | file      (marker/append-only)
             HEAR | ts | file | engine | transcript | seconds | ms to make
verdicts     JUDGE | label | claude|owner | yes|no | date                                  (same rows as img-hai)
candidates   EDIT | id=.. | type=voice_knob_delta|voice_new|pron_add|pron_change|sfx_param_delta|sfx_add | target=.. | ... | reason="..." | proposer=.. | status=candidate
```
Candidate types validate like `img_edit`: bounded deltas (pitch within +-3 semitones per edit), known ids, no `|` in values, `reason` required, unknown phonemes rejected against an allowed set, a new voice needs a base that exists. A negative-test file for every rule (as `candidates/bad/` for images).

## 5. The voice ladder (honest about difficulty)
- **V0 (rules + our knobs):** the front end is espeak-ng (text normalisation, our pronunciation map override, phoneme string). The sound is produced by our own C synthesis (formant synthesis or phoneme-to-wave) or, for the first slice, by espeak-ng's own output. On top, **our DSP**: time-scale and pitch shift (resample + overlap-add, later PSOLA), formant shift, brightness. This alone delivers the **pitch knob, renaming, new voices** the owner asked for, and works for all pets (Kokoro has no pitch). Robotic by design; fits the pets and games.
- **V1 (our recordings):** diphone/unit selection from recordings we make ourselves (the owner's voice, or any voice we have the rights to), cut and indexed by our own C tools. More natural than V0, the data is ours, and building the index is itself a training task with a measurable grader (see section 6).
- **V2 (a small trained model):** only after V0/V1 produce a judged corpus. Teacher data may include Kokoro output (Apache 2.0 reported; confirm) and our judged recordings. The in-house training stack follows `tiny-ai.goal` / the tomom pipeline (hand-built, graded, held-out set before growing), run on debil/Mac. **Not started; no claim it will beat Kokoro.** Kokoro stays the "good voice" baseline until V2 earns a yes from both judges.

## 6. Training and grading with attrition (what is learned, by whom)
| What is tuned | Drafter | Grader (real code or human) |
|---|---|---|
| a voice's knobs (pitch, speed, formant) | LLM proposes a DESCRIBE line "too high / too fast"; fixed table maps it to a bounded delta | validator; claude + owner judge a sample sentence set; promote by a human |
| pronunciations | CMUdict / espeak / an LLM look-up draft `PRON` candidates | **round trip**: synthesise, listen with the speech-to-text op, compare the transcript to the word (a measurable check), plus human judges |
| V1 unit index | alignment ops | boundary-click detector (a number), listening sample |
| SFX parameters | recipe mutator proposes deltas | duration/peak/spectrum checks (numbers), then two judges |
| speech-to-text (in-house) | word-spotter weights | word error rate on a held-out recording set (the owner's clips; see `INVENTORY`) |
Rule from the pipeline note: DESCRIBE, never CLASSIFY; a model never sets a number; a model never both proposes and approves.

## 7. The windows (x11-hq, khtpm renderer; no new shared renderer branches)
- **voice-fx studio** (`&.hq-apps/voice-fx/`): sidebar list of voices and a words field (cli_io); knobs as +/- buttons (pitch, speed, formant, bright) with numeric lines; `Say`, `Rename`, `New voice from this`, verdict buttons, a batch of preview sentences. Same layout rules as img-hai (see its handbook CODE.md section 7: one scroll list per sidebar, canvas overlay rows for tools, cli_io in the bottom strip; no per-app renderer code).
- **sfx toy** (`&.hq-apps/sfx-hai/` working name): recipe list, a tag/words field, `Make`, a waveform canvas, play, verdict buttons, batch.
- Both write ledger rows and use `relay_drive` tests with ids.

## 8. Plugging into the DAW and the games
- `@.apps/media-daw-hq` today draws tracks and MIDI-ish clips only (README: no WAV, no audio clips). **Slice D1 below adds an audio-clip kind** (a clip that points at a wav file; playback by `paplay`/`aplay` from the manager at first). Until then the toys play their own files.
- Games read sound by **name** from a bank (`SFX` ids), not by file path; a bank row can be re-pointed to a newer judged file without touching game code (one authority per fact).
- Narration: a story page (concept bank text) + a voice id -> a cached wav per paragraph (`SAY` rows). The HEAR path can re-check the narration by transcribing it (round trip), which is how "richer stories" get a checkable quality number.

## 9. Licences and rights
Free models and data only. Record the licence in the row (`engine`/`source`). Voice data: only recordings we have the right to use; **do not clone a real person's voice without their consent** (also an OpenRAIL-type restriction for many models). Kokoro (Apache 2.0 reported, verify), espeak-ng (GPL: it is called as a tool or linked in a separate process; do not copy its code into our tree without checking), CMUdict (check terms). Stable Audio Open's licence is disputed in sources: do not use it for shipped assets before reading the model card.

## 10. Risks
- Naturalness: V0 will sound robotic; V1 and V2 are hard and slow; the plan says so and keeps Kokoro as baseline.
- DSP quality: naive pitch shift via resampling changes duration and formants; needs proper overlap-add/PSOLA (slice S1 measures this).
- CPU: dev machine is weak; synthesis and training go to debil/Mac.
- Whisper/Vosk are outside code; the in-house listener starts as a small command spotter, not general dictation.
- Scope: five tools; do them in the roadmap order, each with a proof.
- Pronunciation bot cannot know every word: unknown words fall back to the front end's rules, and a miss becomes a `PRON` candidate.

## 11. Open questions for the owner
1. Whose recordings may V1 use (your own voice? any rights-cleared set)?
2. Is a robotic V0 voice acceptable as "the house voice" for pets while V1/V2 mature?
3. Which words first for the pronunciation bot: all house vocabulary (concept-bank words), or only names and invented words?
4. Voice-fx studio first, or the SFX toy first (my recommendation: SFX toy first, because it has no model risk and unblocks in-game sound)?
5. Should the DAW grow audio clips now (D1) or after the toys prove themselves?
