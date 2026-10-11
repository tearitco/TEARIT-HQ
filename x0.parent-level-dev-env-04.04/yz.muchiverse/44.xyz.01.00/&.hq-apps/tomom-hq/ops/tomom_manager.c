#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>

/* tomom_manager: <module> for tomom-hq.xhtpm - a native X11-HQ EDITOR for
 * the plain-text weights of tomom (the hand-built word model at
 * #.Z.HUMAN_LLM/3.stage.llm.tomom...).  argv: <house> <pkg> [a3]
 * Design: #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/TOMOM-HQ-DESIGN.md
 *
 * Shape = chain-hq / irc-chat-hq: this ONE process owns all state, polls
 * <pkg>/tomom_action.txt (seq=<n>\ncmd=<VERB[:arg]>) and publishes
 * <pkg>/tomom_ui.txt (key=value, consumed by the xhtpm via vars=).  No
 * X11, no drawing: the text bars ("[#####-----]") are composed HERE.
 *
 * TARGET SAFETY: edits go to a TARGET DIR read from tomom_hq_config.pdl
 * (default = a scratch copy under state/work/tomom, made by the INIT
 * action from the live tomom's vocab_model.txt, meta_rl_weights.txt and
 * config.txt - the live files are only ever READ).  Re-pointing the
 * target at the live tomom is a deliberate later switch, not built.
 * Every weight write is atomic (temp file + rename).
 *
 * AUDIT: every change appends ONE row to state/edits.txt (append-only):
 *   EDIT | id | file | key | old | new | proposer=human | ts [| undo_of=<id>]
 * UNDO appends the inverse row (with undo_of=) and restores the value.
 * Reset-to-original is an ordinary (undoable) edit toward state/work/orig.
 *
 * CPU: the 50 ms loop only polls the action file + the ask child.  Parsed
 * vocab rows are cached in memory (file is ~100 KB); the file is re-read
 * only on INIT/RELOAD, never per tick.  Change detection of the action
 * file = seq growth, never mtime.
 *
 * SUBJECT tab edits the files chatbot_moe_v1 really reads
 * (curriculum/<S>_train/{attention,mlp,output_layer}.txt); Meta-RL / Vocab
 * edit tomom's top-level files, which the chatbot does NOT read.  Ask runs a
 * fixed-seed copy (srand(7)) so before/after answers are comparable. */

#define PL 4096
#define MAXV 4096
#define MAXM 256
#define MAXL 200       /* list rows published per page */
#define PAGE 150
#define STEP 0.05

static char house[PL], pkg[PL], tomom_live[PL];
static char target[PL];                 /* current target dir (absolute) */
static char work_root[PL], orig_dir[PL], edits_path[PL], ask_log[PL];

typedef struct { char *raw; char word[96]; char num[16]; double weight; } VRow;
typedef struct { char key[96]; double val; } MRow;

static VRow  vrows[MAXV];  static int nv = 0;  static char vhdr[512];
static MRow  mrows[MAXM];  static int nm = 0;
static VRow  ovrows[MAXV]; static int onv = 0;    /* originals (snapshot) */
static MRow  omrows[MAXM]; static int onm = 0;

static char tab[12] = "meta";
static int  sel_m = -1, sel_v = -1;
static char flt[128] = "";
static int  vpage = 0;
static int  filt_idx[MAXV]; static int nfilt = 0;
static char status[300] = "";
static int  loaded = 0;                 /* target has been parsed */

/* SUBJECT tab: the per-subject files chatbot_moe_v1 REALLY reads.  Three
 * levels in ONE list: subjects (from curriculum_bank.txt) -> the three
 * weight files of <S>_train -> cells (every float of one file).  A cell is
 * edited as raw token text spliced into the file (all whitespace kept), so
 * an edit/reset/undo changes exactly one token.  See TOMOM-HQ-DESIGN.md. */
#define MAXC 1100
#define MAXS 10
static char subj[MAXS][48]; static int nsubj = 0;
static int  sub_level = 0, sel_subj = -1, sel_file = -1, sel_c = -1;
static const char *SFILE[3] = { "attention_model.txt", "mlp_model.txt", "output_layer.txt" };
static const double SSTEP[3] = { 0.05, 0.01, 0.05 };   /* per-file step: ranges +-0.6 / +-0.05..0.2 / +-0.8 */
static char *cbuf = NULL; static int cn = 0, coff[MAXC], clen[MAXC];
static char cwords[256][100]; static int ncw = 0;      /* the subject's vocab words (labels for output_layer) */
static char cur_rel[160] = "";                          /* rel path of the file in cbuf */
static char orig_tok[48] = "";

/* undo stack, rebuilt from edits.txt at start */
typedef struct { char id[24], file[96], key[128], oldv[32], newv[32]; } Ed;
static Ed ustack[4096]; static int nu = 0;
static int n_edit_rows = 0;

/* ask */
static pid_t ask_pid = -1; static time_t ask_t0 = 0;
static char ask_prompt[320] = "", ask_ans[1024] = "";
static char ask_state[200] = "";

/* ---------------------------------------------------------------- */
static void sanitize(char *s) {
    for (char *p = s; *p; p++)
        if (*p == '|' || *p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
}
static void mkdir_p(const char *path) {
    char t[PL]; snprintf(t, sizeof(t), "%s", path);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = '\0'; mkdir(t, 0755); *p = '/'; }
    mkdir(t, 0755);
}
static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static void now_ts(char *out, size_t n) {
    time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%dT%H:%M:%S", &tm);
}
static char *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)n, f); b[r] = '\0'; fclose(f);
    if (len) *len = r; return b;
}
/* atomic: temp in the same dir, then rename */
static int atomic_write(const char *path, const char *data, size_t n) {
    char tmp[PL]; snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb"); if (!f) return -1;
    if (fwrite(data, 1, n, f) != n) { fclose(f); unlink(tmp); return -1; }
    fflush(f); fsync(fileno(f)); fclose(f);
    return rename(tmp, path);
}
static void run_wait(char *const av[], const char *cwd, int timeout_s) {
    pid_t pid = fork(); if (pid < 0) return;
    if (pid == 0) {
        if (cwd && chdir(cwd) != 0) _exit(126);
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); }
        execvp(av[0], av); _exit(127);
    }
    for (int i = 0; i < timeout_s * 20; i++) {
        if (waitpid(pid, NULL, WNOHANG) == pid) return;
        usleep(50000);
    }
    kill(pid, SIGKILL); waitpid(pid, NULL, 0);
}

