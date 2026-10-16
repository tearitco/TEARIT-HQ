/* sfx_render - the in-house sound-effect renderer: sfx_render <out.wav> <wave> <f0> <f1> <dur> <attack> <decay> <vol> <lp> [seed=1]     (roadmap slice S3, 2026-10-14)
 * wave: sine | square | sawtooth | triangle | noise.  f0, f1: Hz (f1 > 0 = linear sweep f0 -> f1 over dur; 0 = constant; ignored for noise). dur seconds (0.02..5). attack: linear fade-in seconds.
 * decay: linear fade-out seconds ending at dur. vol 0..1.5 (peak scale). lp: Hz of a 2-pole Butterworth low-pass (0 = none). seed: noise seed (same args + seed = same bytes).
 * Output: mono 16-bit 22.05 kHz WAV. Phase is integrated from the instantaneous frequency (no clicks on sweeps). Square and sawtooth are naive (they alias at high pitch: the check in sfx_make
 * catches a frequency above Nyquist; band-limiting is a next step). Replaces the sox stand-in. Exit 0 ok, 1 cannot write, 2 bad arguments.
 * Text-includes ../../../&.widgits/_shared-lib/audio_dsp.c for the WAV writer. Docs: BOOK 19.audio/SFX-RENDERER.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../../../&.widgits/_shared-lib/audio_dsp.c"
#define SR 22050
int main(int argc, char **argv) {
    if (argc < 10) { fprintf(stderr, "usage: sfx_render out.wav wave f0 f1 dur attack decay vol lp [seed]\n"); return 2; }
    const char *wave = argv[2]; double f0 = atof(argv[3]), f1 = atof(argv[4]), dur = atof(argv[5]), att = atof(argv[6]), dec = atof(argv[7]), vol = atof(argv[8]), lp = atof(argv[9]); uint32_t seed = argc > 10 ? (uint32_t)strtoul(argv[10], NULL, 10) : 1;
    int kind = !strcmp(wave, "sine") ? 0 : !strcmp(wave, "square") ? 1 : !strcmp(wave, "sawtooth") ? 2 : !strcmp(wave, "triangle") ? 3 : !strcmp(wave, "noise") ? 4 : -1;
    if (kind < 0 || dur < 0.02 || dur > 5 || att < 0 || dec < 0 || att + dec > dur * 1.0001 || vol < 0 || vol > 1.5 || f0 < 0 || f1 < 0 || (kind != 4 && f0 < 1) || f0 > SR / 2 - 1 || f1 > SR / 2 - 1 || lp < 0 || lp >= SR / 2) { fprintf(stderr, "bad arguments (wave sine|square|sawtooth|triangle|noise; 0.02<=dur<=5; attack+decay<=dur; 0<=vol<=1.5; 1<=f0<11024 for tonal; lp<11025)\n"); return 2; }
    long n = (long)(dur * SR + 0.5); short *o = malloc((size_t)n * 2); if (!o) return 1;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    if (lp > 0) { double w0 = 2 * M_PI * lp / SR, al = sin(w0) / (2 * 0.70710678), c = cos(w0), a0 = 1 + al; b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0; }
    double ph = 0; uint32_t rs = seed ? seed : 1;
    for (long i = 0; i < n; i++) { double t = (double)i / SR, f = f1 > 0 ? f0 + (f1 - f0) * (t / dur) : f0, v;
        if (kind == 4) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; v = (double)rs / 2147483648.0 - 1.0; }
        else { ph += 2 * M_PI * f / SR; if (ph > 2 * M_PI) ph -= 2 * M_PI; double u = ph / (2 * M_PI); v = kind == 0 ? sin(ph) : kind == 1 ? (u < 0.5 ? 1 : -1) : kind == 2 ? 2 * u - 1 : (u < 0.5 ? 4 * u - 1 : 3 - 4 * u); }
        double e = 1; if (att > 0 && t < att) e = t / att; if (dec > 0 && t > dur - dec) { double d = (dur - t) / dec; if (d < e) e = d; if (e < 0) e = 0; }
        v *= e * vol; if (lp > 0) { double y = b0 * v + z1; z1 = b1 * v - a1 * y + z2; z2 = b2 * v - a2 * y; v = y; }
        if (v > 1) v = 1; if (v < -1) v = -1; o[i] = (short)lrint(v * 32767.0); }
    int rc = adsp_write_wav(argv[1], o, n, SR) ? 1 : 0; free(o); return rc;
}
