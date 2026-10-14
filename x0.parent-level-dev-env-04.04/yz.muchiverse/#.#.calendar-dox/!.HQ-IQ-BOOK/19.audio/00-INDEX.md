# 19.audio - in-house voice, speech-to-text, and sound effects (started 2026-10-14)

Owner's direction (2026-10-14): train our own voice software; a separate SFX generator toy; sound-to-text in the same pipeline for richer stories and narration; one pronunciation map every voice uses; knobs to change pitch and to rename and create voices in a voice-fx studio; all built in-house in C / ops / events / pal (also for fun, learning and productization), with outside tools only as drafters and baselines. Same rails as the picture pipeline (img-hai): word bank and concept bank, a model proposes, real code validates, a different human promotes, two judges, nothing generated at game runtime.

| Doc | What |
|---|---|
| `VOICE-AND-SFX-DESIGN.md` | The whole design: goals, rails, formats, the five tools, how they plug into the DAW, the learning loop, licences, risks. Read first. |
| `PRONUNCIATION-MAP.md` | The one word-to-phonemes file and its overrides; how every voice reads it. |
| `SFX-TOY-DESIGN.md` | The sound-effect generator toy: recipes, tags, a first proof. |
| `LISTENER-L0-L1.md` | Built and measured: speech detection, capture, whisper `hear`, `wer`, synthetic accuracy set (WER 4.4%; 1.1% with a hot-word hint, leakage caveat). |
| `LISTENER-STT-DESIGN.md` | The listener: capture, VAD, STT engines (whisper baseline, in-house spotter), HEAR rows, checks, slices L0-L5. Design only. |
| `HOTSPRINGS-VOICE-MODE-DESIGN.md` | The voice mode (v2 of the 2026-09-28 design): button + Space-to-arm, states, Kokoro out, cursword caveat. Design only. |
| `LEARNING-FROM-HUMAN-SPEECH.md` | What learning from the owner's speech means: five loops, what it will not do, data flow, privacy. Design only. |
| `VOICE-FX.md` | Voice character: formant, EQ, vibrato, breath, accents, the six pet recipes, proof and problems. |
| `PRON-CHECK.md` | Slice S2: the pronunciation bank, validator, voice override, proof, limits. |
| `SFX-RENDERER.md` | Slice S3: the in-house sound renderer, proof, limits. |
| `VOICE-DSP.md` | Slice S1: the in-house pitch/speed op, how it works, proof numbers, limits. |
| `VOICE-ROADMAP.md` | Milestones with proof/KPI, order, and the first three slices. |
| `INVENTORY-AND-PRIOR-ART.md` | What the house and the old projects already have (and do not), what exists online (unverified), licences to check. |

Related today: `@.apps/pet-trainer/VOICE-OFFLINE.md` (pets speak with offline Kokoro), `16.game/IMG-HAI-BREAKTHROUGH.md` (the pattern copied), `17.ai/`.
Status: **design docs plus two scaffold windows** (`44.xyz.01.00/&.hq-apps/voice-fx/`, `&.hq-apps/sfx-hai/`, each with a README handoff); the in-house engines, pronunciation map and listening ops are **not built**. Labels used: *read* = from a file, nothing run; *ran* = run by claude and observed; *online, unverified* = from a web search, check the project page.