/* ---------------------------------------------------------------- */
/* config pdl: "SECTION | target_dir | <path>" (relative = vs pkg)    */

static char cfg_path[PL];
static void load_config(void) {
    char def[PL]; snprintf(def, sizeof(def), "%s/state/work/tomom", pkg);
    snprintf(target, sizeof(target), "%s", def);
    char *b = slurp(cfg_path, NULL);
    if (!b) {
        FILE *f = fopen(cfg_path, "w");
        if (f) {
            fprintf(f, "# tomom-hq config. target_dir = dir whose vocab_model.txt / meta_rl_weights.txt are edited.\n"
                       "# Relative = against this package dir. Default = scratch copy (safe). Do not point at the live\n"
                       "# tomom unless you mean it.\n"
                       "SECTION      | target_dir          | state/work/tomom\n");
            fclose(f);
        }
        return;
    }
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (ln[0] == '#') continue;
        char *c1 = strchr(ln, '|'); if (!c1) continue;
        char *c2 = strchr(c1 + 1, '|'); if (!c2) continue;
        char key[64]; snprintf(key, sizeof(key), "%.*s", (int)(c2 - c1 - 1), c1 + 1);
        char *k = key; while (*k == ' ') k++;
        for (char *e = k + strlen(k); e > k && e[-1] == ' '; ) *--e = '\0';
        if (strcmp(k, "target_dir") != 0) continue;
        char *v = c2 + 1; while (*v == ' ') v++;
        for (char *e = v + strlen(v); e > v && (e[-1] == ' ' || e[-1] == '\r'); ) *--e = '\0';
        if (!*v) continue;
        if (v[0] == '/') snprintf(target, sizeof(target), "%s", v);
        else snprintf(target, sizeof(target), "%s/%s", pkg, v);
    }
    free(b);
}

/* ---------------------------------------------------------------- */
/* parsing                                                            */

static void parse_vocab(const char *dir, VRow *rows, int *n, char *hdr) {
    *n = 0;
    char p[PL]; snprintf(p, sizeof(p), "%s/vocab_model.txt", dir);
    char *b = slurp(p, NULL); if (!b) return;
    int first = 1;
    for (char *s = b; *s && *n < MAXV; ) {
        char *e = strchr(s, '\n'); size_t l = e ? (size_t)(e - s) : strlen(s);
        char *raw = malloc(l + 1); memcpy(raw, s, l); raw[l] = '\0';
        if (l && raw[l - 1] == '\r') raw[l - 1] = '\0';
        if (first) { if (hdr) snprintf(hdr, 512, "%s", raw); free(raw); first = 0; }
        else if (raw[0]) {
            VRow *r = &rows[*n];
            char w[96] = "", nu_[16] = ""; double a, pe, wt = 0;
            char *cp = raw; int t = 0; char tok[256];
            while (*cp) {
                while (*cp == ' ') cp++;
                if (!*cp) break;
                int k = 0; while (*cp && *cp != ' ' && k < 255) tok[k++] = *cp++;
                tok[k] = '\0';
                if (t == 0) snprintf(nu_, sizeof(nu_), "%s", tok);
                else if (t == 1) snprintf(w, sizeof(w), "%s", tok);
                else if (t == 4) wt = atof(tok);
                t++;
            }
            (void)a; (void)pe;
            if (t >= 9) { r->raw = raw; snprintf(r->word, sizeof(r->word), "%s", w);
                          snprintf(r->num, sizeof(r->num), "%s", nu_); r->weight = wt; (*n)++; }
            else free(raw);
        } else free(raw);
        if (!e) break; s = e + 1;
    }
    free(b);
}
static void free_vocab(VRow *rows, int *n) { for (int i = 0; i < *n; i++) free(rows[i].raw); *n = 0; }
static void parse_meta(const char *dir, MRow *rows, int *n) {
    *n = 0;
    char p[PL]; snprintf(p, sizeof(p), "%s/meta_rl_weights.txt", dir);
    char *b = slurp(p, NULL); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln && *n < MAXM; ln = strtok(NULL, "\n")) {
        char k[96]; double v;
        if (sscanf(ln, "%95s %lf", k, &v) == 2) { snprintf(rows[*n].key, 96, "%s", k); rows[*n].val = v; (*n)++; }
    }
    free(b);
}
static void load_target(void) {
    free_vocab(vrows, &nv); nm = 0; loaded = 0;
    char p[PL]; snprintf(p, sizeof(p), "%s/vocab_model.txt", target);
    if (!exists(p)) { snprintf(status, sizeof(status), "target has no vocab_model.txt - press 'init scratch copy'"); return; }
    parse_vocab(target, vrows, &nv, vhdr);
    parse_meta(target, mrows, &nm);
    free_vocab(ovrows, &onv); onm = 0;
    if (exists(orig_dir)) { parse_vocab(orig_dir, ovrows, &onv, NULL); parse_meta(orig_dir, omrows, &onm); }
    loaded = 1;
    snprintf(status, sizeof(status), "loaded %d vocab rows, %d meta rows", nv, nm);
}
static void rebuild_filter(void) {
    nfilt = 0;
    for (int i = 0; i < nv; i++)
        if (!flt[0] || strcasestr(vrows[i].word, flt)) filt_idx[nfilt++] = i;
    int pages = nfilt ? (nfilt + PAGE - 1) / PAGE : 1;
    if (vpage >= pages) vpage = pages - 1; if (vpage < 0) vpage = 0;
}

/* ---------------------------------------------------------------- */
/* writing                                                            */

