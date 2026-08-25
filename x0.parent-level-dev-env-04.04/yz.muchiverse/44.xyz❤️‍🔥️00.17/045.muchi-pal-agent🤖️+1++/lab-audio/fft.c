/* fft - real radix-2 Cooley-Tukey FFT, doing windowed STFT pitch
 * detection on a WAV file. Stage 1 of ROADMAP-models.txt §10.4's
 * audio-to-MIDI-events pipeline: audio -> per-window dominant pitch.
 * Deliberately does NOT do onset/duration assembly into discrete notes
 * yet (turning a stream of per-window pitch guesses into properly
 * segmented notes - onset detection, run-length grouping - is a real
 * algorithm that deserves its own pass and its own test, not rushed in
 * here). Monophonic only (one dominant pitch per window) - real
 * polyphonic transcription is a much harder problem, out of scope.
 *
 * wav_read() below is a copy of lab-audio/wav_io.c's implementation, per
 * this whole environment's no-shared-headers doctrine - every op that
 * needs WAV reading carries its own copy rather than including a header.
 *
 * Usage: fft.+x <in.wav> <window_size> <hop_size> <out.csv>
 *   window_size must be a power of 2 (2048 is a reasonable default -
 *   ~46ms at 44100 Hz, a common STFT window size for pitch detection).
 *   hop_size is how far the window advances each step (e.g. window_size/2
 *   for 50% overlap).
 * Output CSV columns: window_index,dominant_hz,midi_note,magnitude */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ---- WAV read (copy of wav_io.c's wav_read - see this file's header
 * comment for why it's duplicated, not shared) ---- */
static uint32_t read_u32le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t read_u16le(const unsigned char *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static int wav_read(const char *path, int16_t **samples_out, uint32_t *num_samples_out,
                     uint32_t *sample_rate_out, uint16_t *channels_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char header[12];
    if (fread(header, 1, 12, f) != 12) { fclose(f); return 0; }
    if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) { fclose(f); return 0; }
    uint32_t sample_rate = 0, data_bytes = 0;
    uint16_t channels = 0, bits_per_sample = 0, audio_format = 0;
    long data_offset = -1;
    for (;;) {
        unsigned char chunk_hdr[8];
        if (fread(chunk_hdr, 1, 8, f) != 8) break;
        uint32_t chunk_size = read_u32le(chunk_hdr + 4);
        if (memcmp(chunk_hdr, "fmt ", 4) == 0) {
            unsigned char fmt_body[16];
            if (chunk_size < 16 || fread(fmt_body, 1, 16, f) != 16) { fclose(f); return 0; }
            audio_format = read_u16le(fmt_body + 0);
            channels = read_u16le(fmt_body + 2);
            sample_rate = read_u32le(fmt_body + 4);
            bits_per_sample = read_u16le(fmt_body + 14);
            if (chunk_size > 16) fseek(f, chunk_size - 16, SEEK_CUR);
        } else if (memcmp(chunk_hdr, "data", 4) == 0) {
            data_bytes = chunk_size;
            data_offset = ftell(f);
            fseek(f, chunk_size, SEEK_CUR);
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
        if (chunk_size % 2 == 1) fseek(f, 1, SEEK_CUR);
    }
    if (audio_format != 1 || bits_per_sample != 16 || data_offset < 0 || channels == 0) { fclose(f); return 0; }
    uint32_t num_samples = data_bytes / 2 / channels;
    int16_t *samples = malloc((size_t)num_samples * channels * sizeof(int16_t));
    if (!samples) { fclose(f); return 0; }
    fseek(f, data_offset, SEEK_SET);
    unsigned char *raw = malloc(data_bytes);
    if (!raw || fread(raw, 1, data_bytes, f) != data_bytes) { free(raw); free(samples); fclose(f); return 0; }
    for (uint32_t i = 0; i < num_samples * channels; i++) samples[i] = (int16_t)read_u16le(raw + i * 2);
    free(raw);
    fclose(f);
    *samples_out = samples; *num_samples_out = num_samples; *sample_rate_out = sample_rate; *channels_out = channels;
    return 1;
}

/* ---- complex arithmetic + radix-2 Cooley-Tukey FFT ---- */
typedef struct { double re, im; } Complex;

static int is_power_of_two(int n) { return n > 0 && (n & (n - 1)) == 0; }

/* In-place iterative radix-2 FFT (Cooley-Tukey, decimation in time).
 * n must be a power of 2 - a standard, well-known algorithm, not a
 * simplification: bit-reversal permutation, then log2(n) butterfly
 * stages. */
static void fft_inplace(Complex *x, int n) {
    /* Bit-reversal permutation */
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { Complex tmp = x[i]; x[i] = x[j]; x[j] = tmp; }
    }
    /* Butterfly stages */
    for (int len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * M_PI / (double)len;
        Complex wlen = { cos(ang), sin(ang) };
        for (int i = 0; i < n; i += len) {
            Complex w = { 1.0, 0.0 };
            for (int k = 0; k < len / 2; k++) {
                Complex u = x[i + k];
                Complex v = { x[i + k + len / 2].re * w.re - x[i + k + len / 2].im * w.im,
                              x[i + k + len / 2].re * w.im + x[i + k + len / 2].im * w.re };
                x[i + k].re = u.re + v.re;
                x[i + k].im = u.im + v.im;
                x[i + k + len / 2].re = u.re - v.re;
                x[i + k + len / 2].im = u.im - v.im;
                double new_w_re = w.re * wlen.re - w.im * wlen.im;
                double new_w_im = w.re * wlen.im + w.im * wlen.re;
                w.re = new_w_re; w.im = new_w_im;
            }
        }
    }
}

static double hz_to_midi_note(double hz) {
    if (hz <= 0.0) return -1.0;
    return 69.0 + 12.0 * log2(hz / 440.0);
}

/* Musically-relevant frequency range for peak-picking - roughly C2 (~65Hz)
 * to C7 (~2093Hz), wide enough for most monophonic melodic content
 * without picking up sub-bass rumble or ultrasonic noise as "the pitch". */
#define MIN_HZ 65.0
#define MAX_HZ 2100.0

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <in.wav> <window_size> <hop_size> <out.csv>\n", argv[0]);
        return 1;
    }
    const char *in_path = argv[1];
    int window_size = atoi(argv[2]);
    int hop_size = atoi(argv[3]);
    const char *out_path = argv[4];

    if (!is_power_of_two(window_size)) {
        fprintf(stderr, "Error: window_size must be a power of 2 (got %d)\n", window_size);
        return 1;
    }
    if (hop_size <= 0) {
        fprintf(stderr, "Error: hop_size must be positive\n");
        return 1;
    }

    int16_t *samples = NULL;
    uint32_t num_samples = 0, sample_rate = 0;
    uint16_t channels = 0;
    if (!wav_read(in_path, &samples, &num_samples, &sample_rate, &channels)) {
        fprintf(stderr, "Error: could not read %s (not 16-bit PCM WAV?)\n", in_path);
        return 1;
    }
    if (channels != 1) {
        fprintf(stderr, "Warning: %d channels found, only channel 0 is analyzed (mono assumed for v1)\n", channels);
    }

    Complex *buf = malloc(sizeof(Complex) * window_size);
    if (!buf) { fprintf(stderr, "Error: malloc failed\n"); free(samples); return 1; }

    FILE *out = fopen(out_path, "w");
    if (!out) { fprintf(stderr, "Error: could not open %s for writing\n", out_path); free(buf); free(samples); return 1; }
    fprintf(out, "window_index,dominant_hz,midi_note,magnitude\n");

    int window_index = 0;
    for (uint32_t start = 0; start + (uint32_t)window_size <= num_samples; start += (uint32_t)hop_size) {
        /* Hann window, then load into the complex buffer (imaginary part 0) */
        for (int i = 0; i < window_size; i++) {
            double sample = samples[(start + (uint32_t)i) * channels] / 32768.0;
            double hann = 0.5 * (1.0 - cos(2.0 * M_PI * i / (window_size - 1)));
            buf[i].re = sample * hann;
            buf[i].im = 0.0;
        }

        fft_inplace(buf, window_size);

        /* Peak-pick the dominant bin within [MIN_HZ, MAX_HZ], using only
         * the first half of the spectrum (real-input FFT is symmetric,
         * the second half is redundant). */
        int min_bin = (int)(MIN_HZ * window_size / sample_rate);
        int max_bin = (int)(MAX_HZ * window_size / sample_rate);
        if (max_bin >= window_size / 2) max_bin = window_size / 2 - 1;
        if (min_bin < 1) min_bin = 1; /* skip DC bin */

        int peak_bin = min_bin;
        double peak_mag = 0.0;
        for (int bin = min_bin; bin <= max_bin; bin++) {
            double mag = sqrt(buf[bin].re * buf[bin].re + buf[bin].im * buf[bin].im);
            if (mag > peak_mag) { peak_mag = mag; peak_bin = bin; }
        }

        double dominant_hz = (double)peak_bin * sample_rate / window_size;
        double midi_note = hz_to_midi_note(dominant_hz);
        fprintf(out, "%d,%.2f,%.2f,%.4f\n", window_index, dominant_hz, midi_note, peak_mag);
        window_index++;
    }

    fclose(out);
    free(buf);
    free(samples);
    printf("Wrote %d windows to %s\n", window_index, out_path);
    return 0;
}
