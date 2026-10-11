/* music_seq - deterministic pattern generator for the music daemon (MUSIC-DAEMON v0, EDEN-PLAYABLE-LOOP plan section 4).
 *
 * Usage: music_seq [--data DIR] [--mood NAME] [--seed N|TEXT] [--tod dawn|day|dusk|night] [--weather clear|rain]
 *                  [--phrase N] [--safe] [-o FILE]            (pattern text to stdout when no -o)
 * Reads DIR/mood.pdl, scales.pdl, instruments.pdl (DIR defaults to "."). Same seed + mood + tod + weather + phrase + data rows =
 * byte-identical pattern: the generator is integer-only (splitmix64 PRNG, no floats, no clock, no pid), one PRNG stream per track
 * so adding an instrument does not shift the others. Heal: a missing/garbage data file, unknown mood, or --safe gives the built-in
 * safe pattern (C-major-ish pad+bass+flute at 90 bpm) instead of an error; the header line `fallback=` says so.
 *
 * Pattern format = the Muchi DAW's own sequence line (103.media-studio/103.daw/ops/daw_main.c save_state): `NOTE track=%d pitch=%d
 * start=%d len=%d vel=%d` in PPQN=480 ticks, with `tempo=` and `trackN name=...` lines as in its daw_state.txt, plus bars=/beats=/inst=.
 * Self-contained: libc only. House rule: no header+link sharing; the tiny pdl row reader below is copied inline in music_synth.c too. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define PPQN 480
#define MAXN 16
#define MAXI 16
#define BEATS 4
static const char *g_data = ".";

/* ---------- pdl rows ---------- */
static char *trim(char *s) { char *e; while (*s == ' ' || *s == '\t') s++; e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0; return s; }
/* split line on '|' into f[]; returns field count (fields trimmed in place) */
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int csv_ints(const char *s, int *out, int max) { int n = 0; while (*s && n < max) { out[n++] = atoi(s); const char *c = strchr(s, ','); if (!c) break; s = c + 1; } return n; }
static int csv_words(const char *s, char out[][24], int max) { int n = 0; while (*s && n < max) { int k = 0; while (*s && *s != ',' && k < 23) out[n][k++] = *s++; out[n][k] = 0; if (strcmp(trim(out[n]), "-") && out[n][0]) { char t[24]; strcpy(t, trim(out[n])); strcpy(out[n], t); n++; } if (*s == ',') s++; } return n; }
static int has_word(char w[][24], int n, const char *s) { for (int i = 0; i < n; i++) if (!strcmp(w[i], s)) return 1; return 0; }

/* ---------- prng ---------- */
static uint64_t splitmix(uint64_t *s) { uint64_t z = (*s += 0x9E3779B97F4A7C15ULL); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
static uint64_t fnv(const char *s) { uint64_t h = 1469598103934665603ULL; while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; } return h; }
static int rnd(uint64_t *s, int mod) { return (int)((splitmix(s) >> 11) % (uint64_t)mod); }

