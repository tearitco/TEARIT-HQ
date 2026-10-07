/* music_wav_audit_op - independent WAV checker for the music_daemon harness (it does NOT share code with music_synth, so it can disagree with it).
 * Usage: music_wav_audit_op <file.wav> [expect_hz tol_hz]
 * Prints one line of key=value words: header_ok=1|0 (RIFF/WAVE/fmt PCM mono 16-bit, chunk sizes match the real file size) rate= samples= secs= peak= clipped= (|s|>=32767)
 * nonzero= freq_hz= (rising zero crossings: (n-1)/(t_last-t_first)) and, when expect_hz is given, pitch_ok=1|0 (|freq-expect|<=tol). Exit 0 always for a readable file, 1 if unreadable.
 * A harness cannot judge how the music SOUNDS; this only measures. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: music_wav_audit_op file.wav [expect_hz tol_hz]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { printf("header_ok=0 unreadable=1\n"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(sz > 0 ? sz : 1); size_t got = fread(b, 1, sz, f); fclose(f);
    if (sz < 44 || got != (size_t)sz) { printf("header_ok=0 short=1\n"); return 0; }
    uint32_t riff = b[4] | b[5] << 8 | b[6] << 16 | (uint32_t)b[7] << 24, fmtlen = b[16] | b[17] << 8, rate = b[24] | b[25] << 8 | b[26] << 16 | (uint32_t)b[27] << 24;
    uint32_t dlen = b[40] | b[41] << 8 | b[42] << 16 | (uint32_t)b[43] << 24; int fmt = b[20] | b[21] << 8, ch = b[22] | b[23] << 8, bits = b[34] | b[35] << 8;
    int ok = !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WAVEfmt ", 8) && !memcmp(b + 36, "data", 4) && fmtlen == 16 && fmt == 1 && ch == 1 && bits == 16 && rate >= 8000
             && riff == (uint32_t)sz - 8 && dlen == (uint32_t)sz - 44;
    long n = (sz - 44) / 2, nonzero = 0, clipped = 0, ncross = 0, first = -1, last = -1; int peak = 0, prev = 0;
    for (long i = 0; i < n; i++) {
        int v = (int16_t)(b[44 + 2 * i] | b[45 + 2 * i] << 8), a = abs(v);
        if (a > peak) peak = a; if (a >= 32767) clipped++; if (v) nonzero++;
        if (i > 0 && prev < 0 && v >= 0) { ncross++; if (first < 0) first = i; last = i; }
        prev = v;
    }
    double freq = (ncross >= 2 && last > first) ? (double)(ncross - 1) * rate / (double)(last - first) : 0.0;
    printf("header_ok=%d rate=%u samples=%ld secs=%.3f peak=%d clipped=%ld nonzero=%ld freq_hz=%.1f", ok, rate, n, rate ? (double)n / rate : 0.0, peak, clipped, nonzero, freq);
    if (argc >= 4) printf(" pitch_ok=%d", fabs(freq - atof(argv[2])) <= atof(argv[3]));
    printf("\n"); free(b); return 0;
}
