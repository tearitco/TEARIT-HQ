#define _GNU_SOURCE
/* class_audio_op - narrate a DAY/NIGHT class text to ONE mp3 WITHOUT new Python (owner 2026-11-09: "no new Python"; the voices are DATA, not a table inside a script).
 *   class_audio_op parse  <class.txt> [--voices <voices.pdl>] [--print lines|chunks|chars|maxchunk]               parse only, no network: CHARACTERS|A,B  then per speech LINE|n|CHAR  and one `kokoro_voice|text` row per chunk, then LINES|n CHUNKS|n CHARS|n
 *   class_audio_op render <class.txt> <out.mp3> [--voices <voices.pdl>] [--threads N] [--force]   narrate every chunk on the TTS machine, join with pauses, 96k mp3 (an existing out.mp3 is refused unless --force; written as .part then renamed)
 *   class_audio_op level  <audio> [--windows N]                             mean volume in dB, one line per equal window (default one window), via ffmpeg volumedetect: lets a case assert the audio is not silent
 *   class_audio_op voices                                                   the Kokoro voice names the TTS machine has (read from its voices file)
 * VOICES (data): harness/voices/class_voices.pdl, rows `VOICE | CHARACTER | kokoro voice | lang | note`; default path = <this binary>/../../voices/class_voices.pdl, or env CLASS_VOICES, or --voices. A character with no row is a HARD ERROR naming it (exit 3):
 * there is no default voice, so a new speaker can never be narrated by accident in somebody else's voice.
 * PARSER (the rules of CLASSES/convert_night_to_audio.py, same intent): header `# Characters: A, B, C` (every name must have a voice row; a header entry that is not an UPPERCASE name, like the numbered scraps in DAY_10, is ignored with a note);
 * body `**NAME:** text`; `# ...`, `=...`, `---` and blank lines are skipped; text lines that follow a speaker line with no blank line between belong to the same speech (the Python parser dropped them); a speaker that is not in the header is an error.
 * Cleaning (Kokoro reads these badly): markdown (** * ` [t](url) >), emoji and symbol blocks, arrows, ' -- ' runs become '. ', an em dash becomes ', ', underscores become spaces. Chunks: at sentence ends, at most 400 characters (a longer sentence is cut at , ; : or a space).
 * Narration: for each chunk `one_line.py <base64 text> <voice> <threads>` runs on the machine named by env PRESENT_TTS_HOST (user@host; never hard-coded here; unset = refuse) over `ssh -n -o BatchMode=yes`, a wav comes back on stdout (same call as video_op).
 * Kokoro is Python on THAT machine: an outside service, not our code. one_line.py always phonemizes en-us, so the `lang` column is recorded as data but not yet used (the older render_lines.py honoured it).
 * Pauses: 450 ms after every speech, 150 ms between chunks of one speech. Every ffmpeg/ffprobe runs under `nice -n 15`; fork/exec only (no system, no popen); a timeout per chunk (remote `timeout 200`, local 240 s); one retry per chunk.
 * Start `render` behind cpu_gate (the case and the report show how). Exit: 0 ok, 1 a step failed or PRESENT_TTS_HOST unset (stderr says which), 2 usage, 3 bad input (unknown character, no lines, unreadable file). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#define P 4096
#define CHUNK 400
#define MAXV 64
#define MAXL 4000
#define MAXC 12000
#define CSZ 520

struct voice { char who[64], kv[40], lang[16]; };
static struct voice V[MAXV]; static int nv;
struct line { char who[64]; int first, n; };
static struct line L[MAXL]; static int nl;
static char (*C)[CSZ]; static int nc;      /* chunk text */
static int Cline[MAXC];                    /* chunk -> line index */
static char chars[MAXV][64]; static int nchars;

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec / 1e9; }

