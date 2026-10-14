/* sfx_hai_manager - the sfx-hai SFX toy window (SCAFFOLD, 2026-10-14). Design: BOOK 19.audio/SFX-TOY-DESIGN.md. Handbook: README.md next to this file.
 * Forked by the renderer (module tag in sfx-hai.xhtpm) as: sfx_hai_manager <house_root> [package_dir]. Writes state/ui.txt (+ canvas.raw/receipt for the waveform).
 * WHAT IT DOES: lists recipes (sfx.pdl in this folder, then state/sfx2.pdl working copies, latest row per id wins), selects one, edits frequency (f0 +-10%) and length (dur +-10%),
 *   makes the sound (forks ops/sfx_make: sox stand-in + two numeric checks: duration and peak), plays it, draws the waveform, takes the two judges' verdicts (JUDGE rows in state/verdicts.txt).
 * VERBS (state/request.txt, appended by ops/sfx_hai_req): SEL:<i> WORDS:<text> FREQ:+|- DUR:+|- MAKE VERDICT:<claude|owner>:<yes|no>      (WORDS filters the list by tag; empty clears)
 * NOT BUILT (BOOK 19.audio/VOICE-ROADMAP.md S3): the in-house C renderer, the remaining checks (dominant frequency, DC offset), recipe mutation batches, candidate edits into the bank, DAW import.
 * Style: state files only, size-growth marker for requests, no logging. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#define PL 4096
#define CW 640
#define CH 240
typedef struct { char id[32], tags[64], kind[12], wave[12]; double f0, f1, dur, attack, decay, vol, lp; } Sfx;
static Sfx S[64]; static int nS, sel; static int shown[64], nShown;
static char g_pkg[PL], g_house[PL], g_state[PL], g_words[200], g_msg[200] = "ready", g_last[PL], g_say[300] = "no sound made yet";
static long req_off, say_off; static int dirty = 1; static pid_t say_pid; static unsigned char *comp;
static void handle(const char *cmd);
static int split_bar(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) { while (*f[i] == ' ') f[i]++; char *e = f[i] + strlen(f[i]); while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }
static void draw_wave(void) {
    memset(comp, 0, (size_t)CW * CH * 4); for (int i = 0; i < CW * CH; i++) { comp[i * 4] = 14; comp[i * 4 + 1] = 14; comp[i * 4 + 2] = 20; comp[i * 4 + 3] = 255; }
    for (int x = 0; x < CW; x++) { unsigned char *d = comp + ((size_t)(CH / 2) * CW + x) * 4; d[0] = 50; d[1] = 50; d[2] = 70; }
    if (!g_last[0]) return; FILE *f = fopen(g_last, "rb"); if (!f) return; fseek(f, 0, SEEK_END); long sz = ftell(f); if (sz < 100) { fclose(f); return; } fseek(f, 44, SEEK_SET);   /* sox writes a plain 44-byte header */
    long n = (sz - 44) / 2; short *s = malloc((size_t)n * 2); if (!s || fread(s, 2, (size_t)n, f) != (size_t)n) { free(s); fclose(f); return; } fclose(f);
    for (int x = 0; x < CW; x++) { long a = n * x / CW, b = n * (x + 1) / CW; if (b <= a) b = a + 1; int mn = 0, mx = 0; for (long i = a; i < b && i < n; i++) { if (s[i] < mn) mn = s[i]; if (s[i] > mx) mx = s[i]; }
        int y0 = CH / 2 - mx * (CH / 2 - 4) / 32768, y1 = CH / 2 - mn * (CH / 2 - 4) / 32768; for (int y = y0; y <= y1; y++) { unsigned char *d = comp + ((size_t)y * CW + x) * 4; d[0] = 90; d[1] = 230; d[2] = 140; } }
    free(s);
}
static void write_canvas(void) { draw_wave(); char raw[PL], tmp[PL], rec[PL]; snprintf(raw, sizeof raw, "%s/canvas.raw", g_state); snprintf(tmp, sizeof tmp, "%s/canvas.raw.tmp", g_state); snprintf(rec, sizeof rec, "%s/canvas.receipt.txt", g_state);
    FILE *f = fopen(tmp, "wb"); if (f) { fwrite(comp, 1, (size_t)CW * CH * 4, f); fclose(f); rename(tmp, raw); } FILE *r = fopen(rec, "w"); if (r) { fprintf(r, "overlay_w=%d\noverlay_h=%d\n", CW, CH); fclose(r); } }
