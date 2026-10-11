/* music_synth - renders a music_seq pattern file to a 16-bit mono PCM WAV (MUSIC-DAEMON v0, EDEN-PLAYABLE-LOOP plan section 4).
 *
 * Usage: music_synth [--data DIR] [--volume 0..1] [--rate HZ] [-o OUT.wav] PATTERN|-        (WAV to stdout when no -o; "-" reads the pattern from stdin)
 * Pattern = the Muchi DAW sequence format (`NOTE track= pitch= start= len= vel=` in PPQN=480 ticks, `tempo=`, `bars=`, `beats=`, `trackN name= inst=`).
 * Instruments come from DIR/instruments.pdl (INST | name | role | wave | A | D | S | R | lowpass_hz | gain); an unknown/missing instrument is a plain sine (Heal).
 * Length is EXACTLY bars*beats*60/tempo seconds (notes and release tails are cut at the end, last 5 ms fade out), so a harness can check length = tempo*bars.
 * Mix: every voice summed in float, soft-limited with tanh*0.9 (peak < 0.9 FS, never hard-clips), then * volume; volume 0 is true silence.
 * No clock, no random seed from the environment: noise is a per-note LCG, so the same pattern renders to identical bytes on the same machine.
 *
 * STOLEN FROM (ideas, copied inline, no header/link sharing): 103.media-studio/103.daw/ops/daw_main.c - `midi_hz` (440*2^((p-69)/12)), the NOTE line format and
 * PPQN=480, the tanh master soft-clip and the idea of per-voice envelope+gain; the DAW's synth is a single sine with a crude release, so oscillators other than sine,
 * a true ADSR, the one-pole low-pass and the noise drums are NEW here. Self-contained: libc + libm only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>

#define PPQN 480
#define MAXNOTE 16384
#define MAXINST 32
#define MAXTRK 32
typedef struct { char name[24]; char wave[12]; float a, d, s, r, lp, gain; } Inst;
typedef struct { int track, pitch, start, len, vel; } Note;
static Inst g_inst[MAXINST]; static int g_ninst;
static char g_trk_inst[MAXTRK][24]; static int g_ntrk;
static Note *g_notes; static int g_nnotes;
static int g_tempo = 90, g_bars = 4, g_beats = 4;

static char *trim(char *s) { char *e; while (*s == ' ' || *s == '\t') s++; e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0; return s; }
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static float midi_hz(int p) { return 440.0f * powf(2.0f, (p - 69) / 12.0f); }   /* from daw_main.c */

static void load_instruments(const char *dir) {
    char path[4200], line[512], *f[12]; snprintf(path, sizeof path, "%s/instruments.pdl", dir);
    FILE *fp = fopen(path, "r"); if (!fp) return;
    while (fgets(line, sizeof line, fp) && g_ninst < MAXINST) {
        char *l = trim(line); if (*l == '#' || !*l) continue;
        if (split(l, f, 12) >= 10 && !strcmp(f[0], "INST")) {
            Inst *in = &g_inst[g_ninst++]; snprintf(in->name, sizeof in->name, "%s", f[1]); snprintf(in->wave, sizeof in->wave, "%s", f[3]);
            in->a = (float)atof(f[4]); in->d = (float)atof(f[5]); in->s = (float)atof(f[6]); in->r = (float)atof(f[7]); in->lp = (float)atof(f[8]); in->gain = (float)atof(f[9]);
            if (in->a < 0.0005f) in->a = 0.0005f; if (in->d < 0.0005f) in->d = 0.0005f; if (in->r < 0.001f) in->r = 0.001f;
            if (in->s < 0) in->s = 0; if (in->s > 1) in->s = 1; if (in->gain < 0 || in->gain > 4) in->gain = 0.5f;
        }
    }
    fclose(fp);
}
static const Inst *find_inst(const char *name) {
    static const Inst fallback = {"fallback", "sine", 0.01f, 0.1f, 0.7f, 0.1f, 0, 0.4f};
    for (int i = 0; i < g_ninst; i++) if (!strcmp(g_inst[i].name, name)) return &g_inst[i];
    return &fallback;
}
static int load_pattern(FILE *fp) {
    char line[512];
    g_notes = malloc(sizeof(Note) * MAXNOTE); if (!g_notes) return 0;
    while (fgets(line, sizeof line, fp)) {
        int t, p, s, l, v;
        if (!strncmp(line, "tempo=", 6)) g_tempo = atoi(line + 6);
        else if (!strncmp(line, "bars=", 5)) g_bars = atoi(line + 5);
        else if (!strncmp(line, "beats=", 6)) g_beats = atoi(line + 6);
        else if (sscanf(line, "NOTE track=%d pitch=%d start=%d len=%d vel=%d", &t, &p, &s, &l, &v) == 5 && g_nnotes < MAXNOTE) {
            if (t < 0 || t >= MAXTRK || s < 0 || l <= 0) continue;
            g_notes[g_nnotes++] = (Note){t, p, s, l, v};
        } else if (!strncmp(line, "track", 5) && isdigit((unsigned char)line[5])) {
            int ti = atoi(line + 5); char *ip = strstr(line, "inst="); if (ti >= 0 && ti < MAXTRK && ip) { sscanf(ip + 5, "%23s", g_trk_inst[ti]); if (ti + 1 > g_ntrk) g_ntrk = ti + 1; }
        }
    }
    if (g_tempo < 20 || g_tempo > 400) g_tempo = 90; if (g_bars < 1 || g_bars > 64) g_bars = 4; if (g_beats < 1 || g_beats > 12) g_beats = 4;
    return 1;
}
static uint32_t lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }
static float nz(uint32_t *s) { return (float)((int32_t)lcg(s)) / 2147483648.0f; }   /* -1..1 */

