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
/* 8-tap Blackman-windowed sinc interpolation at fractional position t of x[0..n): flat to about 0.8 of Nyquist (cubic droops earlier and was eating the voice's high frequencies). */
static double adsp_interp8(const short *x, long n, double t) {
    long k = (long)floor(t); double u = t - k, acc = 0, wsum = 0; for (int j = -3; j <= 4; j++) { double d = u - j, sinc = fabs(d) < 1e-9 ? 1.0 : sin(M_PI * d) / (M_PI * d), wn = 0.42 + 0.5 * cos(M_PI * d / 4.0) + 0.08 * cos(2 * M_PI * d / 4.0), w = sinc * wn; long i = k + j; if (i < 0) i = 0; if (i >= n) i = n - 1; acc += x[i] * w; wsum += w; }
    return wsum != 0 ? acc / wsum : 0;
}
/* read x at positions i*step (8-tap windowed sinc, was cubic); output length = n / step. step > 1 = higher pitch and shorter. */
static short *adsp_resample(const short *x, long n, double step, long *outn) {
    long ol = (long)(n / step); if (ol < 1) return NULL; short *r = malloc((size_t)ol * 2); if (!r) return NULL;
    for (long i = 0; i < ol; i++) { double v = adsp_interp8(x, n, i * step); if (v > 32767) v = 32767; if (v < -32768) v = -32768; r[i] = (short)lrint(v); }
    *outn = ol; return r;
}
/* pitch shift by `st` semitones keeping the duration: stretch by r = 2^(st/12) with WSOLA, then read back r times faster. NOTE: this moves the formants with the pitch (a "chipmunk"/"giant" voice); a
 * formant-preserving shift needs PSOLA or a phase vocoder (not built). Tempo change keeping pitch: adsp_wsola(x, n, sr, 1.0 / speed). */
static short *adsp_pitch_shift(const short *x, long n, int sr, double st, long *outn) {
    double r = pow(2.0, st / 12.0); if (fabs(st) < 1e-9) { short *c = malloc((size_t)n * 2); if (c) { memcpy(c, x, (size_t)n * 2); *outn = n; } return c; }
    long sl = 0; short *s = adsp_wsola(x, n, sr, r, &sl); if (!s) return NULL; short *o = adsp_resample(s, sl, r, outn); free(s); return o;
}

/* ---- voice character effects (added 2026-10-14; BOOK 19.audio/VOICE-FX.md) ---- */
static void adsp_ifft(AdspCx *x, int n) { for (int i = 0; i < n; i++) x[i].im = -x[i].im; adsp_fft(x, n); for (int i = 0; i < n; i++) { x[i].re /= n; x[i].im = -x[i].im / n; } }
/* Formant shift that keeps pitch and duration: STFT (Hann, N=1024, hop N/4); per frame the spectral envelope is estimated by cepstral smoothing (keep quefrencies below one pitch period of a 450 Hz voice),
 * the envelope is re-sampled along frequency by `ratio` (>1 moves the resonances up = a smaller vocal tract), the frame's own phases and fine structure (the harmonics) are kept, and frames are overlap-added.
 * ratio 0.6..1.7 is sensible; quality falls off at the extremes. In-place on a copy; returns malloc'd buffer of length n (caller frees) or NULL. */
