# The SFX toy (design, 2026-10-14)

A separate window and bank for in-game sound effects: blips, hits, pickups, engines, doors, footsteps, ambience. Separate from voice because the engine, the data and the checks differ; shared with voice: the word/concept bank, the ledger, the two judges, the candidate-edit rails, the DAW. **Status: a scaffold window exists (see `&.hq-apps/sfx-hai/`, stand-in renderer uses `sox`); the in-house C synth is not built.**

## 1. Why procedural first
A recipe (oscillator, envelope, filter, noise, pitch slide) is a few numbers: tiny, deterministic, editable, trainable by attrition with plain numeric graders. The sfxr family proved it for retro game sound. Neural text-to-SFX (Stable Audio Open etc.) needs a GPU and has a disputed licence (INVENTORY section 3); it can only be an off-line drafter on debil/Mac later.

## 2. The bank
`sfx.pdl`: `SFX | id | tags | kind | params`. Example (kinds and params to be fixed in slice S3):
```
SFX | coin_pickup | pickup,coin,bright | synth | wave=square f0=988 f1=1319 slide_at=0.08 dur=0.22 attack=0.002 decay=0.18 vol=0.6
SFX | door_creak  | door,wood,slow     | synth | wave=saw f0=140 f1=95 dur=0.9 noise=0.15 lp=900 vol=0.5
SFX | car_hit     | car,crash,heavy    | synth | wave=noise dur=0.5 lp=2400 hp=60 attack=0.001 decay=0.45 thump=70 vol=0.8
```
Tags are words from the word bank (so a story or a game rule can ask for `door,wood`). One id never changes; a better version replaces the file the id points at through a ledger row, not by overwriting.

## 3. The renderer (C op, in-house)
`sfx_render <recipe row | id> <out.wav>`: oscillators (sine, square, saw, triangle, noise), ADSR envelope, one-pole low/high-pass, pitch slide, optional bit-crush, 22.05 or 44.1 kHz mono 16-bit PCM, deterministic (seeded noise). Numbers checks (real code, no listening): duration within 5% of `dur`, peak between 0.05 and 0.98 (no silence, no clipping), the dominant frequency of the first 50 ms within 5% of `f0` for tonal kinds, no DC offset. Stand-in until it exists: the scaffold calls `sox synth` with the same params.

## 4. Candidates and judging (same as images)
`EDIT | type=sfx_param_delta | target=<id> | key=f0 | delta=... | ...` bounded (frequency +-20%, duration +-30%, volume +-0.2); `sfx_add`; validator; trial overlay (render with the candidate, not promoted); judge `JUDGE | label | claude|owner | yes|no`; a human other than the proposer promotes. A drafter may mutate recipes at random within the bounds to produce a batch (attrition); the numeric checks above reject duds before any human listens.

## 5. Sound to text (the same pipeline)
Each judged sound gets words: its tags plus a one-line DESCRIBE (`WHAT | HOW | WHERE`) written by a model and validated for length and vocabulary; the listening op (whisper.cpp for speech; for effects a spectral fingerprint: duration, loudness envelope, dominant frequencies) adds a `HEAR` row. This is how a story page can be assembled from sounds (`door_creak` + `footstep_wood` + a voice line) and how a sound can be found from words.

## 6. DAW link
`@.apps/media-daw-hq` needs an **audio-clip kind** (a clip that names a wav, played by the manager). Until then the toy plays with `paplay`. Games load by SFX id from the bank.

## 7. First proof (slice S3)
Render `coin_pickup` and `car_hit`; the four numeric checks pass; a deliberately broken recipe (silent, clipped, wrong frequency) fails each check; the owner hears them; the window shows a waveform and the verdict buttons; one relay test makes a sound by pressing buttons by id. Report heard vs measured separately.