static int write_meta(void) {
    size_t cap = (size_t)nm * 140 + 16; char *b = malloc(cap); size_t o = 0;
    for (int i = 0; i < nm; i++) o += (size_t)snprintf(b + o, cap - o, "%s %.6f\n", mrows[i].key, mrows[i].val);
    char p[PL]; snprintf(p, sizeof(p), "%s/meta_rl_weights.txt", target);
    int r = atomic_write(p, b, o); free(b); return r;
}
/* token 4 (weight) rewritten, every other token kept verbatim */
static void vrow_set_weight(VRow *r, double w) {
    size_t L = strlen(r->raw) + 40; char *nb = malloc(L); size_t o = 0; int t = 0;
    const char *cp = r->raw; char tok[256];
    while (*cp) {
        while (*cp == ' ') cp++;
        if (!*cp) break;
        int k = 0; while (*cp && *cp != ' ' && k < 255) tok[k++] = *cp++;
        tok[k] = '\0';
        if (t) nb[o++] = ' ';
        if (t == 4) o += (size_t)snprintf(nb + o, L - o, "%.6f", w);
        else { memcpy(nb + o, tok, (size_t)k); o += (size_t)k; }
        t++;
    }
    nb[o] = '\0'; free(r->raw); r->raw = nb; r->weight = w;
}
static int write_vocab(void) {
    size_t cap = strlen(vhdr) + 2; for (int i = 0; i < nv; i++) cap += strlen(vrows[i].raw) + 1;
    char *b = malloc(cap + 1); size_t o = 0;
    o += (size_t)sprintf(b + o, "%s\n", vhdr);
    for (int i = 0; i < nv; i++) o += (size_t)sprintf(b + o, "%s\n", vrows[i].raw);
    char p[PL]; snprintf(p, sizeof(p), "%s/vocab_model.txt", target);
    int r = atomic_write(p, b, o); free(b); return r;
}

static void append_edit(const char *file, const char *key, const char *oldv, const char *newv,
                        const char *undo_of, char *idout) {
    char ts[32]; now_ts(ts, sizeof(ts));
    n_edit_rows++;
    char id[24]; snprintf(id, sizeof(id), "E%d", n_edit_rows);
    mkdir_p(work_root);
    FILE *f = fopen(edits_path, "a"); if (!f) return;
    fprintf(f, "EDIT | %s | %s | %s | %s | %s | proposer=human | %s", id, file, key, oldv, newv, ts);
    if (undo_of) fprintf(f, " | undo_of=%s", undo_of);
    fputc('\n', f); fclose(f);
    if (idout) snprintf(idout, 24, "%s", id);
}

/* apply file/key := newv ; returns 0 ok, fills oldv */
static int stok_set(const char *rel, int idx, const char *newstr, char *oldv);
static int apply_set_s(const char *file, const char *key, const char *newstr, char *oldv) {
    double newv = atof(newstr);
    if (!strncmp(file, "curriculum/", 11)) {
        const char *c = strchr(key, ':'); if (!c) return -1;
        return stok_set(file, atoi(key), newstr, oldv);
    }
    if (strcmp(file, "meta_rl_weights.txt") == 0) {
        for (int i = 0; i < nm; i++) if (strcmp(mrows[i].key, key) == 0) {
            snprintf(oldv, 32, "%.6f", mrows[i].val); mrows[i].val = newv; return write_meta();
        }
    } else if (strcmp(file, "vocab_model.txt") == 0) {
        char num[16]; const char *c = strchr(key, ':'); if (!c) return -1;
        snprintf(num, sizeof(num), "%.*s", (int)(c - key), key);
        for (int i = 0; i < nv; i++) if (strcmp(vrows[i].num, num) == 0) {
            snprintf(oldv, 32, "%.6f", vrows[i].weight); vrow_set_weight(&vrows[i], newv); return write_vocab();
        }
    }
    return -1;
}