static short *adsp_formant_shift(const short *x, long n, int sr, double ratio, long *outn) {
    const int N = 1024, H = N / 4, K = N / 2; int Q = (int)(sr / 450.0); if (Q < 8) Q = 8; if (Q > N / 4) Q = N / 4; if (n < N) return NULL;
    double *w = malloc(N * sizeof *w), *ola = calloc((size_t)n + 2 * N, sizeof *ola), *nm = calloc((size_t)n + 2 * N, sizeof *nm), *env = malloc((K + 1) * sizeof *env), *mag = malloc((K + 1) * sizeof *mag); AdspCx *X = malloc(N * sizeof *X), *C = malloc(N * sizeof *C); short *res = malloc((size_t)n * 2);
    if (!w || !ola || !nm || !env || !mag || !X || !C || !res) { free(w); free(ola); free(nm); free(env); free(mag); free(X); free(C); free(res); return NULL; }
    for (int i = 0; i < N; i++) w[i] = 0.5 - 0.5 * cos(2.0 * M_PI * (i + 0.5) / N);
    for (long s = -(N - H); s < n; s += H) {
        for (int i = 0; i < N; i++) { long k = s + i; X[i].re = (k >= 0 && k < n) ? x[k] / 32768.0 * w[i] : 0; X[i].im = 0; } adsp_fft(X, N);
        for (int k = 0; k <= K; k++) { mag[k] = sqrt(X[k].re * X[k].re + X[k].im * X[k].im) + 1e-9; C[k].re = log(mag[k]); C[k].im = 0; } for (int k = K + 1; k < N; k++) { C[k].re = C[N - k].re; C[k].im = 0; }
        adsp_ifft(C, N); for (int q = Q + 1; q < N - Q; q++) { C[q].re = 0; C[q].im = 0; } adsp_fft(C, N); for (int k = 0; k <= K; k++) env[k] = exp(C[k].re);
        for (int k = 0; k <= K; k++) { double src = k / ratio; if (src > K) src = K; int i0 = (int)src; int i1 = i0 + 1 > K ? K : i0 + 1; double fr = src - i0, e2 = env[i0] * (1 - fr) + env[i1] * fr, g = e2 / env[k]; if (g > 8) g = 8; if (g < 0.125) g = 0.125; X[k].re *= g; X[k].im *= g; }
        X[0].im = 0; X[K].im = 0; for (int k = K + 1; k < N; k++) { X[k].re = X[N - k].re; X[k].im = -X[N - k].im; } adsp_ifft(X, N);
        for (int i = 0; i < N; i++) { long k = s + i; if (k >= 0 && k < n) { ola[k] += X[i].re * w[i]; nm[k] += w[i] * w[i]; } } }
    for (long i = 0; i < n; i++) { double v = nm[i] > 1e-6 ? ola[i] / nm[i] * 32768.0 : 0; if (v > 32767) v = 32767; if (v < -32768) v = -32768; res[i] = (short)lrint(v); }
    free(w); free(ola); free(nm); free(env); free(mag); free(X); free(C); *outn = n; return res;
}
/* RBJ shelving filter in place: kind 0 = low shelf, 1 = high shelf; f0 = corner Hz; gain_db +- . */
static void adsp_shelf(short *x, long n, int sr, int kind, double f0, double gain_db) {
    if (fabs(gain_db) < 1e-6 || n < 2) return; double A = pow(10.0, gain_db / 40.0), w0 = 2 * M_PI * f0 / sr, cw = cos(w0), sw = sin(w0), al = sw / 2 * sqrt(2.0), sq = 2 * sqrt(A) * al, b0, b1, b2, a0, a1, a2;
    if (kind == 0) { b0 = A * ((A + 1) - (A - 1) * cw + sq); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - sq); a0 = (A + 1) + (A - 1) * cw + sq; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - sq; }
    else { b0 = A * ((A + 1) + (A - 1) * cw + sq); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - sq); a0 = (A + 1) - (A - 1) * cw + sq; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - sq; }
    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0; double z1 = 0, z2 = 0; for (long i = 0; i < n; i++) { double v = x[i], y = b0 * v + z1; z1 = b1 * v - a1 * y + z2; z2 = b2 * v - a2 * y; if (y > 32767) y = 32767; if (y < -32768) y = -32768; x[i] = (short)lrint(y); }
}
/* vibrato: a delay that swings sinusoidally so the pitch wobbles by +-`cents` at `hz` (delay amplitude = (2^(cents/1200)-1) / (2 pi hz)); linear interpolation. In place. */
static void adsp_vibrato(short *x, long n, int sr, double hz, double cents) {
    if (hz <= 0 || cents <= 0 || n < 2) return; double amp = (pow(2.0, cents / 1200.0) - 1.0) / (2 * M_PI * hz) * sr, base = amp + 1; short *c = malloc((size_t)n * 2); if (!c) return; memcpy(c, x, (size_t)n * 2);
    for (long i = 0; i < n; i++) { double t = i - (base + amp * sin(2 * M_PI * hz * i / sr)); if (t < 0) { x[i] = 0; continue; } double v = adsp_interp8(c, n, t); if (v > 32767) v = 32767; if (v < -32768) v = -32768; x[i] = (short)lrint(v); } free(c);
}
/* breath: noise (high-passed by differencing) mixed in proportion to the voice's own short-term level, so it is only there while speaking. mix 0..0.3. Deterministic. In place. */
static void adsp_breath(short *x, long n, int sr, double mix) {
    if (mix <= 0 || n < 2) return; uint32_t rs = 2463534242u; double env = 0, prev = 0, a = exp(-1.0 / (0.01 * sr)); for (long i = 0; i < n; i++) { env = a * env + (1 - a) * fabs((double)x[i]); rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; double nz = (double)rs / 2147483648.0 - 1.0, hp = nz - prev; prev = nz; double y = x[i] + mix * env * hp * 1.2; if (y > 32767) y = 32767; if (y < -32768) y = -32768; x[i] = (short)lrint(y); }
}

