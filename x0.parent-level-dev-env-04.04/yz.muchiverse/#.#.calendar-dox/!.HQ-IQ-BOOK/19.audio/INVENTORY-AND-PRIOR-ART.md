# Inventory and prior art (2026-10-14)

## 1. In the house now
| Thing | State | Evidence |
|---|---|---|
| Pet speech, offline | **ran**: Kokoro-82M via `@.apps/pet-trainer/ops/pet_tts_local.py`; en/ja/zh voices; no Korean voice; no pitch control; 4-9 s for a new phrase (2 threads, niced), cached as wav | `@.apps/pet-trainer/VOICE-OFFLINE.md` |
| Edge TTS | broken: HTTP 403 on the websocket since 2026-10-12, re-probed 2026-10-14 | `convert_night_to_audio_local.py` header; probe |
| Kokoro files | `~/.local/share/kokoro/kokoro-v1.0.int8.onnx` + `voices-v1.0.bin`; python `kokoro-onnx`, `onnxruntime`, `espeakng-loader`, `pyttsx3`, `gTTS` installed | pip list |
| Tools | `sox`, `ffmpeg`, `paplay`, `aplay` present | `which` |
| `media-daw-hq` (+ `media-daw`, source `103.media-studio/103.daw`) | *read*: demo tracks with MIDI-ish clips, transport, piano roll, mixer chips. **No audio clips, no WAV, no real synth output** ("Software synth only; no audio clips/WAV; no automation lanes" - HOW2_DAW.md). Manager is 238 lines of C, file-backed actions | `@.apps/media-daw-hq/README.md` |
| `103.media-studio/100.tts-point-2-anything` | *read*: shell scripts that read a document to speech | listing |
| pet-trainer `voices.pdl` | VOICE (species -> voice, rate, pitch), HUM (sox melodies: wave, base Hz, semitone steps, note ms), KMAP (edge voice -> Kokoro voice) | file |
| `pet_hum.sh` | *read*: builds a short melody with sox from a HUM row, cached | `pet-trainer/ops/pet_hum.sh` |
| famous-llm / tomom | a real hand-built LLM pipeline exists (memory note); the pattern for "train our own small model with a grader" | memory `famous-llm-tomom-is-real` |

## 2. In the old projects on the external drive (all *read*, nothing run)
Root: `.../🪆️.a0/` (path has emoji; see the owner's list).
- `🪆️.📟️.a0/🎤️/speech-2-promt-4-ai-fingersaver/` and `🎤️voice.2.chat]good-idea.nov5/`: about 1,700 lines of C written 2025: `2.tts...mp3!.c` (espeak-ng -> mp3 via ffmpeg libs, 226 lines), `3.stt]raw]wordfile.c` (speech-to-text from raw audio to a word file, 267), `4.s&t.2.wordfile]train.c` (194), `5.orch.c` (194), `6.voca_builder.c` (388), `7.enhanced_stt.c` (350), `#.audio_capture.c` (57, PulseAudio), plus a long chat (`#.stt.no-libs.how!?.txt`) planning speech-to-text with only gcc/ffmpeg/pulse/arrays. Goal in the owner's Dec 5 note: voice control of a small AI so fingers can rest.
- `🪆️📲️]internal.symlinx.4xl/wisp&vosk-s2t_2.0/`: `explain_vosk.txt`, `explain_whisper.txt`, whisper.cpp project (`whisper-stt_v0.4-er`, `ggml-tiny.en.bin`, chunked capture; notes: exact transcript of a 3-second test clip, 10-second runs work, chunked streams had a bug), Vosk with three models (note: the 111 MB model matched the 2 GB one).
Use: the C capture and orchestrator pieces are starting points; the espeak-ng C code is the base for the in-house voice's front end; whisper.cpp is the working speech-to-text baseline. None of it has been compiled or run in this session.

## 3. Online (web search 2026-10-14; thin and partly commercial: **verify on each project's own page**)
| Need | Candidate | Reported licence | Note |
|---|---|---|---|
| Train a TTS voice | Piper (VITS family) | MIT | CPU inference; training steps not confirmed by the search |
| | VITS (architecture) | MIT | basis of many models |
| | Kokoro-82M | Apache 2.0 | what we run; training code availability not confirmed |
| Text -> sound effects | Stable Audio Open (+ `stable-audio-tools` training code) | **sources disagree**: community licence (free under a revenue cap), non-commercial research, or OpenRAIL-M | trained on ~500k CC-0 / CC-BY / CC-Sampling+ recordings; needs a GPU in practice |
| | jsfxr / sfxr style synths | not checked | procedural, tiny, no neural net: the model for the in-house SFX recipes |
| Voice conversion | RVC (retrieval-based) | not checked | ~1 s lag on an RTX 3060 in one review; needs GPU |
| Speech to text | whisper.cpp, Vosk | not checked here (both were run by the owner) | baselines |
| Pronunciations | CMUdict, espeak-ng rules | not checked | the data source for the pronunciation map |
Nothing found that covers the whole chain. **Not searched:** AudioLDM, formant/pitch libraries, Coqui-style trainers, current status of any of these.
Licence rule (owner): free models only, and read the licence before shipping any generated asset.