/* ---- subject-file helpers ------------------------------------------ */
static int tok_parse(char *b, int *off, int *len, int max) {
    int n = 0; char *p = b;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        if (!*p) break;
        char *st = p; while (*p && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') p++;
        off[n] = (int)(st - b); len[n] = (int)(p - st); n++;
    }
    return n;
}
/* token idx of dir/rel as text; 0 ok */
static int stok_get(const char *dir, const char *rel, int idx, char *out, size_t osz) {
    char p[PL]; snprintf(p, sizeof(p), "%s/%s", dir, rel);
    char *b = slurp(p, NULL); if (!b) return -1;
    int off[MAXC], len[MAXC]; int n = tok_parse(b, off, len, MAXC);
    int r = -1;
    if (idx >= 0 && idx < n) { snprintf(out, osz, "%.*s", len[idx] < (int)osz - 1 ? len[idx] : (int)osz - 1, b + off[idx]); r = 0; }
    free(b); return r;
}
/* splice newstr over token idx of target/rel, atomically; oldv gets the old token */
static int stok_set(const char *rel, int idx, const char *newstr, char *oldv) {
    char p[PL]; snprintf(p, sizeof(p), "%s/%s", target, rel);
    size_t L; char *b = slurp(p, &L); if (!b) return -1;
    int off[MAXC], len[MAXC]; int n = tok_parse(b, off, len, MAXC);
    if (idx < 0 || idx >= n) { free(b); return -1; }
    snprintf(oldv, 32, "%.*s", len[idx] < 31 ? len[idx] : 31, b + off[idx]);
    size_t nl = strlen(newstr); char *nb = malloc(L + nl + 1);
    memcpy(nb, b, (size_t)off[idx]); memcpy(nb + off[idx], newstr, nl);
    memcpy(nb + off[idx] + nl, b + off[idx] + len[idx], L - (size_t)(off[idx] + len[idx]));
    int r = atomic_write(p, nb, L - (size_t)len[idx] + nl);
    free(nb); free(b); return r;
}
static void rel_file(char *out, size_t n, int si, int fi) { snprintf(out, n, "curriculum/%s_train/%s", subj[si], SFILE[fi]); }
static void ensure_curriculum(void) {   /* whole tomom curriculum + chatbot source into scratch AND the orig snapshot */
    char bank[PL], oc[PL]; snprintf(bank, sizeof(bank), "%s/curriculum_bank.txt", target); snprintf(oc, sizeof(oc), "%s/curriculum", orig_dir);
    char a1[PL], a2[PL], a3[PL], a4[PL];
    snprintf(a1, sizeof(a1), "%s/curriculum", tomom_live); snprintf(a2, sizeof(a2), "%s/curriculum_bank.txt", tomom_live);
    snprintf(a3, sizeof(a3), "%s/chatbot_moe_v1.c", tomom_live); snprintf(a4, sizeof(a4), "%s/", target);
    if (!exists(bank)) { char *cpv[] = { "cp", "-rn", a1, a2, a3, a4, NULL }; run_wait(cpv, NULL, 30); }
    if (!exists(oc)) { char a5[PL]; snprintf(a5, sizeof(a5), "%s/", orig_dir); char *cpo[] = { "cp", "-rn", a1, a5, NULL }; mkdir_p(orig_dir); run_wait(cpo, NULL, 30); }
}
static void load_subjects(void) {
    nsubj = 0; char p[PL]; snprintf(p, sizeof(p), "%s/curriculum_bank.txt", target);
    char *b = slurp(p, NULL); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln && nsubj < MAXS; ln = strtok(NULL, "\n")) {
        char *a = strchr(ln, '/'); if (!a) continue; a++;
        char *e = strchr(a, '/'); if (!e) continue;
        snprintf(subj[nsubj++], 48, "%.*s", (int)(e - a), a);
    }
    free(b);
}
static void load_cells(void) {
    free(cbuf); cbuf = NULL; cn = 0; ncw = 0; cur_rel[0] = '\0';
    if (sel_subj < 0 || sel_file < 0) return;
    rel_file(cur_rel, sizeof(cur_rel), sel_subj, sel_file);
    char p[PL]; snprintf(p, sizeof(p), "%s/%s", target, cur_rel);
    cbuf = slurp(p, NULL); if (!cbuf) { cur_rel[0] = '\0'; return; }
    cn = tok_parse(cbuf, coff, clen, MAXC);
    snprintf(p, sizeof(p), "%s/curriculum/%s/%s.txt", target, subj[sel_subj], subj[sel_subj]);
    char *v = slurp(p, NULL); if (!v) return;
    int first = 1;
    for (char *ln = strtok(v, "\n"); ln && ncw < 256; ln = strtok(NULL, "\n")) {
        if (first) { first = 0; continue; }
        char num[16], w[100]; if (sscanf(ln, "%15s %99s", num, w) == 2) snprintf(cwords[ncw++], 100, "%s", w);
    }
    free(v);
}
static void cell_label(int i, char *out, size_t n) {
    const char *w;
    if (sel_file == 0) { static const char *m[3] = { "W_q", "W_k", "W_v" }; snprintf(out, n, "%s[%d][%d]", m[(i / 49) % 3], (i % 49) / 7, i % 7); }
    else if (sel_file == 1) { if (i < 112) snprintf(out, n, "w[%d][%d]", i / 16, i % 16); else snprintf(out, n, "b[%d]", i - 112); }
    else {
        int vs = ncw > 0 ? ncw : 1;
        if (i < 16 * vs) { w = (i % vs) < ncw ? cwords[i % vs] : "?"; snprintf(out, n, "w[%d][%s]", i / vs, w); }
        else { int j = i - 16 * vs; snprintf(out, n, "bias[%s]", j < ncw ? cwords[j] : "?"); }
    }
    sanitize(out);
}
static double cell_val(int i) { char t[48]; snprintf(t, sizeof(t), "%.*s", clen[i] < 47 ? clen[i] : 47, cbuf + coff[i]); return atof(t); }
static void rebuild_cell_filter(void) {
    nfilt = 0; char lb[160];
    for (int i = 0; i < cn; i++) { cell_label(i, lb, sizeof(lb)); if (!flt[0] || strcasestr(lb, flt)) filt_idx[nfilt++] = i; }
    int pages = nfilt ? (nfilt + PAGE - 1) / PAGE : 1;
    if (vpage >= pages) vpage = pages - 1; if (vpage < 0) vpage = 0;
}
static double cur_step(void) { return !strcmp(tab, "subject") && sel_file >= 0 ? SSTEP[sel_file] : STEP; }
static char sel_rel[160];
static int cur_sel(const char **file, char *key, size_t ksz, double *val, double *orig, int *has_orig) {
    *has_orig = 0;
    if (!loaded) return -1;
    if (strcmp(tab, "meta") == 0 && sel_m >= 0 && sel_m < nm) {
        *file = "meta_rl_weights.txt"; snprintf(key, ksz, "%s", mrows[sel_m].key); *val = mrows[sel_m].val;
        for (int i = 0; i < onm; i++) if (!strcmp(omrows[i].key, mrows[sel_m].key)) { *orig = omrows[i].val; *has_orig = 1; }
        return 0;
    }
    if (strcmp(tab, "vocab") == 0 && sel_v >= 0 && sel_v < nv) {
        *file = "vocab_model.txt"; snprintf(key, ksz, "%s:%s.weight", vrows[sel_v].num, vrows[sel_v].word);
        sanitize(key); *val = vrows[sel_v].weight;
        for (int i = 0; i < onv; i++) if (!strcmp(ovrows[i].num, vrows[sel_v].num)) { *orig = ovrows[i].weight; *has_orig = 1; }
        return 0;
    }
    if (strcmp(tab, "subject") == 0 && sub_level == 2 && sel_c >= 0 && sel_c < cn && cbuf) {
        char lb[160]; cell_label(sel_c, lb, sizeof(lb));
        snprintf(sel_rel, sizeof(sel_rel), "%s", cur_rel); *file = sel_rel;
        snprintf(key, ksz, "%d:%s", sel_c, lb); *val = cell_val(sel_c);
        if (stok_get(orig_dir, cur_rel, sel_c, orig_tok, sizeof(orig_tok)) == 0) { *orig = atof(orig_tok); *has_orig = 1; }
        return 0;
    }
    return -1;
}
static void do_change_s(const char *newbuf) {
    const char *file; char key[128], oldv[32]; double val, orig; int ho;
    if (cur_sel(&file, key, sizeof(key), &val, &orig, &ho) != 0) { snprintf(status, sizeof(status), "select a row first"); return; }
    if (apply_set_s(file, key, newbuf, oldv) != 0) { snprintf(status, sizeof(status), "write failed for %s", key); return; }
    char id[24]; append_edit(file, key, oldv, newbuf, NULL, id);
    if (nu < 4096) { Ed *e = &ustack[nu++]; snprintf(e->id, 24, "%s", id); snprintf(e->file, 96, "%s", file);
        snprintf(e->key, 128, "%s", key); snprintf(e->oldv, 32, "%s", oldv); snprintf(e->newv, 32, "%s", newbuf); }
    if (!strncmp(file, "curriculum/", 11)) { int keep = sel_c; load_cells(); sel_c = keep; }
    snprintf(status, sizeof(status), "%s: %s %s -> %s", id, key, oldv, newbuf);
}
static void do_change(double newv) {
    char nb[32];
    if (!strcmp(tab, "subject")) snprintf(nb, sizeof(nb), "%.9g", newv); else snprintf(nb, sizeof(nb), "%.6f", newv);
    do_change_s(nb);
}
static void do_undo(void) {
    if (nu <= 0) { snprintf(status, sizeof(status), "nothing to undo"); return; }
    Ed e = ustack[nu - 1]; char cur[32];
    if (apply_set_s(e.file, e.key, e.oldv, cur) != 0) { snprintf(status, sizeof(status), "undo of %s failed (key not in target)", e.id); nu--; return; }
    nu--;
    if (!strncmp(e.file, "curriculum/", 11)) { int keep = sel_c; load_cells(); sel_c = keep; }
    char id[24]; append_edit(e.file, e.key, cur, e.oldv, e.id, id);
    snprintf(status, sizeof(status), "%s: undid %s (%s back to %s)", id, e.id, e.key, e.oldv);
}
static void load_undo_stack(void) {
    nu = 0; n_edit_rows = 0;
    char *b = slurp(edits_path, NULL); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (strncmp(ln, "EDIT |", 6) != 0) continue;
        n_edit_rows++;
        char *f[10]; int nf = 0; char *s = ln;
        while (nf < 10) { f[nf++] = s; char *c = strstr(s, " | "); if (!c) break; *c = '\0'; s = c + 3; }
        if (nf >= 9 && strncmp(f[8], "undo_of=", 8) == 0) { if (nu > 0) nu--; }
        else if (nf >= 7 && nu < 4096) {
            Ed *e = &ustack[nu++]; snprintf(e->id, 24, "%s", f[1]); snprintf(e->file, 96, "%s", f[2]);
            snprintf(e->key, 128, "%s", f[3]); snprintf(e->oldv, 32, "%s", f[4]); snprintf(e->newv, 32, "%s", f[5]);
        }
    }
    free(b);
}

