# listener (slices L0 and L1, built 2026-10-14)

The house's ears: speech detection, microphone capture, speech to text, word error rate. Design and plan: BOOK `19.audio/LISTENER-STT-DESIGN.md` (privacy rails, ladder), `LEARNING-FROM-HUMAN-SPEECH.md`, `HOTSPRINGS-VOICE-MODE-DESIGN.md`. Proof and numbers: BOOK `19.audio/LISTENER-L0-L1.md`.

## Ops (`ops/+x/` after `bash ops/build.sh`)
| Op | Does |
|---|---|
| `vad <in.wav> [hangover_ms] [min_ms] [--split dir]` | speech spans (`SEG | start_ms | end_ms`); exit 3 = none. Energy VAD in `_shared-lib/audio_dsp.c` (`adsp_vad`): finds loud stretches, not speech as such |
| `listen_capture <out.wav> [--max-s] [--wait-s] [--hang-ms] [--file in.wav] [--data DIR] [--arm ...]` | ONE armed session: records one utterance (waits for speech, stops on a real pause), writes `<data>/listening.txt` (the indicator) while the source is open, one `LISTEN` row per session. **Default source is the microphone (`parecord`)**; `--file` streams a wav in real time instead (tests). Run only when a person arms it |
| `hear <in.wav> [--data DIR] [--home DIR] [--prompt "hot words"] [--keep] [--consent owner\|synthetic\|none]` | converts to 16 kHz, **speech gate** (no speech = exit 3, no transcript, no row), whisper.cpp tiny.en, prints the transcript, one `HEAR` row. Recordings are NOT kept unless `--keep` |
| `wer "<reference>" "<hypothesis>"` | word error rate: `WER | rate | errors | ref words | S | I | D` |
`tools/synth_set.py make|eval` is a one-off harness (python allowed for one-offs; a pal harness is the house goal, not built): speaks `data/synth_set.pdl` with the house voices and evaluates `hear` + `wer` on it. **Synthetic speech, not a person.**

## Install of the whisper engine (outside the repository, on purpose)
Everything private lives under `~/.local/share/house-listener/` (never in git; recordings, ledger):
`whisper/bin/whisper-cli`, `whisper/lib/{libwhisper.so.1,libggml.so.0,libggml-cpu.so.0,libggml-base.so.0}` (real files, no symlinks), `whisper/models/ggml-tiny.en.bin` (77,704,715 bytes), `whisper/LICENSE-whisper.cpp` (MIT, read from the tree).
Installed 2026-10-14 by copying from the owner's old project on the external drive (`.../wisp&vosk-s2t_2.0/whisper-stt_v0.4-er/whisper.cpp/build` + `ggml-tiny.en.bin`; whisper.cpp 1.8.2 libraries). sha256 (first 16 hex): whisper-cli `9fc7600d62546d35`, model `921e4cf8686fdd99`. If it is ever lost: rebuild whisper.cpp (cmake, CPU only) on debil or this machine and download `ggml-tiny.en.bin` from the whisper.cpp model list, then put the same layout there. The op checks the layout and says what is missing.

## Rows (appended to `<data>/ledger.txt`)
`LISTEN | ts | arm source | seconds open | utterances | note` and `HEAR | ts | utterance file or - | whisper-tiny.en | transcript | seconds of speech | ms to make | confidence or - | consent`. Not built yet: `FIXED` (a human correction), the voice-fx "listen" button (L2), the command spotter (L3).

## Safety, as built
No always-on listening: the microphone is opened only by `listen_capture` without `--file`, which only an arm action runs. The indicator file exists exactly while the source is open (checked while running and after exit). Silence and steady noise produce no utterance, no file and no transcript. `hear` does not store audio by default; `consent` is recorded per row. Nothing here sends audio anywhere.

## Not verified
The real microphone path (`parecord`; claude did not open the microphone, no one armed it), real human speech (all tests are synthetic TTS), noisy rooms, accents, long dictation, the whisper chunking bug from the owner's old notes (this design cuts at pauses instead of fixed chunks, but that was not tested against the bug).
