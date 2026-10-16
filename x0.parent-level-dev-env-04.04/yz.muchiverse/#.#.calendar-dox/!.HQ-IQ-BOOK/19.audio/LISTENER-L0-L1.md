# Listener L0 and L1: what was built and measured (2026-10-14)

Ops, install and rows: `44.xyz.01.00/&.widgits/listener/README.md`. Design: `LISTENER-STT-DESIGN.md`. **Everything below is synthetic speech (the house's offline voices), file-sourced; the microphone was never opened and no human speech was tested.**

## 1. Whisper works from the owner's old build
`whisper-cli` (whisper.cpp 1.8.2, MIT licence file read from the tree) + `ggml-tiny.en.bin` copied to `~/.local/share/house-listener/whisper/` (needs four shared libraries beside it; copied as real files). Test: a Kokoro-spoken "Open the file and then press enter." transcribed exactly. Time: 2.6 to 3.0 s per 3 s clip (ffmpeg + load + decode) on this machine when idle; **70 to 111 s for the same kind of clip while the load average was about 20** (browser, windows, renders): the dev machine is weak; heavy listening belongs on debil/Mac.

## 2. Measurements
| Check | Result |
|---|---|
| `wer` known cases | identical text 0.0; empty hypothesis 1.0; one wrong word of four 0.25; two insertions on two words 1.0; empty reference exit 2 (no rate) |
| `vad` on a constructed file (3 spoken sentences with known positions; 0.4 s and 1.5 s gaps; faint -60 dBFS noise) | spans 0.94-6.44 s and 7.88-10.30 s against truth 1.00-6.46 and 7.96-10.32: within 100 ms; the 0.4 s gap merged (hangover 700 ms), the 1.5 s gap split |
| `vad` on speech over a steady -45 dBFS hiss | one span 0.94-3.16 s (truth 1.00-3.24) |
| `vad` on silence (-70 dBFS) and on steady hiss only (-38 dBFS) | no segments, exit 3 (negatives) |
| `hear` on 3 clips and the 3-sentence file | exact transcripts, including the 3 sentences in one transcript |
| `hear` negatives | silence and hiss: exit 3, no transcript, **no HEAR row** (ledger rows 5 before and after); missing file exit 1; missing whisper install exit 1 with the layout message; bad `--consent` exit 2. The speech gate is required: whisper invents text on silence |
| `listen_capture --file` (real-time streaming through ffmpeg) | speech over hiss captured (2.70 s utterance) and transcribed exactly; **the indicator file held the pid while open and was gone after**; silence and steady hiss: exit 3, no file, `LISTEN ... 0 | no speech`; 3-sentence file: one utterance per session (sentences 1+2, stopped at the 1.5 s gap), transcribed exactly; three bad-argument cases exit 2; one `LISTEN` row per session |
| synthetic accuracy set (12 sentences, 4 voices, 91 words, `data/synth_set.pdl`) | **WER 0.044 (4 errors)**, all on house words: `khtpm` -> "KHDPM", `tomom` -> "Tomam", `brixel` -> "bricksle", `colab` -> "collab" |
| same set with whisper's initial prompt "khtpm, tomom, brixel, colab, pdl" | **WER 0.011 (1 error)**: tomom, brixel and colab fixed; `khtpm` still "KHDPM". **The hot words were written knowing the test sentences, so this is a demonstration that the mechanism works (learning loop 2 in LEARNING-FROM-HUMAN-SPEECH.md), not an estimate for new sentences.** A real estimate needs held-out sentences |

## 3. What this means
- The pipeline works end to end on files: gate, transcribe, log, score, with negatives.
- Plain whisper tiny handles ordinary English from clean synthetic voices; **the house's own words are where it fails**, which is exactly what hot words and the pronunciation map are for. A hot-word list file and a `FIXED` correction flow are the next pieces of that loop.
- The microphone path (`parecord`) and live latency are untested; human speech, noise and accents are unknown.

## 4. Not built / next
L2: a voice-fx "listen" button (file mode for the relay test, then the microphone armed by the owner) and typed corrections (`FIXED` rows); a hot-word list file read by `hear`; held-out sentences and the owner's own clips for real numbers; the command spotter (L3); the round trip (L4: speak a word with a voice, hear it, compare: this set is already a prototype of it); hotsprings wiring. The whisper chunking bug in the owner's notes is untested against this design.
