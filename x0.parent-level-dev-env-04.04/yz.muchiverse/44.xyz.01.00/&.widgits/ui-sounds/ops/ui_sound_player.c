/* ui_sound_player - U2 of BOOK 19.audio/UI-SOUNDS-AND-STARTUP-DESIGN.md: reads UI cue lines from a marker file and plays the rendered sounds. One process per desktop.
 *   ui_sound_player --pdl <ui_sounds.pdl> --sounds <dir> --settings <hq_ui.pdl> --cues <file> [--dry-run <log>] [--drain] [--now <ms>] [--max-bytes <n>]
 *   --pdl       the CUE / THEME rows (event -> sound id, min_gap_ms, category; first THEME is the default theme)
 *   --sounds    the directory ui_sound_render wrote: <sounds>/<theme>/<id>.wav
 *   --settings  hq_ui.pdl, re-read for every batch of cues: sound_enabled (1) sound_volume (40) sound_ui (1) sound_startup (1) sound_theme (first THEME); missing key = the default in brackets
 *   --cues      append-only marker file; a line is `<monotonic_ms> <pid> <cue name>` (CLOCK_MONOTONIC, system wide). Live mode seeks to the END first (old cues are never replayed).
 *   --dry-run   write what WOULD happen to <log> instead of opening the sound server; decisions are identical to live
 *   --drain     process the whole file from the start once and exit (tests); without it: loop, checking the file size every 30 ms, forever
 *   --now       with --drain: the clock for the stale check (default = the last cue's own time, i.e. nothing is stale)
 *   --max-bytes live mode: when the file is larger than this and fully consumed, truncate it to 0 (default 65536)
 * Decision per cue, in this order (the log line says which): unknown name -> `skip unknown`; sound_enabled=0 -> `skip muted`; category ui with sound_ui=0 -> `skip ui_off`; category startup with sound_startup=0 -> `skip startup_off`;
 *   older than 150 ms (startup exempt) -> `drop stale`; closer than the cue's min_gap_ms to the last PLAYED cue of that name, from ANY pid -> `coalesce`; volume 0 -> `skip volume0`; else `played <name> theme=<t> vol=<n> latency_ms=<now - cue time>`.
 *   A theme with no such wav falls back to the first theme (`fallback theme=<x>-><y>`); no wav at all -> `skip nofile`. A malformed line -> `skip bad_line`.
 * Live playback: one pa_simple stream (libpulse-simple), 44.1 kHz mono s16, sounds loaded on first use and scaled by volume/100; sounds play in order, never overlapped. No sound server: say so once on stderr and exit 0 (the desktop is unaffected).
 * Exit: 0 normal (drain finished, or no sound server), 2 usage / unreadable --pdl / --settings is allowed to be missing. Build: sh ops/build_ui_sound_player.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include "audio_dsp.c"
#define MAXC 64
#define MAXT 16
typedef struct { char name[48], sound[48], cat[24]; long gap, last; } CueDef;
static CueDef CU[MAXC];
static int ncue = 0;
static char TH[MAXT][48];
static int nth = 0;
static FILE *g_log = NULL;
static int g_dry = 0;
static pa_simple *g_pa = NULL;
static long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void logf_(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    FILE *o = g_log ? g_log : stdout;
    vfprintf(o, fmt, ap);
    fputc('\n', o);
    fflush(o);
    va_end(ap);
}
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
static int load_pdl(const char *path) {
    FILE *fp = fopen(path, "r");
    char ln[1024];
    if (!fp) {
        return 0;
    }
    while (fgets(ln, sizeof ln, fp)) {
        char *f[8];
        char *s = ln;
        while (*s == ' ') {
            s++;
        }
        if (!strncmp(s, "THEME", 5) && nth < MAXT) {
            if (split_bar(s, f, 8) >= 2) {
                cp(TH[nth++], sizeof TH[0], f[1]);
            }
        } else if (!strncmp(s, "CUE", 3) && ncue < MAXC) {
            if (split_bar(s, f, 8) >= 5) {
                CueDef *c = &CU[ncue++];
                cp(c->name, sizeof c->name, f[1]);
                cp(c->sound, sizeof c->sound, f[2]);
                c->gap = atol(f[3]);
                cp(c->cat, sizeof c->cat, f[4]);
                c->last = -1000000000L;
            }
        }
    }
    fclose(fp);
    return nth > 0 && ncue > 0;
}
typedef struct { int enabled, volume, ui, startup; char theme[48]; } Set;
static void load_settings(const char *path, Set *s) {
    s->enabled = 1;
    s->volume = 40;
    s->ui = 1;
    s->startup = 1;
    cp(s->theme, sizeof s->theme, TH[0]);
    FILE *fp = fopen(path, "r");
    char ln[512];
    if (!fp) {
        return;
    }
    while (fgets(ln, sizeof ln, fp)) {
        char *eq = strchr(ln, '=');
        if (!eq || ln[0] == '#') {
            continue;
        }
        *eq = 0;
        char *v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        char *e;
        long n = strtol(v, &e, 10);
        int isnum = *v && !*e;
        if (!strcmp(ln, "sound_enabled") && isnum) {
            s->enabled = n != 0;
        } else if (!strcmp(ln, "sound_volume") && isnum) {
            s->volume = n < 0 ? 0 : n > 100 ? 100 : (int)n;
        } else if (!strcmp(ln, "sound_ui") && isnum) {
            s->ui = n != 0;
        } else if (!strcmp(ln, "sound_startup") && isnum) {
            s->startup = n != 0;
        } else if (!strcmp(ln, "sound_theme") && *v) {
            cp(s->theme, sizeof s->theme, v);
        }
    }
    fclose(fp);
}
static int wav_path(const char *dir, const char *theme, const char *id, char *out, size_t n) {
    if (strlen(dir) + strlen(theme) + strlen(id) + 8 > n) {
        return 0;
    }
    cp(out, n, dir);
    strcat(out, "/");
    strcat(out, theme);
    strcat(out, "/");
    strcat(out, id);
    strcat(out, ".wav");
    return access(out, R_OK) == 0;
}
static void play_file(const char *path, int vol) {
    short *pcm = NULL;
    long n = 0;
    int sr = 0;
    if (adsp_read_wav(path, &pcm, &n, &sr) != 0 && !pcm) {
        return;
    }
    for (long i = 0; i < n; i++) {
        pcm[i] = (short)(pcm[i] * vol / 100);
    }
    int err = 0;
    pa_simple_write(g_pa, pcm, (size_t)n * 2, &err);
    free(pcm);
}
static void handle(const char *line, const Set *s, const char *sounds, long now) {
    long ts = 0;
    long pid = 0;
    char name[64] = "";
    if (sscanf(line, "%ld %ld %63s", &ts, &pid, name) != 3) {
        logf_("skip bad_line");
        return;
    }
    CueDef *c = NULL;
    for (int i = 0; i < ncue; i++) {
        if (!strcmp(CU[i].name, name)) {
            c = &CU[i];
        }
    }
    if (!c) {
        logf_("skip unknown %s", name);
        return;
    }
    int is_startup = !strcmp(c->cat, "startup");
    if (!s->enabled) {
        logf_("skip muted %s", name);
    } else if (!is_startup && !s->ui) {
        logf_("skip ui_off %s", name);
    } else if (is_startup && !s->startup) {
        logf_("skip startup_off %s", name);
    } else if (!is_startup && now - ts > 150) {
        logf_("drop stale %s age_ms=%ld", name, now - ts);
    } else if (ts - c->last < c->gap) {
        logf_("coalesce %s gap_ms=%ld<%ld", name, ts - c->last, c->gap);
    } else if (s->volume == 0) {
        logf_("skip volume0 %s", name);
    } else {
        char path[1200];
        const char *theme = s->theme;
        if (!wav_path(sounds, theme, c->sound, path, sizeof path)) {
            if (!wav_path(sounds, TH[0], c->sound, path, sizeof path)) {
                logf_("skip nofile %s", name);
                return;
            }
            logf_("fallback theme=%s->%s", theme, TH[0]);
            theme = TH[0];
        }
        c->last = ts;
        logf_("played %s theme=%s vol=%d latency_ms=%ld", name, theme, s->volume, now - ts);
        if (!g_dry) {
            play_file(path, s->volume);
        }
    }
}
int main(int argc, char **argv) {
    const char *pdl = NULL, *sounds = NULL, *settings = NULL, *cues = NULL, *dry = NULL;
    int drain = 0, have_now = 0;
    long fixed_now = 0, max_bytes = 65536;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--pdl") && i + 1 < argc) {
            pdl = argv[++i];
        } else if (!strcmp(argv[i], "--sounds") && i + 1 < argc) {
            sounds = argv[++i];
        } else if (!strcmp(argv[i], "--settings") && i + 1 < argc) {
            settings = argv[++i];
        } else if (!strcmp(argv[i], "--cues") && i + 1 < argc) {
            cues = argv[++i];
        } else if (!strcmp(argv[i], "--dry-run") && i + 1 < argc) {
            dry = argv[++i];
        } else if (!strcmp(argv[i], "--drain")) {
            drain = 1;
        } else if (!strcmp(argv[i], "--now") && i + 1 < argc) {
            fixed_now = atol(argv[++i]);
            have_now = 1;
        } else if (!strcmp(argv[i], "--max-bytes") && i + 1 < argc) {
            max_bytes = atol(argv[++i]);
        } else {
            fprintf(stderr, "ui_sound_player: unknown or incomplete argument '%s'\n", argv[i]);
            return 2;
        }
    }
    if (!pdl || !sounds || !settings || !cues) {
        fprintf(stderr, "usage: ui_sound_player --pdl F --sounds DIR --settings F --cues F [--dry-run LOG] [--drain] [--now MS] [--max-bytes N]\n");
        return 2;
    }
    if (!load_pdl(pdl)) {
        fprintf(stderr, "ui_sound_player: %s is unreadable or has no THEME / CUE rows\n", pdl);
        return 2;
    }
    if (dry) {
        g_dry = 1;
        g_log = fopen(dry, "a");
        if (!g_log) {
            fprintf(stderr, "ui_sound_player: cannot write %s\n", dry);
            return 2;
        }
    } else {
        pa_sample_spec ss = { PA_SAMPLE_S16LE, 44100, 1 };
        int err = 0;
        g_pa = pa_simple_new(NULL, "LyfeDesk", PA_STREAM_PLAYBACK, NULL, "ui sounds", &ss, NULL, NULL, &err);
        if (!g_pa) {
            fprintf(stderr, "ui_sound_player: no sound server (%s); UI sounds are off\n", pa_strerror(err));
            return 0;
        }
    }
    long off = 0;
    if (!drain) {
        struct stat st;
        if (stat(cues, &st) == 0) {
            off = st.st_size;
        }
    }
    for (;;) {
        struct stat st;
        if (stat(cues, &st) != 0) {
            if (drain) {
                fprintf(stderr, "ui_sound_player: cannot read %s\n", cues);
                return 2;
            }
            off = 0;
        } else {
            if (st.st_size < off) {
                off = 0;
            }
            if (st.st_size > off) {
                FILE *fp = fopen(cues, "r");
                if (fp) {
                    char ln[256];
                    Set set;
                    load_settings(settings, &set);
                    fseek(fp, off, SEEK_SET);
                    while (fgets(ln, sizeof ln, fp)) {
                        size_t l = strlen(ln);
                        if (l == 0 || ln[l - 1] != '\n') {
                            break;      /* a half-written line: wait for the rest */
                        }
                        off += (long)l;
                        long ts = 0;
                        long now = have_now ? fixed_now : (drain ? 0 : now_ms());
                        if (drain && !have_now) {
                            sscanf(ln, "%ld", &ts);
                            now = ts;
                        }
                        handle(ln, &set, sounds, now);
                    }
                    fclose(fp);
                }
            }
            if (!drain && off > max_bytes && stat(cues, &st) == 0 && st.st_size == off) {
                if (truncate(cues, 0) == 0) {
                    off = 0;
                    logf_("truncated cues");
                }
            }
        }
        if (drain) {
            return 0;
        }
        usleep(30000);
    }
}
