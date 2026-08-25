/* synth - basic parameterized synth: renders MIDI-shaped note events to
 * WAV. Stage 3 of ROADMAP-models.txt §10.4's pipeline (note events ->
 * audio). One sine oscillator per note, shaped by an ADSR envelope,
 * mixed into the output buffer.
 *
 * The ADSR/waveform constants below are deliberately pulled out as named
 * values, not buried in the math, specifically so a future pass can load
 * them from a curriculum-shaped file instead of hardcoding - "different
 * weight-tuned synths" (see §10.4's RL connection) means these numbers
 * become learned/tunable parameters later, reinforced by the same
 * /rate-driven feedback mechanism §5 already designs for text. Not wired
 * to anything tunable yet - this file just renders one fixed voice
 * correctly, which has to work before tuning it means anything.
 *
 * wav_write() below is a copy of lab-audio/wav_io.c's implementation, per
 * this whole environment's no-shared-headers doctrine.
 *
 * Note-event CSV format (matches ROADMAP-models.txt §10.4's documented
 * shape): one line per note, no header row -
 *   pitch|velocity|start_tick|duration|channel
 *   pitch: MIDI note number (0-127, 69=A4=440Hz)
 *   velocity: 0-127 (loudness)
 *   start_tick/duration: in ticks - TICKS_PER_SECOND below defines the
 *     tick rate (simplest possible fixed-tempo mapping for v1; real tempo/
 *     time-signature handling is a later concern, not attempted here)
 *   channel: reserved, unused in v1 (every voice renders identically
 *     regardless of channel - per-channel timbre is a natural next step
 *     once this works)
 *
 * Usage:
 *   synth.+x <notes.csv> <out.wav>
 *   synth.+x gen_test <out.csv>
 *       Writes a short hardcoded test melody (a C-major arpeggio) in the
 *       CSV format above - lets the whole notes-to-WAV path be exercised
 *       before a real transcription/generation source produces one. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define SAMPLE_RATE 44100
#define TICKS_PER_SECOND 1000.0 /* 1 tick = 1ms - simplest fixed mapping for v1 */

/* ---- ADSR envelope + oscillator parameters - see this file's header
 * comment on why these are named constants, not inline magic numbers. ---- */
#define ADSR_ATTACK_SEC   0.02
#define ADSR_DECAY_SEC    0.05
#define ADSR_SUSTAIN_LEVEL 0.7   /* fraction of peak amplitude held during sustain */
#define ADSR_RELEASE_SEC 0.10

/* ---- WAV write (copy of wav_io.c's wav_write) ---- */
static void write_u32le(FILE *f, uint32_t v) {
    unsigned char b[4] = { (unsigned char)(v), (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    fwrite(b, 1, 4, f);
}
static void write_u16le(FILE *f, uint16_t v) {
    unsigned char b[2] = { (unsigned char)(v), (unsigned char)(v >> 8) };
    fwrite(b, 1, 2, f);
}
static int wav_write(const char *path, const int16_t *samples, uint32_t num_samples,
                      uint32_t sample_rate, uint16_t channels) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    uint16_t bits_per_sample = 16;
    uint16_t block_align = (uint16_t)(channels * (bits_per_sample / 8));
    uint32_t byte_rate = sample_rate * block_align;
    uint32_t data_bytes = num_samples * block_align;
    fwrite("RIFF", 1, 4, f); write_u32le(f, 36 + data_bytes); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); write_u32le(f, 16); write_u16le(f, 1); write_u16le(f, channels);
    write_u32le(f, sample_rate); write_u32le(f, byte_rate); write_u16le(f, block_align); write_u16le(f, bits_per_sample);
    fwrite("data", 1, 4, f); write_u32le(f, data_bytes);
    for (uint32_t i = 0; i < num_samples * channels; i++) write_u16le(f, (uint16_t)samples[i]);
    fclose(f);
    return 1;
}

typedef struct {
    int pitch;
    int velocity;
    double start_sec;
    double duration_sec;
    int channel;
} NoteEvent;

static double midi_to_hz(int pitch) {
    return 440.0 * pow(2.0, (pitch - 69) / 12.0);
}

/* Standard ADSR: ramps 0->1 over attack, 1->sustain_level over decay,
 * holds sustain_level until duration ends, then ramps sustain_level->0
 * over release. t is seconds since note start; duration is the note's
 * held length (release happens AFTER duration, extending the tail). */
static double adsr_envelope(double t, double duration) {
    if (t < 0.0) return 0.0;
    if (t < ADSR_ATTACK_SEC) {
        return t / ADSR_ATTACK_SEC;
    } else if (t < ADSR_ATTACK_SEC + ADSR_DECAY_SEC) {
        double decay_t = (t - ADSR_ATTACK_SEC) / ADSR_DECAY_SEC;
        return 1.0 - decay_t * (1.0 - ADSR_SUSTAIN_LEVEL);
    } else if (t < duration) {
        return ADSR_SUSTAIN_LEVEL;
    } else if (t < duration + ADSR_RELEASE_SEC) {
        double release_t = (t - duration) / ADSR_RELEASE_SEC;
        return ADSR_SUSTAIN_LEVEL * (1.0 - release_t);
    }
    return 0.0;
}