/* ---- speech detection (added 2026-10-14; BOOK 19.audio/LISTENER-STT-DESIGN.md) ---- */
typedef struct { long start, end; } AdspSeg;
static double adsp_frame_db(const short *x, long n) { double a = 0; for (long i = 0; i < n; i++) a += (double)x[i] * x[i]; return 20.0 * log10(sqrt(n ? a / n : 0) / 32768.0 + 1e-9); }
/* Energy VAD over a whole buffer: 20 ms frames in dBFS; noise floor = 10th percentile of the frame levels; threshold = max(floor + 9 dB, -50 dBFS); speech starts after 5 frames (100 ms) above it and ends
 * after `hang_ms` of frames below it; a segment shorter than `min_ms` is dropped; each kept segment is padded by 100 ms. Returns the number of segments written (up to `max`).
 * This detects LOUDNESS, not speech: a steady hum or hiss sets the floor and is ignored, but a loud non-speech burst is a segment. */
static int adsp_vad(const short *x, long n, int sr, int hang_ms, int min_ms, AdspSeg *out, int max) {
    long fl = sr / 50; long nf = n / fl; if (nf < 5) return 0; double *db = malloc((size_t)nf * sizeof *db), *srt = malloc((size_t)nf * sizeof *srt); if (!db || !srt) { free(db); free(srt); return 0; }
    for (long i = 0; i < nf; i++) { db[i] = adsp_frame_db(x + i * fl, fl); srt[i] = db[i]; }
    for (long i = 1; i < nf; i++) { double v = srt[i]; long j = i - 1; while (j >= 0 && srt[j] > v) { srt[j + 1] = srt[j]; j--; } srt[j + 1] = v; }      /* insertion sort: nf is small (50 per second) */
    double floor_db = srt[nf / 10], thr = floor_db + 9.0; if (thr < -50.0) thr = -50.0; free(srt);
    int cnt = 0; long hang = hang_ms / 20, run = 0, quiet = 0, start = -1, last_loud = -1; int in = 0;
    for (long i = 0; i <= nf; i++) { int loud = i < nf && db[i] > thr;
        if (!in) { if (loud) { run++; if (run >= 5) { in = 1; start = i - run + 1; last_loud = i; quiet = 0; } } else run = 0; }
        else { if (loud) { last_loud = i; quiet = 0; } else { quiet++; } if (quiet >= hang || i == nf) { long s = start * fl - sr / 10, e = (last_loud + 1) * fl + sr / 10; if (s < 0) s = 0; if (e > n) e = n; if ((e - s) * 1000 / sr >= min_ms && cnt < max) { out[cnt].start = s; out[cnt].end = e; cnt++; } in = 0; run = 0; quiet = 0; } } }
    free(db); return cnt;
}
