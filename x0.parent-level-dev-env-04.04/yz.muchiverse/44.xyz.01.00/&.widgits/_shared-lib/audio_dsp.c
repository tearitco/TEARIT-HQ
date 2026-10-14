/* audio_dsp.c - pure audio helpers, TEXT-INCLUDED (house rule: pure code with 2+ consumers is text-included, never header+link; no I/O except adsp_read_wav, no globals).
 * Consumers (2026-10-14): ops/audio_probe.c (the measuring op), &.hq-apps/sfx-hai/ops/sfx_make.c (SFX checks). Planned: voice DSP tests, pronunciation round trip, listener features.
 * Contents: adsp_read_wav (PCM16 WAV, first channel), adsp_fft (in-place radix-2 Cooley-Tukey, n a power of two; same algorithm as 045.muchi-pal-agent/lab-audio/fft.c),
 *   adsp_dominant_hz (Hann window, magnitude peak, parabolic interpolation; skips bins below 20 Hz), adsp_dc / adsp_peak / adsp_rms (full scale = 1.0).
 * Needs <stdio.h> <stdlib.h> <string.h> <math.h> <stdint.h> included first and -lm. Design: BOOK 19.audio/SFX-TOY-DESIGN.md section 3. */
typedef struct { double re, im; } AdspCx;
static void adsp_fft(AdspCx *x, int n) {
    for (int i = 1, j = 0; i < n; i++) { int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { AdspCx t = x[i]; x[i] = x[j]; x[j] = t; } }
    for (int len = 2; len <= n; len <<= 1) { double ang = -2.0 * M_PI / (double)len; AdspCx wl = { cos(ang), sin(ang) };
        for (int i = 0; i < n; i += len) { AdspCx w = { 1.0, 0.0 }; for (int k = 0; k < len / 2; k++) { AdspCx u = x[i + k], v = { x[i + k + len / 2].re * w.re - x[i + k + len / 2].im * w.im, x[i + k + len / 2].re * w.im + x[i + k + len / 2].im * w.re };
            x[i + k].re = u.re + v.re; x[i + k].im = u.im + v.im; x[i + k + len / 2].re = u.re - v.re; x[i + k + len / 2].im = u.im - v.im; double nr = w.re * wl.re - w.im * wl.im, ni = w.re * wl.im + w.im * wl.re; w.re = nr; w.im = ni; } } }
}
/* returns 0 ok; *s malloc'd (caller frees), *n samples, *sr sample rate. Handles extra chunks; 16-bit PCM only (1 or 2 channels: first channel kept). */
static int adsp_read_wav(const char *path, short **s, long *n, int *sr) {
    FILE *f = fopen(path, "rb"); if (!f) return 1; unsigned char h[12]; if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return 2; }
    int ch = 0, bits = 0, rate = 0; unsigned char c[8];
    while (fread(c, 1, 8, f) == 8) { uint32_t sz = c[4] | (c[5] << 8) | (c[6] << 16) | ((uint32_t)c[7] << 24);
        if (!memcmp(c, "fmt ", 4)) { unsigned char b[16]; if (sz < 16 || fread(b, 1, 16, f) != 16) { fclose(f); return 3; } ch = b[2] | (b[3] << 8); rate = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24); bits = b[14] | (b[15] << 8); if (sz > 16) fseek(f, (long)sz - 16, SEEK_CUR); }
        else if (!memcmp(c, "data", 4)) { if (bits != 16 || ch < 1) { fclose(f); return 4; } long frames = (long)sz / (2 * ch); short *buf = malloc((size_t)(frames > 0 ? frames : 1) * 2); short *raw = malloc((size_t)sz + 2); if (!buf || !raw) { free(buf); free(raw); fclose(f); return 5; }
            size_t got = fread(raw, 1, sz, f); frames = (long)got / (2 * ch); for (long i = 0; i < frames; i++) buf[i] = raw[i * ch]; free(raw); fclose(f); *s = buf; *n = frames; *sr = rate; return 0; }
        else fseek(f, (long)sz + (sz & 1), SEEK_CUR); }
    fclose(f); return 6;
}
/* win must be a power of two; analyses win samples starting at `start` (zero-padded past the end). Returns Hz, or -1 if the window is silent (peak magnitude ~0). */
static double adsp_dominant_hz(const short *s, long n, int sr, long start, int win) {
    AdspCx *x = calloc((size_t)win, sizeof *x); if (!x) return -1; int wpk = 0; for (int i = 0; i < win; i++) { long k = start + i; if (k >= 0 && k < n) { int a = s[k] < 0 ? -s[k] : s[k]; if (a > wpk) wpk = a; } }
    if (wpk < 4) { free(x); return -1; }      /* under 4 LSB (about -78 dB): silence or converter dither, not a pitch */
    for (int i = 0; i < win; i++) { long k = start + i; double v = (k >= 0 && k < n) ? s[k] / 32768.0 : 0.0; x[i].re = v * (0.5 - 0.5 * cos(2.0 * M_PI * i / (win - 1))); }
    adsp_fft(x, win); int lo = (int)(20.0 * win / sr) + 1, best = -1; double bm = 0; double *m = malloc((size_t)(win / 2 + 1) * sizeof *m); if (!m) { free(x); return -1; }
    for (int i = 0; i <= win / 2; i++) { m[i] = sqrt(x[i].re * x[i].re + x[i].im * x[i].im); if (i >= lo && i < win / 2 && m[i] > bm) { bm = m[i]; best = i; } }
    double hz = -1; if (best > 0 && bm > 1e-6) { double a = m[best - 1], b = m[best], c = m[best + 1], d = a - 2 * b + c, off = d != 0 ? 0.5 * (a - c) / d : 0; if (off > 0.5) off = 0.5; if (off < -0.5) off = -0.5; hz = (best + off) * (double)sr / win; }
    free(m); free(x); return hz;
}
static double adsp_dc(const short *s, long n) { double a = 0; for (long i = 0; i < n; i++) a += s[i]; return n ? a / n / 32768.0 : 0; }
static double adsp_peak(const short *s, long n) { double p = 0; for (long i = 0; i < n; i++) { double v = s[i] < 0 ? -(double)s[i] : (double)s[i]; if (v > p) p = v; } return p / 32768.0; }
static double adsp_rms(const short *s, long n) { double a = 0; for (long i = 0; i < n; i++) a += (double)s[i] * s[i]; return n ? sqrt(a / n) / 32768.0 : 0; }