static int load_notes(const char *path, NoteEvent **notes_out) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    int capacity = 64, count = 0;
    NoteEvent *notes = malloc(sizeof(NoteEvent) * capacity);
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        int pitch, velocity, channel;
        double start_tick, duration_tick;
        if (sscanf(line, "%d|%d|%lf|%lf|%d", &pitch, &velocity, &start_tick, &duration_tick, &channel) != 5) {
            fprintf(stderr, "Warning: skipping unparseable line: %s\n", line);
            continue;
        }

        if (count >= capacity) {
            capacity *= 2;
            notes = realloc(notes, sizeof(NoteEvent) * capacity);
        }
        notes[count].pitch = pitch;
        notes[count].velocity = velocity;
        notes[count].start_sec = start_tick / TICKS_PER_SECOND;
        notes[count].duration_sec = duration_tick / TICKS_PER_SECOND;
        notes[count].channel = channel;
        count++;
    }
    fclose(f);
    *notes_out = notes;
    return count;
}

static void write_test_notes(const char *out_path) {
    /* A C-major arpeggio: C4, E4, G4, C5, each 400 ticks (0.4s) long,
     * back to back, then a final chord isn't attempted (v1 is
     * monophonic-in, but nothing stops multiple notes overlapping at
     * synth time - this test stays sequential for clarity). */
    FILE *f = fopen(out_path, "w");
    if (!f) { fprintf(stderr, "Error: could not write %s\n", out_path); return; }
    fprintf(f, "60|100|0|400|0\n");    /* C4 */
    fprintf(f, "64|100|400|400|0\n");  /* E4 */
    fprintf(f, "67|100|800|400|0\n");  /* G4 */
    fprintf(f, "72|100|1200|600|0\n"); /* C5, held slightly longer */
    fclose(f);
    printf("Wrote test melody (C major arpeggio) to %s\n", out_path);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <notes.csv> <out.wav>\n", argv[0]);
        printf("       %s gen_test <out.csv>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "gen_test") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: missing out.csv\n"); return 1; }
        write_test_notes(argv[2]);
        return 0;
    }

    if (argc < 3) { fprintf(stderr, "Error: missing out.wav\n"); return 1; }
    const char *notes_path = argv[1];
    const char *out_path = argv[2];

    NoteEvent *notes = NULL;
    int note_count = load_notes(notes_path, &notes);
    if (note_count < 0) {
        fprintf(stderr, "Error: could not read %s\n", notes_path);
        return 1;
    }
    if (note_count == 0) {
        fprintf(stderr, "Error: no valid note events found in %s\n", notes_path);
        free(notes);
        return 1;
    }

    double total_duration = 0.0;
    for (int i = 0; i < note_count; i++) {
        double end = notes[i].start_sec + notes[i].duration_sec + ADSR_RELEASE_SEC;
        if (end > total_duration) total_duration = end;
    }

    uint32_t num_samples = (uint32_t)(total_duration * SAMPLE_RATE) + 1;
    double *mix_buf = calloc(num_samples, sizeof(double));
    if (!mix_buf) { fprintf(stderr, "Error: malloc failed\n"); free(notes); return 1; }

    for (int i = 0; i < note_count; i++) {
        double hz = midi_to_hz(notes[i].pitch);
        double amp = (notes[i].velocity / 127.0);
        uint32_t start_sample = (uint32_t)(notes[i].start_sec * SAMPLE_RATE);
        uint32_t note_samples = (uint32_t)((notes[i].duration_sec + ADSR_RELEASE_SEC) * SAMPLE_RATE);

        for (uint32_t s = 0; s < note_samples && start_sample + s < num_samples; s++) {
            double t = (double)s / SAMPLE_RATE;
            double env = adsr_envelope(t, notes[i].duration_sec);
            double osc = sin(2.0 * M_PI * hz * t);
            mix_buf[start_sample + s] += amp * env * osc;
        }
    }

    /* Normalize to avoid clipping if multiple notes overlap and sum past
     * [-1, 1], then convert to 16-bit PCM. */
    double peak = 0.0;
    for (uint32_t i = 0; i < num_samples; i++) {
        if (fabs(mix_buf[i]) > peak) peak = fabs(mix_buf[i]);
    }
    double scale = (peak > 1.0) ? (0.95 / peak) : 0.95;

    int16_t *pcm = malloc(num_samples * sizeof(int16_t));
    for (uint32_t i = 0; i < num_samples; i++) {
        pcm[i] = (int16_t)(mix_buf[i] * scale * 32767.0);
    }

    if (wav_write(out_path, pcm, num_samples, SAMPLE_RATE, 1)) {
        printf("Rendered %d notes -> %s (%.2fs, %u samples)\n", note_count, out_path, total_duration, num_samples);
    } else {
        fprintf(stderr, "Error: could not write %s\n", out_path);
    }

    free(pcm);
    free(mix_buf);
    free(notes);
    return 0;
}