/* ---------------------------------------------------------------- */
/* init: scratch copy of the three weight files + orig snapshot       */

static int in_scratch(void) {
    char def[PL]; snprintf(def, sizeof(def), "%s/state/work/", pkg);
    return strncmp(target, def, strlen(def)) == 0;
}
static void cp_file(const char *srcdir, const char *name, const char *dstdir) {
    char s[PL], d[PL]; snprintf(s, sizeof(s), "%s/%s", srcdir, name); snprintf(d, sizeof(d), "%s/%s", dstdir, name);
    size_t n; char *b = slurp(s, &n); if (!b) return;
    atomic_write(d, b, n); free(b);
}
static void do_init(void) {
    if (!in_scratch()) { snprintf(status, sizeof(status), "init refused: target is not under state/work (never overwrites a non-scratch dir)"); return; }
    char p[PL]; snprintf(p, sizeof(p), "%s/vocab_model.txt", target);
    if (exists(p)) { snprintf(status, sizeof(status), "scratch copy already exists (edits kept) - delete state/work/tomom to re-init"); load_target(); return; }
    mkdir_p(target); mkdir_p(orig_dir);
    const char *fl[] = { "vocab_model.txt", "meta_rl_weights.txt", "config.txt" };
    for (int i = 0; i < 3; i++) { cp_file(tomom_live, fl[i], target); cp_file(tomom_live, fl[i], orig_dir); }
    load_target();
    if (loaded) snprintf(status, sizeof(status), "scratch copy created from live tomom (read-only source): %d vocab, %d meta rows", nv, nm);
}

/* ---------------------------------------------------------------- */
/* ask: tomom's chatbot_moe_v1 on the scratch copy                    */

