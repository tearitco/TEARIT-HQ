# sfx-hai (scaffold, 2026-10-14)

An x11-hq window for the SFX toy: **list sound recipes, filter by tag, make a sound, check it with numbers, nudge frequency/length, see the waveform, judge it.** Built as a starting point for the in-game sound pipeline. Design: BOOK `19.audio/SFX-TOY-DESIGN.md`; plan `VOICE-ROADMAP.md` (slice S3 is the next real step). Same pattern as img-hai and voice-fx.

## Run
`sh '&.hq-apps/sfx-hai/button.sh' run` / `stop`. Window 1060x700. Test: `relay_drive <renderer pid> run '&.hq-apps/sfx-hai/tests/sfx-hai-smoke.pdl' <winid>` (fresh window, no other 1060x700 window open). Last run 2026-10-14: 13/13 pass; it makes a real sound and the checks report ok. Negative checks (run `ops/+x/sfx_make.+x <pkg> <label> sine 440 0 0.2 0.01 0.1 0.01 0` for silent, vol 3 for clipped): reported `FAIL silent` and `FAIL clipped` as designed.

## What is real and what is a stand-in
| Part | State |
|---|---|
| window, recipe list, tag filter, select, frequency/length nudges (working copies), waveform, verdicts | **built, relay-tested** |
| renderer | **in-house C** (2026-10-14): `ops/sfx_render.c` (sine/square/saw/triangle/noise, sweep, attack/decay, low-pass, seeded, deterministic); `ops/sfx_make.c` calls it and runs the checks. No sox for sounds. Details and proof: BOOK `19.audio/SFX-RENDERER.md` |
| numeric checks | **4 of 4 built** (2026-10-14): duration within 5%, peak 0.05..0.98, dominant frequency of the first window within 8% (at least 2 FFT bins) of the recipe's expected start frequency (tonal waves; noise skipped), |DC| < 0.02. Frequency and DC use our own FFT (`&.widgits/_shared-lib/audio_dsp.c`). Negative tests: silent and clipped recipes fail the peak check; a 15,000 Hz recipe (above Nyquist at 22.05 kHz, aliases to 8,674 Hz) fails the frequency check. **Not negatively tested:** the duration check (the renderer uses the recipe's own duration, so it can only fail if the renderer is wrong) and the DC check inside the window (it was proven in `audio_probe` on a 0.3 DC test file only; sox cannot make a DC-offset recipe) |
| bank edits through candidates, mutation batches, DAW import, sound-to-text (`HEAR` rows) | **not built** |

## Files
`sfx_hai_manager.c` (header lists verbs), `ops/sfx_hai_req.c`, `ops/sfx_make.c` (render + checks + ledger + play), `sfx.pdl` (the bank: `SFX | id | tags | kind | params`), `sfx-hai.xhtpm/.css`, `button.sh`, `build.sh`, `tests/`. State (not in git): `state/ui.txt`, `request.txt`, `sfx2.pdl` (working copies, latest row per id wins), `audio/*.wav`, `ledger.txt` (`SFX` rows with the check text), `verdicts.txt`, `say.txt`, `last.txt`.

**Play button:** "▶ Play the last sound" (sidebar, id `play`) replays the most recent sound (`PLAY` verb, `paplay`); `Say`/`Make` still play automatically once.

## How to add something
- **A recipe:** add an `SFX` row to `sfx.pdl` (params in its header). Press Make; read the check text.
- **A check:** add it in `sfx_make.c` where `chk` is built; use the FFT helpers in `_shared-lib/audio_dsp.c` (`adsp_dominant_hz`, `adsp_dc`, `adsp_peak`, `adsp_rms`) or measure a file by hand with `&.widgits/_shared-lib/ops/+x/audio_probe.+x <wav> [start_ms] [window]`; add a bad recipe that must fail it.
- **The renderer:** `ops/sfx_render.c` (see SFX-RENDERER.md section 3 for how to add a wave, envelope or filter; re-run its proof table after any change).
- **A button / layout:** see `voice-fx/README.md` and img-hai's EXTENDING.md recipe A.
- **DAW:** needs an audio-clip kind in `@.apps/media-daw-hq` first (roadmap D1).
