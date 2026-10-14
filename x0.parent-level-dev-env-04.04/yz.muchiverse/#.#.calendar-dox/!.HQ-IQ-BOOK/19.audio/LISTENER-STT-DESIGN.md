# The listener: speech to text in the house (design, draft 1, 2026-10-14)

Design only. Nothing here is built. Context: `VOICE-AND-SFX-DESIGN.md` (rails, ladder), `INVENTORY-AND-PRIOR-ART.md` (what exists), `LEARNING-FROM-HUMAN-SPEECH.md` (what is learned from it), `HOTSPRINGS-VOICE-MODE-DESIGN.md` (the first consumer). Owner direction (2026-10-14): train our own, no reliance on outside services, C / ops / events / pal, attrition with the same rails as pictures; speech-to-text and sound-to-text in the same pipeline.

## 1. Where we really are
| Thing | State |
|---|---|
| a working STT outside the house | **yes, on the external drive** (owner's 2025 projects): whisper.cpp + `ggml-tiny.en.bin` (about 75 MB; owner's note: exact transcript of a 3 s test clip; 10 s runs work; **chunked streaming had a bug**: "! works, ? is buggy"), Vosk with three English models (owner's note: the 111 MB model matched the 2 GB one). A C orchestrator and ALSA capture code exist there (`whisper-stt_v0.4-er`: `audio_capture.c`, `chunked_audio_capture.c`, `orchestrator*.c`) and two plans (`architecture_plan.txt`, `future_roadmap_v0.txt`). The whisper.cpp tree on the drive has `build/bin/{main,bench,quantize}`. Read only; **none of it was compiled or run by claude** |
| STT inside the house | **none**: no op, no window, no ledger row |
| capture tools on this machine | `arecord`, `parecord`, `ffmpeg`, python `PyAudio` present; no whisper or vosk package installed |
| the house's own STT choice | open ("STT engine: not yet chosen", AI-PUSH-ROADMAP-AND-NUANCES.md, hotsprings addendum) |

## 2. Rails (same as img-hai and voice)
Real code validates; a model never sets a number; proposer is not promoter; every output is a file plus a ledger row; never overwrite (`_r2`); heavy work on debil/Mac (the tiny model is light enough for the dev machine, measure first); free software only, licence read first (**whisper.cpp and Vosk licences are from memory, not checked: verify**); tests drive the real window through `relay_drive`.
**Privacy rails (new, because a microphone is involved):** nothing listens unless a person armed it (a button or Space, see Hotsprings); a visible "listening" indicator whenever the mic is open; recordings and transcripts are the owner's private data: stored under the owner's user data path (the `xyzfs/users` rules in AGENTS.md: never staged, never in a code branch, never pushed), never in a tracked folder; one command deletes a day's recordings; no cloud STT.

## 3. Architecture (each box is a small op: reads files, writes files)
```
 arm (button / Space) -> [L0 capture op]  parecord/arecord -> 16 kHz mono wav chunks (marker file)
                              |
                         [VAD op]  energy + hangover (C, uses audio_dsp.c): start of speech, end of speech (a real pause, not a timer)
                              |   utterance wav
                         [STT engine op]  E0 whisper.cpp binary (baseline)  |  E1 in-house spotter (commands)  |  E2 in-house small model (later)
                              |   transcript
                         HEAR row (ledger)  ->  consumers: voice-fx "listen" button, hotsprings chat, command router, learning (LEARNING-FROM-HUMAN-SPEECH.md)
```
- **Deterministic test mode:** every op can read a wav file instead of the mic (`listen_file <wav>`), so relay tests and graders run without a person speaking.
- **VAD:** frame energy in dBFS over 20 ms, adaptive noise floor, start after 100 ms above floor+9 dB, end after a configurable hangover (default 700 ms of quiet). The hangover is the "real, sustained pause" the hotsprings design asks for, one implementation for everyone (the design doc already says: do not build a second pause detector).
- **E0 baseline:** whisper.cpp `main`/`whisper-cli` on `ggml-tiny.en.bin`, run as a separate process (fork + exec + watchdog, as everywhere), wav converted with `ffmpeg -ar 16000 -ac 1 -c:a pcm_s16le`. Source of the binary: build from the drive's tree or from upstream on debil; do not rely on the drive being mounted: copy the model and tree into a house path outside git (the model is large) and record sha256.
- **E1 command spotter:** the owner's first goal is voice-driven control with a small vocabulary ("up", "enter", "control c", "listen", preset commands). A small-vocabulary recogniser is trainable in house: MFCC features (add mel filterbank + DCT to `audio_dsp.c`; the FFT is there), dynamic time warping against enrolled templates, then a tiny network trained on debil/Mac. Grader: accuracy on held-out utterances, **false-accept rate on non-command speech and silence** (a spotter that fires on noise is worse than none).
- **E2 small model:** only after E0/E1 produce a judged corpus. Training stack per `tiny-ai.goal` / the tomom pattern; not promised.
- **Sounds, not speech:** non-speech input (sound effects, room noise) gets a **spectral fingerprint** (duration, loudness envelope, dominant frequencies from `audio_dsp.c`) and a tag, written as `HEAR` rows with `engine=fingerprint`; a model only describes (DESCRIBE), never decides.

## 4. Data formats (pipe rows)
```
HEAR   | ts | utterance file | engine | transcript | seconds of audio | ms to make | confidence or - | consent
LISTEN | ts | arm source (button|space|file) | seconds open | utterances | note              (one row per armed session; the audit trail of when the mic was open)
FIXED  | ts | HEAR ts | corrected text | by (owner|claude)                                   (a human correction: audio + right text = training pair)
verdicts  JUDGE | label | claude|owner | yes|no | date                                        (same rows as img-hai; used for spotter accept lists)
commands.pdl  CMD | phrase | action (a verb in an existing request protocol) | arm-required | note     (the command bank; edited by candidate edits)
```
`consent` is `owner` for the owner's own speech; recordings of anyone else are not kept.

## 5. Checks (numbers, not impressions)
- **WER** (word error rate) of E0 on a held-out set of the owner's clips against their `FIXED` transcripts; the same for E1/E2 on the command set. A fixed script (C op `wer`) computes it: edit distance over words, deterministic, with negative tests (identical text 0.0, empty hypothesis 1.0).
- **VAD:** on synthetic files with known speech spans (tones or recorded speech with silence gaps), start/end within 100 ms; **silence-only and noise-only files produce no utterance** (negative).
- **Latency:** time from end of speech to transcript, per engine, measured and recorded in `HEAR` (`ms to make`).
- **Safety:** commands from a spotter only run when armed; a command row says whether it needs confirmation; destructive verbs are never bound to a voice command without an explicit confirm step.

## 6. Slices (each ends with a visible/measurable proof; do in order)
| # | Slice | Proof |
|---|---|---|
| L0 | `listen` capture + VAD ops, file mode | VAD finds the spans of a test file within 100 ms; silence gives none; a `LISTEN` row per session; screenshot of a window showing "listening" |
| L1 | E0 wrapper op + `HEAR` rows + `wer` op | the owner's clips (to be supplied) transcribed; WER number recorded; latency recorded; whisper chunk bug reproduced or shown fixed |
| L2 | voice-fx "listen" button (and `FIXED` correction by typing) | relay case drives a file-mode listen; transcript appears; a correction is stored |
| L3 | E1 command spotter | held-out accuracy and false-accept numbers vs E0 on the same clips |
| L4 | round trip R1 (speak -> listen -> compare) | pass rate per voice in `HEAR` rows (feeds the pronunciation map) |
| L5 | learning loop (A1) | one candidate (a `PRON` row or a command) accepted and one rejected, both recorded |

## 7. Risks and open questions
- **Accuracy of tiny models** on the owner's voice and the house vocabulary (names like `khtpm`, `tomom`): that is what the pronunciation map and hot-word lists are for.
- **The whisper chunk bug** (`?` vs `!`) is unexplained; reproduce before trusting streaming.
- **CPU:** weak dev machine; measure; move to debil/Mac if needed.
- **Microphone hardware and noise** (the owner's notes mention using voice during rest from fingers or back).
- **Consent and storage:** where exactly the owner wants recordings kept, and for how long (proposal: owner's user data path, 30 days, delete command).
- Questions for the owner: (1) may recordings of your voice be kept for training, and where? (2) which commands first? (3) is a wake word wanted, or only explicit arming? (4) English only, or ja/zh like some pets?
