#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* csv_lab_manager: <module> for csv-lab-hq.xhtpm - a LIVE VIEW of the CSV / Chinese
 * translation pipeline plus owner accept/reject actions.  Same shape as knowledge_manager:
 * this ONE process owns state, polls <pkg>/csv_lab_action.txt (seq=<n>\ncmd=<VERB[:arg]>)
 * and publishes <pkg>/csv_lab_ui.txt (key=value lines for vars=).  No renderer C.
 *
 *   csv_lab_manager <house_root> <pkg>            daemon (100 ms poll loop)
 *   csv_lab_manager --once <house_root> <pkg>     read feed + state, apply at most one pending
 *                                                 action, publish, print "ui=written" or
 *                                                 "ui=unchanged", exit 0 (harness mode)
 *
 * FEED (read only, '|' separated, written by the pipeline; path from csv_lab_config.pdl):
 *   HEAD|run|bank|stage|done|total        stage: extract|batch|translate|lint|score|merge|done|error
 *   QUOTA|groq_req|used|limit   QUOTA|groq_tok_min|used|limit   QUOTA|mac_calls|n|0
 *   JOB|bank|id|en|zh|pinyin|score(1-5 or -)|status   status: new|translated|lint_fail|low_score|ok|accepted|rejected
 *   LOG|HH:MM:SS|short line   (the last LOGSHOW lines are shown, newest at the bottom)
 * A row that does not parse (wrong field count, unknown status, bad number, unknown type) is
 * skipped and counted, never fatal.  A later JOB row with the same bank+id replaces the earlier one.
 *
 * WRITES: only (a) <review>: append-only, EXACTLY  REVIEW|<bank>|<id>|accept|reject  (4 fields,
 * no timestamp); (b) <cmd>: append-only  CMD|<text>  (one line per typed command); (c) its own
 * scratch under <pkg> (csv_lab_ui.txt, state/csv_lab_state.txt, the daemon's action-file reset).
 * The feed is never opened for writing.  Change detection = content hash + size of the feed and
 * the SIZE of the review / cmd files, never st_mtime.  The ui file is rewritten only when its
 * text differs from what is already on disk (idempotent), and in the daemon feed-driven
 * republishes are at most every REPUB_MS. */

#define PL 4096
#define MAXJ 2000
#define MAXREV 4096
#define MAXB 24
#define LOGKEEP 40
#define LOGSHOW 6
#define REPUB_MS 500
#define FEEDMAX (4 * 1024 * 1024)

typedef struct { char bank[40], id[40], en[200], zh[200], py[200], score[4], status[16]; } Job;
typedef struct { char bank[40], id[40]; int verdict; } Rev;   /* verdict 1 accept, 2 reject */

static char house[PL], pkg[PL], cfg_path[PL], feed_path[PL], rev_path[PL], cmd_path[PL], state_path[PL], ui_path[PL], act_path[PL];
static Job jobs[MAXJ]; static int njob = 0, n_over = 0, n_bad = 0;
static Rev revs[MAXREV]; static int nrev = 0;
static char banks[MAXB][40]; static int nbank = 0;
static int feed_ok = 0;                       /* feed file exists and is readable */
static int have_head = 0; static char h_run[48], h_bank[40], h_stage[16]; static long h_done = 0, h_total = 0;
static int have_q[3]; static long q_used[3], q_lim[3];
static char logl[LOGKEEP][160]; static int nlog = 0;
static char flt_bank[40] = "ALL", flt_status[12] = "ALL";
static char sel_bank[40] = "", sel_id[40] = "";
static char cmd_last[200] = "";
static int last_seq = 0;
static char msg[300] = "";

static const char *STAT[7] = { "new", "translated", "lint_fail", "low_score", "ok", "accepted", "rejected" };
static const char *QNAME[3] = { "groq_req", "groq_tok_min", "mac_calls" };

static void cp(char *d, size_t n, const char *s) { size_t l = strlen(s); if (l >= n) l = n - 1; memcpy(d, s, l); d[l] = '\0'; }
/* the ui file is key=value lines: a value must be one line.  Bytes >= 0x80 (UTF-8, CJK) pass through untouched. */
static void sanitize(char *s) { for (char *p = s; *p; p++) if (*p == '\n' || *p == '\r' || *p == '\t' || *p == '|') *p = ' '; }
static void mkdir_p(const char *path) {
    char t[PL]; cp(t, sizeof(t), path);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = '\0'; mkdir(t, 0755); *p = '/'; }
    mkdir(t, 0755);
}
static char *slurp(const char *path, size_t max, long *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) n = 0;
    if ((size_t)n > max) n = (long)max;
    char *b = malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)n, f); b[r] = '\0'; fclose(f); if (len) *len = (long)r; return b;
}
static long fsize(const char *p) { struct stat st; return stat(p, &st) == 0 ? (long)st.st_size : 0; }
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    for (char *e = s + strlen(s); e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'); ) *--e = '\0';
    return s;
}
/* split on '|' in place; returns the field count (stops at max, the rest stays in the last field) */
static int split(char *line, char **f, int max) {
    int nf = 0; char *s = line;
    while (nf < max) { f[nf++] = s; char *c = strchr(s, '|'); if (!c) break; *c = '\0'; s = c + 1; }
    for (int i = 0; i < nf; i++) f[i] = trim(f[i]);
    return nf;
}
static int all_digits(const char *s) { if (!*s) return 0; for (; *s; s++) if (*s < '0' || *s > '9') return 0; return 1; }
static unsigned long djb(const char *s, long n) { unsigned long h = 5381; for (long i = 0; i < n; i++) h = h * 33 + (unsigned char)s[i]; return h; }
static int stat_idx(const char *s) { for (int i = 0; i < 7; i++) if (!strcmp(s, STAT[i])) return i; return -1; }

