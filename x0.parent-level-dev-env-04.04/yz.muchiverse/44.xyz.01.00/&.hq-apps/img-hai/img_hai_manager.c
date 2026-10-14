#define _GNU_SOURCE
/* img_hai_manager - the brain of img-hai, the text-to-image studio window. FULL DOCS: &.hq-apps/img-hai/docs/ (start at README.md; CODE.md has this map in more detail).
 *
 * WHAT IT IS: a small background process started by the shared renderer (khtpm_core_render.+x) through the <module> tag in img-hai.xhtpm. It owns all state. The window only draws
 * what this process writes to state/ui.txt (text) and state/canvas.raw (pixels) and sends clicks back as lines appended to state/request.txt (by ops/img_hai_req.+x).
 * Usage: img_hai_manager <house_root> [package_dir]. Exits by itself when the renderer is gone (module_parent.pid check). No other output; no extra logging by design.
 *
 * LOOP (main): every 60-200 ms: poll_requests() -> poll_job() -> dispatch_queue() -> if dirty { write_canvas(); write_ui(); }.
 * DATA IN : data/img/draft/<day>/img/* and <package>/tiles/* (the tiles, any image format stb_image reads), data/img/draft/<day>/verdicts.txt and <package>/verdicts.txt (JUDGE rows),
 *           models.pdl (HOST/MODEL rows), batches/*.pdl (BATCH rows), data/img/parts.pdl (part names), state/request.txt (clicks), state/job.txt (job status lines).
 * DATA OUT: state/ui.txt, state/canvas.raw + canvas.receipt.txt, state/thumbs/*.png, state/prompt_*.txt, verdict lines, <package>/tiles/mix_<n>.png (Export),
 *           data/img/accept_candidates.pdl (Accept; test_* tiles go to state/accept_test.pdl instead).
 * CHILD PROCESSES (all fork+exec, never popen/system): ops/+x/img_hai_job.+x (one per busy host: ssh/scp, runs img_draft.py remotely) and &.widgits/concept-bank/ops/+x/img_prompt.+x (builds a prompt, short-lived).
 *
 * REQUEST VERBS (one per line in state/request.txt; handle()): TILE:<n> place tile n at the cursor | SEL:<n> select placement n | MOVE:L|R|U|D move the selected placement | CUR:L|R|U|D move the cursor
 *   ROT turn 90 degrees | FLIP | VIS hide/show | DEL | UP / DOWN change draw order | CELL:<px> cell size 16..256 | CLEAR | EXPORT mix -> new tile | RESCAN | WORDS:<text> set the free prompt
 *   DRAFT draft on the selected model's host | DRAFTALL one draft on every host | PARTNEXT / MODELNEXT cycle part / model | BATCHNEXT cycle the batch file | BATCHRUN load the selected batch and run it
 *   BATCH:<name> load batches/<name>.pdl | BATCHSTOP cancel waiting jobs | VERDICT:owner|claude:yes|no judge the selected placement's tile | ACCEPT write a candidate if both said yes.
 *
 * DATA STRUCTURES: Tile T[] (path, label, size, thumb) | Place P[] (tile, cell x/y, rot 0-3, flip, vis) | Judge J[] (label, claude, owner) | Host H[] / Model M[] (from models.pdl)
 *   Job Q[] (batch queue: part, words, model, overlay, seed, state 0 waiting 1 running 2 done 3 failed) | job_pid[host] / job_slot[host] (the running child and which Q entry it is).
 * FUNCTION INDEX: tiles: has_img_ext scan_dir make_thumb scan_tiles | judging: load_judges judge_of | hosts/models/parts: load_models load_parts | prompts: build_prompt
 *   drafting: start_on start_draft start_all host_of_model | batches: scan_batches load_batch dispatch_queue queue_counts poll_job | canvas: draw_place write_canvas | window: write_ui
 *   input: handle poll_requests | lifecycle: parent_alive bye main.
 * RENDERER QUIRKS THIS CODE WORKS AROUND (details in docs/CODE.md section 7): sprite= draws only .png and as BGRA (thumbs swap red/blue), the canvas blits raw RGBA 1:1 centred, nav numbers shift when rows are added.
 * HOUSE RULES KEPT: change detection by file size growth (request.txt, job.txt), never mtime; no popen; no per-app C in the shared renderer; state under state/ (untracked); pure C. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include <fcntl.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#undef STBI_ONLY_PNG
#include "../../&.widgits/_shared-lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../&.widgits/_shared-lib/stb_image_write.h"
#define PL 4096
#define CW 640
#define CH 480
#define THUMB 72
#define MAXT 256
#define MAXP 64
typedef struct { char path[PL], label[96]; long size; char thumb[PL]; } Tile;
typedef struct { int tile, cx, cy, rot, flip, vis; } Place;
static char g_house[PL], g_pkg[PL], g_state[PL], g_draft[PL], g_msg[200] = "";
static Tile T[MAXT]; static int nT;
static Place P[MAXP]; static int nP, sel = -1, cell = 128, curx = 1, cury = 1, dirty = 1;
static unsigned char *comp; static char g_words[400] = "", g_job[200] = "no job";
typedef struct { char id[96], host[24], steps[8], size[8]; } Model; static Model M[16]; static int nM, selM;
typedef struct { char name[24], target[64], dir[64], py[64], prefix[64]; } Host; static Host H[4]; static int nH;
static char PARTS[16][40]; static int nParts, selPart; static long job_off = 0; static pid_t job_pid[4]; static int job_slot[4];
typedef struct { char part[40], words[300], model[96], overlay[64]; int seed, state; } Job;   /* state 0 waiting, 1 running, 2 done, 3 failed */
static Job Q[256]; static int nQ; static char g_batch[64] = ""; static char BN[16][40]; static int nBN, selBN;
static long req_off = 0;

