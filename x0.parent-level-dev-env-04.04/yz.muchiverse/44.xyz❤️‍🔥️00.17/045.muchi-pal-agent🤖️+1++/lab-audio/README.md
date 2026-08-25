# lab-audio

Exploratory scaffolding for the audio pipeline described in
`../ROADMAP-models.txt` §10.4. Not wired into muchi-pal-chat's own
`ops/+x/` build yet - integration shape is still an open question (§10.2).

## What's here (real, tested code - not stubs)

- `wav_io.c` - WAV read/write reference (mono 16-bit PCM). `info` mode
  reads a real file; `gen_test` writes a synthetic sine-wave WAV, useful
  as known-ground-truth input for `fft.c`.
- `fft.c` - real radix-2 Cooley-Tukey FFT doing windowed STFT pitch
  detection. Stage 1 of audio -> MIDI-events (per-window dominant pitch
  only - does NOT yet assemble windows into discrete notes, see its own
  header comment).
- `synth.c` - basic sine-oscillator + ADSR-envelope synth. Renders a
  `pitch|velocity|start_tick|duration|channel` note-event CSV to WAV.
  `gen_test` writes a short hardcoded C-major arpeggio to try it without
  a real transcription source yet.

Verified round-trip: `synth.c gen_test` -> `synth.c` (render to WAV) ->
`fft.c` (analyze it back) recovers all four notes' pitches correctly
(within FFT bin resolution - less than a semitone off each time). And
independently: `wav_io.c gen_test` at 220/440/880 Hz -> `fft.c` correctly
tracks each frequency, not a fixed/broken bin.

## Build

```bash
./compile_lab.sh
```
Compiles into `+x/`, matching IQABOD's own `lab/compile_lab.sh` convention.

## Try it

```bash
./+x/wav_io.+x gen_test /tmp/test.wav 440 1.0
./+x/fft.+x /tmp/test.wav 2048 1024 /tmp/pitch.csv
cat /tmp/pitch.csv

./+x/synth.+x gen_test /tmp/notes.csv
./+x/synth.+x /tmp/notes.csv /tmp/melody.wav
./+x/wav_io.+x info /tmp/melody.wav
```

## Not done yet (see ROADMAP-models.txt §10.4 for the full list)

Onset/duration note assembly (turning `fft.c`'s per-window pitch stream
into a real note-event CSV), binary `.mid` I/O, the actual training loop,
`lame`/`ffmpeg` wiring for MP3 export, and making the synth's ADSR/
waveform parameters actually tunable (the "different weight-tuned
synths" / RL-reward connection).