static void load_config(void) {
    snprintf(feed_path, sizeof(feed_path), "%s/&.widgits/concept-bank/state/csv_lab_ui.txt", house);
    snprintf(rev_path, sizeof(rev_path), "%s/&.widgits/concept-bank/state/csv_lab_review.txt", house);
    snprintf(cmd_path, sizeof(cmd_path), "%s/&.widgits/concept-bank/state/csv_lab_cmd.txt", house);
    char *b = slurp(cfg_path, 65536, NULL); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (ln[0] == '#') continue;
        char *c1 = strchr(ln, '|'); if (!c1) continue; char *c2 = strchr(c1 + 1, '|'); if (!c2) continue;
        *c2 = '\0'; char *key = trim(c1 + 1), *v = trim(c2 + 1); if (!*v) continue;
        char *dst = !strcmp(key, "feed") ? feed_path : !strcmp(key, "review") ? rev_path : !strcmp(key, "cmd") ? cmd_path : NULL;
        if (!dst) continue;
        if (v[0] == '/') cp(dst, PL, v); else snprintf(dst, PL, "%s/%s", house, v);
    }
    free(b);
}

/* ------------------------------------------------------------------ feed */
static void add_bank(const char *b) {
    for (int i = 0; i < nbank; i++) if (!strcmp(banks[i], b)) return;
    if (nbank < MAXB) cp(banks[nbank++], sizeof(banks[0]), b);
}
static void load_feed(void) {
    njob = n_over = n_bad = nbank = nlog = have_head = 0; memset(have_q, 0, sizeof(have_q)); feed_ok = 0;
    char *b = slurp(feed_path, FEEDMAX, NULL); if (!b) return;
    feed_ok = 1;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        ln = trim(ln); if (!*ln || ln[0] == '#') continue;
        char orig[1024]; cp(orig, sizeof(orig), ln);
        char *f[10]; int nf = split(ln, f, 10);
        if (!strcmp(f[0], "HEAD")) {
            if (nf != 6 || !all_digits(f[4]) || !all_digits(f[5]) || !*f[1] || !*f[3]) { n_bad++; continue; }
            have_head = 1; cp(h_run, sizeof(h_run), f[1]); cp(h_bank, sizeof(h_bank), f[2]); cp(h_stage, sizeof(h_stage), f[3]);
            h_done = atol(f[4]); h_total = atol(f[5]);
            sanitize(h_run); sanitize(h_bank); sanitize(h_stage);
        } else if (!strcmp(f[0], "QUOTA")) {
            int k = -1; if (nf == 4) for (int i = 0; i < 3; i++) if (!strcmp(f[1], QNAME[i])) k = i;
            if (nf != 4 || !all_digits(f[2]) || !all_digits(f[3])) { n_bad++; continue; }
            if (k < 0) continue;                      /* a well-formed quota we do not draw: ignored, not an error */
            have_q[k] = 1; q_used[k] = atol(f[2]); q_lim[k] = atol(f[3]);
        } else if (!strcmp(f[0], "JOB")) {
            int sc_ok = nf == 8 && (!strcmp(f[6], "-") || (strlen(f[6]) == 1 && f[6][0] >= '1' && f[6][0] <= '5'));
            if (nf != 8 || !*f[1] || !*f[2] || !sc_ok || stat_idx(f[7]) < 0) { n_bad++; continue; }
            int at = -1;
            for (int i = 0; i < njob; i++) if (!strcmp(jobs[i].bank, f[1]) && !strcmp(jobs[i].id, f[2])) { at = i; break; }
            if (at < 0) { if (njob >= MAXJ) { n_over++; continue; } at = njob++; }
            Job *j = &jobs[at];
            cp(j->bank, sizeof(j->bank), f[1]); cp(j->id, sizeof(j->id), f[2]); cp(j->en, sizeof(j->en), f[3]); cp(j->zh, sizeof(j->zh), f[4]);
            cp(j->py, sizeof(j->py), f[5]); cp(j->score, sizeof(j->score), f[6]); cp(j->status, sizeof(j->status), f[7]);
            sanitize(j->bank); sanitize(j->id); sanitize(j->en); sanitize(j->zh); sanitize(j->py);
            add_bank(j->bank);
        } else if (!strcmp(f[0], "LOG")) {
            if (nf < 3) { n_bad++; continue; }
            /* the line itself may contain '|': take everything after the time field from the untouched copy */
            const char *c1 = strchr(orig, '|'), *c2 = c1 ? strchr(c1 + 1, '|') : NULL;
            char *rest = trim(c2 ? (char *)c2 + 1 : f[2]);
            if (nlog == LOGKEEP) { memmove(logl[0], logl[1], sizeof(logl[0]) * (LOGKEEP - 1)); nlog--; }
            char t[160]; snprintf(t, sizeof(t), "%.10s  %.140s", f[1], rest); sanitize(t); cp(logl[nlog++], sizeof(logl[0]), t);
        } else n_bad++;
    }
    free(b);
}
/* review rows: REVIEW|bank|id|accept|reject   (exactly 4 fields; last row per bank+id wins) */
static void load_reviews(void) {
    nrev = 0; char *b = slurp(rev_path, FEEDMAX, NULL); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        char *f[8]; int nf = split(ln, f, 8);
        if (nf != 4 || strcmp(f[0], "REVIEW") != 0) continue;
        int v = !strcmp(f[3], "accept") ? 1 : !strcmp(f[3], "reject") ? 2 : 0; if (!v || !*f[1] || !*f[2]) continue;
        int at = -1; for (int i = 0; i < nrev; i++) if (!strcmp(revs[i].bank, f[1]) && !strcmp(revs[i].id, f[2])) { at = i; break; }
        if (at < 0) { if (nrev >= MAXREV) continue; at = nrev++; }
        cp(revs[at].bank, sizeof(revs[at].bank), f[1]); cp(revs[at].id, sizeof(revs[at].id), f[2]); revs[at].verdict = v;
    }
    free(b);
}
static int review_of(const Job *j) {
    for (int i = 0; i < nrev; i++) if (!strcmp(revs[i].bank, j->bank) && !strcmp(revs[i].id, j->id)) return revs[i].verdict;
    return 0;
}
/* effective status = the owner's review outranks the pipeline's own status */
static const char *eff(const Job *j) { int r = review_of(j); return r == 1 ? "accepted" : r == 2 ? "rejected" : j->status; }
static int needs_review(const Job *j) {
    if (review_of(j)) return 0;
    return !strcmp(j->status, "lint_fail") || !strcmp(j->status, "low_score") || !strcmp(j->status, "translated");
}
static int passes(const Job *j) {
    if (strcmp(flt_bank, "ALL") && strcmp(flt_bank, j->bank)) return 0;
    if (!strcmp(flt_status, "REVIEW")) return needs_review(j);
    if (!strcmp(flt_status, "ACCEPTED")) return !strcmp(eff(j), "accepted");
    if (!strcmp(flt_status, "REJECTED")) return !strcmp(eff(j), "rejected");
    return 1;
}
static const char *row_cls(const char *st) {
    if (!strcmp(st, "lint_fail")) return "bad-row";
    if (!strcmp(st, "low_score")) return "warn-row";
    if (!strcmp(st, "translated")) return "tr-row";
    if (!strcmp(st, "ok") || !strcmp(st, "accepted")) return "ok-row";
    if (!strcmp(st, "rejected")) return "rej-row";
    return "new-row";
}