static int has_img_ext(const char *n) { const char *d = strrchr(n, '.'); if (!d) return 0; static const char *ext[] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tga", ".psd", ".hdr", ".ppm", ".pgm", ".pnm", NULL };
    for (int i = 0; ext[i]; i++) if (!strcasecmp(d, ext[i])) return 1; return 0; }
static int cmp_tile(const void *a, const void *b) { return strcmp(((const Tile *)a)->path, ((const Tile *)b)->path); }
static void scan_dir(const char *dir) { DIR *d = opendir(dir); if (!d) return; struct dirent *e; while ((e = readdir(d)) && nT < MAXT) { if (e->d_name[0] == '.' || !has_img_ext(e->d_name)) continue; Tile *t = &T[nT]; memset(t, 0, sizeof *t);
        snprintf(t->path, PL, "%s/%s", dir, e->d_name); struct stat st; if (stat(t->path, &st) || !S_ISREG(st.st_mode)) continue; t->size = (long)st.st_size; snprintf(t->label, sizeof t->label, "%s", e->d_name); char *dot = strrchr(t->label, '.'); if (dot) *dot = 0; nT++; } closedir(d); }
static void make_thumb(Tile *t, int idx) {
    snprintf(t->thumb, PL, "%s/thumbs/s%d_%ld.png", g_state, idx, t->size);
    struct stat st; if (!stat(t->thumb, &st)) return;
    int w, h, c; unsigned char *src = stbi_load(t->path, &w, &h, &c, 4); if (!src) { t->thumb[0] = 0; return; }
    int tw = w, th = h; double s = (double)THUMB / (w > h ? w : h); if (s < 1.0) { tw = (int)(w * s + .5); th = (int)(h * s + .5); } else { int k = (int)s; if (k < 1) k = 1; tw = w * k; th = h * k; }
    if (tw < 1) tw = 1; if (th < 1) th = 1;
    unsigned char *o = malloc((size_t)tw * th * 4); if (!o) { stbi_image_free(src); return; }
    for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) { int x0 = x * w / tw, x1 = (x + 1) * w / tw, y0 = y * h / th, y1 = (y + 1) * h / th; if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
        long a[4] = { 0, 0, 0, 0 }, n = 0; for (int yy = y0; yy < y1 && yy < h; yy++) for (int xx = x0; xx < x1 && xx < w; xx++) { unsigned char *p = src + ((size_t)yy * w + xx) * 4; for (int k = 0; k < 4; k++) a[k] += p[k]; n++; }
        unsigned char *q = o + ((size_t)y * tw + x) * 4; for (int k = 0; k < 4; k++) q[k] = n ? (unsigned char)(a[k] / n) : 0; unsigned char sw = q[0]; q[0] = q[2]; q[2] = sw; }   /* the renderer blits a .png sprite as BGRA, so red and blue are swapped here to show true colours */
    stbi_write_png(t->thumb, tw, th, 4, o, tw * 4); free(o); stbi_image_free(src);
}
static int split_bar(char *s, char **f, int max);
typedef struct { char label[96], claude[4], owner[4]; } Judge; static Judge J[512]; static int nJ;
static Judge *judge_of(const char *label, int create) { for (int i = 0; i < nJ; i++) if (!strcmp(J[i].label, label)) return &J[i]; if (!create || nJ >= 512) return NULL; Judge *j = &J[nJ++]; memset(j, 0, sizeof *j); snprintf(j->label, 96, "%s", label); return j; }
/* JUDGE | <tile label> | claude|owner | yes|no | date  : append-only in <day>/verdicts.txt, the latest line per (label, who) wins */
static void load_judges(void) {
    nJ = 0; DIR *d = opendir(g_draft); if (!d) return; struct dirent *e; char *f[8];
    while ((e = readdir(d))) { if (e->d_name[0] == '.') continue; char p[PL], ln[512]; snprintf(p, sizeof p, "%s/%s/verdicts.txt", g_draft, e->d_name); FILE *fp = fopen(p, "r"); if (!fp) continue;
        while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "JUDGE", 5)) continue; int n = split_bar(ln, f, 8); if (n < 4) continue; Judge *j = judge_of(f[1], 1); if (!j) continue; char *dst = !strcmp(f[2], "claude") ? j->claude : !strcmp(f[2], "owner") ? j->owner : NULL; if (dst) snprintf(dst, 4, "%s", f[3]); }
        fclose(fp); }
    closedir(d);
    { char p2[PL], ln2[512]; snprintf(p2, sizeof p2, "%s/verdicts.txt", g_pkg); FILE *fp2 = fopen(p2, "r"); if (fp2) { while (fgets(ln2, sizeof ln2, fp2)) { if (strncmp(ln2, "JUDGE", 5)) continue; int n = split_bar(ln2, f, 8); if (n < 4) continue; Judge *j = judge_of(f[1], 1); if (!j) continue; char *dst = !strcmp(f[2], "claude") ? j->claude : !strcmp(f[2], "owner") ? j->owner : NULL; if (dst) snprintf(dst, 4, "%s", f[3]); } fclose(fp2); } }
}
static const char *jv(const char *v) { return v[0] ? v : "?"; }
static void scan_tiles(void) {
    char thd[PL]; snprintf(thd, sizeof thd, "%s/thumbs", g_state); mkdir(thd, 0755);
    nT = 0; DIR *d = opendir(g_draft);
    if (d) { struct dirent *e; while ((e = readdir(d))) { if (e->d_name[0] == '.') continue; char sub[PL]; snprintf(sub, sizeof sub, "%s/%s/img", g_draft, e->d_name); scan_dir(sub); } closedir(d); }
    char tl[PL]; snprintf(tl, sizeof tl, "%s/tiles", g_pkg); scan_dir(tl);
    qsort(T, nT, sizeof T[0], cmp_tile);
    for (int i = 0; i < nT; i++) make_thumb(&T[i], i);
}

