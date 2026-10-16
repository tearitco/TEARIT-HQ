# Learning from the owner's speech (design, draft 1, 2026-10-14)

Question asked: "how does this learn from my human speech?" Honest answer today: **it does not**; there is no listener. This page says what "learning" should mean, what data it needs, what it may and may not do, and how each kind of learning is graded and promoted. Rails as everywhere: a model describes, real code validates, two judges, a different human promotes, nothing automatic; plus the privacy rails of `LISTENER-STT-DESIGN.md` section 2. Not built.

## 1. Five things speech can teach the house (each is a loop with a grader)
| # | Learns | From | Candidate edit | Grader (real code) | Judges / promotion |
|---|---|---|---|---|---|
| 1 | **Pronunciations** (how words are said, how the STT mishears house words) | `HEAR` rows where transcript != `FIXED` text, plus the round trip (speak a word, listen, compare) | `pron_add` / `pron_change` (PRON-CHECK.md) | `pron_check` + round-trip pass rate | owner listens; human promotes |
| 2 | **Hot words / vocabulary** (names like `khtpm`, `tomom`, pet names) | corrections (`FIXED`) | add to the STT hot-word list (a data file) | WER on a held-out set must not rise | owner + claude |
| 3 | **Command phrases** ("up", "enter", presets) | enrolled utterances (the owner says each command several times) | `cmd_add` / `cmd_change` in `commands.pdl` | held-out accuracy and false-accept rate on speech and silence | owner; destructive verbs need a confirm step |
| 4 | **The STT itself** (a better E1/E2 model) | audio + corrected text pairs | a new model file, trained on debil/Mac | WER and command accuracy on a held-out set the model never saw | both judges; promotion by a human other than the trainer |
| 5 | **Voice style targets** (what the owner calls a good pet voice) | judged `SAY` rows (yes/no per recipe) | `voice_knob_delta` / `fx_*` edits (VOICE-FX.md recipes) | knob bounds; audio checks (pitch/duration) | owner's ears; this does **not** use the owner's voice as a model |

## 2. What it will not do (until the owner says otherwise)
- **No voice cloning.** The owner's recordings are for recognition (STT, commands), not for synthesising the owner's voice or anyone else's. Cloning a real person's voice needs their explicit consent and a separate decision (design doc section 9).
- **No learning from anyone else's speech**; only audio the owner armed the mic for, marked `consent=owner`.
- **No automatic promotion.** A model may DESCRIBE ("the engine heard 'tom um', expected 'tomom'"); real code turns that into a bounded candidate row; two judges; a different human promotes.
- **No cloud, no sharing, no push of recordings.** Stored under the owner's user data path, never staged, never pushed (AGENTS.md `xyzfs/users` rules); deletable by one command.
- **No always-on listening.**

## 3. Data flow
```
armed session -> utterance wav -> STT -> HEAR row ----> shown to owner, who may FIX the text (FIXED row)
                                            |                         |
                                            |                         +--> pairs (audio, right text): training/eval sets (split fixed up front: train / dev / held-out; held-out never trained on)
                                            +--> misses (HEAR != FIXED) -> candidate edits (1, 2, 3 above)
 judged SAY rows (voice recipes) -> candidate edits (5)
```
Everything is an append-only row or a file; change detection by marker (size growth), never mtime; a corrected transcript never overwrites the original (`FIXED` is a new row).

## 4. What has to exist first (in order)
1. L0 capture + VAD, L1 whisper wrapper + `HEAR` + `wer` (LISTENER-STT-DESIGN.md section 6).
2. A small **evaluation set of the owner's clips** with right transcripts (suggest 50 utterances: 20 house vocabulary, 20 everyday, 10 commands) split 30/10/10. Without it every accuracy claim is an impression. The owner supplies or records it; the tooling can prompt a script of sentences to read.
3. The correction UI (type the right text; `FIXED` row).
4. Then the loops above, one at a time, each with one accepted and one rejected candidate recorded (roadmap A1).

## 5. Honest limits
Small models will mishear; the held-out numbers decide whether E1/E2 are worth keeping versus the whisper baseline; accents and microphones change everything, so results are per-owner and per-room, not general. A listener trained on one person's voice is a personal tool.

## 6. Open questions for the owner
(1) May your recordings be kept for training, where, and for how long? (2) Will you record the 50-utterance set? (3) Which commands first? (4) English only? (5) Should pets/entities react to your speech (they are Watch records for the attrition model, NIGHT 30) or is speech only for commands and chat?