/* -------------------------------------------------------------- state file */
static void save_state(void) {
    FILE *f = fopen(state_path, "w"); if (!f) return;
    char c[200]; cp(c, sizeof(c), cmd_last); sanitize(c);
    fprintf(f, "bank=%s\nstatus=%s\nsel_bank=%s\nsel_id=%s\nlast_seq=%d\ncmd_last=%s\n", flt_bank, flt_status, sel_bank, sel_id, last_seq, c);
    fclose(f);
}
static void load_state(void) {
    char *b = slurp(state_path, 65536, NULL); if (!b) return;
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (!strncmp(ln, "bank=", 5)) cp(flt_bank, sizeof(flt_bank), ln + 5);
        else if (!strncmp(ln, "status=", 7)) cp(flt_status, sizeof(flt_status), ln + 7);
        else if (!strncmp(ln, "sel_bank=", 9)) cp(sel_bank, sizeof(sel_bank), ln + 9);
        else if (!strncmp(ln, "sel_id=", 7)) cp(sel_id, sizeof(sel_id), ln + 7);
        else if (!strncmp(ln, "last_seq=", 9)) last_seq = atoi(ln + 9);
        else if (!strncmp(ln, "cmd_last=", 9)) cp(cmd_last, sizeof(cmd_last), ln + 9);
    }
    free(b);
}

