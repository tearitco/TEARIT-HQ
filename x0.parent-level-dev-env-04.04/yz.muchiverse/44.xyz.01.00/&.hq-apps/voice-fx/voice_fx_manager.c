/* voice_fx_manager - the voice-fx studio window (SCAFFOLD, 2026-10-14). Design: BOOK 19.audio/VOICE-AND-SFX-DESIGN.md. Handbook: README.md next to this file.
 * Forked by the renderer (module tag in voice-fx.xhtpm) as: voice_fx_manager <house_root> [package_dir]. Writes state/ui.txt (+ canvas.raw/receipt for the waveform).
 * WHAT IT DOES: lists voices (the pet voices from @.apps/pet-trainer/voices.pdl, then state/voices2.pdl rows, latest row per id wins), selects one, edits its knobs
 *   (pitch in semitones, speed), renames it, makes a new voice from it, says the words field with it (forks ops/voice_say: Kokoro stand-in + sox pitch), draws the last
 *   sound's waveform, takes the two judges' verdicts (JUDGE rows in state/verdicts.txt, same rows as img-hai).
 * VERBS (state/request.txt, one per line, appended by ops/voice_fx_req): SEL:<i> WORDS:<text> PITCH:+1|-1 SPEED:+|- RENAME NEW SAY VERDICT:<claude|owner>:<yes|no>
 * NOT BUILT (see BOOK 19.audio/VOICE-ROADMAP.md): our own C DSP/engines (v0/v1/v2), pronunciation map, formant/brightness knobs (stored, not applied), candidate edits.
 * Style: state files only, size-growth marker for requests, no logging. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#define PL 4096
#define CW 640
#define CH 240
typedef struct { char id[24], name[48], engine[12], base[48]; int pitch; double speed; } Voice;
static Voice V[32]; static int nV, sel;
static char g_pkg[PL], g_house[PL], g_state[PL], g_words[200], g_msg[200] = "ready", g_last[PL], g_say[200] = "no sound made yet";
static long req_off, say_off; static int dirty = 1; static pid_t say_pid; static unsigned char *comp;
static int split_bar(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) { while (*f[i] == ' ') f[i]++; char *e = f[i] + strlen(f[i]); while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }
static Voice *voice_by_id(const char *id) { for (int i = 0; i < nV; i++) if (!strcmp(V[i].id, id)) return &V[i]; if (nV >= 32) return NULL; Voice *v = &V[nV++]; memset(v, 0, sizeof *v); snprintf(v->id, sizeof v->id, "%s", id); return v; }
static void load_voices(void) {
    static const char *sp[] = { "bunny", "bear", "cat", "frog", "mouse", "dragon" }; char p[PL], ln[512], *f[8]; nV = 0;
    snprintf(p, sizeof p, "%s/@.apps/pet-trainer/voices.pdl", g_house); FILE *fp = fopen(p, "r");
    if (fp) { while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "VOICE", 5) || ln[5] == '2') continue; int n = split_bar(ln, f, 8); int k = atoi(f[1]); if (n < 4 || k < 0 || k > 5) continue; Voice *v = voice_by_id(sp[k]); if (!v) continue; snprintf(v->name, sizeof v->name, "%s", sp[k]); snprintf(v->engine, sizeof v->engine, "kokoro"); snprintf(v->base, sizeof v->base, "%s", f[2]); v->speed = 1.0 + atof(f[3]) / 100.0; v->pitch = 0; } fclose(fp); }
    snprintf(p, sizeof p, "%s/voices2.pdl", g_state); fp = fopen(p, "r");
    if (fp) { while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "VOICE2", 6)) continue; int n = split_bar(ln, f, 10); if (n < 7) continue; Voice *v = voice_by_id(f[1]); if (!v) continue; snprintf(v->name, sizeof v->name, "%s", f[2]); snprintf(v->engine, sizeof v->engine, "%s", f[3]); snprintf(v->base, sizeof v->base, "%s", f[4]); v->pitch = atoi(f[5]); v->speed = atof(f[6]); } fclose(fp); }
    if (sel >= nV) sel = 0;
}
static void save_voice(Voice *v) { char p[PL]; snprintf(p, sizeof p, "%s/voices2.pdl", g_state); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "VOICE2 | %s | %s | %s | %s | %d | %.2f | 0 | 0 | scaffold row\n", v->id, v->name, v->engine, v->base, v->pitch, v->speed); fclose(f); } }
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
static void write_ui(void) {
    char dst[PL], tmp[PL]; snprintf(dst, sizeof dst, "%s/ui.txt", g_state); snprintf(tmp, sizeof tmp, "%s/ui.txt.tmp", g_state); FILE *f = fopen(tmp, "w"); if (!f) return; Voice *v = nV ? &V[sel] : NULL;
    fprintf(f, "canvas_raw=%s/canvas.raw\nstatus_line=voice-fx  voices=%d  selected=%s\nv_count=%d\n", g_state, nV, v ? v->name : "-", nV);
    for (int i = 0; i < nV; i++) fprintf(f, "v_%d_label=%s%s  (%s, pitch %+d st, speed %.2f)\nv_%d_class=%s\nv_%d_action='%s/ops/+x/voice_fx_req.+x' 'SEL:%d'\n", i, i == sel ? "* " : "", V[i].name, V[i].engine, V[i].pitch, V[i].speed, i, i == sel ? "active" : "", i, g_pkg, i);
    fprintf(f, "sel_line=%s\nsay_line=%s\nmsg=%s\nwords_line=%s\n", v ? v->base : "-", g_say, g_msg, g_words[0] ? g_words : "(no words: Say uses a default line)");
    { const char *k[] = { "pm", "pp", "sm", "sp", "say", "rename", "new", "jc_yes", "jc_no", "jo_yes", "jo_no" }; const char *vb[] = { "PITCH:-1", "PITCH:+1", "SPEED:-", "SPEED:+", "SAY", "RENAME", "NEW", "VERDICT:claude:yes", "VERDICT:claude:no", "VERDICT:owner:yes", "VERDICT:owner:no" };
      for (int i = 0; i < 11; i++) fprintf(f, "%s_action='%s/ops/+x/voice_fx_req.+x' '%s'\n", k[i], g_pkg, vb[i]); }
    fprintf(f, "words_action='%s/ops/+x/voice_fx_req.+x' 'WORDS'\n", g_pkg); fclose(f); rename(tmp, dst);
}
static void handle(const char *cmd) {
    Voice *v = nV ? &V[sel] : NULL;
    if (!strncmp(cmd, "SEL:", 4)) { int i = atoi(cmd + 4); if (i >= 0 && i < nV) sel = i; }
    else if (!strncmp(cmd, "WORDS", 5)) { const char *w = cmd[5] == ':' ? cmd + 6 : ""; while (*w == ' ') w++; snprintf(g_words, sizeof g_words, "%s", w); for (char *c = g_words; *c; c++) if (*c == '|') *c = ' '; size_t n = strlen(g_words); while (n && g_words[n - 1] == ' ') g_words[--n] = 0; snprintf(g_msg, sizeof g_msg, g_words[0] ? "words set" : "words cleared"); }
    else if (v && !strncmp(cmd, "PITCH:", 6)) { v->pitch += cmd[6] == '-' ? -1 : 1; if (v->pitch < -12) v->pitch = -12; if (v->pitch > 12) v->pitch = 12; save_voice(v); snprintf(g_msg, sizeof g_msg, "%s pitch %+d st", v->name, v->pitch); }
    else if (v && !strncmp(cmd, "SPEED:", 6)) { v->speed += cmd[6] == '-' ? -0.05 : 0.05; if (v->speed < 0.5) v->speed = 0.5; if (v->speed > 1.6) v->speed = 1.6; save_voice(v); snprintf(g_msg, sizeof g_msg, "%s speed %.2f", v->name, v->speed); }
    else if (v && !strcmp(cmd, "RENAME")) { if (!g_words[0]) snprintf(g_msg, sizeof g_msg, "type the new name in the words field first"); else { snprintf(v->name, sizeof v->name, "%.47s", g_words); save_voice(v); snprintf(g_msg, sizeof g_msg, "renamed (the id %s never changes)", v->id); } }
    else if (v && !strcmp(cmd, "NEW")) { if (nV >= 32) snprintf(g_msg, sizeof g_msg, "voice list is full"); else { char id[24]; snprintf(id, sizeof id, "v%ld", (long)time(NULL) % 1000000); Voice *n = voice_by_id(id); if (n) { Voice c = *v; snprintf(c.id, sizeof c.id, "%s", id); snprintf(c.name, sizeof c.name, "%.40s 2", v->name); *n = c; save_voice(n); sel = (int)(n - V); snprintf(g_msg, sizeof g_msg, "new voice %s made from %s", n->id, v->name); } } }
    else if (v && !strcmp(cmd, "SAY")) {
        if (say_pid > 0) { snprintf(g_msg, sizeof g_msg, "still making the last sound"); } else if (!v->base[0] || !strcmp(v->base, "-")) snprintf(g_msg, sizeof g_msg, "this voice has no engine voice");
        else { char sp[16], pt[16], label[96], text[260]; snprintf(sp, sizeof sp, "%.2f", v->speed); snprintf(pt, sizeof pt, "%d", v->pitch); snprintf(label, sizeof label, "%s_p%+d_s%.0f", v->id, v->pitch, v->speed * 100); snprintf(text, sizeof text, "%s", g_words[0] ? g_words : "Hello, I am a voice in the studio.");
            char op[PL]; snprintf(op, sizeof op, "%s/ops/+x/voice_say.+x", g_pkg); pid_t p = fork(); if (p == 0) { setsid(); execl(op, op, g_pkg, g_house, v->base, sp, pt, label, text, (char *)NULL); _exit(127); } if (p > 0) { say_pid = p; snprintf(g_msg, sizeof g_msg, "saying with %s ...", v->name); } } }
    else if (v && !strncmp(cmd, "VERDICT:", 8)) { char who[8] = "", yn[4] = ""; if (sscanf(cmd + 8, "%7[^:]:%3s", who, yn) == 2 && g_last[0]) { char p[PL]; snprintf(p, sizeof p, "%s/verdicts.txt", g_state); FILE *f = fopen(p, "a"); const char *b = strrchr(g_last, '/'); char date[16]; time_t t = time(NULL); strftime(date, sizeof date, "%Y-%m-%d", localtime(&t)); if (f) { fprintf(f, "JUDGE | %s | %s | %s | %s\n", b ? b + 1 : g_last, who, yn, date); fclose(f); } snprintf(g_msg, sizeof g_msg, "%s said %s", who, yn); } else snprintf(g_msg, sizeof g_msg, "make a sound first"); }
    dirty = 1;
}
static void poll_requests(void) { char p[PL]; snprintf(p, sizeof p, "%s/request.txt", g_state); struct stat st; if (stat(p, &st) || st.st_size <= req_off) return; FILE *f = fopen(p, "r"); if (!f) return; fseek(f, req_off, SEEK_SET);
    char line[512]; while (fgets(line, sizeof line, f)) { size_t n = strlen(line); if (!n || line[n - 1] != '\n') break; line[n - 1] = 0; req_off += (long)n; if (line[0]) handle(line); } fclose(f); }
static void poll_say(void) {
    if (say_pid > 0) { int st; if (waitpid(say_pid, &st, WNOHANG) == say_pid) { say_pid = 0; dirty = 1; } }
    char p[PL]; snprintf(p, sizeof p, "%s/say.txt", g_state); struct stat s; if (stat(p, &s) || s.st_size <= say_off) return; FILE *f = fopen(p, "r"); if (!f) return; fseek(f, say_off, SEEK_SET); char ln[PL];
    while (fgets(ln, sizeof ln, f)) { size_t n = strlen(ln); if (!n || ln[n - 1] != '\n') break; ln[n - 1] = 0; say_off += (long)n; snprintf(g_say, sizeof g_say, "%.190s", ln); }
    fclose(f); snprintf(p, sizeof p, "%s/last.txt", g_state); f = fopen(p, "r"); if (f) { if (fgets(g_last, sizeof g_last, f)) g_last[strcspn(g_last, "\n")] = 0; fclose(f); } dirty = 1;
}
static int parent_alive(void) { char p[PL]; snprintf(p, sizeof p, "%s/module_parent.pid", g_pkg); FILE *f = fopen(p, "r"); if (!f) return 1; int pid = 0; if (fscanf(f, "%d", &pid) != 1) pid = 0; fclose(f); return pid <= 0 || kill(pid, 0) == 0; }
static void bye(int s) { (void)s; _exit(0); }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: voice_fx_manager <house_root> [package_dir]\n"); return 1; }
    snprintf(g_house, sizeof g_house, "%s", argv[1]); snprintf(g_pkg, sizeof g_pkg, "%s", argc > 2 ? argv[2] : ""); if (!g_pkg[0]) snprintf(g_pkg, sizeof g_pkg, "%s/&.hq-apps/voice-fx", g_house);
    snprintf(g_state, sizeof g_state, "%s/state", g_pkg); mkdir(g_state, 0755); signal(SIGTERM, bye); comp = calloc((size_t)CW * CH * 4, 1); if (!comp) return 1;
    { char p[PL]; struct stat st; snprintf(p, sizeof p, "%s/request.txt", g_state); if (!stat(p, &st)) req_off = st.st_size; snprintf(p, sizeof p, "%s/say.txt", g_state); if (!stat(p, &st)) say_off = st.st_size; }
    load_voices(); write_canvas(); write_ui();
    for (;;) { usleep(dirty ? 60000 : 200000); poll_requests(); poll_say(); if (dirty) { write_canvas(); write_ui(); dirty = 0; } if (!parent_alive()) break; }
    return 0;
}
