# Voice character: effects, accents and pet recipes (2026-10-14)

Owner request: change the pet pitches if desired and modulate other effects so voices sound like different people and accents. **Everything here is measured, not heard** (demos in `XO/25.games-plan/19-audio/demo_pet_*`); the recipe values are claude's first guesses and need your ears.

## 1. The tools (all in-house C, text-include `_shared-lib/audio_dsp.c`)
`_shared-lib/ops/audio_fx <in.wav> <out.wav> [key=value ...]`, applied in this order:
| Key | Range | What it does | How it was measured |
|---|---|---|---|
| `speed` | 0.5..2 | tempo, pitch kept (WSOLA) | slice S1 (VOICE-DSP.md) |
| `formant` | -6..+6 semitone-equivalents | vocal-tract size: moves the resonances, **keeps pitch and duration** (STFT, cepstral envelope warp, Hann N=1024 hop 256) | synthetic vowel (150 Hz saw through an 800 Hz resonator): F0 150.0 Hz unchanged (0.0%), duration exact; resonance 791 -> 929 Hz at +3.86 st (requested x1.25, got x1.17) and -> 595 Hz at -3.86 st (requested x0.80, got x0.75): within about 7%, limited by the harmonic spacing (150 Hz) |
| `pitch` | -12..+12 st | pitch, duration kept; formants move with it | S1 |
| `vib_hz` + `vib_cents` | 0..12 Hz, 0..100 cents | vibrato (swinging delay, 8-tap sinc interpolation) | 440 Hz tone, 6 Hz, 50 cents: 428.0..452.2 Hz (ideal 427.5..452.9) |
| `warm` | -12..+12 dB | low shelf at 250 Hz | 100 Hz tone +5.84 dB for +6 requested; 2 kHz tone unchanged |
| `bright` | -12..+12 dB | high shelf at 3 kHz | 6 kHz tone +5.84 / -5.84 dB for +-6; 200 Hz unchanged |
| `breath` | 0..0.3 | noise mixed in proportion to the voice's own level (only while speaking) | silence stays silent (rms 0.00000); voiced part gains noise as expected |
Negatives: formant 7, bright 13, an unknown key, a key without a value, breath 0.5, vib_hz 13 all exit 2; missing file exit 1.
A peak limiter scales the result down if it passes 0.98.

## 2. Problems found and fixed during the work
- The first vibrato used linear interpolation, which low-passed the voice (energy above 4 kHz fell from 0.110 to 0.052 of total). Replaced by an 8-tap Blackman-windowed sinc (`adsp_interp8`), which also replaced the cubic resampler in the pitch shift: 0.082 now. Earlier proofs re-run after the change (pitch +12/-12/+7: 880.57 / 219.63 / 658.70 Hz, durations exact; vibrato range unchanged).
- **Still open:** pitch shifting by WSOLA also dulls the highest frequencies a little (energy above 4 kHz 0.110 -> 0.072 at +2 st): overlap-adding noisy segments loses coherence. A pet that is pitched up gets a bit duller; the `bright` shelf partly compensates.

## 3. Accents (`ACCENT` rows in `@.apps/pet-trainer/voices.pdl`)
An accent rewrites each word's phonemes before the voice speaks (English voices only; words with a PRON override are left alone; `^` and `$` anchor to word start/end): `th-stop` (th -> t/d: "de water"), `drop-r` (final r-colour dropped), `w-for-v`, `short-i`. Shown for "the water is very good": default `ðə wˈɔːɾɚ ɪz vˈɛɹi ɡˈʊd`; th-stop `də wˈɔːɾɚ ɪz vˈɛɹi ɡˈʊd`. Accents need the phoneme path, which phonemizes word by word (a little less natural sentence rhythm than whole-line phonemizing).
Real accents differ in rhythm, vowel quality and intonation, not only a few consonants: these four are crude caricature rules, a starting point for the owner to extend. Another cheap accent lever is the base voice and language (en-GB vs en-US Kokoro voices already differ), which the pets already use.

## 4. The pet recipes (`FX` rows, same file) and what was measured
A call is matched by voice name + pitch column (the C apps pass `--voice` and `--pitch`, not the species). Median F0 (autocorrelation, about 2% estimator noise) and spectral centroid of "Hello there, this is the water I think is very good." with the final DSP (plain = `--no-fx`):
| Pet | Recipe | F0 plain -> fx | Centroid plain -> fx |
|---|---|---|---|
| bunny (Ana +30Hz) | pitch +2, formant +1.5, bright +2, vibrato 5 Hz 12 c | 184 -> 202 Hz | 4265 -> 3833 Hz |
| bear (Guy -25Hz) | pitch -3, formant -2, warm +4, bright -2 | 110 -> 93 Hz | 3041 -> 1921 Hz |
| cat (Maisie +15Hz) | pitch +1, formant +0.5, bright +3, breath 0.05 | 186 -> 197 Hz | 2677 -> 3342 Hz |
| frog (Eric -10Hz) | pitch -1, formant -1, warm +2, vibrato 7 Hz 35 c, accent th-stop | 153 -> 145 Hz | 2065 -> 1779 Hz |
| mouse (Ana +90Hz) | pitch +5, formant +3, bright +4, vibrato 8 Hz 20 c | 188 -> 250 Hz | 4279 -> 4136 Hz |
| dragon (Ryan -35Hz) | pitch -4, formant -3, warm +5, bright -1, breath 0.04, accent drop-r | 137 -> 114 Hz | 2714 -> 2045 Hz |
The six pets are now separated in pitch (medians 93 to 250 Hz) and in brightness (centroids 1.8 to 4.1 kHz). The bunny and mouse centroids still fall a little although their recipes brighten them: the pitch shift's high-frequency dulling (section 2) outweighs the shelf. rpg-pet pets also have rows (ja Nanami, zh Xiaoyi/Xiaoxiao, Thomas, Keita); the Korean pet still has no voice. Centroid is a rough stand-in for "brightness" (it includes noise and sibilants): treat the column as indicative.
**Not verified:** how any of it sounds; whether the characters suit the pets; non-English voices with formant shifts; many pets speaking at once; speech cached earlier (cached wavs from before are still the old sound until deleted: `@.apps/*/state/audio/say_*.wav`).

## 5. Use and change
- A pet's character: edit its `FX` row in `@.apps/pet-trainer/voices.pdl` (no rebuild; delete the cached wav to hear it). Turn all effects off: `--no-fx` or delete the rows.
- A new accent: add `ACCENT | id | from | to | note` rows and `accent=id` to an FX row. Test the phonemes with `pet_tts_local.py --phonemes-only --voice <v> --pitch=<p> --text "..."`.
- One-off: `audio_fx in.wav out.wav formant=2 bright=3`.
- **voice-fx window (built 2026-10-14):** every effect above has +/- buttons, plus an accent cycle button; each voice stores its own values (pets start from their FX rows); Say runs engine -> `audio_fx`. Relay case 33/33 from clean state (one earlier run failed once with "no sound" and was not reproduced). Window is 1200x700 (the right-hand button column overlapped the waveform at 1060 wide; the waveform is now 420 px).
- Next: judged recipes through candidate edits (roadmap A1); a real speaker-style model (V2).