/* ---------- model ---------- */
typedef struct { char name[24], scale[24]; int root, tempo, bars, density, prog[16], nprog; char insts[MAXI][24]; int ninst; } Mood;
typedef struct { int tempo_pct, dens_pct, shift; char add[MAXI][24], drop[MAXI][24]; int nadd, ndrop; } Var;
static Mood safe_mood(void) {
    Mood m; memset(&m, 0, sizeof m); strcpy(m.name, "safe"); strcpy(m.scale, "major"); m.root = 57; m.tempo = 90; m.bars = 4; m.density = 50;
    int p[4] = {0, 4, 5, 3}; memcpy(m.prog, p, sizeof p); m.nprog = 4; strcpy(m.insts[0], "pad"); strcpy(m.insts[1], "bass"); strcpy(m.insts[2], "flute"); m.ninst = 3; return m;
}
static int g_scale[MAXN], g_nscale;
static int find_scale(const char *name) {
    char path[4200], line[512], *f[8]; snprintf(path, sizeof path, "%s/scales.pdl", g_data);
    FILE *fp = fopen(path, "r"); if (!fp) return 0;
    while (fgets(line, sizeof line, fp)) { char *l = trim(line); if (*l == '#' || !*l) continue; int n = split(l, f, 8); if (n >= 3 && !strcmp(f[0], "SCALE") && !strcmp(f[1], name)) { g_nscale = csv_ints(f[2], g_scale, MAXN); fclose(fp); return g_nscale >= 2; } }
    fclose(fp); return 0;
}
static int load_mood(const char *name, Mood *m) {
    char path[4200], line[1024], *f[12]; snprintf(path, sizeof path, "%s/mood.pdl", g_data);
    FILE *fp = fopen(path, "r"); if (!fp) return 0;
    while (fgets(line, sizeof line, fp)) {
        char *l = trim(line); if (*l == '#' || !*l) continue; int n = split(l, f, 12);
        if (n >= 9 && !strcmp(f[0], "MOOD") && !strcmp(f[1], name)) {
            memset(m, 0, sizeof *m); snprintf(m->name, sizeof m->name, "%s", f[1]); snprintf(m->scale, sizeof m->scale, "%s", f[2]);
            m->root = atoi(f[3]); m->tempo = atoi(f[4]); m->bars = atoi(f[5]); m->density = atoi(f[6]); m->nprog = csv_ints(f[7], m->prog, 16); m->ninst = csv_words(f[8], m->insts, MAXI);
            fclose(fp);
            if (m->root < 24 || m->root > 100 || m->tempo < 30 || m->tempo > 240 || m->bars < 1 || m->bars > 32 || m->nprog < 1 || m->ninst < 1) return 0;   /* garbage row -> Heal */
            if (m->density < 0) m->density = 0; if (m->density > 100) m->density = 100;
            return 1;
        }
    }
    fclose(fp); return 0;
}
static Var load_var(const char *verb, const char *name) {
    Var v; memset(&v, 0, sizeof v); v.tempo_pct = 100; v.dens_pct = 100;     /* unknown name -> neutral */
    char path[4200], line[1024], *f[12]; snprintf(path, sizeof path, "%s/mood.pdl", g_data);
    FILE *fp = fopen(path, "r"); if (!fp) return v;
    while (fgets(line, sizeof line, fp)) {
        char *l = trim(line); if (*l == '#' || !*l) continue; int n = split(l, f, 12);
        if (n >= 7 && !strcmp(f[0], verb) && !strcmp(f[1], name)) {
            v.tempo_pct = atoi(f[2]); v.dens_pct = atoi(f[3]); v.shift = atoi(f[4]); v.nadd = csv_words(f[5], v.add, MAXI); v.ndrop = csv_words(f[6], v.drop, MAXI);
            if (v.tempo_pct < 30 || v.tempo_pct > 300) v.tempo_pct = 100; if (v.dens_pct < 0 || v.dens_pct > 300) v.dens_pct = 100;
            break;
        }
    }
    fclose(fp); return v;
}
/* role of an instrument: instruments.pdl row, else the name itself if it is a role word, else lead */
static void role_of(const char *inst, char *role, size_t n) {
    char path[4200], line[512], *f[12]; snprintf(path, sizeof path, "%s/instruments.pdl", g_data);
    FILE *fp = fopen(path, "r");
    if (fp) { while (fgets(line, sizeof line, fp)) { char *l = trim(line); if (*l == '#' || !*l) continue; int c = split(l, f, 12); if (c >= 4 && !strcmp(f[0], "INST") && !strcmp(f[1], inst)) { snprintf(role, n, "%s", f[2]); fclose(fp); return; } } fclose(fp); }
    static const char *roles[] = {"pad", "bass", "lead", "arp", "bells", "drums", "bed"};
    for (int i = 0; i < 7; i++) if (!strcmp(inst, roles[i])) { snprintf(role, n, "%s", inst); return; }
    snprintf(role, n, "lead");
}