/* fork/exec with stdout to a file (or /dev/null), stdin from /dev/null, a wall timeout (seconds, 0 = none). Returns the exit code, -1 on a signal, -2 on timeout. */
static int g_err_to_out;      /* 1: the child's stderr goes where its stdout goes (ffmpeg volumedetect prints on stderr) */
static int run(char *const argv[], const char *outfile, int timeout_s) {
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) {
        int fd = open(outfile ? outfile : "/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd < 0) _exit(126); dup2(fd, 1); if (g_err_to_out) dup2(fd, 2); close(fd);
        int in = open("/dev/null", O_RDONLY); if (in >= 0) { dup2(in, 0); close(in); }
        execvp(argv[0], argv); _exit(127);
    }
    double t0 = now(); int st = 0;
    for (;;) {
        pid_t r = waitpid(pid, &st, WNOHANG); if (r == pid) break; if (r < 0 && errno != EINTR) return -1;
        if (timeout_s > 0 && now() - t0 > timeout_s) { kill(pid, SIGKILL); waitpid(pid, &st, 0); return -2; }
        usleep(50000);
    }
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/* ---------- voices as data ---------- */
static int load_voices(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) { fprintf(stderr, "class_audio_op: cannot read the voices file %s\n", path); return -1; }
    char ln[1024]; nv = 0;
    while (fgets(ln, sizeof ln, f)) { char *t = trim(ln); if (!*t || *t == '#') continue; char *fld[6] = { 0 }; int n = 0; char *p = t;
        while (n < 6) { fld[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; }
        if (n < 4 || strcmp(trim(fld[0]), "VOICE")) continue;
        if (nv >= MAXV) break;
        snprintf(V[nv].who, sizeof V[nv].who, "%s", trim(fld[1])); snprintf(V[nv].kv, sizeof V[nv].kv, "%s", trim(fld[2])); snprintf(V[nv].lang, sizeof V[nv].lang, "%s", trim(fld[3]));
        if (!V[nv].who[0] || !V[nv].kv[0]) continue; nv++; }
    fclose(f);
    if (!nv) { fprintf(stderr, "class_audio_op: %s has no VOICE rows\n", path); return -1; }
    return 0;
}
static const struct voice *find_voice(const char *who) { for (int i = 0; i < nv; i++) if (!strcmp(V[i].who, who)) return &V[i]; return NULL; }

/* ---------- text cleaning ---------- */
static int is_name_char(unsigned char c) { return (c >= 'A' && c <= 'Z') || c == ' ' || (c >= '0' && c <= '9'); }
static int valid_name(const char *s) { if (!*s || s[0] == ' ') return 0; for (; *s; s++) if (!is_name_char((unsigned char)*s)) return 0; return 1; }

static void put(char *o, size_t cap, size_t *j, const char *s) { size_t n = strlen(s); if (*j + n + 1 < cap) { memcpy(o + *j, s, n); *j += n; o[*j] = 0; } }
static void clean(const char *in, char *o, size_t cap) {
    size_t j = 0; o[0] = 0; const unsigned char *p = (const unsigned char *)in;
    while (*p) {
        unsigned cp; int len;
        if (*p < 0x80) { cp = *p; len = 1; }
        else if ((*p & 0xE0) == 0xC0 && p[1]) { cp = ((p[0] & 0x1Fu) << 6) | (p[1] & 0x3Fu); len = 2; }
        else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { cp = ((p[0] & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu); len = 3; }
        else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) { cp = ((p[0] & 0x07u) << 18) | ((p[1] & 0x3Fu) << 12) | ((p[2] & 0x3Fu) << 6) | (p[3] & 0x3Fu); len = 4; }
        else { p++; continue; }
        if (len == 1) {
            if (cp == '*' || cp == '`') { p++; continue; }
            if (cp == '_') { put(o, cap, &j, " "); p++; continue; }
            if (cp == '-' && p[1] == '-') { while (*p == '-') p++; while (j && o[j - 1] == ' ') o[--j] = 0; put(o, cap, &j, ". "); continue; }      /* a run of 2+ hyphens, as the Python parser did */
            if (cp == '[') { const unsigned char *c = p + 1; while (*c && *c != ']' && *c != '\n') c++;      /* [text](url) -> text */
                if (*c == ']' && c[1] == '(') { const unsigned char *e = c + 2; while (*e && *e != ')') e++; if (*e == ')') { char tmp[CSZ * 2]; size_t n = (size_t)(c - p - 1); if (n >= sizeof tmp) n = sizeof tmp - 1; memcpy(tmp, p + 1, n); tmp[n] = 0; put(o, cap, &j, tmp); p = e + 1; continue; } } }
            char one[2] = { (char)cp, 0 }; put(o, cap, &j, one); p++; continue;
        }
        p += len;
        if (cp == 0x2014) { while (j && o[j - 1] == ' ') o[--j] = 0; put(o, cap, &j, ", "); continue; }
        if (cp == 0x2013) { put(o, cap, &j, "-"); continue; }
        if (cp == 0x2018 || cp == 0x2019) { put(o, cap, &j, "'"); continue; }
        if (cp == 0x201C || cp == 0x201D) { put(o, cap, &j, "\""); continue; }
        if (cp == 0x2026) { put(o, cap, &j, "..."); continue; }
        if (cp == 0xA0) { put(o, cap, &j, " "); continue; }
        if (cp >= 0x1F000 || (cp >= 0x2190 && cp <= 0x21FF) || (cp >= 0x2300 && cp <= 0x23FF) || (cp >= 0x25A0 && cp <= 0x27BF) || (cp >= 0x2900 && cp <= 0x2BFF) ||
            cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D || cp == 0x20E3 || cp == 0x200B || cp == 0xA9 || cp == 0xAE || cp == 0x2122 || (cp >= 0xE000 && cp <= 0xF8FF)) {      /* emoji, arrows, symbols, joiners, selectors */
            if (cp >= 0x2190 && cp <= 0x21FF) put(o, cap, &j, " "); continue; }
        char enc[5]; int k = 0; if (cp < 0x800) { enc[k++] = (char)(0xC0 | (cp >> 6)); enc[k++] = (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { enc[k++] = (char)(0xE0 | (cp >> 12)); enc[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); enc[k++] = (char)(0x80 | (cp & 0x3F)); }
        else { continue; }
        enc[k] = 0; put(o, cap, &j, enc);      /* other letters (accents) stay */
    }
    /* collapse whitespace, trim, drop a leading '>' / '#' blockquote or heading mark */
    char *w = o, *r = o; int sp = 1; for (; *r; r++) { if (*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r') { if (!sp) *w++ = ' '; sp = 1; } else { *w++ = *r; sp = 0; } }
    if (w > o && w[-1] == ' ') w--; *w = 0;
    if (o[0] == '>') { char *t = o + 1; while (*t == ' ') t++; memmove(o, t, strlen(t) + 1); }
}

/* ---------- chunking ---------- */
static void add_chunk(int li, const char *s, size_t n) {
    if (nc >= MAXC) { fprintf(stderr, "class_audio_op: too many chunks (max %d)\n", MAXC); exit(3); }
    while (n && *s == ' ') { s++; n--; } while (n && s[n - 1] == ' ') n--;
    if (!n) return;
    if (n >= CSZ) n = CSZ - 1;
    memcpy(C[nc], s, n); C[nc][n] = 0; Cline[nc] = li; L[li].n++; nc++;
}
/* a sentence longer than CHUNK: cut at the last , ; : or space inside the first CHUNK characters */
static void add_long(int li, const char *s) {
    size_t n = strlen(s);
    while (n > CHUNK) { size_t cut = 0; for (size_t i = CHUNK; i > CHUNK / 2; i--) if (s[i - 1] == ',' || s[i - 1] == ';' || s[i - 1] == ':') { cut = i; break; }
        if (!cut) for (size_t i = CHUNK; i > CHUNK / 2; i--) if (s[i] == ' ') { cut = i; break; }
        if (!cut) cut = CHUNK; add_chunk(li, s, cut); s += cut; n -= cut; }
    add_chunk(li, s, n);
}
static void chunk_text(int li, const char *text) {
    char cur[CSZ * 2] = ""; size_t cl = 0; const char *p = text;
    while (*p) {
        const char *e = p; while (*e) { if ((*e == '.' || *e == '!' || *e == '?') && (e[1] == ' ' || !e[1])) { e++; break; } e++; }      /* the sentence runs to a . ! ? followed by a space */
        char part[CSZ * 4]; size_t pn = (size_t)(e - p); if (pn >= sizeof part) pn = sizeof part - 1; memcpy(part, p, pn); part[pn] = 0; char *pt = trim(part); pn = strlen(pt);
        if (pn) {
            if (cl && cl + 1 + pn > CHUNK) { add_chunk(li, cur, cl); cl = 0; cur[0] = 0; }
            if (pn > CHUNK) { if (cl) { add_chunk(li, cur, cl); cl = 0; cur[0] = 0; } add_long(li, pt); }
            else { if (cl) cur[cl++] = ' '; memcpy(cur + cl, pt, pn); cl += pn; cur[cl] = 0; }
        }
        p = e; while (*p == ' ') p++;
    }
    if (cl) add_chunk(li, cur, cl);
}

/* ---------- parse ---------- */
static void end_speech(int li, char *buf) {
    if (li < 0) return;
    char cleaned[8192]; clean(buf, cleaned, sizeof cleaned);
    if (cleaned[0]) chunk_text(li, cleaned);
    buf[0] = 0;
}
static int parse_class(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) { fprintf(stderr, "class_audio_op: cannot read %s\n", path); return 3; }
    C = malloc((size_t)MAXC * CSZ); if (!C) return 1;
    nl = nc = nchars = 0; int have_header = 0; char ln[16384]; static char buf[16384]; buf[0] = 0; int cur = -1; int rc = 0;
    while (fgets(ln, sizeof ln, f)) {
        char *s = trim(ln);
        if (!strncmp(s, "# Characters:", 13)) { have_header = 1; char *p = s + 13;
            while (p && *p) { char *comma = strchr(p, ','); if (comma) *comma = 0; char *nm = trim(p);
                if (valid_name(nm)) { if (nchars < MAXV) snprintf(chars[nchars++], sizeof chars[0], "%s", nm); } else if (*nm) fprintf(stderr, "class_audio_op: note: header entry ignored (not an UPPERCASE name): %.40s\n", nm);
                p = comma ? comma + 1 : NULL; }
            continue; }
        if (!*s) { end_speech(cur, buf); cur = -1; continue; }
        if (*s == '#' || *s == '=' || !strcmp(s, "---")) continue;
        if (!strncmp(s, "**", 2)) { char *c = strstr(s + 2, ":**");
            if (c) { char who[64]; size_t n = (size_t)(c - (s + 2)); if (n < sizeof who) { memcpy(who, s + 2, n); who[n] = 0;
                    if (valid_name(who)) { end_speech(cur, buf); cur = -1;
                        if (!find_voice(who)) { fprintf(stderr, "class_audio_op: no voice for character '%s' (add a VOICE row to the voices file; there is no default voice)\n", who); rc = 3; break; }
                        if (have_header) { int ok = 0; for (int i = 0; i < nchars; i++) if (!strcmp(chars[i], who)) ok = 1; if (!ok) { fprintf(stderr, "class_audio_op: speaker '%s' is not in the '# Characters:' header\n", who); rc = 3; break; } }
                        if (nl >= MAXL) { fprintf(stderr, "class_audio_op: too many lines\n"); rc = 3; break; }
                        cur = nl++; snprintf(L[cur].who, sizeof L[cur].who, "%s", who); L[cur].first = nc; L[cur].n = 0; snprintf(buf, sizeof buf, "%s", trim(c + 3)); continue; } } } }
        if (cur >= 0) { size_t bl = strlen(buf); if (bl + strlen(s) + 2 < sizeof buf) { if (bl) buf[bl++] = ' '; strcpy(buf + bl, s); } }      /* continuation of the same speech */
    }
    if (!rc) end_speech(cur, buf);
    fclose(f);
    if (rc) return rc;
    for (int i = 0; i < nchars; i++) if (!find_voice(chars[i])) { fprintf(stderr, "class_audio_op: no voice for character '%s' (named in the '# Characters:' header)\n", chars[i]); return 3; }
    int speaking = 0; for (int i = 0; i < nl; i++) if (L[i].n) speaking++;
    if (!speaking) { fprintf(stderr, "class_audio_op: no spoken lines parsed in %s\n", path); return 3; }
    return 0;
}
static void default_voices(char *o, size_t cap) {
    const char *e = getenv("CLASS_VOICES"); if (e && *e) { snprintf(o, cap, "%s", e); return; }
    char exe[P]; ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1); if (n <= 0) { snprintf(o, cap, "class_voices.pdl"); return; } exe[n] = 0;
    char *sl = strrchr(exe, '/'); if (sl) *sl = 0; snprintf(o, cap, "%s/../../voices/class_voices.pdl", exe);
}
static void print_parse(const char *key) {
    if (key) { long chs = 0, mx = 0; int lines = 0; for (int i = 0; i < nl; i++) if (L[i].n) lines++; for (int i = 0; i < nc; i++) { long n = (long)strlen(C[i]); chs += n; if (n > mx) mx = n; }
        if (!strcmp(key, "lines")) printf("%d\n", lines); else if (!strcmp(key, "chunks")) printf("%d\n", nc); else if (!strcmp(key, "chars")) printf("%ld\n", chs); else if (!strcmp(key, "maxchunk")) printf("%ld\n", mx); else fprintf(stderr, "class_audio_op: --print takes lines, chunks, chars or maxchunk\n");
        return; }
    printf("CHARACTERS|"); for (int i = 0; i < nchars; i++) printf("%s%s", i ? "," : "", chars[i]);
    if (!nchars) { int first = 1; for (int i = 0; i < nl; i++) { int dup = 0; for (int j = 0; j < i; j++) if (!strcmp(L[j].who, L[i].who)) dup = 1; if (!dup) { printf("%s%s", first ? "" : ",", L[i].who); first = 0; } } }
    printf("\n"); long chs = 0; int lines = 0;
    for (int i = 0; i < nl; i++) { if (!L[i].n) continue; lines++; const struct voice *v = find_voice(L[i].who); printf("LINE|%d|%s\n", lines, L[i].who);
        for (int k = 0; k < L[i].n; k++) { printf("%s|%s\n", v->kv, C[L[i].first + k]); chs += (long)strlen(C[L[i].first + k]); } }
    size_t mx = 0; for (int i = 0; i < nc; i++) if (strlen(C[i]) > mx) mx = strlen(C[i]);
    printf("LINES|%d\nCHUNKS|%d\nCHARS|%ld\nMAXCHUNK|%zu\n", lines, nc, chs, mx);
}

/* ---------- narration ---------- */
static void b64(const char *s, char *o) { static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; size_t n = strlen(s), j = 0;
    for (size_t i = 0; i < n; i += 3) { unsigned v = (unsigned)(unsigned char)s[i] << 16 | (i + 1 < n ? (unsigned)(unsigned char)s[i + 1] << 8 : 0) | (i + 2 < n ? (unsigned char)s[i + 2] : 0u); o[j++] = T[v >> 18 & 63]; o[j++] = T[v >> 12 & 63]; o[j++] = i + 1 < n ? T[v >> 6 & 63] : '='; o[j++] = i + 2 < n ? T[v & 63] : '='; } o[j] = 0; }
static int host_ok(const char *h) { if (!h || !*h || h[0] == '-') return 0; for (; *h; h++) if (!((*h >= 'a' && *h <= 'z') || (*h >= 'A' && *h <= 'Z') || (*h >= '0' && *h <= '9') || *h == '@' || *h == '.' || *h == '_' || *h == '-' || *h == ':')) return 0; return 1; }
static const char *need_host(void) {
    const char *h = getenv("PRESENT_TTS_HOST");
    if (!h || !*h) { fprintf(stderr, "class_audio_op: PRESENT_TTS_HOST is not set (user@host of the narration machine); refusing: no host is built in and narration never runs locally\n"); return NULL; }
    if (!host_ok(h)) { fprintf(stderr, "class_audio_op: PRESENT_TTS_HOST has characters that are not allowed in a host name\n"); return NULL; }
    return h;
}
/* the Kokoro voice names on the TTS machine (names of the .npy entries of its voices file); returns the count, names in out[] */
static int remote_voices(const char *host, char out[][40], int max, const char *tmp) {
    char o[P]; snprintf(o, sizeof o, "%s/voices.txt", tmp);
    char *av[] = { "ssh", "-n", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", (char *)host, "python3 -m zipfile -l ~/house-tts/voices-v1.0.bin", NULL };
    if (run(av, o, 60) != 0) { fprintf(stderr, "class_audio_op: could not list the voices on %s (ssh failed)\n", host); return -1; }
    FILE *f = fopen(o, "r"); if (!f) return -1; char ln[512]; int n = 0;
    while (fgets(ln, sizeof ln, f)) { char *e = strstr(ln, ".npy"); if (!e) continue; *e = 0; char *nm = trim(ln); if (n < max && *nm && strlen(nm) < 40 && !strchr(nm, ' ')) snprintf(out[n++], 40, "%s", nm); }
    fclose(f); return n;
}
static int mk_tmp(char *tmp, size_t cap) { const char *b = getenv("TMPDIR"); snprintf(tmp, cap, "%s/class_audio_XXXXXX", b && *b ? b : "/tmp"); return mkdtemp(tmp) ? 0 : -1; }
static void rm_tmp(const char *tmp, int n_wav) {
    char p[P]; const char *fixed[] = { "vol.txt", "dur.txt", "voices.txt", "list.txt", "p450.wav", "p150.wav" };
    for (size_t i = 0; i < sizeof fixed / sizeof *fixed; i++) { snprintf(p, sizeof p, "%s/%s", tmp, fixed[i]); unlink(p); }
    for (int i = 0; i < n_wav; i++) { snprintf(p, sizeof p, "%s/c%04d.wav", tmp, i); unlink(p); }
    rmdir(tmp);
}
static double probe_dur(const char *file, const char *tmp) {
    char o[P]; snprintf(o, sizeof o, "%s/dur.txt", tmp); char *av[] = { "nice", "-n", "15", "ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", (char *)file, NULL };
    if (run(av, o, 60) != 0) return 0; FILE *f = fopen(o, "r"); double d = 0; if (f) { if (fscanf(f, "%lf", &d) != 1) d = 0; fclose(f); } return d;
}
/* canonical PCM wav header: sample rate and channels (soundfile writes fmt first) */
static int wav_fmt(const char *path, unsigned *sr, unsigned *ch) {
    FILE *f = fopen(path, "rb"); if (!f) return -1; unsigned char h[44]; size_t n = fread(h, 1, sizeof h, f); fclose(f);
    if (n < 44 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4) || memcmp(h + 12, "fmt ", 4)) return -1;
    *ch = h[22] | (unsigned)h[23] << 8; *sr = h[24] | (unsigned)h[25] << 8 | (unsigned)h[26] << 16 | (unsigned)h[27] << 24; return 0;
}

static int cmd_render(const char *cls, const char *out, const char *vpath, int threads, int force) {
    const char *host = need_host(); if (!host) return 1;
    if (!force && access(out, F_OK) == 0) { fprintf(stderr, "class_audio_op: %s exists; refusing to overwrite (use --force)\n", out); return 1; }
    if (load_voices(vpath)) return 3;
    int rc = parse_class(cls); if (rc) return rc;
    char tmp[P]; if (mk_tmp(tmp, sizeof tmp)) { fprintf(stderr, "class_audio_op: cannot make a scratch folder\n"); return 1; }
    /* every voice the class needs must exist on the TTS machine, checked BEFORE the first narration */
    static char rv[128][40]; int nrv = remote_voices(host, rv, 128, tmp); if (nrv < 0) { rm_tmp(tmp, 0); return 1; }
    for (int i = 0; i < nl; i++) { const struct voice *v = find_voice(L[i].who); int ok = 0; for (int k = 0; k < nrv; k++) if (!strcmp(rv[k], v->kv)) ok = 1;
        if (!ok) { fprintf(stderr, "class_audio_op: the voice '%s' (character %s) does not exist on %s\n", v->kv, L[i].who, host); rm_tmp(tmp, 0); return 3; } }
    int total = 0; for (int i = 0; i < nl; i++) total += L[i].n;
    double t0 = now(); unsigned sr0 = 0, ch0 = 0; int done = 0; double audio = 0; char wpath[P];
    for (int i = 0; i < nl; i++) { const struct voice *v = find_voice(L[i].who);
        for (int k = 0; k < L[i].n; k++) {
            int ci = L[i].first + k; static char e[CSZ * 2]; b64(C[ci], e); snprintf(wpath, sizeof wpath, "%s/c%04d.wav", tmp, ci);
            char rcmd[CSZ * 3]; snprintf(rcmd, sizeof rcmd, "cd ~/house-tts && PYTHONPATH=lib nice -n 10 timeout 200 python3 one_line.py %s %s %d", e, v->kv, threads);
            char *tv[] = { "ssh", "-n", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", "-o", "ServerAliveInterval=15", (char *)host, rcmd, NULL };
            int ok = 0; for (int attempt = 0; attempt < 2 && !ok; attempt++) { int r = run(tv, wpath, 240); unsigned sr, ch;
                if (r == 0 && !wav_fmt(wpath, &sr, &ch)) { if (!sr0) { sr0 = sr; ch0 = ch; } if (sr == sr0 && ch == ch0) ok = 1; }
                if (!ok) fprintf(stderr, "class_audio_op: chunk %d/%d (%s) attempt %d failed (exit %d)\n", ci + 1, total, L[i].who, attempt + 1, r); }
            if (!ok) { fprintf(stderr, "class_audio_op: narration failed at chunk %d/%d of %s (ssh %s)\n", ci + 1, total, cls, host); rm_tmp(tmp, nc); return 1; }
            double d = probe_dur(wpath, tmp); if (d < 0.2) { fprintf(stderr, "class_audio_op: chunk %d decodes to %.2f s: not audio\n", ci + 1, d); rm_tmp(tmp, nc); return 1; }
            audio += d; done++;
            printf("chunk %d/%d %s %s %.1f s audio, %.0f s elapsed\n", done, total, L[i].who, v->kv, d, now() - t0); fflush(stdout);
        } }
    double narr = now() - t0;
    char p450[P], p150[P]; snprintf(p450, sizeof p450, "%s/p450.wav", tmp); snprintf(p150, sizeof p150, "%s/p150.wav", tmp);
    for (int w = 0; w < 2; w++) { char src[64], dur[16]; snprintf(src, sizeof src, "anullsrc=r=%u:cl=%s", sr0, ch0 == 1 ? "mono" : "stereo"); snprintf(dur, sizeof dur, w ? "0.15" : "0.45");
        char *av[] = { "nice", "-n", "15", "ffmpeg", "-y", "-nostdin", "-loglevel", "error", "-f", "lavfi", "-i", src, "-t", dur, "-c:a", "pcm_s16le", w ? p150 : p450, NULL };
        if (run(av, NULL, 120) != 0) { fprintf(stderr, "class_audio_op: ffmpeg could not make the pause\n"); rm_tmp(tmp, nc); return 1; } }
    char lst[P]; snprintf(lst, sizeof lst, "%s/list.txt", tmp); FILE *l = fopen(lst, "w"); if (!l) { rm_tmp(tmp, nc); return 1; }
    for (int i = 0; i < nl; i++) for (int k = 0; k < L[i].n; k++) { int ci = L[i].first + k; fprintf(l, "file '%s/c%04d.wav'\n", tmp, ci); fprintf(l, "file '%s'\n", k + 1 < L[i].n ? p150 : p450); }
    fclose(l);
    char part[P]; snprintf(part, sizeof part, "%s.part", out);
    char *fa[] = { "nice", "-n", "15", "ffmpeg", "-y", "-nostdin", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", lst, "-c:a", "libmp3lame", "-b:a", "96k", "-f", "mp3", part, NULL };
    if (run(fa, NULL, 900) != 0) { fprintf(stderr, "class_audio_op: the final ffmpeg failed\n"); unlink(part); rm_tmp(tmp, nc); return 1; }
    double mp3 = probe_dur(part, tmp);
    if (mp3 < audio * 0.9) { fprintf(stderr, "class_audio_op: the mp3 lasts %.1f s but the narration alone is %.1f s: refusing it\n", mp3, audio); unlink(part); rm_tmp(tmp, nc); return 1; }
    if (rename(part, out)) { fprintf(stderr, "class_audio_op: cannot rename %s\n", part); rm_tmp(tmp, nc); return 1; }
    rm_tmp(tmp, nc);
    printf("mp3=%s lines=%d chunks=%d narration_audio_s=%.1f mp3_s=%.1f wall_narration_s=%.0f factor=%.2f\n", out, nl, total, audio, mp3, narr, narr > 0 ? audio / narr : 0.0);
    return 0;
}

/* level: mean volume (dB) of an audio file, or of N equal windows of it (one number per line); measured by ffmpeg volumedetect (niced); no number is an error */
static int cmd_level(const char *file, int windows) {
    char tmp[P]; if (mk_tmp(tmp, sizeof tmp)) return 1;
    double dur = probe_dur(file, tmp); if (dur <= 0) { fprintf(stderr, "class_audio_op: cannot read the duration of %s\n", file); rm_tmp(tmp, 0); return 1; }
    if (windows < 1) windows = 1; char o[P]; snprintf(o, sizeof o, "%s/vol.txt", tmp); int rc = 0;
    for (int w = 0; w < windows; w++) {
        char ss[32], tt[32]; snprintf(ss, sizeof ss, "%.3f", dur * w / windows); snprintf(tt, sizeof tt, "%.3f", dur / windows);
        char *av[] = { "nice", "-n", "15", "ffmpeg", "-nostdin", "-hide_banner", "-ss", ss, "-t", tt, "-i", (char *)file, "-af", "volumedetect", "-f", "null", "-", NULL };
        g_err_to_out = 1; int r = run(av, o, 300); g_err_to_out = 0;
        FILE *f = fopen(o, "r"); char ln[1024]; double mean = 0; int got = 0;
        while (f && fgets(ln, sizeof ln, f)) { char *m = strstr(ln, "mean_volume:"); if (m && sscanf(m + 12, "%lf", &mean) == 1) got = 1; }
        if (f) fclose(f);
        if (r != 0 || !got) { fprintf(stderr, "class_audio_op: no mean volume for window %d of %s\n", w + 1, file); rc = 1; break; }
        printf("%.1f\n", mean);
    }
    unlink(o); rm_tmp(tmp, 0); return rc;
}
int main(int argc, char **argv) {
    char vpath[P]; default_voices(vpath, sizeof vpath); int threads = 6, force = 0; const char *pos[4]; int np = 0; int windows = 1; const char *pkey = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--voices") && i + 1 < argc) snprintf(vpath, sizeof vpath, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--threads") && i + 1 < argc) { threads = atoi(argv[++i]); if (threads < 1 || threads > 16) threads = 6; }
        else if (!strcmp(argv[i], "--force")) force = 1;
        else if (!strcmp(argv[i], "--print") && i + 1 < argc) pkey = argv[++i];
        else if (!strcmp(argv[i], "--windows") && i + 1 < argc) windows = atoi(argv[++i]);
        else if (np < 4) pos[np++] = argv[i];
    }
    if (argc >= 2 && !strcmp(argv[1], "parse") && np == 1) { if (load_voices(vpath)) return 3; int rc = parse_class(pos[0]); if (rc) return rc; print_parse(pkey); return 0; }
    if (argc >= 2 && !strcmp(argv[1], "render") && np == 2) return cmd_render(pos[0], pos[1], vpath, threads, force);
    if (argc >= 2 && !strcmp(argv[1], "level") && np == 1) return cmd_level(pos[0], windows);
    if (argc >= 2 && !strcmp(argv[1], "voices") && np == 0) {
        const char *host = need_host(); if (!host) return 1; char tmp[P]; if (mk_tmp(tmp, sizeof tmp)) return 1;
        static char rv[128][40]; int n = remote_voices(host, rv, 128, tmp); rm_tmp(tmp, 0); if (n < 0) return 1;
        for (int i = 0; i < n; i++) printf("%s\n", rv[i]); return 0; }
    fprintf(stderr, "usage: class_audio_op parse <class.txt> [--voices f] | render <class.txt> <out.mp3> [--voices f] [--threads N] [--force] | voices   (render/voices need env PRESENT_TTS_HOST=user@host)\n");
    return 2;
}
