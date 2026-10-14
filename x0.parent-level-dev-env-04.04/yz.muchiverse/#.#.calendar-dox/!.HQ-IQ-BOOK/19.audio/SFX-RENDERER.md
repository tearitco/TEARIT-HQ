# SFX renderer in house C (slice S3, 2026-10-14)

## 1. What
`&.hq-apps/sfx-hai/ops/sfx_render.c` renders one recipe to a mono 16-bit 22.05 kHz WAV, replacing the sox stand-in:
`sfx_render <out.wav> <wave> <f0> <f1> <dur> <attack> <decay> <vol> <lp> [seed=1]`.
- waves: sine, square, sawtooth, triangle, white noise (xorshift, seeded). f1 > 0 = linear frequency sweep f0 -> f1 over dur, phase integrated (no clicks). attack = linear fade-in; decay = linear fade-out ending at dur; vol = peak scale (0..1.5); lp = 2-pole Butterworth low-pass in Hz (0 = off).
- deterministic: same arguments and seed give the same bytes (sha256 checked); a different seed differs.
- argument rejection (exit 2): unknown wave, dur under 20 ms or over 5 s, attack+decay longer than dur, vol over 1.5, tonal f0 under 1 Hz, any frequency at or above Nyquist (11,025 Hz), lp at or above Nyquist.
`ops/sfx_make` (used by the sfx-hai window) calls it, then measures the file in-process with `_shared-lib/audio_dsp.c` and runs the four numeric checks (duration, peak, dominant frequency, DC). No sox is used for sounds any more; `paplay` plays them.

## 2. Proof (2026-10-14, measured, not heard)
| Recipe (bank) | measured |
|---|---|
| coin_pickup (square 988->1319 Hz, 0.22 s) | dominant 1023 Hz (expected ~1023), peak 0.50, DC 0.001, 0.220 s |
| door_creak (saw 140->95 Hz, lp 900) | 138 Hz (~138), peak 0.39, 0.900 s |
| blip_select (sine 660, 0.08 s) | 661 Hz (660), peak 0.50, 0.080 s |
| car_hit (noise, lp 2400) | peak 0.56, DC -0.002 (frequency check skipped for noise) |
| triangle 330 Hz | 328.95 Hz, peak 0.60 |
| same numbers as the old sox renders | dominant 1023 / 138 / 661 Hz identical to the sox stand-in's results |
| determinism | seed 7 twice: identical sha256; seed 8: different |
| negatives through `sfx_make` | silent recipe (vol 0.01): FAIL silent; clipped (square vol 1.4): FAIL clipped; 12,000 Hz (above Nyquist): rejected by the renderer, status `failed: sfx_render rejected...` |
| negatives of the renderer | 5 bad argument sets all exit 2 |
| window | sfx-hai relay case 13/13 with the C renderer in the loop |
**Not verified:** how it sounds against the sox version or to your ears; the duration check still cannot fail (the renderer always writes the requested length: it can only fail if the renderer breaks); DC check negative inside the window.
**Known limits:** square and sawtooth are naive (alias at high pitch; band-limiting via PolyBLEP is a next step); only linear sweeps; no exponential envelopes, no noise colours other than white, no layering; 22.05 kHz only.

## 3. Change it
Add a wave: new `kind` and a line in the sample loop. Add an envelope shape: change `e`. Add a filter: add a second biquad stage. After any change re-run the table above (all rows) and the determinism check. Recipes live in `&.hq-apps/sfx-hai/sfx.pdl`; the bank changes only through candidate edits (not built).

## 4. Next
Mutation batch (random bounded recipe variants -> render -> checks reject duds -> two judges), recipes for engine/footsteps/door/crash, DAW import (needs the audio-clip kind), `HEAR` rows (sound-to-text), band-limited oscillators.