/* ---------- generation ---------- */
static FILE *out;
static int g_scale_pitch(int root, int idx) { int n = g_nscale, oct = idx / n, st = idx % n; if (st < 0) { st += n; oct--; } return root + g_scale[st] + 12 * oct; }
static void note(int track, int pitch, int start, int len, int vel) {
    if (pitch < 0) pitch = 0; if (pitch > 127) pitch = 127; if (vel < 1) vel = 1; if (vel > 127) vel = 127;
    fprintf(out, "NOTE track=%d pitch=%d start=%d len=%d vel=%d\n", track, pitch, start, len, vel);
}
static void gen_track(int ti, const char *role, const Mood *m, int root, int density, uint64_t seed, const char *iname) {
    uint64_t r = seed ^ fnv(iname); int n = g_nscale, bar_t = PPQN * BEATS, E = PPQN / 2;
    int cur = n + 2, busy = -1;
    if (!strcmp(role, "bed")) { note(ti, 60, 0, m->bars * bar_t, 50); return; }
    for (int b = 0; b < m->bars; b++) {
        int c = m->prog[b % m->nprog], t0 = b * bar_t;
        if (!strcmp(role, "pad")) { for (int k = 0; k < 3; k++) note(ti, g_scale_pitch(root, c + 2 * k), t0, bar_t, 52 + rnd(&r, 8)); }
        else if (!strcmp(role, "bass")) {
            note(ti, g_scale_pitch(root, c - n), t0, 360, 92);
            if (rnd(&r, 100) < 40 + density / 2) note(ti, g_scale_pitch(root, c - n + (rnd(&r, 2) ? 4 : 0)), t0 + 2 * PPQN, 360, 84);
            for (int k = 1; k < 4; k += 2) if (rnd(&r, 100) < density / 3) note(ti, g_scale_pitch(root, c - n), t0 + k * PPQN, 200, 70);
        } else if (!strcmp(role, "lead")) {
            for (int s = 0; s < 8; s++) {
                int st = t0 + s * E, on = (s == 0) || rnd(&r, 100) < density * 6 / 10;
                int step = rnd(&r, 5) - 2;
                if (!on || st < busy) continue;
                cur += step; if (s == 0) cur = n + c + 2 * rnd(&r, 3);
                if (cur < n - 2) cur = n - 2; if (cur > 2 * n + 2) cur = 2 * n + 2;
                int len = E * (1 + rnd(&r, 2)); note(ti, g_scale_pitch(root, cur), st, len, 70 + rnd(&r, 25)); busy = st + len;
            }
        } else if (!strcmp(role, "arp")) {
            for (int s = 0; s < 8; s++) if (rnd(&r, 100) < density + 20) note(ti, g_scale_pitch(root, c + n + 2 * (s % 3)), t0 + s * E, 200, 56 + rnd(&r, 14));
        } else if (!strcmp(role, "bells")) {
            if (rnd(&r, 100) < density / 2 + 10) note(ti, g_scale_pitch(root, c + 2 * n + 2 * rnd(&r, 3)), t0 + PPQN * rnd(&r, 4), 960, 60 + rnd(&r, 15));
        } else if (!strcmp(role, "drums")) {
            note(ti, 36, t0, 200, 100); if (rnd(&r, 100) < 70) note(ti, 36, t0 + 2 * PPQN, 200, 90);
            note(ti, 38, t0 + PPQN, 200, 80); note(ti, 38, t0 + 3 * PPQN, 200, 80);
            for (int s = 0; s < 8; s++) if (rnd(&r, 100) < density) note(ti, 42, t0 + s * E, 80, 40 + rnd(&r, 25));
        }
    }
}