static int split_bar(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) { while (*f[i] == ' ') f[i]++; char *e = f[i] + strlen(f[i]); while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }
static void load_models(void) {
    char p[PL], ln[1024], *f[8]; snprintf(p, sizeof p, "%s/models.pdl", g_pkg); FILE *fp = fopen(p, "r"); if (!fp) return; nM = nH = 0;
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split_bar(ln, f, 8);
        if (!strcmp(f[0], "HOST") && n >= 5 && nH < 4) { snprintf(H[nH].name, 24, "%s", f[1]); snprintf(H[nH].target, 64, "%s", f[2]); snprintf(H[nH].dir, 64, "%s", f[3]); snprintf(H[nH].py, 64, "%s", f[4]); snprintf(H[nH].prefix, 64, "%s", n >= 6 && f[5][0] ? f[5] : "nice -n 15"); nH++; }
        else if (!strcmp(f[0], "MODEL") && n >= 6 && nM < 16) { snprintf(M[nM].id, 96, "%s", f[1]); snprintf(M[nM].host, 24, "%s", f[2]); snprintf(M[nM].steps, 8, "%s", f[4]); snprintf(M[nM].size, 8, "%s", f[5]); nM++; } }
    fclose(fp);
}
static void load_parts(void) {
    char p[PL], ln[1024], *f[8]; snprintf(p, sizeof p, "%s/&.widgits/concept-bank/data/img/parts.pdl", g_house); FILE *fp = fopen(p, "r"); if (!fp) return; nParts = 0;
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split_bar(ln, f, 8); if (n >= 3 && !strcmp(f[0], "PART") && nParts < 16) snprintf(PARTS[nParts++], 40, "%s", f[1]); }
    fclose(fp);
}
/* fork+exec img_prompt for a part; stdout goes to a file in state/ (no popen) */
static int build_prompt(const char *part, const char *outfile, const char *overlay) {
    char words[PL], parts[PL], bin[PL]; snprintf(words, sizeof words, "%s/&.widgits/concept-bank/data/img/words.pdl", g_house); snprintf(parts, sizeof parts, "%s/&.widgits/concept-bank/data/img/parts.pdl", g_house); snprintf(bin, sizeof bin, "%s/&.widgits/concept-bank/ops/+x/img_prompt.+x", g_house);
    pid_t p = fork(); if (p < 0) return -1;
    if (p == 0) { int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) { dup2(fd, 1); close(fd); } char ov[PL] = ""; if (overlay && overlay[0]) snprintf(ov, sizeof ov, "%s/&.widgits/concept-bank/data/img/candidates/%s.cand", g_house, overlay); char *a[] = { bin, words, parts, (char *)part, "0.30", ov, NULL }; if (!ov[0]) a[4] = NULL; execv(bin, a); _exit(127); }
    for (int t = 0; t < 50; t++) { int st; if (waitpid(p, &st, WNOHANG) == p) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(100000); }
    kill(p, SIGKILL); waitpid(p, NULL, 0); return -1;
}
static int start_on(int hi, int mi, const char *part, const char *words, int seedv, const char *overlay) {
    if (hi < 0 || hi >= nH || mi < 0 || mi >= nM) return 0;
    if (job_pid[hi] > 0) { snprintf(g_msg, sizeof g_msg, "%s is busy with a draft", H[hi].name); return 0; }
    Host *h = &H[hi]; char jpart[80]; if (overlay && overlay[0]) snprintf(jpart, sizeof jpart, "%s_%s", part, overlay); else snprintf(jpart, sizeof jpart, "%s", part);   /* an overlay (a trial tuning) is part of the picture's name so it can never replace the untuned one */
    char pf[PL]; snprintf(pf, sizeof pf, "%s/state/prompt_%s_%s_%d.txt", g_pkg, jpart, h->name, seedv);
    if (words[0]) { FILE *f = fopen(pf, "w"); if (!f) return 0; fprintf(f, "PROMPT: %s\nNEGATIVE: text, watermark, blurry\n", words); fclose(f); }
    else if (build_prompt(part, pf, overlay) != 0) { snprintf(g_msg, sizeof g_msg, "prompt build failed for %s", part); return 0; }
    char seed[16]; snprintf(seed, sizeof seed, "%d", seedv);
    pid_t p = fork(); if (p < 0) return 0;
    if (p == 0) { setsid(); char bin[PL]; snprintf(bin, sizeof bin, "%s/ops/+x/img_hai_job.+x", g_pkg); char *a[] = { bin, g_pkg, g_house, h->target, h->dir, h->py, M[mi].id, M[mi].steps, M[mi].size, seed, jpart, pf, h->prefix, NULL }; execv(bin, a); _exit(127); }
    job_pid[hi] = p; job_slot[hi] = -1; snprintf(g_msg, sizeof g_msg, "draft started on %s: %s / %s", h->name, part, M[mi].id); return 1;
}
static int host_of_model(int mi) { for (int i = 0; i < nH; i++) if (!strcmp(H[i].name, M[mi].host)) return i; return -1; }
static void start_draft(void) { if (!nM || !nParts) { snprintf(g_msg, sizeof g_msg, "no model or part"); return; } int hi = host_of_model(selM); if (hi < 0) { snprintf(g_msg, sizeof g_msg, "model host not in models.pdl"); return; } start_on(hi, selM, g_words[0] ? "free" : PARTS[selPart], g_words, (int)(time(NULL) % 100000) + hi, ""); }
/* delegate: one draft on every host at once, each with the first model listed for that host */
static void start_all(void) { int started = 0; for (int hi = 0; hi < nH; hi++) { int mi = -1; for (int i = 0; i < nM; i++) if (!strcmp(M[i].host, H[hi].name)) { mi = i; break; } if (mi >= 0 && job_pid[hi] <= 0 && start_on(hi, mi, g_words[0] ? "free" : PARTS[selPart], g_words, (int)(time(NULL) % 100000) + hi, "")) started++; } snprintf(g_msg, sizeof g_msg, "drafts started on %d host(s)", started); }
/* batches/<name>.pdl rows: BATCH | label | part or "free" | words ("-" = use the word bank for the part) | seeds | model id or "any"
 * Each row becomes <seeds> jobs with seeds 1..n. Idle hosts take the next waiting job whose model is "any" (the host's first model) or on that host. */
