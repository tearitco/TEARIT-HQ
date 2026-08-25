/* wav_io - reference WAV read/write implementation, mono or multi-channel
 * 16-bit PCM. No library needed - a WAV file is just a small RIFF header
 * plus raw samples, per ROADMAP-models.txt §10.4's format guidance (WAV
 * is the only format anything in this pipeline directly synthesizes into
 * - MP3 is export-only, via an external tool, never hand-rolled here).
 *
 * This is a REFERENCE implementation to copy from, not a shared library -
 * per this whole environment's no-shared-headers doctrine (see
 * cdda-tpm-std-fast.txt / mutaclsym's dox), any future op that needs to
 * read or write WAV should copy wav_read()/wav_write() into its own
 * source file, the same way resolve_root() is duplicated into every op
 * across this whole project tree, not #include this file.
 *
 * Usage:
 *   wav_io.+x info <file.wav>
 *       Prints sample_rate, channels, num_samples, duration_sec - proves
 *       the read path against a real file.
 *   wav_io.+x gen_test <out.wav> <freq_hz> <duration_sec>
 *       Writes a mono 16-bit PCM sine wave at the given frequency/duration
 *       - a known-ground-truth test file for fft.c's pitch detection to
 *       be checked against (if fft.c detects anything other than
 *       ~freq_hz on a file generated this way, something is wrong). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define SAMPLE_RATE_DEFAULT 44100

/* ---- little-endian raw I/O helpers (WAV fields are all little-endian,
 * regardless of host byte order - write explicitly rather than trusting
 * struct layout/packing, which is not portable). ---- */
static void write_u32le(FILE *f, uint32_t v) {
    unsigned char b[4] = { (unsigned char)(v), (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    fwrite(b, 1, 4, f);
}
static void write_u16le(FILE *f, uint16_t v) {
    unsigned char b[2] = { (unsigned char)(v), (unsigned char)(v >> 8) };
    fwrite(b, 1, 2, f);
}
static uint32_t read_u32le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t read_u16le(const unsigned char *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

/* Writes a 16-bit PCM WAV file. Returns 1 on success, 0 on failure. */
static int wav_write(const char *path, const int16_t *samples, uint32_t num_samples,
                      uint32_t sample_rate, uint16_t channels) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    uint16_t bits_per_sample = 16;
    uint16_t block_align = (uint16_t)(channels * (bits_per_sample / 8));
    uint32_t byte_rate = sample_rate * block_align;
    uint32_t data_bytes = num_samples * block_align;

    fwrite("RIFF", 1, 4, f);
    write_u32le(f, 36 + data_bytes);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    write_u32le(f, 16);          /* PCM fmt chunk size */
    write_u16le(f, 1);           /* audio format = PCM */
    write_u16le(f, channels);
    write_u32le(f, sample_rate);
    write_u32le(f, byte_rate);
    write_u16le(f, block_align);
    write_u16le(f, bits_per_sample);

    fwrite("data", 1, 4, f);
    write_u32le(f, data_bytes);
    for (uint32_t i = 0; i < num_samples * channels; i++) {
        write_u16le(f, (uint16_t)samples[i]);
    }

    fclose(f);
    return 1;
}

/* Reads a 16-bit PCM WAV file. Allocates *samples_out (caller frees).
 * Returns 1 on success, 0 on failure (including non-PCM/non-16-bit files
 * - this reference implementation deliberately doesn't handle every WAV
 * variant, just the one wav_write() above produces). */
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
        if (chunk_size % 2 == 1) fseek(f, 1, SEEK_CUR); /* chunks are word-aligned */
    }

    if (audio_format != 1 || bits_per_sample != 16 || data_offset < 0 || channels == 0) {
        fclose(f);
        return 0;
    }

    uint32_t num_samples = data_bytes / 2 / channels;
    int16_t *samples = malloc((size_t)num_samples * channels * sizeof(int16_t));
    if (!samples) { fclose(f); return 0; }

    fseek(f, data_offset, SEEK_SET);
    unsigned char *raw = malloc(data_bytes);
    if (!raw || fread(raw, 1, data_bytes, f) != data_bytes) { free(raw); free(samples); fclose(f); return 0; }
    for (uint32_t i = 0; i < num_samples * channels; i++) {
        samples[i] = (int16_t)read_u16le(raw + i * 2);
    }
    free(raw);
    fclose(f);

    *samples_out = samples;
    *num_samples_out = num_samples;
    *sample_rate_out = sample_rate;
    *channels_out = channels;
    return 1;
}

static void generate_test_tone(const char *out_path, double freq_hz, double duration_sec) {
    uint32_t sample_rate = SAMPLE_RATE_DEFAULT;
    uint32_t num_samples = (uint32_t)(duration_sec * sample_rate);
    int16_t *samples = malloc(num_samples * sizeof(int16_t));
    if (!samples) { fprintf(stderr, "Error: malloc failed\n"); return; }

    double amplitude = 0.5 * 32767.0; /* leave headroom, avoid clipping */
    for (uint32_t i = 0; i < num_samples; i++) {
        double t = (double)i / (double)sample_rate;
        samples[i] = (int16_t)(amplitude * sin(2.0 * M_PI * freq_hz * t));
    }

    if (wav_write(out_path, samples, num_samples, sample_rate, 1)) {
        printf("Wrote %s: %.1f Hz sine, %.2fs, %u samples @ %u Hz\n", out_path, freq_hz, duration_sec, num_samples, sample_rate);
    } else {
        fprintf(stderr, "Error: could not write %s\n", out_path);
    }
    free(samples);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s info <file.wav>\n", argv[0]);
        printf("       %s gen_test <out.wav> <freq_hz> <duration_sec>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: missing file.wav\n"); return 1; }
        int16_t *samples = NULL;
        uint32_t num_samples = 0, sample_rate = 0;
        uint16_t channels = 0;
        if (!wav_read(argv[2], &samples, &num_samples, &sample_rate, &channels)) {
            fprintf(stderr, "Error: could not read %s (not 16-bit PCM WAV?)\n", argv[2]);
            return 1;
        }
        printf("sample_rate=%u\n", sample_rate);
        printf("channels=%u\n", channels);
        printf("num_samples=%u\n", num_samples);
        printf("duration_sec=%.3f\n", (double)num_samples / (double)sample_rate);
        free(samples);
        return 0;
    } else if (strcmp(argv[1], "gen_test") == 0) {
        if (argc < 5) { fprintf(stderr, "Error: missing out.wav/freq_hz/duration_sec\n"); return 1; }
        generate_test_tone(argv[2], atof(argv[3]), atof(argv[4]));
        return 0;
    }

    fprintf(stderr, "Error: unknown command '%s'\n", argv[1]);
    return 1;
}