static void poll_requests(void) { char p[PL]; snprintf(p, sizeof p, "%s/request.txt", g_state); struct stat st; if (stat(p, &st) || st.st_size <= req_off) return; FILE *f = fopen(p, "r"); if (!f) return; fseek(f, req_off, SEEK_SET);
    char line[512]; while (fgets(line, sizeof line, f)) { size_t n = strlen(line); if (!n || line[n - 1] != '\n') break; line[n - 1] = 0; req_off += (long)n; if (line[0]) handle(line); } fclose(f); }
static int parent_alive(void) { char p[PL]; snprintf(p, sizeof p, "%s/module_parent.pid", g_pkg); FILE *f = fopen(p, "r"); if (!f) return 1; int pid = 0; if (fscanf(f, "%d", &pid) != 1) pid = 0; fclose(f); return pid <= 0 || kill(pid, 0) == 0; }
static void bye(int s) { (void)s; _exit(0); }

static Sfx *sfx_by_id(const char *id) { for (int i = 0; i < nS; i++) if (!strcmp(S[i].id, id)) return &S[i]; if (nS >= 64) return NULL; Sfx *s = &S[nS++]; memset(s, 0, sizeof *s); snprintf(s->id, sizeof s->id, "%s", id); return s; }
static void parse_params(Sfx *s, char *p) { char *t, *sv; for (t = strtok_r(p, " ", &sv); t; t = strtok_r(NULL, " ", &sv)) { char *e = strchr(t, '='); if (!e) continue; *e = 0; const char *k = t, *val = e + 1;
    if (!strcmp(k, "wave")) snprintf(s->wave, sizeof s->wave, "%s", val); else if (!strcmp(k, "f0")) s->f0 = atof(val); else if (!strcmp(k, "f1")) s->f1 = atof(val); else if (!strcmp(k, "dur")) s->dur = atof(val);
    else if (!strcmp(k, "attack")) s->attack = atof(val); else if (!strcmp(k, "decay")) s->decay = atof(val); else if (!strcmp(k, "vol")) s->vol = atof(val); else if (!strcmp(k, "lp")) s->lp = atof(val); } }
