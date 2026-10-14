# sfx-hai (scaffold, 2026-10-14)

An x11-hq window for the SFX toy: **list sound recipes, filter by tag, make a sound, check it with numbers, nudge frequency/length, see the waveform, judge it.** Built as a starting point for the in-game sound pipeline. Design: BOOK `19.audio/SFX-TOY-DESIGN.md`; plan `VOICE-ROADMAP.md` (slice S3 is the next real step). Same pattern as img-hai and voice-fx.

## Run
`sh '&.hq-apps/sfx-hai/button.sh' run` / `stop`. Window 1060x700. Test: `relay_drive <renderer pid> run '&.hq-apps/sfx-hai/tests/sfx-hai-smoke.pdl' <winid>` (fresh window, no other 1060x700 window open). Last run 2026-10-14: 13/13 pass; it makes a real sound and the checks report ok. Negative checks (run `ops/+x/sfx_make.+x <pkg> <label> sine 440 0 0.2 0.01 0.1 0.01 0` for silent, vol 3 for clipped): reported `FAIL silent` and `FAIL clipped` as designed.

## What is real and what is a stand-in
| Part | State |
|---|---|
| window, recipe list, tag filter, select, frequency/length nudges (working copies), waveform, verdicts | **built, relay-tested** |
| renderer | **stand-in**: `sox synth` via `ops/sfx_make.c` (the in-house C synth is roadmap S3) |
| numeric checks | **2 of 4 built**: duration within 5% and peak between 0.05 and 0.98. Not built: dominant frequency within 5% of f0, no DC offset. The duration check cannot fail yet (the renderer uses the recipe's own duration): it needs the in-house renderer or a mutation batch to test negatively |
| bank edits through candidates, mutation batches, DAW import, sound-to-text (`HEAR` rows) | **not built** |

## Files
`sfx_hai_manager.c` (header lists verbs), `ops/sfx_hai_req.c`, `ops/sfx_make.c` (render + checks + ledger + play), `sfx.pdl` (the bank: `SFX | id | tags | kind | params`), `sfx-hai.xhtpm/.css`, `button.sh`, `build.sh`, `tests/`. State (not in git): `state/ui.txt`, `request.txt`, `sfx2.pdl` (working copies, latest row per id wins), `audio/*.wav`, `ledger.txt` (`SFX` rows with the check text), `verdicts.txt`, `say.txt`, `last.txt`.

## How to add something
- **A recipe:** add an `SFX` row to `sfx.pdl` (params in its header). Press Make; read the check text.
- **A check:** add it in `sfx_make.c` after the `sox stat` step (dominant frequency: use `sox out -n stat -freq` or compute an FFT in C); add a bad recipe that must fail it.
- **The C renderer (S3):** write `sfx_render` (oscillators, ADSR, filters, seeded noise) with the same arguments as `sfx_make`'s recipe; swap it in for the sox call; keep the checks; compare with the sox output by the same checks.
- **A button / layout:** see `voice-fx/README.md` and img-hai's EXTENDING.md recipe A.
- **DAW:** needs an audio-clip kind in `@.apps/media-daw-hq` first (roadmap D1).
