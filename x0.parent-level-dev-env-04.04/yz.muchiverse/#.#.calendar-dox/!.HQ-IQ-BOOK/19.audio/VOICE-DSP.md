# Voice DSP: pitch and speed in house C (slice S1, 2026-10-14)

What was built, how to use it, how to change it, how it was proven, what was not.

## 1. What
`&.widgits/_shared-lib/audio_dsp.c` (text-include) gained: `adsp_write_wav`, `adsp_wsola` (time stretch keeping pitch), `adsp_resample` (cubic), `adsp_pitch_shift`. The op `&.widgits/_shared-lib/ops/audio_shift.c`:
`audio_shift <in.wav> <out.wav> [pitch_st=0] [speed=1.0]`. pitch -12..+12 semitones with the duration kept; speed 0.5..2.0 with the pitch kept; both allowed (speed first). Mono 16-bit PCM in and out, same sample rate. Exit 0 ok, 1 unreadable or under 50 ms, 2 knob out of range.
Used by `&.hq-apps/voice-fx` (its Say button: Kokoro -> `audio_shift` -> wav) instead of sox. Measure any file with `audio_probe`.

## 2. How it works
- **Time stretch (WSOLA):** Hann-windowed frames of 25 ms overlap-added at half-frame spacing. Each frame's input position is searched within +-10 ms of the nominal position for the best waveform match (cross-correlation, every second sample) with the natural continuation of the previous frame, which keeps periodic sounds in phase.
- **Pitch shift:** stretch by r = 2^(st/12), then read the result r times faster with a 4-point cubic interpolator: duration back to the original, pitch times r.
- **Consequence:** the formants (vocal tract resonances) move with the pitch, so up is "chipmunk", down is "giant". That suits pets and game characters but is not a formant-preserving shift. **Formant knob: not built.** It needs PSOLA (pitch-synchronous) or a phase vocoder with spectral-envelope correction; both are listed under next steps. The `VOICE2` row's formant and brightness columns stay 0 and are ignored.

## 3. Proof (2026-10-14; measured by `audio_probe` and a numpy autocorrelation harness, not by ear)
| Case | Result |
|---|---|
| 440 Hz tone, +12 st | 880.57 Hz (err 0.06%), duration 2.000 s (0.00%) |
| 440 Hz, -12 st | 219.63 Hz (0.17%), 2.000 s |
| 440 Hz, +7 st / 200 Hz, -5 st / 440 Hz, +1 st | 658.70 (0.08%), 150.05 (0.15%), 465.66 (0.11%); all 2.000 s |
| 440 Hz, speed 1.5 / 0.5 | 440.33 Hz (0.07%), 1.333 s; 440.34 Hz (0.08%), 4.000 s |
| 440 Hz, +12 st and speed 2 together | 880.55 Hz, 1.000 s |
| negatives | pitch 13 -> exit 2; speed 3 -> exit 2; missing file -> exit 1; 20 ms file -> exit 1 |
| real speech (Kokoro, 3.86 s) | duration exact in all four cases; median F0 within 1.7% (+5 st), 3.2% (-5 st), 2.4% (speed 1.5, pitch unchanged), 2.6% (+12 st and speed 0.8) of the target. **The estimator itself shows about 2% noise (the speed-only case should be 0%)**, so these figures bound the error, they do not pin it down |
| window | voice-fx relay case 20/20 with the new DSP in the loop; the resulting file was 24 kHz, 1.45 s |
Pure tones are the easy case. Pass criterion used: dominant frequency within 1% and duration within 2% for tones.
**Not verified:** how it sounds. Demo files for the owner: `XO/25.games-plan/19-audio/demo_speech_original.wav`, `demo_speech_plus5st.wav`, `demo_speech_minus5st.wav`. Artefacts (warble, roughness) at large shifts, stereo input (first channel only), very noisy input, sounds shorter than ~100 ms.

## 4. Change it
Frame size (25 ms) and search width (10 ms) are the two constants in `adsp_wsola`; shorter frames follow fast pitch changes, longer frames sound smoother on low voices. To compare variants: render the same speech file with each and run the numpy harness idea above (median F0 and duration) plus a listen. To add formant control: add `adsp_psola` or a phase-vocoder with an inverse FFT to `audio_dsp.c` (the forward FFT is there), expose `formant_st` in `audio_shift`, and add the cases above for it.

## 5. Next uses
Pets: `pet_tts_local.py` ignores the VOICE pitch column (Kokoro has none). Convert the Hz column to semitones and run `audio_shift` after synthesis so the six species differ again (not done). SFX: pitch variants of one recipe (coin pickups rising). Pronunciation round trip: measure durations after speed changes.