static void ask_finish(const char *outfile, int killed) {
    char *b = slurp(outfile, NULL); ask_ans[0] = '\0';
    if (b) {
        char *last = NULL, *s = b;
        while ((s = strstr(s, "Response:")) != NULL) { last = s; s += 9; }
        if (last) {
            last += 9; while (*last == ' ') last++;
            size_t n = strcspn(last, "\n"); if (n >= sizeof(ask_ans)) n = sizeof(ask_ans) - 1;
            memcpy(ask_ans, last, n); ask_ans[n] = '\0';
            for (char *p = ask_ans; *p; p++) if (*p == '\r') *p = ' ';
            for (size_t i = strlen(ask_ans); i > 0 && ask_ans[i - 1] == ' '; i--) ask_ans[i - 1] = '\0';
        }
        free(b);
    }
    if (killed) snprintf(ask_state, sizeof(ask_state), "ask timed out after 60 s");
    else if (!ask_ans[0]) snprintf(ask_state, sizeof(ask_state), "no Response line in output");
    else snprintf(ask_state, sizeof(ask_state), "answered in %ds", (int)(time(NULL) - ask_t0));
    char pr[320], an[1024]; snprintf(pr, sizeof(pr), "%s", ask_prompt); snprintf(an, sizeof(an), "%s", ask_ans);
    sanitize(pr); sanitize(an);
    char ts[32]; now_ts(ts, sizeof(ts));
    FILE *f = fopen(ask_log, "a");
    if (f) { fprintf(f, "ASK | %s | %s | %s | target=%s\n", ts, pr, an, target); fclose(f); }
}
static void ask_poll(void) {
    if (ask_pid <= 0) return;
    char out[PL]; snprintf(out, sizeof(out), "%s/ask_out.txt", work_root);
    if (waitpid(ask_pid, NULL, WNOHANG) == ask_pid) { ask_pid = -1; ask_finish(out, 0); }
    else if (time(NULL) - ask_t0 > 60) { kill(ask_pid, SIGKILL); waitpid(ask_pid, NULL, 0); ask_pid = -1; ask_finish(out, 1); }
}
static void do_ask(const char *prompt) {
    if (ask_pid > 0) { snprintf(ask_state, sizeof(ask_state), "still asking, wait"); return; }
    if (!in_scratch()) { snprintf(ask_state, sizeof(ask_state), "ask only runs on the scratch copy"); return; }
    if (!loaded) { snprintf(ask_state, sizeof(ask_state), "init the scratch copy first"); return; }
    ensure_curriculum();
    /* Reproducible asks: chatbot_moe_v1 seeds with srand(time(NULL)) and picks
     * uniformly among the top-5 scores, so two runs differ. We compile a copy of
     * the (live, unmodified) source with that ONE call replaced by srand(7) into
     * the scratch dir: same prompt + same files => same answer. */
    char bin[PL]; snprintf(bin, sizeof(bin), "%s/chatbot_seed7", target);
    if (!exists(bin)) {
        char sp[PL]; snprintf(sp, sizeof(sp), "%s/chatbot_moe_v1.c", target);
        size_t L; char *src = slurp(sp, &L); char *hit = src ? strstr(src, "srand(time(NULL))") : NULL;
        if (!hit) { free(src); snprintf(ask_state, sizeof(ask_state), "cannot find srand(time(NULL)) in chatbot source"); return; }
        char *ns = malloc(L + 16); size_t pre = (size_t)(hit - src);
        memcpy(ns, src, pre); memcpy(ns + pre, "srand(7)", 8); strcpy(ns + pre + 8, hit + 17);
        char dp[PL]; snprintf(dp, sizeof(dp), "%s/chatbot_seed7.c", target);
        atomic_write(dp, ns, strlen(ns)); free(ns); free(src);
        char *gc[] = { "gcc", "-O2", "-w", "-o", "chatbot_seed7", "chatbot_seed7.c", "-lm", NULL };
        run_wait(gc, target, 60);
        if (!exists(bin)) { snprintf(ask_state, sizeof(ask_state), "build of chatbot_seed7 failed"); return; }
    }
    snprintf(ask_prompt, sizeof(ask_prompt), "%s", prompt); ask_ans[0] = '\0';
    char out[PL]; snprintf(out, sizeof(out), "%s/ask_out.txt", work_root);
    pid_t pid = fork(); if (pid < 0) return;
    if (pid == 0) {
        if (chdir(target) != 0) _exit(126);
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        int o = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644), dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 2); }
        if (o >= 0) dup2(o, 1);
        execl("./chatbot_seed7", "chatbot_seed7", "curriculum_bank.txt", prompt, "12", "0.5", (char *)NULL);
        _exit(127);
    }
    ask_pid = pid; ask_t0 = time(NULL);
    snprintf(ask_state, sizeof(ask_state), "asking ...");
}

/* ---------------------------------------------------------------- */
/* projection                                                         */