static void load_sfx(void) {
    char ln[512], *f[6]; nS = 0; const char *files[2] = { "sfx.pdl", "state/sfx2.pdl" };
    for (int k = 0; k < 2; k++) { char p[PL]; snprintf(p, sizeof p, "%s/%s", g_pkg, files[k]); FILE *fp = fopen(p, "r"); if (!fp) continue;
        while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "SFX", 3)) continue; int n = split_bar(ln, f, 5); if (n < 5) continue; Sfx *s = sfx_by_id(f[1]); if (!s) continue; snprintf(s->tags, sizeof s->tags, "%s", f[2]); snprintf(s->kind, sizeof s->kind, "%s", f[3]); parse_params(s, f[4]); } fclose(fp); }
    nShown = 0; for (int i = 0; i < nS; i++) if (!g_words[0] || strstr(S[i].tags, g_words) || strstr(S[i].id, g_words)) shown[nShown++] = i;
    if (sel >= nShown) sel = 0;
}
static void save_sfx(Sfx *s) { char p[PL]; snprintf(p, sizeof p, "%s/sfx2.pdl", g_state); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "SFX | %s | %s | %s | wave=%s f0=%.0f f1=%.0f dur=%.3f attack=%.3f decay=%.3f vol=%.2f lp=%.0f\n", s->id, s->tags, s->kind, s->wave, s->f0, s->f1, s->dur, s->attack, s->decay, s->vol, s->lp); fclose(f); } }
static void write_ui(void) {
    char dst[PL], tmp[PL]; snprintf(dst, sizeof dst, "%s/ui.txt", g_state); snprintf(tmp, sizeof tmp, "%s/ui.txt.tmp", g_state); FILE *f = fopen(tmp, "w"); if (!f) return; Sfx *c = nShown ? &S[shown[sel]] : NULL;
    fprintf(f, "canvas_raw=%s/canvas.raw\nstatus_line=sfx-hai  sounds=%d  shown=%d  selected=%s\ns_count=%d\n", g_state, nS, nShown, c ? c->id : "-", nShown);
    for (int i = 0; i < nShown; i++) { Sfx *s = &S[shown[i]]; fprintf(f, "s_%d_label=%s%s  [%s]\ns_%d_class=%s\ns_%d_action='%s/ops/+x/sfx_hai_req.+x' 'SEL:%d'\n", i, i == sel ? "* " : "", s->id, s->tags, i, i == sel ? "active" : "", i, g_pkg, i); }
    if (c) fprintf(f, "sel_line=%s f0=%.0f f1=%.0f dur=%.2f vol=%.2f\n", c->wave, c->f0, c->f1, c->dur, c->vol); else fprintf(f, "sel_line=-\n");
    fprintf(f, "say_line=%s\nmsg=%s\nwords_line=%s\n", g_say, g_msg, g_words[0] ? g_words : "(type a tag in the words field to filter the list)");
    { const char *k[] = { "fm", "fp", "dm", "dp", "make", "jc_yes", "jc_no", "jo_yes", "jo_no", "play" }; const char *vb[] = { "FREQ:-", "FREQ:+", "DUR:-", "DUR:+", "MAKE", "VERDICT:claude:yes", "VERDICT:claude:no", "VERDICT:owner:yes", "VERDICT:owner:no", "PLAY" };
      for (int i = 0; i < 10; i++) fprintf(f, "%s_action='%s/ops/+x/sfx_hai_req.+x' '%s'\n", k[i], g_pkg, vb[i]); }
    fprintf(f, "words_action='%s/ops/+x/sfx_hai_req.+x' 'WORDS'\n", g_pkg); fclose(f); rename(tmp, dst);
}
static void handle(const char *cmd) {
    Sfx *c = nShown ? &S[shown[sel]] : NULL;
    if (!strncmp(cmd, "SEL:", 4)) { int i = atoi(cmd + 4); if (i >= 0 && i < nShown) sel = i; }
    else if (!strncmp(cmd, "WORDS", 5)) { const char *w = cmd[5] == ':' ? cmd + 6 : ""; while (*w == ' ') w++; snprintf(g_words, sizeof g_words, "%s", w); for (char *q = g_words; *q; q++) if (*q == '|') *q = ' '; size_t n = strlen(g_words); while (n && g_words[n - 1] == ' ') g_words[--n] = 0; sel = 0; load_sfx(); snprintf(g_msg, sizeof g_msg, g_words[0] ? "filter: %d of %d shown" : "filter cleared (%d of %d shown)", nShown, nS); }
    else if (c && !strncmp(cmd, "FREQ:", 5)) { if (c->f0 > 0) { c->f0 *= cmd[5] == '-' ? 0.9 : 1.1; if (c->f1 > 0) c->f1 *= cmd[5] == '-' ? 0.9 : 1.1; save_sfx(c); snprintf(g_msg, sizeof g_msg, "%s f0 %.0f Hz (working copy)", c->id, c->f0); } else snprintf(g_msg, sizeof g_msg, "this recipe has no frequency (noise)"); }
    else if (c && !strncmp(cmd, "DUR:", 4)) { c->dur *= cmd[4] == '-' ? 0.9 : 1.1; if (c->dur < 0.03) c->dur = 0.03; if (c->dur > 3) c->dur = 3; save_sfx(c); snprintf(g_msg, sizeof g_msg, "%s dur %.2f s (working copy)", c->id, c->dur); }
    else if (c && !strcmp(cmd, "MAKE")) {
        if (say_pid > 0) snprintf(g_msg, sizeof g_msg, "still making the last sound");
        else { char f0[24], f1[24], du[24], at[24], de[24], vo[24], lp[24], label[96]; snprintf(f0, sizeof f0, "%.0f", c->f0); snprintf(f1, sizeof f1, "%.0f", c->f1); snprintf(du, sizeof du, "%.3f", c->dur); snprintf(at, sizeof at, "%.3f", c->attack); snprintf(de, sizeof de, "%.3f", c->decay); snprintf(vo, sizeof vo, "%.2f", c->vol); snprintf(lp, sizeof lp, "%.0f", c->lp);
            snprintf(label, sizeof label, "%s_f%s_d%.0f", c->id, f0, c->dur * 1000); char op[PL]; snprintf(op, sizeof op, "%s/ops/+x/sfx_make.+x", g_pkg); pid_t p = fork(); if (p == 0) { setsid(); execl(op, op, g_pkg, label, c->wave, f0, f1, du, at, de, vo, lp, (char *)NULL); _exit(127); } if (p > 0) { say_pid = p; snprintf(g_msg, sizeof g_msg, "making %s ...", c->id); } } }
    else if (!strcmp(cmd, "PLAY")) { if (!g_last[0] || access(g_last, R_OK)) snprintf(g_msg, sizeof g_msg, "no sound to play yet: make one first"); else { pid_t p = fork(); if (p == 0) { setsid(); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 1); dup2(nul, 2); } execlp("paplay", "paplay", "--volume=32000", g_last, (char *)NULL); _exit(127); } if (p > 0) { snprintf(g_msg, sizeof g_msg, "playing %s", strrchr(g_last, '/') ? strrchr(g_last, '/') + 1 : g_last); signal(SIGCHLD, SIG_IGN); } } }
    else if (c && !strncmp(cmd, "VERDICT:", 8)) { char who[8] = "", yn[4] = ""; if (sscanf(cmd + 8, "%7[^:]:%3s", who, yn) == 2 && g_last[0]) { char p[PL]; snprintf(p, sizeof p, "%s/verdicts.txt", g_state); FILE *f = fopen(p, "a"); const char *b = strrchr(g_last, '/'); char date[16]; time_t t = time(NULL); strftime(date, sizeof date, "%Y-%m-%d", localtime(&t)); if (f) { fprintf(f, "JUDGE | %s | %s | %s | %s\n", b ? b + 1 : g_last, who, yn, date); fclose(f); } snprintf(g_msg, sizeof g_msg, "%s said %s", who, yn); } else snprintf(g_msg, sizeof g_msg, "make a sound first"); }
    dirty = 1;
}
static void poll_say(void) {
    if (say_pid > 0) { int st; if (waitpid(say_pid, &st, WNOHANG) == say_pid) { say_pid = 0; dirty = 1; } }
    char p[PL]; snprintf(p, sizeof p, "%s/say.txt", g_state); struct stat s; if (stat(p, &s) || s.st_size <= say_off) return; FILE *f = fopen(p, "r"); if (!f) return; fseek(f, say_off, SEEK_SET); char ln[PL];
    while (fgets(ln, sizeof ln, f)) { size_t n = strlen(ln); if (!n || ln[n - 1] != '\n') break; ln[n - 1] = 0; say_off += (long)n; snprintf(g_say, sizeof g_say, "%.290s", ln); }
    fclose(f); snprintf(p, sizeof p, "%s/last.txt", g_state); f = fopen(p, "r"); if (f) { if (fgets(g_last, sizeof g_last, f)) g_last[strcspn(g_last, "\n")] = 0; fclose(f); } dirty = 1;
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: sfx_hai_manager <house_root> [package_dir]\n"); return 1; }
    snprintf(g_house, sizeof g_house, "%s", argv[1]); snprintf(g_pkg, sizeof g_pkg, "%s", argc > 2 ? argv[2] : ""); if (!g_pkg[0]) snprintf(g_pkg, sizeof g_pkg, "%s/&.hq-apps/sfx-hai", g_house);
    snprintf(g_state, sizeof g_state, "%s/state", g_pkg); mkdir(g_state, 0755); signal(SIGTERM, bye); comp = calloc((size_t)CW * CH * 4, 1); if (!comp) return 1;
    { char p[PL]; struct stat st; snprintf(p, sizeof p, "%s/request.txt", g_state); if (!stat(p, &st)) req_off = st.st_size; snprintf(p, sizeof p, "%s/say.txt", g_state); if (!stat(p, &st)) say_off = st.st_size; }
    load_sfx(); write_canvas(); write_ui();
    for (;;) { usleep(dirty ? 60000 : 200000); poll_requests(); poll_say(); if (dirty) { write_canvas(); write_ui(); dirty = 0; } if (!parent_alive()) break; }
    return 0;
}