static void scan_batches(void) { char dir[PL]; snprintf(dir, sizeof dir, "%s/batches", g_pkg); DIR *d = opendir(dir); nBN = 0; if (!d) return; struct dirent *e; while ((e = readdir(d)) && nBN < 16) { size_t l = strlen(e->d_name); if (l > 4 && !strcmp(e->d_name + l - 4, ".pdl") && l - 4 < 40) { snprintf(BN[nBN], 40, "%.*s", (int)(l - 4), e->d_name); nBN++; } } closedir(d);
    for (int i = 0; i < nBN; i++) for (int k = i + 1; k < nBN; k++) if (strcmp(BN[i], BN[k]) > 0) { char t[40]; memcpy(t, BN[i], 40); memcpy(BN[i], BN[k], 40); memcpy(BN[k], t, 40); } if (selBN >= nBN) selBN = 0; }
static void load_batch(const char *name) {
    char p[PL], ln[1024], *f[8]; snprintf(p, sizeof p, "%s/batches/%s.pdl", g_pkg, name); FILE *fp = fopen(p, "r"); if (!fp) { snprintf(g_msg, sizeof g_msg, "no batch file batches/%s.pdl", name); return; } nQ = 0;
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split_bar(ln, f, 8); if (n < 6 || strcmp(f[0], "BATCH")) continue; int seeds = atoi(f[4]); if (seeds < 1) seeds = 1;
        for (int k = 1; k <= seeds && nQ < 256; k++) { Job *j = &Q[nQ++]; memset(j, 0, sizeof *j); if (!strcmp(f[2], "free")) snprintf(j->part, 40, "free_%s", f[1]); else snprintf(j->part, 40, "%s", f[2]); snprintf(j->words, 300, "%s", strcmp(f[3], "-") ? f[3] : ""); snprintf(j->model, 96, "%s", f[5]); snprintf(j->overlay, 64, "%s", n >= 7 && strcmp(f[6], "-") ? f[6] : ""); j->seed = k; } }
    fclose(fp); snprintf(g_batch, sizeof g_batch, "%s", name); snprintf(g_msg, sizeof g_msg, "batch %s loaded: %d jobs", name, nQ);
}
static void dispatch_queue(void) {
    for (int hi = 0; hi < nH; hi++) { if (job_pid[hi] > 0) continue; int first = -1; for (int i = 0; i < nM; i++) if (!strcmp(M[i].host, H[hi].name)) { first = i; break; } if (first < 0) continue;
        for (int q = 0; q < nQ; q++) { Job *j = &Q[q]; if (j->state != 0) continue; int mi = -1; if (!strcmp(j->model, "any")) mi = first; else for (int i = 0; i < nM; i++) if (!strcmp(M[i].id, j->model) && !strcmp(M[i].host, H[hi].name)) mi = i; if (mi < 0) continue;
            if (start_on(hi, mi, j->part, j->words, j->seed, j->overlay)) { j->state = 1; job_slot[hi] = q; dirty = 1; } else j->state = 3; break; } }
}
static void queue_counts(int *w, int *r, int *d, int *x) { *w = *r = *d = *x = 0; for (int q = 0; q < nQ; q++) { switch (Q[q].state) { case 0: (*w)++; break; case 1: (*r)++; break; case 2: (*d)++; break; default: (*x)++; } } }
static void poll_job(void) {
    for (int i = 0; i < 4; i++) if (job_pid[i] > 0) { int st; if (waitpid(job_pid[i], &st, WNOHANG) == job_pid[i]) { int ok = WIFEXITED(st) && WEXITSTATUS(st) == 0; if (job_slot[i] >= 0 && job_slot[i] < nQ) Q[job_slot[i]].state = ok ? 2 : 3; job_pid[i] = 0; job_slot[i] = -1; scan_tiles(); dirty = 1; } }
    char p[PL]; snprintf(p, sizeof p, "%s/job.txt", g_state); struct stat st; if (stat(p, &st) || st.st_size <= job_off) return;
    FILE *f = fopen(p, "r"); if (!f) return; fseek(f, job_off, SEEK_SET); char ln[300], last[300] = ""; while (fgets(ln, sizeof ln, f)) { size_t n = strlen(ln); if (!n || ln[n - 1] != '\n') break; ln[n - 1] = 0; job_off += (long)n; snprintf(last, sizeof last, "%s", ln); } fclose(f);
    if (last[0]) { snprintf(g_job, sizeof g_job, "%s", last); dirty = 1; }
}
/* composite one tile into the canvas at cell (cx,cy), fitted inside the cell, with rotation and flip */
static void draw_place(const Place *p) {
    if (p->tile < 0 || p->tile >= nT) return; int w, h, c; unsigned char *src = stbi_load(T[p->tile].path, &w, &h, &c, 4); if (!src) return;
    int rw = (p->rot & 1) ? h : w, rh = (p->rot & 1) ? w : h; int k = cell / (rw > rh ? rw : rh); double sc; int dw, dh;
    if (k >= 1) { dw = rw * k; dh = rh * k; sc = 0; } else { sc = (double)cell / (rw > rh ? rw : rh); dw = (int)(rw * sc + .5); dh = (int)(rh * sc + .5); if (dw < 1) dw = 1; if (dh < 1) dh = 1; }
    int ox = p->cx * cell + (cell - dw) / 2, oy = p->cy * cell + (cell - dh) / 2;
    for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) { int dx = ox + x, dy = oy + y; if (dx < 0 || dy < 0 || dx >= CW || dy >= CH) continue;
        int rx = k >= 1 ? x / k : (int)(x / sc), ry = k >= 1 ? y / k : (int)(y / sc); if (rx >= rw) rx = rw - 1; if (ry >= rh) ry = rh - 1; if (p->flip) rx = rw - 1 - rx;
        int sx, sy; switch (p->rot & 3) { case 1: sx = ry; sy = h - 1 - rx; break; case 2: sx = w - 1 - rx; sy = h - 1 - ry; break; case 3: sx = w - 1 - ry; sy = rx; break; default: sx = rx; sy = ry; }
        if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue; unsigned char *s = src + ((size_t)sy * w + sx) * 4, *d = comp + ((size_t)dy * CW + dx) * 4; float a = s[3] / 255.0f; if (a <= 0) continue;
        for (int q = 0; q < 3; q++) d[q] = (unsigned char)(d[q] * (1 - a) + s[q] * a); }
    stbi_image_free(src);
}
static void write_canvas(void) {
    for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) { int g = (((x / cell) + (y / cell)) & 1); unsigned char v = g ? 44 : 34; unsigned char *d = comp + ((size_t)y * CW + x) * 4;
        if (x % cell == 0 || y % cell == 0) v = 60; d[0] = v; d[1] = v; d[2] = v + 6; d[3] = 255; }
    for (int i = 0; i < nP; i++) if (P[i].vis) draw_place(&P[i]);
    int cx0 = curx * cell, cy0 = cury * cell; for (int i = 0; i < cell; i++) { int pts[4][2] = { { cx0 + i, cy0 }, { cx0 + i, cy0 + cell - 1 }, { cx0, cy0 + i }, { cx0 + cell - 1, cy0 + i } };
        for (int q = 0; q < 4; q++) if (pts[q][0] < CW && pts[q][1] < CH) { unsigned char *d = comp + ((size_t)pts[q][1] * CW + pts[q][0]) * 4; d[0] = 240; d[1] = 200; d[2] = 60; } }
    char raw[PL], tmp[PL], rec[PL]; snprintf(raw, sizeof raw, "%s/canvas.raw", g_state); snprintf(tmp, sizeof tmp, "%s/canvas.raw.tmp", g_state); snprintf(rec, sizeof rec, "%s/canvas.receipt.txt", g_state);
    FILE *f = fopen(tmp, "wb"); if (f) { fwrite(comp, 1, (size_t)CW * CH * 4, f); fclose(f); rename(tmp, raw); } FILE *r = fopen(rec, "w"); if (r) { fprintf(r, "overlay_w=%d\noverlay_h=%d\n", CW, CH); fclose(r); }
}
static void write_ui(void) {
    char dst[PL], tmp[PL]; snprintf(dst, sizeof dst, "%s/ui.txt", g_state); snprintf(tmp, sizeof tmp, "%s/ui.txt.tmp", g_state); FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "canvas_raw=%s/canvas.raw\n", g_state);
    fprintf(f, "tiles_count=%d\n", nT); for (int i = 0; i < nT; i++) { Judge *jt = judge_of(T[i].label, 0); char tl[160]; snprintf(tl, sizeof tl, "%s%s%s", T[i].label, jt ? " c:" : "", jt ? jv(jt->claude) : ""); if (jt) { char o[16]; snprintf(o, sizeof o, " o:%s", jv(jt->owner)); strncat(tl, o, sizeof tl - strlen(tl) - 1); }
        fprintf(f, "t_%d_label=%s\nt_%d_sprite=%s\nt_%d_action='%s/ops/+x/img_hai_req.+x' 'TILE:%d'\nt_%d_show=%d\n", i, tl, i, T[i].thumb, i, g_pkg, i, i, T[i].thumb[0] ? 1 : 0); }
    fprintf(f, "place_count=%d\n", nP); for (int i = 0; i < nP; i++) { fprintf(f, "p_%d_label=%c %d %s @%d,%d r%d%s\np_%d_class=%s\np_%d_action='%s/ops/+x/img_hai_req.+x' 'SEL:%d'\n", i, P[i].vis ? '*' : '-', i + 1, P[i].tile < nT ? T[P[i].tile].label : "?", P[i].cx, P[i].cy, P[i].rot * 90, P[i].flip ? " flip" : "", i, i == sel ? "active" : "", i, g_pkg, i); }
    fprintf(f, "status_line=img-hai  tiles=%d  placed=%d  cell=%dpx  cursor=%d,%d\n", nT, nP, cell, curx, cury);
    { char jl[300] = "judge: place a tile, then judge it"; if (sel >= 0 && P[sel].tile < nT) { Judge *j = judge_of(T[P[sel].tile].label, 0); snprintf(jl, sizeof jl, "judge %s: claude=%s owner=%s", T[P[sel].tile].label, j ? jv(j->claude) : "?", j ? jv(j->owner) : "?"); } fprintf(f, "judge_line=%s\n", jl); }
    { const char *jk[] = { "jo_yes", "jo_no", "jc_yes", "jc_no", "accept", "draftall", "batchrun", "batchstop", "batchsel" }; const char *jvb[] = { "VERDICT:owner:yes", "VERDICT:owner:no", "VERDICT:claude:yes", "VERDICT:claude:no", "ACCEPT", "DRAFTALL", "BATCHRUN", "BATCHSTOP", "BATCHNEXT" }; for (int q = 0; q < 9; q++) fprintf(f, "%s_action='%s/ops/+x/img_hai_req.+x' '%s'\n", jk[q], g_pkg, jvb[q]); }
    fprintf(f, "batchsel_line=batch file: %s (click to change)\n", nBN ? BN[selBN] : "(none)");
    { int w, r, d, x; queue_counts(&w, &r, &d, &x); if (nQ) fprintf(f, "queue_line=batch %s: %d waiting, %d running, %d done, %d failed/cancelled\n", g_batch, w, r, d, x); else fprintf(f, "queue_line=no batch loaded\n"); }
    fprintf(f, "msg=%s\n", g_msg);
    fprintf(f, "part_line=part: %s   model: %s\npart_action='%s/ops/+x/img_hai_req.+x' 'PARTNEXT'\nmodel_action='%s/ops/+x/img_hai_req.+x' 'MODELNEXT'\n", nParts ? PARTS[selPart] : "-", nM ? M[selM].id : "-", g_pkg, g_pkg);
    fprintf(f, "prompt_line=%s\njob_line=%s\nwords_action='%s/ops/+x/img_hai_req.+x' 'WORDS'\ndraft_action='%s/ops/+x/img_hai_req.+x' 'DRAFT'\n", g_words[0] ? g_words : "(no words yet: type above, Enter)", g_job, g_pkg, g_pkg);
    const char *v[] = { "MOVE:L", "MOVE:R", "MOVE:U", "MOVE:D", "ROT", "FLIP", "VIS", "DEL", "UP", "DOWN", "CLEAR", "EXPORT", "RESCAN", "CELL:32", "CELL:64", "CELL:128", "CUR:L", "CUR:R", "CUR:U", "CUR:D" };
    const char *k[] = { "mv_l", "mv_r", "mv_u", "mv_d", "rot", "flip", "vis", "del", "up", "down", "clear", "export", "rescan", "c32", "c64", "c128", "cu_l", "cu_r", "cu_u", "cu_d" };
    for (int i = 0; i < 20; i++) fprintf(f, "%s_action='%s/ops/+x/img_hai_req.+x' '%s'\n", k[i], g_pkg, v[i]);
    fclose(f); rename(tmp, dst);
}
static void handle(char *cmd) {
    snprintf(g_msg, sizeof g_msg, "%s", cmd);
    if (!strncmp(cmd, "WORDS:", 6)) { const char *w = cmd + 6; while (*w == ' ') w++; snprintf(g_words, sizeof g_words, "%s", w); size_t wl = strlen(g_words); while (wl && g_words[wl - 1] == ' ') g_words[--wl] = 0; snprintf(g_msg, sizeof g_msg, g_words[0] ? "words set" : "words cleared: prompts come from the word bank again"); }   /* leading/trailing spaces are trimmed; a request of only spaces clears the free words */
    else if (!strncmp(cmd, "VERDICT:", 8) && sel >= 0 && P[sel].tile < nT) { char who[16] = "", val[8] = ""; if (sscanf(cmd + 8, "%15[^:]:%7s", who, val) == 2 && (!strcmp(who, "owner") || !strcmp(who, "claude")) && (!strcmp(val, "yes") || !strcmp(val, "no"))) {
            char vp[PL]; snprintf(vp, sizeof vp, "%s", T[P[sel].tile].path); char *sl = strrchr(vp, '/'); if (sl) *sl = 0; sl = strrchr(vp, '/'); if (sl) { strcpy(sl, "/verdicts.txt"); FILE *vf = fopen(vp, "a"); if (vf) { time_t t = time(NULL); char day[16]; strftime(day, sizeof day, "%Y-%m-%d", localtime(&t)); fprintf(vf, "JUDGE | %s | %s | %s | %s\n", T[P[sel].tile].label, who, val, day); fclose(vf); load_judges(); snprintf(g_msg, sizeof g_msg, "%s says %s to %s", who, val, T[P[sel].tile].label); } } } }
    else if (!strcmp(cmd, "ACCEPT")) { if (sel < 0 || P[sel].tile >= nT) snprintf(g_msg, sizeof g_msg, "select a placed tile first"); else { Judge *j = judge_of(T[P[sel].tile].label, 0);
            if (!j || strcmp(j->claude, "yes") || strcmp(j->owner, "yes")) snprintf(g_msg, sizeof g_msg, "accept needs BOTH yes (claude=%s owner=%s)", j ? jv(j->claude) : "?", j ? jv(j->owner) : "?");
            else { char ap[PL]; if (!strncmp(T[P[sel].tile].label, "test_", 5)) snprintf(ap, sizeof ap, "%s/accept_test.pdl", g_state); else snprintf(ap, sizeof ap, "%s/&.widgits/concept-bank/data/img/accept_candidates.pdl", g_house); struct stat st; int fresh = stat(ap, &st) != 0; FILE *af = fopen(ap, "a"); if (af) { if (fresh) fputs("SECTION | KEY | VALUE\n------------------------------\n# CAND | tile label | image path | claude | owner | day : accepted by BOTH judges; a candidate for the brixel concept row, a human promotes it (no direct bank write)\n", af); time_t t = time(NULL); char day[16]; strftime(day, sizeof day, "%Y-%m-%d", localtime(&t)); fprintf(af, "CAND | %s | %s | yes | yes | %s\n", T[P[sel].tile].label, T[P[sel].tile].path, day); fclose(af); snprintf(g_msg, sizeof g_msg, "accepted %s (candidate written)", T[P[sel].tile].label); } } } }
    else if (!strcmp(cmd, "DRAFT")) start_draft();
    else if (!strcmp(cmd, "DRAFTALL")) start_all();
    else if (!strncmp(cmd, "BATCH:", 6)) load_batch(cmd + 6);
    else if (!strcmp(cmd, "BATCHNEXT")) { scan_batches(); if (nBN) selBN = (selBN + 1) % nBN; }
    else if (!strcmp(cmd, "BATCHRUN")) { scan_batches(); if (nBN) load_batch(BN[selBN]); else snprintf(g_msg, sizeof g_msg, "no batches/*.pdl files"); }
    else if (!strcmp(cmd, "BATCHSTOP")) { for (int q = 0; q < nQ; q++) if (Q[q].state == 0) Q[q].state = 3; snprintf(g_msg, sizeof g_msg, "waiting jobs cancelled (running ones finish)"); }
    else if (!strcmp(cmd, "PARTNEXT") && nParts) selPart = (selPart + 1) % nParts;
    else if (!strcmp(cmd, "MODELNEXT") && nM) selM = (selM + 1) % nM;
    else if (!strncmp(cmd, "TILE:", 5)) { int n = atoi(cmd + 5); if (n >= 0 && n < nT && nP < MAXP) { P[nP] = (Place){ n, curx, cury, 0, 0, 1 }; sel = nP++; } }
    else if (!strncmp(cmd, "SEL:", 4)) { int n = atoi(cmd + 4); if (n >= 0 && n < nP) sel = n; }
    else if (!strncmp(cmd, "CELL:", 5)) { int n = atoi(cmd + 5); if (n >= 16 && n <= 256) { cell = n; if (curx * cell >= CW) curx = CW / cell - 1; if (cury * cell >= CH) cury = CH / cell - 1; } }
    else if (!strncmp(cmd, "CUR:", 4)) { char c = cmd[4]; if (c == 'L' && curx > 0) curx--; else if (c == 'R' && (curx + 1) * cell < CW) curx++; else if (c == 'U' && cury > 0) cury--; else if (c == 'D' && (cury + 1) * cell < CH) cury++; }
    else if (!strncmp(cmd, "MOVE:", 5) && sel >= 0) { char c = cmd[5]; Place *p = &P[sel]; if (c == 'L' && p->cx > 0) p->cx--; else if (c == 'R' && (p->cx + 1) * cell < CW) p->cx++; else if (c == 'U' && p->cy > 0) p->cy--; else if (c == 'D' && (p->cy + 1) * cell < CH) p->cy++; curx = p->cx; cury = p->cy; }
    else if (!strcmp(cmd, "ROT") && sel >= 0) P[sel].rot = (P[sel].rot + 1) & 3;
    else if (!strcmp(cmd, "FLIP") && sel >= 0) P[sel].flip = !P[sel].flip;
    else if (!strcmp(cmd, "VIS") && sel >= 0) P[sel].vis = !P[sel].vis;
    else if (!strcmp(cmd, "DEL") && sel >= 0) { memmove(&P[sel], &P[sel + 1], (size_t)(nP - sel - 1) * sizeof P[0]); nP--; if (sel >= nP) sel = nP - 1; }
    else if (!strcmp(cmd, "UP") && sel >= 0 && sel + 1 < nP) { Place t = P[sel]; P[sel] = P[sel + 1]; P[sel + 1] = t; sel++; }
    else if (!strcmp(cmd, "DOWN") && sel > 0) { Place t = P[sel]; P[sel] = P[sel - 1]; P[sel - 1] = t; sel--; }
    else if (!strcmp(cmd, "CLEAR")) { nP = 0; sel = -1; }
    else if (!strcmp(cmd, "RESCAN")) scan_tiles();
    else if (!strcmp(cmd, "EXPORT")) { write_canvas(); char p[PL]; int n = 1; struct stat st; do snprintf(p, sizeof p, "%s/tiles/mix_%d.png", g_pkg, n++); while (!stat(p, &st)); char td[PL]; snprintf(td, sizeof td, "%s/tiles", g_pkg); mkdir(td, 0755);
        stbi_write_png(p, CW, CH, 4, comp, CW * 4); snprintf(g_msg, sizeof g_msg, "exported %s", strrchr(p, '/') + 1); scan_tiles(); }
    dirty = 1;
}
static void poll_requests(void) {
    char p[PL]; snprintf(p, sizeof p, "%s/request.txt", g_state); struct stat st; if (stat(p, &st) || st.st_size <= req_off) return; FILE *f = fopen(p, "r"); if (!f) return; fseek(f, req_off, SEEK_SET);
    char line[512]; while (fgets(line, sizeof line, f)) { size_t n = strlen(line); if (!n || line[n - 1] != '\n') break; line[n - 1] = 0; req_off += (long)n; if (line[0]) handle(line); } fclose(f);
}
static int parent_alive(void) { char p[PL]; snprintf(p, sizeof p, "%s/module_parent.pid", g_pkg); FILE *f = fopen(p, "r"); if (!f) return 1; long pid = 0; int ok = fscanf(f, "%ld", &pid); fclose(f); if (ok != 1 || pid <= 0) return 1; return kill((pid_t)pid, 0) == 0 || errno == EPERM; }
static void bye(int s) { (void)s; _exit(0); }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: img_hai_manager <house_root> [package_dir]\n"); return 1; }
    snprintf(g_house, sizeof g_house, "%s", argv[1]); snprintf(g_pkg, sizeof g_pkg, "%s", argc > 2 ? argv[2] : "");
    if (!g_pkg[0]) snprintf(g_pkg, sizeof g_pkg, "%s/&.hq-apps/img-hai", g_house);
    snprintf(g_state, sizeof g_state, "%s/state", g_pkg); mkdir(g_state, 0755); snprintf(g_draft, sizeof g_draft, "%s/&.widgits/concept-bank/data/img/draft", g_house);
    signal(SIGTERM, bye); comp = calloc((size_t)CW * CH * 4, 1); if (!comp) return 1;
    { char p[PL]; snprintf(p, sizeof p, "%s/request.txt", g_state); struct stat st; if (!stat(p, &st)) req_off = st.st_size; }   /* ignore requests from before this start */
    load_models(); load_parts(); { char p[PL]; snprintf(p, sizeof p, "%s/job.txt", g_state); struct stat st; if (!stat(p, &st)) job_off = st.st_size; }
    load_judges(); scan_batches(); scan_tiles(); write_canvas(); write_ui();
    for (;;) { usleep(dirty ? 60000 : 200000); poll_requests(); poll_job(); dispatch_queue(); if (dirty) { write_canvas(); write_ui(); dirty = 0; } if (!parent_alive()) break; }
    return 0;
}
