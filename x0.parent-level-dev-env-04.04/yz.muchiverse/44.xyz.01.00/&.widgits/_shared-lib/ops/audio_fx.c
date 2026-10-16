/* audio_fx - make a mono 16-bit WAV sound like a different person: audio_fx <in.wav> <out.wav> [key=value ...]      (BOOK 19.audio/VOICE-FX.md, 2026-10-14)
 * keys (all optional, defaults = no change): speed (0.5..2, pitch kept), formant (semitone-equivalent -6..+6: vocal-tract size; pitch and duration kept), pitch (semitones -12..+12, duration kept;
 * formants move with it), bright (dB -12..+12, high shelf at 3 kHz), warm (dB -12..+12, low shelf at 250 Hz), vib_hz (0..12) with vib_cents (0..100), breath (0..0.3).
 * Order: speed, formant, pitch, vibrato, warm, bright, breath, then a peak limiter (scale down if the peak passes 0.98). Pure, deterministic. Exit 0 ok, 1 unreadable/too short (< 0.1 s), 2 bad key or value out of range.
 * Text-includes ../audio_dsp.c; link -lm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../audio_dsp.c"
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: audio_fx in.wav out.wav [speed= formant= pitch= bright= warm= vib_hz= vib_cents= breath=]\n"); return 2; }
    double speed = 1, formant = 0, pitch = 0, bright = 0, warm = 0, vh = 0, vc = 0, breath = 0;
    for (int i = 3; i < argc; i++) { char *e = strchr(argv[i], '='); if (!e) { fprintf(stderr, "bad argument '%s' (key=value)\n", argv[i]); return 2; } *e = 0; double v = atof(e + 1); const char *k = argv[i]; int ok = 1;
        if (!strcmp(k, "speed")) { speed = v; ok = v >= 0.5 && v <= 2; } else if (!strcmp(k, "formant")) { formant = v; ok = v >= -6 && v <= 6; } else if (!strcmp(k, "pitch")) { pitch = v; ok = v >= -12 && v <= 12; }
        else if (!strcmp(k, "bright")) { bright = v; ok = v >= -12 && v <= 12; } else if (!strcmp(k, "warm")) { warm = v; ok = v >= -12 && v <= 12; } else if (!strcmp(k, "vib_hz")) { vh = v; ok = v >= 0 && v <= 12; }
        else if (!strcmp(k, "vib_cents")) { vc = v; ok = v >= 0 && v <= 100; } else if (!strcmp(k, "breath")) { breath = v; ok = v >= 0 && v <= 0.3; } else { fprintf(stderr, "unknown key '%s'\n", k); return 2; }
        if (!ok) { fprintf(stderr, "%s out of range\n", k); return 2; } }
    short *s; long n; int sr; if (adsp_read_wav(argv[1], &s, &n, &sr)) { fprintf(stderr, "cannot read %s as 16-bit PCM wav\n", argv[1]); return 1; } if (n < sr / 10) { fprintf(stderr, "too short (under 0.1 s)\n"); free(s); return 1; }
    short *c = s; long cn = n; long on; short *t;
    if (fabs(speed - 1) > 1e-9) { t = adsp_wsola(c, cn, sr, 1.0 / speed, &on); if (!t) { free(c); return 1; } free(c); c = t; cn = on; }
    if (fabs(formant) > 1e-9) { t = adsp_formant_shift(c, cn, sr, pow(2.0, formant / 12.0), &on); if (!t) { free(c); return 1; } free(c); c = t; cn = on; }
    if (fabs(pitch) > 1e-9) { t = adsp_pitch_shift(c, cn, sr, pitch, &on); if (!t) { free(c); return 1; } free(c); c = t; cn = on; }
    adsp_vibrato(c, cn, sr, vh, vc); adsp_shelf(c, cn, sr, 0, 250, warm); adsp_shelf(c, cn, sr, 1, 3000, bright); adsp_breath(c, cn, sr, breath);
    double pk = adsp_peak(c, cn); if (pk > 0.98) { double g = 0.98 / pk; for (long i = 0; i < cn; i++) c[i] = (short)lrint(c[i] * g); }
    int rc = adsp_write_wav(argv[2], c, cn, sr) ? 1 : 0; free(c); return rc;
}
