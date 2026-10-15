# Stock-take, 2026-10-13 to 2026-10-14: everything changed, explained, and what it gives momentum toward

Legend: **Proven** = I observed it work with evidence. **Measured** = numbers from instruments, not heard/seen by the owner. **Unproven** = built or planned, not shown to work. Commit hashes are on branch `claude`. HOUSE and BOOK are defined in `00-START-HERE.md`.

## A. The five workstreams (and why each exists)

### A1. img-hai: a text-to-image studio window (earlier; docs in `&.hq-apps/img-hai/docs/`, 7 files)
- **What**: an x11-hq window (1060x900, default cell 128) where words (weighted) become a prompt, remote hosts draft pictures, a contact sheet is judged, pixel art is made from the result (`img_pixel`: median-cut palette, block or average mode, `pixel_modes.pdl`), and parts are placed on a canvas for brixel-style objects.
- **Compute**: drafts run **remotely** on debil and the Mac (bk-sdm-tiny and tiny-sd, CPU); batches pull jobs from a queue; this machine only coordinates.
- **Judging**: two judges (claude and the owner) with a gated Accept. **The owner has judged nothing yet; no picture is accepted.**
- **Bank tuning**: candidate edits to the word bank with overlay trial. `tune-1` lifted claude's yes-rate from 1/12 to 4/11 and was **not promoted** (owner has not judged). Treat as a working example of the promote-by-evidence loop, not as a result.
- **Incident to know**: 9 pictures were once overwritten; 6 survive only as 256 px sheet crops. Naming is now never-overwrite. Lesson text: BOOK `03-pitfalls/IMG-HAI-PITFALLS.md`, `16.game/IMG-HAI-BREAKTHROUGH.md`.
- **Known gap**: canvas layout is not persisted. No taskbar menu entry (launch with `button.sh run`).
- Commits: `72889cc94` img_pixel, `ff2339f7a` pixel modes wired into the draft job, `60af91503` default cell 128, `cb2258719`/`6a18ddb4e` handoff and lesson docs.
- **Momentum toward**: pictures for the brixel/pet/game world (G3a in the games roadmap) without paid APIs.