/* ------------------------------------------------------------------ ui text */
typedef struct { char *buf; size_t sz; FILE *f; } Out;
static void bar(long used, long lim, int w, char *o) {
    long n = lim > 0 ? (used * w + lim / 2) / lim : 0; if (n > w) n = w; if (n < 0) n = 0;
    o[0] = '['; for (int i = 0; i < w; i++) o[1 + i] = i < n ? '#' : '-'; o[w + 1] = ']'; o[w + 2] = '\0';
}
static int pct(long used, long lim) { return lim > 0 ? (int)(used * 100 / lim) : 0; }
static Job *sel_job(void) {
    if (!sel_id[0]) return NULL;
    for (int i = 0; i < njob; i++) if (!strcmp(jobs[i].bank, sel_bank) && !strcmp(jobs[i].id, sel_id)) return &jobs[i];
    return NULL;
}
static void kv(FILE *f, const char *k, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void kv(FILE *f, const char *k, const char *fmt, ...) {
    char t[1100]; va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof(t), fmt, ap); va_end(ap); sanitize(t);
    fprintf(f, "%s=%s\n", k, t);
}
static void build_ui(Out *o) {
    FILE *f = o->f;
    if (flt_bank[0] && strcmp(flt_bank, "ALL")) { int ok = 0; for (int i = 0; i < nbank; i++) if (!strcmp(banks[i], flt_bank)) ok = 1; if (!ok && feed_ok) cp(flt_bank, sizeof(flt_bank), "ALL"); }
    int nshown = 0, nrv = 0; for (int i = 0; i < njob; i++) { if (passes(&jobs[i])) nshown++; if (needs_review(&jobs[i])) nrv++; }
    char b2[64];
    if (!feed_ok) {
        kv(f, "head", "CSV Lab  -  waiting for pipeline");
        kv(f, "st_run", "waiting for pipeline");
        kv(f, "st_prog", "no feed file yet");
        kv(f, "status", "waiting for pipeline - feed not found (csv_lab_config.pdl: feed)");
    } else {
        kv(f, "head", "CSV Lab  -  %s", have_head ? h_run : "no run header yet");
        if (have_head) {
            kv(f, "st_run", "run %s    bank %s    stage %s", h_run, h_bank, h_stage);
            bar(h_done, h_total, 24, b2);
            kv(f, "st_prog", "%s %ld/%ld  %d%%", b2, h_done, h_total, pct(h_done, h_total));
        } else { kv(f, "st_run", "feed present, no HEAD row yet"); kv(f, "st_prog", " "); }
        if (n_bad) kv(f, "status", "%d jobs, %d shown, %d need review  /  bank %s, filter %s  /  skipped bad rows: %d", njob, nshown, nrv, flt_bank, flt_status, n_bad);
        else kv(f, "status", "%d jobs, %d shown, %d need review  /  bank %s, filter %s", njob, nshown, nrv, flt_bank, flt_status);
    }
    kv(f, "msg", "%s", msg);
    if (cmd_last[0]) kv(f, "cmdstat", "last command: %.150s   [queued]", cmd_last); else kv(f, "cmdstat", "type a command below (recorded to csv_lab_cmd.txt for the pipeline)");
    kv(f, "stage", "%s", have_head ? h_stage : "");
    kv(f, "feed_state", "%s", feed_ok ? "ok" : "waiting");
    kv(f, "n_jobs", "%d", njob); kv(f, "n_shown", "%d", nshown); kv(f, "n_bad", "%d", n_bad);
    kv(f, "bank", "%s", flt_bank); kv(f, "sfilter", "%s", flt_status);
    /* quota bars */
    const char *ql[3] = { "Groq requests ", "Groq tokens/min", "Mac calls      " };
    for (int k = 0; k < 3; k++) {
        char key[16], ck[16]; snprintf(key, sizeof(key), "q%d", k + 1); snprintf(ck, sizeof(ck), "q%d_cls", k + 1);
        if (!have_q[k]) { kv(f, key, "%s  (no data)", ql[k]); kv(f, ck, "quiet"); continue; }
        if (q_lim[k] > 0) { bar(q_used[k], q_lim[k], 24, b2); kv(f, key, "%s %s %ld/%ld  %d%%", ql[k], b2, q_used[k], q_lim[k], pct(q_used[k], q_lim[k])); kv(f, ck, "%s", pct(q_used[k], q_lim[k]) >= 90 ? "hot" : pct(q_used[k], q_lim[k]) >= 70 ? "warm" : "cool"); }
        else { kv(f, key, "%s %ld calls", ql[k], q_used[k]); kv(f, ck, "cool"); }
    }
    /* bank tabs: ALL + distinct banks, status tabs */
    kv(f, "n_banks", "%d", nbank + 1);
    kv(f, "b_0_label", "ALL banks (%d)", njob); kv(f, "b_0_cls", "%s", !strcmp(flt_bank, "ALL") ? "tab-active" : ""); kv(f, "b_0_act", "BANK 0");
    for (int i = 0; i < nbank; i++) {
        int c = 0; for (int j = 0; j < njob; j++) if (!strcmp(jobs[j].bank, banks[i])) c++;
        char k[24]; snprintf(k, sizeof(k), "b_%d_label", i + 1); kv(f, k, "%.30s (%d)", banks[i], c);
        snprintf(k, sizeof(k), "b_%d_cls", i + 1); kv(f, k, "%s", !strcmp(flt_bank, banks[i]) ? "tab-active" : "");
        snprintf(k, sizeof(k), "b_%d_act", i + 1); kv(f, k, "BANK %d", i + 1);
    }
    const char *sn[4] = { "ALL", "REVIEW", "ACCEPTED", "REJECTED" };
    for (int i = 0; i < 4; i++) { char k[16]; snprintf(k, sizeof(k), "cls_s%d", i); kv(f, k, "%s", !strcmp(flt_status, sn[i]) ? "tab-active" : ""); }
    kv(f, "s1_label", "needs review (%d)", nrv);
    /* log pane: last LOGSHOW lines, newest at the BOTTOM */
    int ls = nlog > LOGSHOW ? nlog - LOGSHOW : 0, nl = nlog - ls;
    kv(f, "n_log", "%d", nl > 0 ? nl : 1);
    if (nl == 0) kv(f, "g_0_text", "%s", feed_ok ? "(no log lines yet)" : "(no feed)");
    for (int i = 0; i < nl; i++) { char k[16]; snprintf(k, sizeof(k), "g_%d_text", i); kv(f, k, "%s", logl[ls + i]); }
    /* job list: every passing job is one row; the selected one is followed by its card + buttons */
    char *lb = NULL; size_t ls2 = 0; FILE *lf = open_memstream(&lb, &ls2); int rn = 0; int fi = 0;
    Job *sj = sel_job();
    if (!feed_ok) { fprintf(lf, "l_0_text=Waiting for the pipeline to write its feed file.\nl_0_cls=quiet-row\nl_0_act=NOOP\n"); rn = 1; }
    for (int i = 0; i < njob && feed_ok; i++) {
        Job *j = &jobs[i]; if (!passes(j)) continue; fi++;
        const char *st = eff(j); int open = (sj == j);
        char t[900]; snprintf(t, sizeof(t), "%s%02d  %.60s   %.60s   %.40s   %s   %s", open ? "v " : "> ", fi, j->en, j->zh, j->py, !strcmp(j->score, "-") ? "-" : j->score, st); sanitize(t);
        fprintf(lf, "l_%d_text=%s\nl_%d_cls=%s%s\nl_%d_act=SEL %d\n", rn, t, rn, row_cls(st), open ? " sel-row" : "", rn, fi); rn++;
        if (open) {
            const char *lines[4]; char c[4][400];
            snprintf(c[0], sizeof(c[0]), "      English   %.150s", j->en); snprintf(c[1], sizeof(c[1]), "      Chinese   %.150s     pinyin %.100s", j->zh, j->py);
            snprintf(c[2], sizeof(c[2]), "      bank %s   id %s   score %s   pipeline status %s   now %s", j->bank, j->id, j->score, j->status, st);
            for (int q = 0; q < 3; q++) { sanitize(c[q]); lines[q] = c[q]; fprintf(lf, "l_%d_text=%s\nl_%d_cls=card-row\nl_%d_act=NOOP\n", rn, lines[q], rn, rn); rn++; }
            fprintf(lf, "l_%d_text=      [ ACCEPT ]  (append one REVIEW row)\nl_%d_cls=card-btn\nl_%d_act=ACCEPT\n", rn, rn, rn); rn++;
            fprintf(lf, "l_%d_text=      [ REJECT ]  (append one REVIEW row)\nl_%d_cls=card-btn rej-btn\nl_%d_act=REJECT\n", rn, rn, rn); rn++;
        }
    }
    if (feed_ok && fi == 0) { fprintf(lf, "l_0_text=(no jobs match bank %s / filter %s)\nl_0_cls=quiet-row\nl_0_act=NOOP\n", flt_bank, flt_status); rn = 1; }
    fclose(lf);
    fprintf(f, "n_list=%d\n", rn); fputs(lb, f); free(lb);
}