static void bar(double v, char *out) {
    double f = (v + 1.0) / 2.0 * 10.0; int n = (int)floor(f + 0.5);
    if (n < 0) n = 0; if (n > 10) n = 10;
    out[0] = '[';
    for (int i = 0; i < 10; i++) out[1 + i] = i < n ? '#' : '-';
    out[11] = ']'; out[12] = '\0';
}
static void wrap_rows(FILE *f, const char *text, int *n) {
    size_t L = strlen(text), i = 0;
    while (i < L && *n < 12) {
        size_t take = L - i > 78 ? 78 : L - i;
        while (i + take < L && take > 40 && text[i + take] != ' ') take--;   /* break at a space if possible */
        fprintf(f, "l_%d_text=%.*s\nl_%d_cls=ask-row\nl_%d_idx=-1\n", *n, (int)take, text + i, *n, *n);
        (*n)++; i += take; while (text[i] == ' ') i++;
    }
}
static void write_ui(void) {
    char dst[PL], tmp[PL]; snprintf(dst, sizeof(dst), "%s/tomom_ui.txt", pkg); snprintf(tmp, sizeof(tmp), "%s.tmp", dst);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    int tm = !strcmp(tab, "meta"), tv = !strcmp(tab, "vocab"), ta = !strcmp(tab, "ask"), ts = !strcmp(tab, "subject");
    const char *tshow = target; size_t pl = strlen(pkg);
    if (!strncmp(target, pkg, pl) && target[pl] == '/') tshow = target + pl + 1;
    fprintf(f, "target=%s\n", tshow);
    fprintf(f, "safe=%s\n", in_scratch() ? "scratch" : "NOT-SCRATCH");
    fprintf(f, "status=%s\n", status);
    fprintf(f, "cur_tab=%s\n", tab);
    fprintf(f, "cls_meta=%s\ncls_vocab=%s\ncls_ask=%s\ncls_subject=%s\n", tm ? "tab-active" : "", tv ? "tab-active" : "", ta ? "tab-active" : "", ts ? "tab-active" : "");
    fprintf(f, "tab_meta=%s\ntab_vocab=%s\ntab_ask=%s\ntab_subject=%s\n", tm ? "1" : "", tv ? "1" : "", ta ? "1" : "", ts ? "1" : "");
    fprintf(f, "tab_edit=%s\n", (((tm || tv) && loaded) || (ts && sub_level == 2)) ? "1" : "");
    fprintf(f, "show_flt=%s\n", (tv || (ts && sub_level == 2)) ? "1" : "");
    fprintf(f, "show_back=%s\n", (ts && sub_level > 0) ? "1" : "");
    char crumb[300] = "";
    if (ts) { snprintf(crumb, sizeof(crumb), "subject%s%s%s%s", sub_level >= 1 ? " / " : "", sub_level >= 1 && sel_subj >= 0 ? subj[sel_subj] : "",
                       sub_level >= 2 ? " / " : "", sub_level >= 2 && sel_file >= 0 ? SFILE[sel_file] : ""); }
    fprintf(f, "crumb=%s\n", crumb);
    fprintf(f, "step=%g\n", cur_step());
    fprintf(f, "flt=%s\n", flt);
    fprintf(f, "undo_n=%d\n", nu);

    /* selection detail */
    const char *file; char key[128]; double val = 0, orig = 0; int ho = 0; char b[16];
    char d1[400] = "", d2[400] = "", d3[400] = "";
    if (cur_sel(&file, key, sizeof(key), &val, &orig, &ho) == 0) {
        bar(val, b);
        if (tm || ts) { snprintf(d1, sizeof(d1), "%s%s%s", ts ? cur_rel : "", ts ? "  " : "", key); snprintf(d2, sizeof(d2), "value %s %+.6f", b, val); }
        else {
            VRow *r = &vrows[sel_v]; char w[96]; snprintf(w, sizeof(w), "%s", r->word); sanitize(w);
            snprintf(d1, sizeof(d1), "#%s  %s", r->num, w);
            int t = 0; char *cp = r->raw, tok[256], fld[7][40];
            while (*cp) { while (*cp == ' ') cp++; if (!*cp) break; int k = 0; while (*cp && *cp != ' ' && k < 255) tok[k++] = *cp++; tok[k] = 0;
                          if (t >= 2 && t < 9) snprintf(fld[t - 2], 40, "%s", tok); t++; }
            snprintf(d2, sizeof(d2), "embedding %s   pe %s   bias1-4 %s %s %s %s", fld[0], fld[1], fld[3], fld[4], fld[5], fld[6]);
            snprintf(d3, sizeof(d3), "weight %s %+.6f", b, val);
        }
        char o3[120] = ""; if (ho) snprintf(o3, sizeof(o3), "   (original %+.6f%s)", orig, fabs(orig - val) < 5e-7 ? ", unchanged" : ", CHANGED");
        if (tm || ts) {
            if (ho) snprintf(d3, sizeof(d3), "original %+.6f  -  %s", orig, fabs(orig - val) < 5e-7 ? "unchanged" : "CHANGED");
            else snprintf(d3, sizeof(d3), "original: n/a");
        } else strncat(d3, o3, sizeof(d3) - strlen(d3) - 1);
    }
    fprintf(f, "sel1=%s\nsel2=%s\nsel3=%s\n", d1, d2, d3);
    fprintf(f, "has_sel=%s\n", d1[0] ? "1" : "");

    /* list */
    int n = 0;
    char pagetxt[120] = ""; int pshow = 0;
    /* body rows are buffered so n_list precedes them */
    char *body = NULL; size_t bsz = 0; FILE *bf = open_memstream(&body, &bsz);
    if (tm && loaded) {
        for (int i = 0; i < nm; i++) {
            char bb[16]; bar(mrows[i].val, bb); char k[96]; snprintf(k, sizeof(k), "%s", mrows[i].key); sanitize(k);
            fprintf(bf, "l_%d_text=%s %+.3f  %s\nl_%d_cls=%s\nl_%d_idx=%d\n", i, bb, mrows[i].val, k, i, i == sel_m ? "row-sel" : "row", i, i);
        }
        n = nm;
    } else if (tv && loaded) {
        rebuild_filter();
        int from = vpage * PAGE, to = from + PAGE; if (to > nfilt) to = nfilt;
        for (int j = from; j < to; j++) {
            int i = filt_idx[j]; char bb[16]; bar(vrows[i].weight, bb); char w[96]; snprintf(w, sizeof(w), "%s", vrows[i].word); sanitize(w);
            fprintf(bf, "l_%d_text=%s %+.3f  %s  #%s\nl_%d_cls=%s\nl_%d_idx=%d\n", n, bb, vrows[i].weight, w, vrows[i].num, n, i == sel_v ? "row-sel" : "row", n, i);
            n++;
        }
        int pages = nfilt ? (nfilt + PAGE - 1) / PAGE : 1;
        snprintf(pagetxt, sizeof(pagetxt), "%d match%s  -  page %d/%d  (rows %d-%d)", nfilt, nfilt == 1 ? "" : "es", vpage + 1, pages, nfilt ? from + 1 : 0, to);
        pshow = 1;
    } else if (ts && nsubj == 0) {
        fprintf(bf, "l_0_text=(no subjects - press init scratch, then reload)\nl_0_cls=row\nl_0_idx=-1\n"); n = 1;
    } else if (ts && sub_level == 0) {
        for (int i = 0; i < nsubj; i++) fprintf(bf, "l_%d_text=%s\nl_%d_cls=%s\nl_%d_idx=%d\n", i, subj[i], i, i == sel_subj ? "row-sel" : "row", i, i);
        n = nsubj;
    } else if (ts && sub_level == 1) {
        for (int i = 0; i < 3; i++) fprintf(bf, "l_%d_text=%s   (step %g)\nl_%d_cls=row\nl_%d_idx=%d\n", i, SFILE[i], SSTEP[i], i, i, i);
        n = 3;
    } else if (ts && sub_level == 2) {
        rebuild_cell_filter();
        int from = vpage * PAGE, to = from + PAGE; if (to > nfilt) to = nfilt;
        for (int j = from; j < to; j++) {
            int i = filt_idx[j]; char bb[16], lb[160]; double v = cell_val(i); bar(v, bb); cell_label(i, lb, sizeof(lb));
            fprintf(bf, "l_%d_text=%s %+.4f  %s\nl_%d_cls=%s\nl_%d_idx=%d\n", n, bb, v, lb, n, i == sel_c ? "row-sel" : "row", n, i);
            n++;
        }
        int pages = nfilt ? (nfilt + PAGE - 1) / PAGE : 1;
        snprintf(pagetxt, sizeof(pagetxt), "%d cell%s  -  page %d/%d  (rows %d-%d)", nfilt, nfilt == 1 ? "" : "s", vpage + 1, pages, nfilt ? from + 1 : 0, to);
        pshow = 1;
    } else if (ta) {
        char line[1400];
        if (ask_prompt[0]) { snprintf(line, sizeof(line), "you: %s", ask_prompt); sanitize(line); wrap_rows(bf, line, &n); }
        if (ask_ans[0]) { snprintf(line, sizeof(line), "tomom: %s", ask_ans); sanitize(line); wrap_rows(bf, line, &n); }
    }
    fclose(bf);
    fprintf(f, "n_list=%d\n", n);
    fputs(body, f); free(body);
    fprintf(f, "page_txt=%s\n", pshow ? pagetxt : "");
    fprintf(f, "show_page=%s\n", pshow ? "1" : "");
    fprintf(f, "ask_state=%s\n", ask_state);
    fprintf(f, "ask_hint=%s\n", ta ? "Type a prompt and press Enter. Runs chatbot_moe_v1 on the scratch copy (note: it reads curriculum/*, not these two weight files)." : "");
    fclose(f);
    rename(tmp, dst);
}

/* ---------------------------------------------------------------- */

