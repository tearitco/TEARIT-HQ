/* audio_shift - change the pitch and/or speed of a mono 16-bit WAV, in house: audio_shift <in.wav> <out.wav> [pitch_st=0] [speed=1.0]
 * pitch_st: semitones, -12..+12, duration kept (WSOLA time stretch then resample; formants move with the pitch, see audio_dsp.c). speed: 0.5..2.0, pitch kept (WSOLA). Both can be given:
 * speed is applied first, then pitch. Output keeps the input sample rate. Text-includes ../audio_dsp.c. Exit 0 ok, 1 cannot read/write or too short (under 50 ms), 2 usage or knob out of range.
 * Roadmap slice S1 (BOOK 19.audio/VOICE-ROADMAP.md). Tests: 19.audio/VOICE-DSP.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../audio_dsp.c"
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: audio_shift in.wav out.wav [pitch_st] [speed]\n"); return 2; }
    double st = argc > 3 ? atof(argv[3]) : 0, sp = argc > 4 ? atof(argv[4]) : 1.0;
    if (st < -12 || st > 12 || sp < 0.5 || sp > 2.0) { fprintf(stderr, "knob out of range (pitch -12..12 semitones, speed 0.5..2.0)\n"); return 2; }
    short *s; long n; int sr; if (adsp_read_wav(argv[1], &s, &n, &sr)) { fprintf(stderr, "cannot read %s as 16-bit PCM wav\n", argv[1]); return 1; }
    if (n < sr / 20) { fprintf(stderr, "too short (under 50 ms)\n"); free(s); return 1; }
    short *cur = s; long cn = n;
    if (fabs(sp - 1.0) > 1e-9) { long on; short *t = adsp_wsola(cur, cn, sr, 1.0 / sp, &on); if (!t) { fprintf(stderr, "too short for the speed change\n"); free(s); return 1; } free(cur); cur = t; cn = on; }
    if (fabs(st) > 1e-9) { long on; short *t = adsp_pitch_shift(cur, cn, sr, st, &on); if (!t) { fprintf(stderr, "too short for the pitch change\n"); free(cur); return 1; } free(cur); cur = t; cn = on; }
    int rc = adsp_write_wav(argv[2], cur, cn, sr) ? 1 : 0; free(cur); return rc;
}