/* publish only when the text differs from the file already on disk */
static int write_ui(void) {
    Out o = { 0 }; char *ub = NULL; size_t us = 0; o.f = open_memstream(&ub, &us);
    build_ui(&o); fclose(o.f);
    long dl = 0; char *old = slurp(ui_path, FEEDMAX, &dl);
    int same = old && (size_t)dl == us && memcmp(old, ub, us) == 0; free(old);
    if (same) { free(ub); return 0; }
    char tmp[PL]; snprintf(tmp, sizeof(tmp), "%s.tmp", ui_path);
    FILE *f = fopen(tmp, "w"); if (!f) { free(ub); return 0; }
    fwrite(ub, 1, us, f); fclose(f); rename(tmp, ui_path); free(ub); return 1;
}

/* ---------------------------------------------------------------- owner actions */
static void review_append(const char *verdict) {
    Job *j = sel_job();
    if (!j) { snprintf(msg, sizeof(msg), "refused: select a job first"); return; }
    char d[PL]; cp(d, sizeof(d), rev_path); char *sl = strrchr(d, '/'); if (sl) { *sl = '\0'; mkdir_p(d); }
    FILE *f = fopen(rev_path, "a"); if (!f) { snprintf(msg, sizeof(msg), "refused: cannot append review file"); return; }
    fprintf(f, "REVIEW|%s|%s|%s\n", j->bank, j->id, verdict); fclose(f);
    load_reviews();
    snprintf(msg, sizeof(msg), "%s recorded for %s/%s", verdict, j->bank, j->id);
}
static void cmd_append(const char *text) {
    char t[400]; cp(t, sizeof(t), text); for (char *p = t; *p; p++) if (*p == '|' || *p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
    char *s = trim(t); if (!*s) return;
    char d[PL]; cp(d, sizeof(d), cmd_path); char *sl = strrchr(d, '/'); if (sl) { *sl = '\0'; mkdir_p(d); }
    FILE *f = fopen(cmd_path, "a"); if (!f) { snprintf(msg, sizeof(msg), "refused: cannot append command file"); return; }
    fprintf(f, "CMD|%s\n", s); fclose(f);
    cp(cmd_last, sizeof(cmd_last), s);
}
static void do_cmd(const char *cmd) {
    msg[0] = '\0';
    if (!strncmp(cmd, "CMD:", 4)) { cmd_append(cmd + 4); return; }
    if (!strcmp(cmd, "NOOP")) return;
    if (!strncmp(cmd, "BANK:", 5)) {
        int n = atoi(cmd + 5);
        if (n == 0) cp(flt_bank, sizeof(flt_bank), "ALL");
        else if (n >= 1 && n <= nbank) cp(flt_bank, sizeof(flt_bank), banks[n - 1]);
        else snprintf(msg, sizeof(msg), "ignored: no bank tab %.20s", cmd + 5);
        sel_id[0] = '\0';
    } else if (!strncmp(cmd, "FILTER:", 7)) {
        static const char *ok[4] = { "ALL", "REVIEW", "ACCEPTED", "REJECTED" }; int hit = 0;
        for (int i = 0; i < 4; i++) if (!strcmp(cmd + 7, ok[i])) hit = 1;
        if (hit) { cp(flt_status, sizeof(flt_status), cmd + 7); sel_id[0] = '\0'; } else snprintf(msg, sizeof(msg), "ignored: unknown filter '%.40s'", cmd + 7);
    } else if (!strncmp(cmd, "SEL:", 4)) {
        int n = atoi(cmd + 4), k = 0, hit = -1;
        for (int i = 0; i < njob; i++) if (passes(&jobs[i]) && ++k == n) { hit = i; break; }
        if (hit < 0) snprintf(msg, sizeof(msg), "ignored: no job row %.20s", cmd + 4);
        else if (!strcmp(sel_bank, jobs[hit].bank) && !strcmp(sel_id, jobs[hit].id)) sel_id[0] = '\0';   /* click the open row again = collapse */
        else { cp(sel_bank, sizeof(sel_bank), jobs[hit].bank); cp(sel_id, sizeof(sel_id), jobs[hit].id); }
    } else if (!strcmp(cmd, "RELOAD")) { load_feed(); load_reviews(); snprintf(msg, sizeof(msg), "reloaded"); }
    else if (!strcmp(cmd, "ACCEPT")) review_append("accept");
    else if (!strcmp(cmd, "REJECT")) review_append("reject");
    else snprintf(msg, sizeof(msg), "ignored: unknown verb '%.60s'", cmd);
}
static int poll_action(void) {
    char *buf = slurp(act_path, 8192, NULL); if (!buf) return 0;
    int seq = 0; char cmd[1024] = "";
    for (char *ls = buf; *ls; ) {
        char *le = strchr(ls, '\n'); size_t ll = le ? (size_t)(le - ls) : strlen(ls);
        if (!strncmp(ls, "seq=", 4)) seq = atoi(ls + 4);
        else if (!strncmp(ls, "cmd=", 4)) { size_t cl = ll - 4; if (cl >= sizeof(cmd)) cl = sizeof(cmd) - 1; memcpy(cmd, ls + 4, cl); cmd[cl] = '\0'; }
        if (!le) break;
        ls = le + 1;
    }
    free(buf);
    char *c = trim(cmd);
    if (seq > last_seq && c[0]) { last_seq = seq; do_cmd(c); return 1; }
    return 0;
}
static unsigned long feed_sig(void) {
    long n = 0; char *b = slurp(feed_path, FEEDMAX, &n);
    unsigned long h = b ? djb(b, n) ^ (unsigned long)n : 1; free(b);
    return h * 7919 + (unsigned long)fsize(rev_path) * 31 + (unsigned long)fsize(cmd_path);
}
static void on_term(int sig) { (void)sig; _exit(0); }
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }

int main(int argc, char **argv) {
    int once = 0, a = 1;
    if (argc > 1 && !strcmp(argv[1], "--once")) { once = 1; a = 2; }
    if (argc < a + 2) { fprintf(stderr, "Usage: %s [--once] <house_root> <package_dir>\n", argv[0]); return 1; }
    cp(house, sizeof(house), argv[a]); cp(pkg, sizeof(pkg), argv[a + 1]);
    snprintf(cfg_path, sizeof(cfg_path), "%s/csv_lab_config.pdl", pkg);
    snprintf(state_path, sizeof(state_path), "%s/state/csv_lab_state.txt", pkg);
    snprintf(ui_path, sizeof(ui_path), "%s/csv_lab_ui.txt", pkg);
    snprintf(act_path, sizeof(act_path), "%s/csv_lab_action.txt", pkg);
    { char sd[PL]; snprintf(sd, sizeof(sd), "%s/state", pkg); mkdir_p(sd); }
    load_config(); load_state(); load_feed(); load_reviews();
    if (once) { poll_action(); save_state(); printf("ui=%s\n", write_ui() ? "written" : "unchanged"); return 0; }
    signal(SIGTERM, on_term); signal(SIGINT, on_term); signal(SIGHUP, on_term);
    last_seq = 0;
    { FILE *f = fopen(act_path, "w"); if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); } }
    unsigned long sig_prev = feed_sig(); long last_pub = now_ms(), last_chk = 0; int pending = 0;
    save_state(); write_ui();
    for (;;) {
        usleep(50000);
        int ch = poll_action();                         /* clicks publish at once */
        long t = now_ms();
        if (t - last_chk >= 100) {                      /* the feed is checked at 10 Hz, published at most every REPUB_MS */
            last_chk = t; unsigned long s = feed_sig();
            if (s != sig_prev) { sig_prev = s; load_feed(); load_reviews(); pending = 1; }
        }
        if (pending && t - last_pub >= REPUB_MS) { pending = 0; ch = 1; }
        if (ch) { last_pub = t; save_state(); write_ui(); }
    }
}