static void do_cmd(const char *cmd) {
    if (!strncmp(cmd, "TAB:", 4)) { snprintf(tab, sizeof(tab), "%s", cmd + 4); sanitize(tab);
        if (strcmp(tab, "meta") && strcmp(tab, "vocab") && strcmp(tab, "ask") && strcmp(tab, "subject")) snprintf(tab, sizeof(tab), "meta");
        if (!strcmp(tab, "subject") && in_scratch() && loaded) { ensure_curriculum(); load_subjects(); } }
    else if (!strncmp(cmd, "SEL:", 4)) { int i = atoi(cmd + 4); if (!strcmp(tab, "meta")) sel_m = i; else if (!strcmp(tab, "vocab")) sel_v = i;
        else if (!strcmp(tab, "subject") && i >= 0) {
            if (sub_level == 0 && i < nsubj) { sel_subj = i; sub_level = 1; }
            else if (sub_level == 1 && i < 3) { sel_file = i; sel_c = -1; flt[0] = 0; vpage = 0; load_cells(); sub_level = 2; }
            else if (sub_level == 2) sel_c = i; } }
    else if (!strcmp(cmd, "BACK")) { if (sub_level > 0) { sub_level--; flt[0] = 0; vpage = 0; sel_c = -1; } }
    else if (!strcmp(cmd, "UP")) { const char *fl; char k[128]; double v, o; int h; if (cur_sel(&fl, k, sizeof(k), &v, &o, &h) == 0) do_change(v + cur_step()); else snprintf(status, sizeof(status), "select a row first"); }
    else if (!strcmp(cmd, "DOWN")) { const char *fl; char k[128]; double v, o; int h; if (cur_sel(&fl, k, sizeof(k), &v, &o, &h) == 0) do_change(v - cur_step()); else snprintf(status, sizeof(status), "select a row first"); }
    else if (!strcmp(cmd, "RESET")) { const char *fl; char k[128]; double v, o; int h;
        if (cur_sel(&fl, k, sizeof(k), &v, &o, &h) != 0) snprintf(status, sizeof(status), "select a row first");
        else if (!h) snprintf(status, sizeof(status), "no original snapshot for this row");
        else if (fabs(o - v) < (!strcmp(tab, "subject") ? 1e-12 : 5e-7)) snprintf(status, sizeof(status), "already original");
        else { if (!strcmp(tab, "subject")) do_change_s(orig_tok); else do_change(o); } }
    else if (!strcmp(cmd, "UNDO")) do_undo();
    else if (!strcmp(cmd, "INIT")) do_init();
    else if (!strcmp(cmd, "RELOAD")) { load_target(); load_undo_stack(); load_subjects(); load_cells(); }
    else if (!strncmp(cmd, "FILTER:", 7)) { snprintf(flt, sizeof(flt), "%s", cmd + 7); vpage = 0; sel_v = -1; sel_c = -1; }
    else if (!strcmp(cmd, "FLTCLR")) { flt[0] = '\0'; vpage = 0; }
    else if (!strcmp(cmd, "PAGE+")) vpage++;
    else if (!strcmp(cmd, "PAGE-")) { if (vpage > 0) vpage--; }
    else if (!strncmp(cmd, "ASK:", 4)) do_ask(cmd + 4);
    write_ui();
}
static void clear_action_file(void) {
    char p[PL]; snprintf(p, sizeof(p), "%s/tomom_action.txt", pkg);
    FILE *f = fopen(p, "w"); if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); }
}
static void poll_action(int *last_seq) {
    char p[PL]; snprintf(p, sizeof(p), "%s/tomom_action.txt", pkg);
    FILE *f = fopen(p, "r"); if (!f) return;
    char buf[4096]; size_t nr = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[nr] = '\0';
    int seq = 0; char cmd[1024] = "";
    for (char *ls = buf; *ls; ) {
        char *le = strchr(ls, '\n'); size_t ll = le ? (size_t)(le - ls) : strlen(ls);
        if (!strncmp(ls, "seq=", 4)) seq = atoi(ls + 4);
        else if (!strncmp(ls, "cmd=", 4)) { size_t cl = ll - 4; if (cl >= sizeof(cmd)) cl = sizeof(cmd) - 1; memcpy(cmd, ls + 4, cl); cmd[cl] = '\0'; }
        if (!le) break; ls = le + 1;
    }
    if (seq > *last_seq && cmd[0]) { *last_seq = seq; do_cmd(cmd); }
}
static void on_term(int sig) { (void)sig; if (ask_pid > 0) kill(ask_pid, SIGKILL); _exit(0); }

static void find_live_tomom(void) {
    char base[PL]; snprintf(base, sizeof(base), "%s/#.Z.HUMAN_LLM", house);
    DIR *d = opendir(base); tomom_live[0] = '\0'; if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) if (!strncmp(e->d_name, "3.stage.llm.tomom", 17)) { snprintf(tomom_live, sizeof(tomom_live), "%s/%s", base, e->d_name); break; }
    closedir(d);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "Usage: %s <house_root> <package_dir> [arg3]\n", argv[0]); return 1; }
    snprintf(house, sizeof(house), "%s", argv[1]); snprintf(pkg, sizeof(pkg), "%s", argv[2]);
    snprintf(cfg_path, sizeof(cfg_path), "%s/tomom_hq_config.pdl", pkg);
    snprintf(work_root, sizeof(work_root), "%s/state/work", pkg);
    snprintf(orig_dir, sizeof(orig_dir), "%s/state/work/orig", pkg);
    snprintf(edits_path, sizeof(edits_path), "%s/state/edits.txt", pkg);
    snprintf(ask_log, sizeof(ask_log), "%s/state/ask_log.txt", pkg);
    mkdir_p(work_root);
    signal(SIGTERM, on_term); signal(SIGINT, on_term); signal(SIGHUP, on_term);
    find_live_tomom(); load_config();
    clear_action_file(); load_undo_stack(); load_target(); load_subjects();
    snprintf(ask_state, sizeof(ask_state), "idle");
    write_ui();
    int last_seq = 0;
    for (;;) {
        usleep(50000);
        poll_action(&last_seq);
        if (ask_pid > 0) { ask_poll(); if (ask_pid <= 0) write_ui(); }
    }
}