int main(int argc, char **argv) {
    const char *mood = "calm", *tod = "day", *wx = "clear", *seeds = "1", *outp = NULL; int phrase = 0, safe = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--data") && i + 1 < argc) g_data = argv[++i]; else if (!strcmp(argv[i], "--mood") && i + 1 < argc) mood = argv[++i];
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seeds = argv[++i]; else if (!strcmp(argv[i], "--tod") && i + 1 < argc) tod = argv[++i];
        else if (!strcmp(argv[i], "--weather") && i + 1 < argc) wx = argv[++i]; else if (!strcmp(argv[i], "--phrase") && i + 1 < argc) phrase = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--safe")) safe = 1; else if (!strcmp(argv[i], "-o") && i + 1 < argc) outp = argv[++i];
        else { fprintf(stderr, "music_seq: bad arg %s\n", argv[i]); return 2; }
    }
    char *end; unsigned long long sv = strtoull(seeds, &end, 10); uint64_t seed = (*seeds && !*end) ? sv : fnv(seeds);
    seed += (uint64_t)phrase * 0x9E3779B97F4A7C15ULL;
    Mood m; const char *fallback = "none"; Var tv, wv;
    if (safe) { m = safe_mood(); fallback = "safe-requested"; }
    else if (!load_mood(mood, &m)) { m = safe_mood(); fallback = "mood-unknown-or-unreadable"; }
    if (!find_scale(m.scale)) { static const int major[7] = {0, 2, 4, 5, 7, 9, 11}; memcpy(g_scale, major, sizeof major); g_nscale = 7; if (strcmp(fallback, "none")) {} else fallback = "scale-missing-used-major"; }
    if (safe) { tv = (Var){0}; tv.tempo_pct = tv.dens_pct = 100; wv = tv; } else { tv = load_var("TOD", tod); wv = load_var("WEATHER", wx); }
    int root = m.root + tv.shift + wv.shift, tempo = m.tempo * tv.tempo_pct / 100 * wv.tempo_pct / 100, density = m.density * tv.dens_pct / 100 * wv.dens_pct / 100;
    if (tempo < 40) tempo = 40; if (tempo > 220) tempo = 220; if (density > 100) density = 100;
    char names[MAXI * 2 + 2][24]; int nn = 0;                 /* mood insts + adds - drops, deduped, in order */
    for (int i = 0; i < m.ninst; i++) if (!has_word(tv.drop, tv.ndrop, m.insts[i]) && !has_word(wv.drop, wv.ndrop, m.insts[i])) snprintf(names[nn++], 24, "%s", m.insts[i]);
    for (int pass = 0; pass < 2; pass++) { Var *v = pass ? &wv : &tv; for (int i = 0; i < v->nadd && nn < MAXI * 2; i++) if (!has_word(names, nn, v->add[i])) snprintf(names[nn++], 24, "%s", v->add[i]); }
    if (nn == 0) snprintf(names[nn++], 24, "pad");
    out = outp ? fopen(outp, "w") : stdout; if (!out) { fprintf(stderr, "music_seq: cannot write %s\n", outp); return 1; }
    fprintf(out, "# music_seq pattern v1 - NOTE lines are the Muchi DAW sequence format (PPQN=%d)\nproject=music_daemon\nmood=%s\nseed=%llu\ntime_of_day=%s\nweather=%s\nfallback=%s\ntempo=%d\nbeats=%d\nbars=%d\nppqn=%d\nn_tracks=%d\n",
            PPQN, m.name, (unsigned long long)seed, safe ? "none" : tod, safe ? "none" : wx, fallback, tempo, BEATS, m.bars, PPQN, nn);
    for (int i = 0; i < nn; i++) fprintf(out, "track%d name=%s inst=%s vol=1.00\n", i, names[i], names[i]);
    for (int i = 0; i < nn; i++) { char role[24]; role_of(names[i], role, sizeof role); gen_track(i, role, &m, root, density, seed, names[i]); }
    if (outp) fclose(out);
    return 0;
}
