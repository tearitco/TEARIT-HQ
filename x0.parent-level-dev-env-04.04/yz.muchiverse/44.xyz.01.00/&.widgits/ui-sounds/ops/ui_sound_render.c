/* ui_sound_render - U1 of BOOK 19.audio/UI-SOUNDS-AND-STARTUP-DESIGN.md: render every SOUND row of ui_sounds.pdl, for every THEME, to out_dir/<theme>/<id>.wav, and check each file.
 *   ui_sound_render <ui_sounds.pdl> <out_dir>
 * Rows (pipe-delimited, '#' comments, a `SECTION | KEY | VALUE` line and a `---` line are skipped):
 *   SEED  | ui | <unsigned 64-bit>                         exactly one; noise sources derive from it (seed ^ hash of "theme/id"), so the same file gives the same bytes
 *   THEME | id | label | note                              at least one; id [a-z0-9_]+
 *   SOUND | theme | id | kind | params | note              one per (theme,id); kinds below
 *   CUE   | event | sound id | min_gap_ms | category       which UI event plays which sound; EVERY theme must define every sound id a CUE names, so switching theme never leaves an event silent
 * kinds / params (key=value separated by spaces):
 *   blip   wave=sine|square|triangle|sawtooth|noise  f0=<Hz>  [f1=<Hz> linear sweep]  dur=<s 0.01..4>  [attack=<s>] [decay=<s>]  vol=<0..1>  [lp=<Hz one-pole low-pass>]
 *   notes  tempo=<30..300 bpm>  seq=<NOTE:beats ...>  wave=...  vol=<0..1>  [attack=<s>] [decay=<s>]  [tail=<s>]     NOTE = A-G, optional # or b, octave 0-8 (C5), or R for a rest; beats > 0
 * Output: mono 16-bit 44.1 kHz WAV (the player's sink upmixes). Fixed 2 ms fade-in and 5 ms fade-out so no file clicks.
 * Checks per file (audio_dsp.c): duration within 20% of the row, peak between 0.02 and 0.7, rms at most 0.2, |DC| under 0.01. A sound that a CUE of category `startup` names is held to more: at most 2.5 s and peak at most 0.5 (it is the one most likely to startle). A failing check names the file; exit 1.
 * Exit: 0 all rendered and checked (prints `OK theme/id dur=.. peak=.. rms=..` per file and `rendered=N themes=T cues=C`), 1 a row or file is bad (prints `LINE n:` / `FAIL theme/id ...`), 2 usage / unreadable file / cannot write.
 * Build: sh ops/build_ui_sound_render.sh   (gcc -std=gnu11 -Wall -Wextra -Werror -O2 -isystem <house>/&.widgits/_shared-lib ... -lm; -isystem because the shared audio_dsp.c is not -Werror clean as a text-include and is not ours to edit) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include "audio_dsp.c"
#define SR 44100
#define MAXR 256
typedef struct { char theme[48], id[48], kind[16], params[512]; int line; } Snd;
typedef struct { char id[48]; } Theme;
typedef struct { char event[48], sound[48], cat[24]; int line; } Cue;
static void cp(char *d, size_t n, const char *s) {
    size_t l = strlen(s);
    if (l >= n) {
        l = n - 1;
    }
    memcpy(d, s, l);
    d[l] = 0;
}
static int split_bar(char *line, char **f, int max) {
    int n = 0;
    char *p = line;
    while (n < max) {
        f[n++] = p;
        char *b = strchr(p, '|');
        if (!b) {
            break;
        }
        *b = 0;
        p = b + 1;
    }
    for (int i = 0; i < n; i++) {
        while (*f[i] == ' ') {
            f[i]++;
        }
        char *e = f[i] + strlen(f[i]);
        while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) {
            *--e = 0;
        }
    }
    return n;
}
static int ident(const char *s) {
    if (!*s || strlen(s) > 40) {
        return 0;
    }
    for (; *s; s++) {
        if (!(islower((unsigned char)*s) || isdigit((unsigned char)*s) || *s == '_')) {
            return 0;
        }
    }
    return 1;
}
static uint64_t mix(uint64_t x) {
    uint64_t z = x + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static uint64_t hash_str(uint64_t seed, const char *s) {
    uint64_t h = mix(seed);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h = mix(h ^ *p);
    }
    return mix(h ^ (uint64_t)strlen(s));
}
/* "k=v k=v" lookup: returns 1 and copies the value if key is present */
static int getp(const char *params, const char *key, char *out, size_t n) {
    size_t kl = strlen(key);
    for (const char *p = params; *p; ) {
        while (*p == ' ') {
            p++;
        }
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            const char *v = p + kl + 1;
            size_t l = strcspn(v, " ");
            if (l >= n) {
                l = n - 1;
            }
            memcpy(out, v, l);
            out[l] = 0;
            return 1;
        }
        p += strcspn(p, " ");
    }
    return 0;
}
static int getnum(const char *params, const char *key, double *v) {
    char b[64];
    if (!getp(params, key, b, sizeof b)) {
        return 0;
    }
    char *e;
    *v = strtod(b, &e);
    return *e == 0 && b[0] != 0 && isfinite(*v) ? 1 : -1;
}
static int wave_ok(const char *w) {
    return !strcmp(w, "sine") || !strcmp(w, "square") || !strcmp(w, "triangle") || !strcmp(w, "sawtooth") || !strcmp(w, "noise");
}
static double osc(const char *w, double ph, uint64_t *rng) {
    double t = ph - floor(ph);
    if (!strcmp(w, "sine")) {
        return sin(2.0 * M_PI * t);
    }
    if (!strcmp(w, "square")) {
        return t < 0.5 ? 1.0 : -1.0;
    }
    if (!strcmp(w, "triangle")) {
        return 4.0 * fabs(t - 0.5) - 1.0;
    }
    if (!strcmp(w, "sawtooth")) {
        return 2.0 * t - 1.0;
    }
    *rng ^= *rng << 13;
    *rng ^= *rng >> 7;
    *rng ^= *rng << 17;
    return (double)(*rng >> 11) / 9007199254740992.0 * 2.0 - 1.0;
}
/* one voice: a tone of f0..f1 over dur seconds with attack / decay (linear), into buf starting at sample off. Returns 0 ok. */
static void voice(double *buf, long cap, long off, const char *wave, double f0, double f1, double dur, double att, double dec, double vol, double lp, uint64_t *rng) {
    long n = (long)(dur * SR);
    double ph = 0.0, lps = 0.0;
    double a = lp > 0 ? 1.0 - exp(-2.0 * M_PI * lp / SR) : 1.0;
    for (long i = 0; i < n && off + i < cap; i++) {
        double t = (double)i / SR;
        double f = f0 + (f1 - f0) * (n > 1 ? (double)i / (double)(n - 1) : 0.0);
        ph += f / SR;
        double e = 1.0;
        if (att > 0 && t < att) {
            e = t / att;
        }
        if (dec > 0 && t > dur - dec) {
            e = fmin(e, (dur - t) / dec);
        }
        double s = osc(wave, ph, rng) * e * vol;
        lps += a * (s - lps);
        buf[off + i] += lp > 0 ? lps : s;
    }
}
static int note_hz(const char *nt, double *hz) {
    static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };      /* A B C D E F G as semitones above C */
    if (!isupper((unsigned char)nt[0]) || nt[0] < 'A' || nt[0] > 'G') {
        return 0;
    }
    int semi = base[nt[0] - 'A'];
    const char *p = nt + 1;
    if (*p == '#') {
        semi++;
        p++;
    } else if (*p == 'b') {
        semi--;
        p++;
    }
    if (!isdigit((unsigned char)*p) || p[1]) {
        return 0;
    }
    int oct = *p - '0';
    *hz = 440.0 * pow(2.0, ((oct - 4) * 12 + semi - 9) / 12.0);
    return 1;
}
/* returns NULL and fills why on a bad row; else a malloc'ed buffer of *n doubles and *dur_exp */
static double *synth(const Snd *s, uint64_t seed, long *n, double *dur_exp, char *why, size_t wn) {
    uint64_t rng = hash_str(seed, s->theme) ^ hash_str(seed, s->id);
    if (!rng) {
        rng = 1;
    }
    char wave[24] = "", b[64];
    double vol = 0, att = 0, dec = 0;
    int r;
    if (!getp(s->params, "wave", wave, sizeof wave) || !wave_ok(wave)) {
        snprintf(why, wn, "wave must be sine, square, triangle, sawtooth or noise");
        return NULL;
    }
    if (getnum(s->params, "vol", &vol) != 1 || vol <= 0 || vol > 1.0) {
        snprintf(why, wn, "vol must be a number in (0, 1]");
        return NULL;
    }
    if ((r = getnum(s->params, "attack", &att)) < 0 || att < 0 || (r = getnum(s->params, "decay", &dec)) < 0 || dec < 0) {
        snprintf(why, wn, "attack / decay must be numbers >= 0");
        return NULL;
    }
    double lp = 0;
    if (getnum(s->params, "lp", &lp) < 0 || lp < 0 || lp >= 20000) {
        snprintf(why, wn, "lp must be a number in 0..20000");
        return NULL;
    }
    if (!strcmp(s->kind, "blip")) {
        double f0 = 0, f1 = 0, dur = 0;
        if (getnum(s->params, "dur", &dur) != 1 || dur < 0.01 || dur > 4.0) {
            snprintf(why, wn, "dur must be 0.01..4 seconds");
            return NULL;
        }
        int hasf0 = getnum(s->params, "f0", &f0);
        if (strcmp(wave, "noise") && (hasf0 != 1 || f0 < 20 || f0 > 18000)) {
            snprintf(why, wn, "f0 must be 20..18000 Hz for a tone");
            return NULL;
        }
        if (hasf0 < 0) {
            snprintf(why, wn, "f0 is not a number");
            return NULL;
        }
        f1 = f0;
        if ((r = getnum(s->params, "f1", &f1)) < 0 || (r == 1 && (f1 < 20 || f1 > 18000))) {
            snprintf(why, wn, "f1 must be 20..18000 Hz");
            return NULL;
        }
        if (att + dec > dur) {
            snprintf(why, wn, "attack + decay is longer than dur");
            return NULL;
        }
        *n = (long)(dur * SR);
        *dur_exp = dur;
        double *buf = calloc((size_t)*n + 1, sizeof *buf);
        voice(buf, *n, 0, wave, f0, f1, dur, att, dec, vol, lp, &rng);
        return buf;
    }
    if (!strcmp(s->kind, "notes")) {
        double tempo = 0, tail = 0;
        if (getnum(s->params, "tempo", &tempo) != 1 || tempo < 30 || tempo > 300) {
            snprintf(why, wn, "tempo must be 30..300");
            return NULL;
        }
        if (getnum(s->params, "tail", &tail) < 0 || tail < 0 || tail > 3) {
            snprintf(why, wn, "tail must be 0..3 seconds");
            return NULL;
        }
        char seq[400];
        if (!getp(s->params, "seq", seq, sizeof seq) || !seq[0]) {
            snprintf(why, wn, "seq is missing");
            return NULL;
        }
        /* seq has no spaces inside getp's token; notes are joined with ',' */
        double total = 0;
        char tmp[400];
        snprintf(tmp, sizeof tmp, "%s", seq);
        int count = 0;
        for (char *t = strtok(tmp, ","); t; t = strtok(NULL, ",")) {
            char *colon = strchr(t, ':');
            double hz, beats;
            char *e;
            if (!colon) {
                snprintf(why, wn, "note '%s' needs NOTE:beats", t);
                return NULL;
            }
            *colon = 0;
            beats = strtod(colon + 1, &e);
            if (*e || beats <= 0 || beats > 16) {
                snprintf(why, wn, "beats '%s' must be a number in (0, 16]", colon + 1);
                return NULL;
            }
            if (strcmp(t, "R") && !note_hz(t, &hz)) {
                snprintf(why, wn, "note '%s' is not A-G[#b]0-8 or R", t);
                return NULL;
            }
            total += beats * 60.0 / tempo;
            count++;
        }
        if (count == 0 || total + tail > 8.0) {
            snprintf(why, wn, "the sequence must have notes and be at most 8 seconds");
            return NULL;
        }
        *dur_exp = total + tail;
        *n = (long)((total + tail) * SR);
        double *buf = calloc((size_t)*n + 1, sizeof *buf);
        snprintf(tmp, sizeof tmp, "%s", seq);
        double at = 0;
        for (char *t = strtok(tmp, ","); t; t = strtok(NULL, ",")) {
            char *colon = strchr(t, ':');
            *colon = 0;
            double beats = strtod(colon + 1, NULL), hz = 0, d = beats * 60.0 / tempo;
            if (strcmp(t, "R") && note_hz(t, &hz)) {
                double nd = d + (tail > 0 ? fmin(tail, 0.3) : 0.0);
                voice(buf, *n, (long)(at * SR), wave, hz, hz, nd, att, dec > 0 ? fmin(dec, nd) : 0, vol, lp, &rng);
            }
            at += d;
        }
        (void)b;
        return buf;
    }
    snprintf(why, wn, "kind '%s' must be blip or notes", s->kind);
    return NULL;
}
int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: ui_sound_render <ui_sounds.pdl> <out_dir>\n");
        return 2;
    }
    if (strlen(argv[2]) > 900) {
        fprintf(stderr, "ui_sound_render: out_dir path is too long\n");
        return 2;
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "ui_sound_render: cannot read %s\n", argv[1]);
        return 2;
    }
    static Snd S[MAXR];
    static Theme T[16];
    static Cue C[MAXR];
    int ns = 0, nt = 0, nc = 0, bad = 0, seeds = 0, lineno = 0;
    uint64_t seed = 0;
    char ln[1024];