/* ---- writing and time/pitch DSP (added 2026-10-14, roadmap slice S1) ---- */
/* mono 16-bit PCM WAV with a plain 44-byte header; returns 0 ok */
static int adsp_write_wav(const char *path, const short *s, long n, int sr) {
    FILE *f = fopen(path, "wb"); if (!f) return 1; uint32_t data = (uint32_t)n * 2, riff = 36 + data, rate = (uint32_t)sr, brate = rate * 2; uint16_t one = 1, two = 2, bits = 16; uint32_t sixteen = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sixteen, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&one, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&brate, 4, 1, f); fwrite(&two, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f); fwrite(s, 2, (size_t)n, f); fclose(f); return 0;
}
/* WSOLA time stretch: output length = n * ratio (ratio > 1 slower/longer, < 1 faster/shorter); pitch is unchanged. Frames of ~25 ms are overlap-added with a Hann window at half-frame spacing;
 * each frame's input position is searched within +-10 ms of the nominal one for the best waveform match (cross-correlation) with the natural continuation of the previous frame, which keeps periodic sounds in phase.
 * Returns a malloc'd buffer (caller frees) and its length in *outn, or NULL. Needs n >= 2 frames. */
static short *adsp_wsola(const short *x, long n, int sr, double ratio, long *outn) {
    int N = (int)(sr * 0.025) & ~1; if (N < 64) N = 64; int Hs = N / 2, delta = (int)(sr * 0.010); long ol = (long)(n * ratio + 0.5); if (n < 2 * N || ol < 1) return NULL;
    double *out = calloc((size_t)ol + 2 * N, sizeof *out), *nm = calloc((size_t)ol + 2 * N, sizeof *nm), *w = malloc((size_t)N * sizeof *w); short *res = malloc((size_t)ol * 2); if (!out || !nm || !w || !res) { free(out); free(nm); free(w); free(res); return NULL; }
    for (int i = 0; i < N; i++) w[i] = 0.5 - 0.5 * cos(2.0 * M_PI * (i + 0.5) / N);
    long prev = 0;
    for (long k = 0;; k++) { long op = k * Hs; if (op >= ol + Hs) break; long pos;
        if (k == 0) pos = 0; else { long nominal = (long)(op / ratio), target = prev + Hs; if (target > n - N) target = n - N; long lo = nominal - delta, hi = nominal + delta; if (lo < 0) lo = 0; if (hi > n - N) hi = n - N; if (lo > hi) lo = hi; pos = lo; double best = -1e30;
            for (long p = lo; p <= hi; p++) { double c = 0; for (int i = 0; i < N; i += 2) c += (double)x[p + i] * x[target + i]; if (c > best) { best = c; pos = p; } } }
        if (pos > n - N) { pos = n - N; } if (pos < 0) { pos = 0; }
        for (int i = 0; i < N; i++) { out[op + i] += x[pos + i] * w[i]; nm[op + i] += w[i]; } prev = pos; }
    for (long i = 0; i < ol; i++) { double v = nm[i] > 1e-3 ? out[i] / nm[i] : 0; if (v > 32767) v = 32767; if (v < -32768) v = -32768; res[i] = (short)lrint(v); }
    free(out); free(nm); free(w); *outn = ol; return res;
}
/* read x at positions i*step (cubic Catmull-Rom); output length = n / step. step > 1 = higher pitch and shorter. */
static short *adsp_resample(const short *x, long n, double step, long *outn) {
    long ol = (long)(n / step); if (ol < 1) return NULL; short *r = malloc((size_t)ol * 2); if (!r) return NULL;
    for (long i = 0; i < ol; i++) { double t = i * step; long k = (long)t; double u = t - k; double p0 = x[k > 0 ? k - 1 : 0], p1 = x[k], p2 = x[k + 1 < n ? k + 1 : n - 1], p3 = x[k + 2 < n ? k + 2 : n - 1];
        double v = 0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (-p0 + 3 * p1 - 3 * p2 + p3) * u * u * u); if (v > 32767) v = 32767; if (v < -32768) v = -32768; r[i] = (short)lrint(v); }
    *outn = ol; return r;
}
/* pitch shift by `st` semitones keeping the duration: stretch by r = 2^(st/12) with WSOLA, then read back r times faster. NOTE: this moves the formants with the pitch (a "chipmunk"/"giant" voice); a
 * formant-preserving shift needs PSOLA or a phase vocoder (not built). Tempo change keeping pitch: adsp_wsola(x, n, sr, 1.0 / speed). */
static short *adsp_pitch_shift(const short *x, long n, int sr, double st, long *outn) {
    double r = pow(2.0, st / 12.0); if (fabs(st) < 1e-9) { short *c = malloc((size_t)n * 2); if (c) { memcpy(c, x, (size_t)n * 2); *outn = n; } return c; }
    long sl = 0; short *s = adsp_wsola(x, n, sr, r, &sl); if (!s) return NULL; short *o = adsp_resample(s, sl, r, outn); free(s); return o;
}
