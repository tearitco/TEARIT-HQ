# voice-fx (scaffold, 2026-10-14)

An x11-hq window for the voice-fx studio: **list voices, select one, change pitch and speed, rename it, make a new voice from it, say words with it, see the waveform, judge it.** Built so the next agent starts with a working window, not a blank page. Design and plan: BOOK `19.audio/` (`VOICE-AND-SFX-DESIGN.md`, `PRONUNCIATION-MAP.md`, `VOICE-ROADMAP.md`). Pattern copied from img-hai (`&.hq-apps/img-hai/docs/CODE.md` explains the renderer contracts and quirks; read section 3 and 7 before changing the layout).

## Run
`sh '&.hq-apps/voice-fx/button.sh' run` (builds if needed) / `stop`. Window 1200x700. Test: `relay_drive <renderer pid> run '&.hq-apps/voice-fx/tests/voice-fx-smoke.pdl' <winid>` (fresh window; stop other 1200x700 windows first, `findwin 1200 700` finds windows by size; start from a clean `state/` without `voices2.pdl`). Last run 2026-10-14: 33/33 pass, including a real offline voice made with all effects (about 8 s). One earlier attempt of the same case failed once with "the voice engine made no sound" and could not be reproduced (a rerun and a direct command both passed): cause unknown, watch for it.

## What is real and what is a stand-in
| Part | State |
|---|---|
| window, voice list, select, pitch/speed, rename, new voice, words field, verdict buttons, waveform | **built, relay-tested** |
| the voice engine | **stand-in**: offline Kokoro through `@.apps/pet-trainer/ops/pet_tts_local.py` (voice name -> KMAP row) |
| pitch, speed, formant, bright, warm, vibrato, breath | **in-house C** (2026-10-14): `&.widgits/_shared-lib/ops/audio_fx` (BOOK 19.audio/VOICE-FX.md, VOICE-DSP.md). Buttons: p-/p+, s-/s+ (bottom strip); form-/form+, brt-/brt+, warm-/warm+, vib-/vib+ (starts 5 Hz, steps of 5 cents, max 60; vib- to 0 switches it off), brth-/brth+ (right column) |
| accent | **built**: the `accent` button cycles none / th-stop / drop-r / w-for-v / short-i (rules in `@.apps/pet-trainer/voices.pdl`, English voices only) |
| pet recipes | the pets start with their `FX` rows from `@.apps/pet-trainer/voices.pdl`; edits are stored per voice in `state/voices2.pdl` (`VOICE2 | id | name | engine | base | pitch | speed | formant | bright | warm | vib_hz | vib_cents | breath | accent`; older shorter rows still load) |
| in-house voices (v0/v1/v2), pronunciation map, candidate edits into a bank, batches, ledger review | **not built** |

## Files
`voice_fx_manager.c` (the brain; header lists verbs), `ops/voice_fx_req.c` (click -> request line), `ops/voice_say.c` (one spoken line: Kokoro engine with the accent -> audio_fx -> wav -> paplay; never overwrites; ledger row with every effect value), `voice-fx.xhtpm` + `.css` (layout), `button.sh`, `build.sh`, `tests/`.
State (not in git): `state/ui.txt` (what the window shows), `request.txt` (clicks, append-only, read by size growth), `voices2.pdl` (latest `VOICE2` row per id wins: this is where renames/knobs live), `audio/*.wav`, `ledger.txt` (`SAY` rows), `verdicts.txt` (`JUDGE` rows, same as img-hai), `say.txt`, `last.txt`.
Voices list = the pet voices in `@.apps/pet-trainer/voices.pdl` (+ `voices2.pdl` rows). A voice's `id` never changes, its display name can.

**Play button:** "▶ Play the last sound" (sidebar, id `play`) replays the most recent sound (`PLAY` verb, `paplay`); `Say`/`Make` still play automatically once.

## How to add something
- **A button:** add a verb in `handle()`, one entry in the `k[]/vb[]` arrays in `write_ui()` (and the loop count), an `<item id=... action="${<id>_action}">` in the template; drive it with `relay_drive press <id>`; add a case. (Same recipe as img-hai EXTENDING A.)
- **Another effect knob:** add the key to `_shared-lib/ops/audio_fx.c` (with its proof in BOOK 19.audio/VOICE-FX.md), a field in `Voice`, load/save in `voice_fx_manager.c` (`load_voices`, `save_voice`), a handler in `handle()`, two buttons in the template and the `k[]/vb[]` arrays, and pass it from `voice_say`.
- **Our own engine:** replace step 1 in `voice_say.c` (the call to `pet_tts_local.py`) with the in-house op; keep the rest.
- **Candidate edits instead of direct edits:** copy `img_edit`'s pattern (`&.widgits/concept-bank/ops/img_edit.c`): validate, overlay trial, promote by a different human.

## Known gaps / honest notes
Not tested: new voice from a full list, speed limits, many verdicts, the Korean pet (no Kokoro voice, the Say button reports no sound), many requests at once. The pet speech path itself is documented in `@.apps/pet-trainer/VOICE-OFFLINE.md`. The window holds up to 32 voices.