static float env(const Inst *in, float t, float gate) {
    float lvl;
    float te = t < gate ? t : gate;
    if (te < in->a) lvl = te / in->a; else if (te < in->a + in->d) lvl = 1.0f - (1.0f - in->s) * (te - in->a) / in->d; else lvl = in->s;
    if (t > gate) { float rr = (t - gate) / in->r; lvl = rr >= 1.0f ? 0.0f : lvl * (1.0f - rr); }
    return lvl;
}
/* one voice into buf[0..total) */
static void render_note(float *buf, long total, const Note *n, int idx, int rate) {
    const Inst *in = find_inst(n->track < g_ntrk ? g_trk_inst[n->track] : "");
    double spt = 60.0 / ((double)g_tempo * PPQN);
    long s0 = (long)floor(n->start * spt * rate + 0.5); if (s0 >= total) return;
    float gate = (float)(n->len * spt), amp = (n->vel / 127.0f) * in->gain * 0.6f;
    int drum = !strcmp(in->wave, "drum"); uint32_t ns = 2463534242u + (uint32_t)idx * 7919u;
    float dur = drum ? (n->pitch == 36 ? 0.30f : n->pitch == 38 ? 0.20f : n->pitch == 42 ? 0.08f : 0.25f) : gate + in->r;
    long nfr = (long)(dur * rate); if (s0 + nfr > total) nfr = total - s0;
    double ph = 0, hz = midi_hz(n->pitch); float lp = 0, lp2 = 0;
    float alpha = in->lp > 0 ? 1.0f - expf(-2.0f * (float)M_PI * in->lp / rate) : 1.0f;
    float hat_alpha = 1.0f - expf(-2.0f * (float)M_PI * 6000.0f / rate), snr_alpha = 1.0f - expf(-2.0f * (float)M_PI * 4000.0f / rate);
    for (long i = 0; i < nfr; i++) {
        float t = (float)i / rate, x;
        if (drum) {
            if (n->pitch == 36) { double f = 45.0 + 90.0 * exp(-t / 0.04); ph += f / rate; x = sinf((float)(2 * M_PI * ph)) * expf(-t / 0.12f); }
            else if (n->pitch == 38) { float w = nz(&ns); lp += snr_alpha * (w - lp); ph += 190.0 / rate; x = (lp * 0.9f + sinf((float)(2 * M_PI * ph)) * 0.4f) * expf(-t / 0.06f); }
            else if (n->pitch == 42) { float w = nz(&ns); lp += hat_alpha * (w - lp); x = (w - lp) * expf(-t / 0.02f); }
            else { ph += (100.0 + (n->pitch - 40) * 6.0) / rate; x = sinf((float)(2 * M_PI * ph)) * expf(-t / 0.10f); }
            x *= 2.0f; if (i + 1 >= nfr - 2) x = 0;
        } else {
            ph += hz / rate; ph -= floor(ph); float p = (float)ph;
            if (!strcmp(in->wave, "square")) x = p < 0.5f ? 0.7f : -0.7f;
            else if (!strcmp(in->wave, "saw")) x = (2.0f * p - 1.0f) * 0.8f;
            else if (!strcmp(in->wave, "triangle")) x = (p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p);
            else if (!strcmp(in->wave, "noise")) x = nz(&ns);
            else x = sinf(2.0f * (float)M_PI * p);
            if (in->lp > 0) { lp += alpha * (x - lp); lp2 += alpha * (lp - lp2); x = lp2; }   /* two cascaded one-poles = 12 dB/oct low-pass */
            x *= env(in, t, gate);
        }
        buf[s0 + i] += x * amp;
    }
}
static void le32(unsigned char *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void le16(unsigned char *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

int main(int argc, char **argv) {
    const char *data = ".", *outp = NULL, *pat = NULL; float vol = 1.0f; int rate = 22050;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--data") && i + 1 < argc) data = argv[++i]; else if (!strcmp(argv[i], "--volume") && i + 1 < argc) vol = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--rate") && i + 1 < argc) rate = atoi(argv[++i]); else if (!strcmp(argv[i], "-o") && i + 1 < argc) outp = argv[++i];
        else if (!pat) pat = argv[i]; else { fprintf(stderr, "music_synth: bad arg %s\n", argv[i]); return 2; }
    }
    if (!pat) { fprintf(stderr, "usage: music_synth [--data DIR] [--volume V] [--rate HZ] [-o OUT.wav] PATTERN|-\n"); return 2; }
    if (!(vol >= 0.0f)) vol = 0.0f; if (vol > 1.0f) vol = 1.0f; if (rate < 8000 || rate > 48000) rate = 22050;
    FILE *pf = strcmp(pat, "-") ? fopen(pat, "r") : stdin; if (!pf) { fprintf(stderr, "music_synth: cannot read %s\n", pat); return 1; }
    if (!load_pattern(pf)) return 1; if (pf != stdin) fclose(pf);
    load_instruments(data);
    double secs = (double)g_bars * g_beats * 60.0 / g_tempo; long total = (long)floor(secs * rate + 0.5);
    if (total < 1 || total > 600L * rate) { fprintf(stderr, "music_synth: bad length\n"); return 1; }
    float *buf = calloc((size_t)total, sizeof(float)); unsigned char *wav = malloc(44 + (size_t)total * 2); if (!buf || !wav) return 1;
    for (int i = 0; i < g_nnotes; i++) render_note(buf, total, &g_notes[i], i, rate);
    long fade = rate / 200;   /* 5 ms */
    unsigned char *d = wav + 44;
    for (long i = 0; i < total; i++) {
        float x = tanhf(buf[i]) * 0.9f * vol; if (i >= total - fade) x *= (float)(total - 1 - i) / fade;
        int v = (int)lrintf(x * 32767.0f); if (v > 32767) v = 32767; if (v < -32768) v = -32768; le16(d + 2 * i, (uint16_t)(int16_t)v);
    }
    memcpy(wav, "RIFF", 4); le32(wav + 4, 36 + (uint32_t)total * 2); memcpy(wav + 8, "WAVEfmt ", 8); le32(wav + 16, 16); le16(wav + 20, 1); le16(wav + 22, 1);
    le32(wav + 24, rate); le32(wav + 28, rate * 2); le16(wav + 32, 2); le16(wav + 34, 16); memcpy(wav + 36, "data", 4); le32(wav + 40, (uint32_t)total * 2);
    FILE *of = outp ? fopen(outp, "wb") : stdout; if (!of) { fprintf(stderr, "music_synth: cannot write %s\n", outp); return 1; }
    size_t want = 44 + (size_t)total * 2, got = fwrite(wav, 1, want, of);
    if (outp) fclose(of); else fflush(of);
    return got == want ? 0 : 1;
}
