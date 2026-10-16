/* audio_probe - measure a WAV: audio_probe <in.wav> [start_ms=0] [window=2048]   (window = FFT size, a power of two)
 * Prints key=value lines: seconds, sample_rate, peak, rms, dc, dominant_hz (of the window starting at start_ms; -1 = silent). Pure measurement, no network, deterministic.
 * Exit 0 ok, 1 cannot read, 2 usage. Text-includes ../audio_dsp.c. Tests (2026-10-14): sine 110/440/1000/3000 Hz and square 300 Hz within 0.5%; DC 0.3 read as 0.3000; near-silence -> -1; bad file exit 1; bad window exit 2. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../audio_dsp.c"
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: audio_probe in.wav [start_ms] [window]\n"); return 2; }
    short *s; long n; int sr; if (adsp_read_wav(argv[1], &s, &n, &sr)) { fprintf(stderr, "cannot read %s as 16-bit PCM wav\n", argv[1]); return 1; }
    int win = argc > 3 ? atoi(argv[3]) : 2048; if (win < 64 || win > 65536 || (win & (win - 1))) { fprintf(stderr, "window must be a power of two, 64..65536\n"); return 2; }
    long start = argc > 2 ? (long)(atof(argv[2]) * sr / 1000.0) : 0;
    printf("seconds=%.4f\nsample_rate=%d\npeak=%.4f\nrms=%.4f\ndc=%.4f\ndominant_hz=%.2f\n", (double)n / sr, sr, adsp_peak(s, n), adsp_rms(s, n), adsp_dc(s, n), adsp_dominant_hz(s, n, sr, start, win));
    free(s); return 0;
}