#define ERR(...) do { printf("LINE %d: ", lineno); printf(__VA_ARGS__); printf("\n"); bad++; } while (0)
    while (fgets(ln, sizeof ln, fp)) {
        lineno++;
        char *s = ln;
        while (*s == ' ' || *s == '\t') {
            s++;
        }
        if (!*s || *s == '#' || *s == '\n' || *s == '\r' || !strncmp(s, "---", 3) || !strncmp(s, "SECTION", 7)) {
            continue;
        }
        char *f[8];
        int n = split_bar(s, f, 8);
        if (!strcmp(f[0], "SEED")) {
            char *e;
            errno = 0;
            if (n < 3 || strcmp(f[1], "ui") || !isdigit((unsigned char)f[2][0])) {
                ERR("SEED row must be SEED | ui | <unsigned number>");
                continue;
            }
            unsigned long long v = strtoull(f[2], &e, 10);
            if (*e || errno) {
                ERR("seed '%s' is not an unsigned 64-bit decimal number", f[2]);
                continue;
            }
            seeds++;
            if (seeds > 1) {
                ERR("more than one SEED row");
            }
            seed = v;
        } else if (!strcmp(f[0], "THEME")) {
            if (n < 3 || !ident(f[1]) || !f[2][0]) {
                ERR("THEME row must be THEME | <id a-z0-9_> | <label> | <note>");
                continue;
            }
            int dup = 0;
            for (int i = 0; i < nt; i++) {
                if (!strcmp(T[i].id, f[1])) {
                    dup = 1;
                }
            }
            if (dup) {
                ERR("duplicate THEME '%s'", f[1]);
                continue;
            }
            if (nt >= 16) {
                ERR("more than 16 themes");
                continue;
            }
            cp(T[nt++].id, sizeof T[0].id, f[1]);
        } else if (!strcmp(f[0], "SOUND")) {
            if (n < 5 || !ident(f[1]) || !ident(f[2])) {
                ERR("SOUND row must be SOUND | <theme> | <id> | <kind> | <params> | <note>");
                continue;
            }
            if (ns >= MAXR) {
                ERR("more than %d sounds", MAXR);
                continue;
            }
            int dup = 0;
            for (int i = 0; i < ns; i++) {
                if (!strcmp(S[i].theme, f[1]) && !strcmp(S[i].id, f[2])) {
                    dup = 1;
                }
            }
            if (dup) {
                ERR("duplicate SOUND %s/%s", f[1], f[2]);
                continue;
            }
            cp(S[ns].theme, sizeof S[0].theme, f[1]);
            cp(S[ns].id, sizeof S[0].id, f[2]);
            cp(S[ns].kind, sizeof S[0].kind, f[3]);
            cp(S[ns].params, sizeof S[0].params, f[4]);
            S[ns].line = lineno;
            ns++;
        } else if (!strcmp(f[0], "CUE")) {
            if (n < 5 || !ident(f[1]) || !ident(f[2])) {
                ERR("CUE row must be CUE | <event> | <sound id> | <min_gap_ms> | <category>");
                continue;
            }
            char *e;
            long gap = strtol(f[3], &e, 10);
            if (*e || !f[3][0] || gap < 0 || gap > 10000) {
                ERR("min_gap_ms '%s' must be an integer 0..10000", f[3]);
                continue;
            }
            if (nc >= MAXR) {
                ERR("more than %d cues", MAXR);
                continue;
            }
            cp(C[nc].event, sizeof C[0].event, f[1]);
            cp(C[nc].sound, sizeof C[0].sound, f[2]);
            cp(C[nc].cat, sizeof C[0].cat, f[4]);
            C[nc].line = lineno;
            nc++;
        } else {
            ERR("unknown row kind '%s' (SEED, THEME, SOUND, CUE)", f[0]);
        }
    }
    fclose(fp);
    lineno = 0;
    if (seeds == 0) {
        ERR("no SEED row");
    }
    if (nt == 0) {
        ERR("no THEME rows");
    }
    if (ns == 0) {
        ERR("no SOUND rows");
    }
    for (int i = 0; i < ns; i++) {
        int ok = 0;
        for (int j = 0; j < nt; j++) {
            if (!strcmp(T[j].id, S[i].theme)) {
                ok = 1;
            }
        }
        if (!ok) {
            lineno = S[i].line;
            ERR("SOUND %s/%s names theme '%s' which has no THEME row", S[i].theme, S[i].id, S[i].theme);
        }
    }
    for (int c = 0; c < nc; c++) {
        for (int j = 0; j < nt; j++) {
            int has = 0;
            for (int i = 0; i < ns; i++) {
                if (!strcmp(S[i].theme, T[j].id) && !strcmp(S[i].id, C[c].sound)) {
                    has = 1;
                }
            }
            if (!has) {
                lineno = C[c].line;
                ERR("CUE '%s' plays sound '%s' which theme '%s' does not define", C[c].event, C[c].sound, T[j].id);
            }
        }
    }
    if (bad) {
        printf("ui_sound_render FAILED: %d problem%s in the rows (nothing rendered)\n", bad, bad == 1 ? "" : "s");
        return 1;
    }
    int rendered = 0, failed = 0;
    for (int i = 0; i < ns; i++) {
        char why[160] = "", dir[1000], path[1100];
        long n = 0;
        double dur_exp = 0;
        double *buf = synth(&S[i], seed, &n, &dur_exp, why, sizeof why);
        if (!buf) {
            printf("FAIL %s/%s line %d: %s\n", S[i].theme, S[i].id, S[i].line, why);
            failed++;
            continue;
        }
        short *pcm = malloc((size_t)(n + 1) * sizeof *pcm);
        long fi = (long)(0.002 * SR), fo = (long)(0.005 * SR);
        for (long k = 0; k < n; k++) {
            double v = buf[k];
            if (k < fi) {
                v *= (double)k / (double)fi;
            }
            if (n - 1 - k < fo) {
                v *= (double)(n - 1 - k) / (double)fo;
            }
            v = fmax(-1.0, fmin(1.0, v));
            pcm[k] = (short)lrint(v * 32767.0);
        }
        free(buf);
        double pk = adsp_peak(pcm, n), rms = adsp_rms(pcm, n), dc = adsp_dc(pcm, n), dur = (double)n / SR;
        char bad_why[160] = "";
        int is_startup = 0;
        for (int c = 0; c < nc; c++) {
            if (!strcmp(C[c].sound, S[i].id) && !strcmp(C[c].cat, "startup")) {
                is_startup = 1;
            }
        }
        if (is_startup && dur > 2.5) {
            snprintf(bad_why, sizeof bad_why, "startup sound is %.3f s, over 2.5 s", dur);
        } else if (is_startup && pk > 0.5) {
            snprintf(bad_why, sizeof bad_why, "startup sound peak %.3f over 0.5", pk);
        } else if (fabs(dur - dur_exp) > 0.2 * dur_exp) {
            snprintf(bad_why, sizeof bad_why, "duration %.3f s is not within 20%% of %.3f s", dur, dur_exp);
        } else if (pk < 0.02) {
            snprintf(bad_why, sizeof bad_why, "silent (peak %.3f)", pk);
        } else if (pk > 0.7) {
            snprintf(bad_why, sizeof bad_why, "too loud (peak %.3f over 0.7)", pk);
        } else if (rms > 0.2) {
            snprintf(bad_why, sizeof bad_why, "too loud (rms %.3f over 0.2)", rms);
        } else if (fabs(dc) > 0.01) {
            snprintf(bad_why, sizeof bad_why, "DC offset %.4f", dc);
        }
        if (bad_why[0]) {
            printf("FAIL %s/%s line %d: %s\n", S[i].theme, S[i].id, S[i].line, bad_why);
            failed++;
            free(pcm);
            continue;
        }
        cp(dir, sizeof dir, argv[2]);
        strcat(dir, "/");
        strcat(dir, S[i].theme);      /* argv[2] <= 900 chars (checked), theme <= 47: fits */
        mkdir(argv[2], 0755);
        if (mkdir(dir, 0755) && errno != EEXIST) {
            fprintf(stderr, "ui_sound_render: cannot create %s\n", dir);
            free(pcm);
            return 2;
        }
        cp(path, sizeof path, dir);
        strcat(path, "/");
        strcat(path, S[i].id);
        strcat(path, ".wav");
        if (adsp_write_wav(path, pcm, n, SR)) {
            fprintf(stderr, "ui_sound_render: cannot write %s\n", path);
            free(pcm);
            return 2;
        }
        free(pcm);
        printf("OK %s/%s dur=%.3f peak=%.3f rms=%.3f\n", S[i].theme, S[i].id, dur, pk, rms);
        rendered++;
    }
    printf("rendered=%d themes=%d cues=%d%s\n", rendered, nt, nc, failed ? " (FAILED files above)" : "");
    return failed ? 1 : 0;
}