### A2. Voice and sound: in-house, offline, owned (BOOK `19.audio/`, 13 docs; demos in `XO/25.games-plan/19-audio/`)
Why: edge-tts returned HTTP 403 from 2026-10-12 on (still failing 2026-10-14), pets were silent, and the owner wants to train the house's own voice software rather than depend on a rate-limited service.
- **Pets speak offline** (`281c400c5`): `@.apps/pet-trainer/ops/pet_tts_local.py` (Kokoro-82M via kokoro-onnx, 2 threads, one synthesis at a time via a lock file) is a drop-in for the edge-tts argument form; `KMAP` rows in `pet-trainer/voices.pdl` map voices; the Korean voice has no mapping (pet stays silent). Six C apps changed one line each (rpg-pet, cdda-hq, gta-hq, df-hq, dsr-rpg, mcl-hq). Cached `say_*.wav` keep the old sound until deleted.
- **Shared DSP in C** (`_shared-lib/audio_dsp.c`, a text-include): WAV I/O, radix-2 FFT, WSOLA time stretch, 8-tap windowed-sinc resampler, pitch shift, cepstral formant shift (changes the vocal-tract colour, keeps pitch and length), RBJ bass/treble shelves, vibrato, breath, energy VAD. Ops in `_shared-lib/ops/`: `audio_probe`, `audio_shift`, `audio_fx`. **Measured**: tones within 0.2% of target, durations exact, speech pitch within ~3%, formant shift moved the resonance ~+17% for +25% asked (pitch kept exactly), shelves within 0.16 dB, pitch shifting dulls the top end somewhat. Commits `c7a3d887e`, `2eb5cd9c9`, `e7b492ccd`.
- **Pronunciation map** (`fde9bd919`): `concept-bank/data/audio/pron.pdl` (empty on purpose), `phonemes.txt`, `ops/pron_check.c` (10/10 deliberately-bad files rejected), override wired into `pet_tts_local.py`.
- **sfx-hai window** (`0647b867f`, `&.hq-apps/sfx-hai/`, 1060x700): recipes (sine/square/saw/triangle/noise, sweep, envelope, lowpass, seeded and deterministic) rendered by C op `sfx_render`; `sfx_make` checks duration, peak, dominant frequency and DC. **Proven**: relay case 15/15; silent, clipped and above-Nyquist recipes fail as designed. Known weakness: the duration check cannot fail yet.
- **voice-fx window** (`53a9b15b5`, `4b2279930`, `&.hq-apps/voice-fx/`, **1200x700**): voices list (pets start from their `FX` rows), buttons for pitch, speed, formant, brightness, warmth, vibrato, breath, accent, say, rename, new voice, verdicts; state in `state/voices2.pdl`. **Proven**: relay case `tests/voice-fx-smoke.pdl` 35/35 from clean state. One earlier run failed once ("the voice engine made no sound"); not reproduced, cause unknown.
- **Play button** (`aa730d5d4`): "▶ Play the last sound" in both windows (`PLAY` verb, `paplay`). There had been none; sounds only played once automatically.
- **Pet voice recipes and accents**: my guesses (median pitch plain to fx: bunny 184 to 202, bear 110 to 93, cat 186 to 197, frog 153 to 145, mouse 188 to 250, dragon 137 to 114 Hz). **Never heard by the owner.** Accent rules (th-stop, drop-r, w-for-v, short-i) are English voices only.
- **Momentum toward**: own voice work V0-V2 (needs the owner's answers), a DAW that can hold clips (roadmap D1, `@.apps/media-daw-hq`), voice in the games.

### A3. Listener: the house gets ears (L0 and L1, `f811584f9`; code `&.widgits/listener/`, docs BOOK `19.audio/LISTENER-L0-L1.md`)
- **Ops**: `vad` (speech spans), `listen_capture` (one armed session; writes `<data>/listening.txt` while the source is open; one `LISTEN` ledger row), `hear` (speech gate then whisper.cpp tiny.en then one `HEAR` row; audio not kept unless `--keep`), `wer` (word error rate), and `tools/synth_set.py` (one-off harness).
- **Engine**: whisper copied from the owner's old project on the external drive to `~/.local/share/house-listener/whisper/` (outside the repo; MIT licence file included; four shared libraries + the 78 MB tiny.en model).
- **Measured, synthetic speech only, file-sourced**: VAD spans within 100 ms of truth; silence and steady hiss produce nothing (no file, no transcript, no ledger row); transcripts exact on clean clips; **WER 0.044 on 12 synthetic sentences** (all four errors on house words: khtpm, tomom, brixel, colab); **0.011 with a hot-word prompt, but the prompt was written knowing the test sentences, so that shows the mechanism, not an estimate for new speech**. Speed 2.6-3.0 s per 3 s clip when idle, 70-111 s under load average 20.
- **NOT done**: the real microphone was never opened (the owner must arm it), no human speech was tested, the whisper chunking bug from the owner's old notes is untested, no voice-fx listen button (L2), no command spotter (L3), no round trip (L4).
- **Design docs** (`c3dbd868b`): `LISTENER-STT-DESIGN.md`, `HOTSPRINGS-VOICE-MODE-DESIGN.md` (voice mode for the hotsprings button, which is an inert button today), `LEARNING-FROM-HUMAN-SPEECH.md`. **Open owner questions** (section 7 of the listener design): may the owner's recordings be kept for training and where; will the owner record ~50 evaluation clips; first voice commands; wake word vs explicit arming; English only; should pets react to speech. Cloning a voice is out of scope without separate consent.
- **Momentum toward**: spoken commands to the house, the speak-hear-compare loop that auto-tunes a voice, and (with `auto`, file 02) hands-free operation.

### A4. Grok merge and renderer fix (`f7f8521ef`, pushed)
- 7 grok commits, 910 paths: new apps `tcg-rpgce` and `civ-rpgce` (dsr-rpg board clones), OHR tile-sheet and bar-skin fixes, and a one-line change in `_shared-lib/khtpm_draw_core.c` (light-skin labels dark instead of always white).
- **How it was done and why it matters to you**: grok works in this same checkout, and its files already sat on disk uncommitted, so a normal `git merge` refused. The merge was built with plumbing (three-way merge in a temporary index, tree verified to equal grok's versions except the shared file, `update-ref` with an old-value guard, then `read-tree HEAD`), so no working file changed. Verified: the merged tree differs from `claude` in exactly grok's 910 paths; `xyzfs/users` untouched (17 tracked files before and after).
- A leftover: `khtpm_draw_core.c` shows modified on disk only because someone set it executable (775 vs repo 644); not mine.
- The owner earlier distrusted grok commits; this time they said "merge it". Memory note updated: ask first, merge when told.
- **Unverified**: grok's art fix has not been looked at on screen in a light skin; the renderer binary does contain it (string check only).

### A5. Desktop reset (2026-10-14)
`sh '$.crypts/button.sh' reset` ran at the owner's request after the merge: killed all 8 windows including a taskbar up 29 h, rebuilt (log: no errors), relaunched taskbar, world-manager, hotbar. I relaunched voice-fx. Evidence: the rebuilt renderer's strings contain `tone-light`.

## B. Tools that make all of this testable (the real asset of the week)
- **`relay_drive`** (`_shared-lib/ops/relay_drive.c`, `ad22b6d1e`): drives any khtpm window like a person through its per-process relay file (`#.desktop/entity_menu_history/<pid>.txt`): `key`, `type`, `click`, `tap`, **`press <id|label:text>`** (finds the element in a fresh frame dump and presses it), `shot` (screenshot), `findwin <w> <h>`, and **`run <cases.pdl>`** (STEP rows with expectations; PASS/FAIL per step). Smoke cases exist for sfx-hai (15 steps) and voice-fx (35 steps). **A case file is a recorded, machine-checkable macro. File 02 builds the `auto` system on exactly this.**
- Caveat from memory: the relay can pass a focus bug that real hardware still shows; do not trust relay alone for grab/focus fixes.
- House conventions: pal harnesses, marker files, per-window `button.sh`, per-app `state/` dir, manager (C) + `.xhtpm` projection.

## C. Open items and owner decisions waiting
1. Owner has not judged any img-hai picture; `tune-1` unpromoted; canvas layout unsaved; no menu entries for img-hai, voice-fx, sfx-hai.
2. Owner has not listened to any voice/SFX/pet demo (files in `XO/25.games-plan/19-audio/demo_*.wav`).
3. Listener questions (A3). Armed cursword (a display-wide keyboard grab) is an owner-undecided possible bug (memory `revisit-armed-cursword`); hotsprings design says voice arming must not depend on it.
4. Colab room post (session 1791851915) needs the owner's Approve click; grok has been told only that the z-cell v1.1 format fits.
5. ~12 TPMOS tracked runtime files and others are dirty from other agents' or the owner's runs. Never commit them.
6. Games lane (grok): df-hq, cdda-hq specs, gta-hq creative design (docs in BOOK `16.game`, XO `25.games-plan`); claude owns infrastructure.

## D. Where this points (the next steps, in order of leverage)
1. **`auto` (file 02)**: turns every window's existing relay + state files into an automation surface. Highest leverage because every toy above already has a relay case.
2. Close the voice loop: listen button (L2) then round trip (L4) gives a measurable objective (intelligibility, WER) for tuning voices unattended.
3. Owner's ears and eyes: judge pictures, listen to demos. Several automation loops are blocked on human verdicts by design.
4. DAW audio clips (D1) so sound can be arranged, not just generated.
5. Cross-machine networking (`palnet_peer.c` hardcodes 127.0.0.1; multi-user is same-machine only today) before any "auto across machines".
