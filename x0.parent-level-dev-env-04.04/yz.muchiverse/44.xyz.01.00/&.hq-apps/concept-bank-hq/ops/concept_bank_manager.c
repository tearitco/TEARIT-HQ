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

/* concept_bank_manager: <module> for concept-bank-hq.xhtpm - a READ-ONLY
 * VIEW of &.widgits/concept-bank/data (masters/, spokes/, candidates/).
 * argv: <house> <pkg> [a3].  Design:
 * #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/CONCEPT-BANK-HQ-DESIGN.md
 *
 * Same shape as tomom-hq: this ONE process owns state, polls
 * <pkg>/concept_bank_action.txt (seq=<n>\ncmd=<VERB[:arg]>), publishes
 * <pkg>/concept_bank_ui.txt.  The bars are composed here; no renderer C.
 *
 * READ-ONLY: this binary never opens a bank file for writing.  The only
 * child it starts is ops/+x/concept_edit_validate.+x, which per its own
 * header "only validates, no partial state is ever written"; it runs via
 * fork+exec+waitpid with a 10 s watchdog (never popen-to-EOF), its stdout
 * goes to a scratch file under <pkg>/state/.  No promote button: promotion
 * is undesigned (AUTO-PROMOTION-RULE.md is a rule, not running code).
 *
 * The master "mirror" shown on a master page is DERIVED here on every
 * render by scanning the spokes (the spoke slots are the one authoritative
 * copy of a weight); the MIRROR_* lines inside master files are ignored.
 * Unparseable lines are never dropped: they are kept and shown, marked. */

#define PL 4096
#define MAXN 64
#define MAXSL 32
#define MAXBAD 8

typedef struct { int idx; char points[96]; double w; int ok; } Slot;
typedef struct {
    char name[96], file[160], kind[24]; int nslots_decl;
    Slot sl[MAXSL]; int nsl;
    char bad[MAXBAD][160]; int nbad;
} Node;
typedef struct {
    char file[160], id[96], type[64], target[96], slot[96], delta[48], status[48], proposer[64], reason[400];
    int n_edit; int malformed; char bad[200]; int empty;
    char verdict[320];             /* "" = not validated yet */
} Cand;

static char house[PL], pkg[PL], cfg_path[PL], bank[PL], state_dir[PL];
static Node masters[MAXN], spokes[MAXN]; static int nmas = 0, nspk = 0;
static Cand cands[MAXN]; static int ncand = 0;
static char tab[12] = "masters";
static int level = 0, sel = -1;
static char status[300] = "";
static pid_t val_pid = -1; static time_t val_t0 = 0; static int val_idx = -1;

static void sanitize(char *s) { for (char *p = s; *p; p++) if (*p == '|' || *p == '\n' || *p == '\r' || *p == '\t') *p = ' '; }
static void mkdir_p(const char *path) {
    char t[PL]; snprintf(t, sizeof(t), "%s", path);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = '\0'; mkdir(t, 0755); *p = '/'; }
    mkdir(t, 0755);
}
static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)n, f); b[r] = '\0'; fclose(f); return b;
}
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    for (char *e = s + strlen(s); e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'); ) *--e = '\0';
    return s;
}

/* config: "SECTION | bank_dir | <path>" relative = against the house root */
static void load_config(void) {
    snprintf(bank, sizeof(bank), "%s/&.widgits/concept-bank", house);
    char *b = slurp(cfg_path);
    if (!b) {
        FILE *f = fopen(cfg_path, "w");
        if (f) { fprintf(f, "# concept-bank-hq config. bank_dir = the concept-bank dir (has data/ and ops/). Relative = against house root.\n"
                            "SECTION      | bank_dir            | &.widgits/concept-bank\n"); fclose(f); }
        return;
    }
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (ln[0] == '#') continue;
        char *c1 = strchr(ln, '|'); if (!c1) continue; char *c2 = strchr(c1 + 1, '|'); if (!c2) continue;
        *c2 = '\0'; if (strcmp(trim(c1 + 1), "bank_dir") != 0) continue;
        char *v = trim(c2 + 1); if (!*v) continue;
        if (v[0] == '/') snprintf(bank, sizeof(bank), "%s", v); else snprintf(bank, sizeof(bank), "%s/%s", house, v);
    }
    free(b);
}

/* ---------------------------------------------------------------- */
static void parse_node(const char *path, const char *fname, Node *n) {
    memset(n, 0, sizeof(*n));
    snprintf(n->file, sizeof(n->file), "%s", fname);
    snprintf(n->name, sizeof(n->name), "%s", fname);
    char *dot = strrchr(n->name, '.'); if (dot) *dot = '\0';       /* fallback name = file stem */
    char *b = slurp(path); if (!b) { snprintf(n->bad[n->nbad++], 160, "unreadable file"); return; }
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        char *l = trim(ln); if (!*l || *l == '#') continue;
        char raw[300]; snprintf(raw, sizeof(raw), "%s", l);
        if (!strncmp(l, "MIRROR_", 7)) continue;                      /* derived block: ignored on purpose */
        char *f[5]; int nf = 0; char *s = l;
        while (nf < 5) { f[nf++] = s; char *c = strstr(s, " | "); if (!c) break; *c = '\0'; s = c + 3; }
        for (int i = 0; i < nf; i++) f[i] = trim(f[i]);
        if (!strcmp(f[0], "NODE") && nf >= 2) snprintf(n->name, sizeof(n->name), "%s", f[1]);
        else if (!strcmp(f[0], "KIND") && nf >= 2) snprintf(n->kind, sizeof(n->kind), "%s", f[1]);
        else if (!strcmp(f[0], "N_SLOTS") && nf >= 2) n->nslots_decl = atoi(f[1]);
        else if (!strcmp(f[0], "SLOT") && nf >= 4 && !strncmp(f[2], "POINTS_TO=", 10) && !strncmp(f[3], "WEIGHT=", 7) && n->nsl < MAXSL) {
            Slot *sp = &n->sl[n->nsl++]; sp->idx = atoi(f[1]); snprintf(sp->points, sizeof(sp->points), "%s", f[2] + 10);
            sp->w = atof(f[3] + 7); sp->ok = 1;
        } else if (n->nbad < MAXBAD) { sanitize(raw); snprintf(n->bad[n->nbad++], 160, "%s", raw); }
    }
    free(b);
}
static int cmp_s(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static int list_dir(const char *dir, const char *suffix, char names[][160], int max) {
    DIR *d = opendir(dir); if (!d) return 0;
    char *tmp[MAXN]; int n = 0; struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        size_t l = strlen(e->d_name), sl = strlen(suffix);
        if (e->d_name[0] != '.' && l > sl && !strcmp(e->d_name + l - sl, suffix)) tmp[n++] = strdup(e->d_name);
    }
    closedir(d); qsort(tmp, (size_t)n, sizeof(char *), cmp_s);
    for (int i = 0; i < n; i++) { snprintf(names[i], 160, "%s", tmp[i]); free(tmp[i]); }
    return n;
}
static int field(const char *line, const char *key, char *out, size_t osz) {
    char pat[64]; snprintf(pat, sizeof(pat), "| %s=", key);
    const char *p = strstr(line, pat); if (!p) return 0; p += strlen(pat);
    size_t n = 0; int q = (*p == '"'); if (q) p++;
    while (p[n] && (q ? p[n] != '"' : (p[n] != ' ' || p[n + 1] != '|')) && n < osz - 1) n++;
    memcpy(out, p, n); out[n] = '\0'; return 1;
}
static void parse_cand(const char *path, const char *fname, Cand *c) {
    memset(c, 0, sizeof(*c)); snprintf(c->file, sizeof(c->file), "%s", fname);
    char *b = slurp(path); if (!b) { c->malformed = 1; snprintf(c->bad, sizeof(c->bad), "unreadable file"); return; }
    for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n")) {
        char *l = trim(ln); if (!*l || *l == '#') continue;
        if (strncmp(l, "EDIT |", 6) != 0) { if (!c->malformed) { c->malformed = 1; snprintf(c->bad, sizeof(c->bad), "%.190s", l); sanitize(c->bad); } continue; }
        c->n_edit++; if (c->n_edit > 1) continue;                      /* first EDIT line is shown */
        char id[96] = "", ty[64] = "", tg[96] = "", sl[96] = "", de[48] = "", st[48] = "", pr[64] = "", re[400] = "";
        field(l, "id", id, sizeof(id)); field(l, "type", ty, sizeof(ty)); field(l, "target", tg, sizeof(tg));
        field(l, "slot", sl, sizeof(sl)); field(l, "delta", de, sizeof(de)); field(l, "status", st, sizeof(st));
        field(l, "proposer", pr, sizeof(pr)); field(l, "reason", re, sizeof(re));
        snprintf(c->id, sizeof(c->id), "%s", id); snprintf(c->type, sizeof(c->type), "%s", ty); snprintf(c->target, sizeof(c->target), "%s", tg);
        snprintf(c->slot, sizeof(c->slot), "%s", sl); snprintf(c->delta, sizeof(c->delta), "%s", de); snprintf(c->status, sizeof(c->status), "%s", st);
        snprintf(c->proposer, sizeof(c->proposer), "%s", pr); snprintf(c->reason, sizeof(c->reason), "%s", re);
        if (!id[0] || !ty[0] || !tg[0] || !de[0]) { c->malformed = 1; snprintf(c->bad, sizeof(c->bad), "EDIT line lacks id/type/target/delta: %.150s", l); sanitize(c->bad); }
    }
    if (c->n_edit == 0 && !c->malformed) c->empty = 1;
    free(b);
}
static void load_bank(void) {
    static char names[MAXN][160]; char dir[PL], p[PL]; int n;
    nmas = nspk = ncand = 0;
    snprintf(dir, sizeof(dir), "%s/data/masters", bank); n = list_dir(dir, ".pdl", names, MAXN);
    for (int i = 0; i < n; i++) { snprintf(p, sizeof(p), "%s/%s", dir, names[i]); parse_node(p, names[i], &masters[nmas++]); }
    snprintf(dir, sizeof(dir), "%s/data/spokes", bank); n = list_dir(dir, ".pdl", names, MAXN);
    for (int i = 0; i < n; i++) { snprintf(p, sizeof(p), "%s/%s", dir, names[i]); parse_node(p, names[i], &spokes[nspk++]); }
    snprintf(dir, sizeof(dir), "%s/data/candidates", bank); n = list_dir(dir, ".txt", names, MAXN);
    for (int i = 0; i < n; i++) { snprintf(p, sizeof(p), "%s/%s", dir, names[i]); char keep[320] = ""; if (i < ncand) snprintf(keep, sizeof(keep), "%s", cands[i].verdict);
                                  parse_cand(p, names[i], &cands[ncand++]); }
    snprintf(status, sizeof(status), "loaded %d masters, %d spokes, %d candidate files (read-only)", nmas, nspk, ncand);
}

/* ---------------------------------------------------------------- */
/* validate: fork+exec+waitpid, 10 s watchdog, scratch stdout          */
static void val_finish(int killed) {
    char out[PL]; snprintf(out, sizeof(out), "%s/validate_out.txt", state_dir);
    Cand *c = &cands[val_idx]; c->verdict[0] = '\0';
    if (killed) { snprintf(c->verdict, sizeof(c->verdict), "TIMEOUT (validator killed after 10 s)"); return; }
    char *b = slurp(out);
    if (b) {
        for (char *ln = strtok(b, "\n"); ln; ln = strtok(NULL, "\n"))
            if (!strncmp(ln, "PASS", 4) || !strncmp(ln, "REJECT", 6)) { snprintf(c->verdict, sizeof(c->verdict), "%.300s", ln); break; }
        free(b);
    }
    if (!c->verdict[0]) snprintf(c->verdict, sizeof(c->verdict), "no PASS/REJECT line in validator output");
    sanitize(c->verdict);
    snprintf(status, sizeof(status), "%s: %.200s", c->file, c->verdict);
}
static void val_poll(void) {
    if (val_pid <= 0) return;
    if (waitpid(val_pid, NULL, WNOHANG) == val_pid) { val_pid = -1; val_finish(0); }
    else if (time(NULL) - val_t0 > 10) { kill(val_pid, SIGKILL); waitpid(val_pid, NULL, 0); val_pid = -1; val_finish(1); }
}
static void do_validate(int i) {
    if (i < 0 || i >= ncand) { snprintf(status, sizeof(status), "open a candidate first"); return; }
    if (val_pid > 0) { snprintf(status, sizeof(status), "validator still running"); return; }
    char bin[PL], file[PL], out[PL];
    snprintf(bin, sizeof(bin), "%s/ops/+x/concept_edit_validate.+x", bank);
    snprintf(file, sizeof(file), "%s/data/candidates/%s", bank, cands[i].file);
    if (!exists(bin)) { snprintf(status, sizeof(status), "validator not built: run &.widgits/concept-bank/ops/build_concept_edit_validate.sh"); return; }
    mkdir_p(state_dir); snprintf(out, sizeof(out), "%s/validate_out.txt", state_dir);
    pid_t pid = fork(); if (pid < 0) return;
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        int o = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644), dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 2); } if (o >= 0) dup2(o, 1);
        execl(bin, bin, bank, file, (char *)NULL); _exit(127);
    }
    val_pid = pid; val_t0 = time(NULL); val_idx = i; snprintf(status, sizeof(status), "validating %s ...", cands[i].file);
}

/* ---------------------------------------------------------------- */
static void bar(double w, char *o) {
    int n = (int)floor(fabs(w) * 10 + 0.5); if (n > 10) n = 10;
    o[0] = '['; for (int i = 0; i < 10; i++) o[1 + i] = i < n ? '#' : '-'; o[11] = ']'; o[12] = '\0';
}
typedef struct { char *buf; size_t sz; FILE *f; int n; } Rows;
static void row(Rows *r, const char *cls, int idx, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
#include <stdarg.h>
static void row(Rows *r, const char *cls, int idx, const char *fmt, ...) {
    char t[700]; va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof(t), fmt, ap); va_end(ap); sanitize(t);
    fprintf(r->f, "l_%d_text=%s\nl_%d_cls=%s\nl_%d_idx=%d\n", r->n, t, r->n, cls, r->n, idx); r->n++;
}
static void node_rows(Rows *r, Node *n) {
    row(r, "row", -1, "%s   (%s, N_SLOTS %d, %d active)", n->name, n->kind[0] ? n->kind : "kind?", n->nslots_decl, n->nsl);
    for (int i = 0; i < n->nsl; i++) { char b[16]; bar(n->sl[i].w, b); row(r, "row", -1, "  slot %d  %s %+.2f  -> %s", n->sl[i].idx, b, n->sl[i].w, n->sl[i].points); }
    if (!n->nsl) row(r, "row", -1, "  (no active slots)");
    for (int i = 0; i < n->nbad; i++) row(r, "bad-row", -1, "  MALFORMED line: %s", n->bad[i]);
}
static void write_ui(void) {
    char dst[PL], tmp[PL]; snprintf(dst, sizeof(dst), "%s/concept_bank_ui.txt", pkg); snprintf(tmp, sizeof(tmp), "%s.tmp", dst);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    int tm = !strcmp(tab, "masters"), ts = !strcmp(tab, "spokes"), tc = !strcmp(tab, "cands");
    const char *bshow = bank; size_t hl = strlen(house); if (!strncmp(bank, house, hl) && bank[hl] == '/') bshow = bank + hl + 1;
    fprintf(f, "head=concept bank  ·  %s  ·  %d masters  %d spokes  %d candidates\n", bshow, nmas, nspk, ncand);
    fprintf(f, "status=%s\ncur_tab=%s\n", status, tab);
    fprintf(f, "cls_masters=%s\ncls_spokes=%s\ncls_cands=%s\n", tm ? "tab-active" : "", ts ? "tab-active" : "", tc ? "tab-active" : "");
    fprintf(f, "show_back=%s\nshow_validate=%s\n", level > 0 ? "1" : "", (tc && level > 0) ? "1" : "");
    /* static tier note (AUTO-PROMOTION-RULE.md, all four tiers); plain text, no promote button */
    static const char *N[4] = {
        "Read-only view; promotion is not built here. preschool + elementary_hs: ALWAYS queue for human review.",
        "associate_bachelor + master_phd: MAY auto-promote spoke_weight_delta if ledger score >= 0.90 AND >= 20 obs.",
        "master_phd may also auto-promote goap_action_describe (edit type not built yet).",
        "Current code (^.hai-horn halo_chat_validate) promotes at tier >= 2 WITHOUT checking the ledger; the rule file is a stub." };
    for (int i = 0; i < 4; i++) fprintf(f, "note%d=%s\n", i + 1, tc ? N[i] : "");
    char crumb[300] = "";
    if (level > 0 && sel >= 0) snprintf(crumb, sizeof(crumb), "%s / %s", tab, tm ? masters[sel].name : ts ? spokes[sel].name : cands[sel].file);
    fprintf(f, "crumb=%s\n", crumb);
    Rows r = { 0 }; r.f = open_memstream(&r.buf, &r.sz);
    if (tm && level == 0) { for (int i = 0; i < nmas; i++) row(&r, "row", i, "%s   (%d slots, mirror %s)", masters[i].name, masters[i].nsl, "derived"); if (!nmas) row(&r, "row", -1, "(no masters found under data/masters)"); }
    else if (tm) {
        Node *m = &masters[sel]; row(&r, "head-row", -1, "MASTER %s  -  its own slots (master -> master):", m->name);
        if (!m->nsl) row(&r, "row", -1, "  (none)");
        for (int i = 0; i < m->nsl; i++) { char b[16]; bar(m->sl[i].w, b); row(&r, "row", -1, "  slot %d  %s %+.2f  -> %s", m->sl[i].idx, b, m->sl[i].w, m->sl[i].points); }
        row(&r, "head-row", -1, "DERIVED mirror (spokes pointing at %s, scanned now, nothing written):", m->name);
        int k = 0;
        for (int s = 0; s < nspk; s++) for (int i = 0; i < spokes[s].nsl; i++) if (!strcmp(spokes[s].sl[i].points, m->name)) {
            char b[16]; bar(spokes[s].sl[i].w, b); row(&r, "row", -1, "  %s %+.2f  <- %s  slot %d", b, spokes[s].sl[i].w, spokes[s].name, spokes[s].sl[i].idx); k++; }
        if (!k) row(&r, "row", -1, "  (no spoke points at this master)");
        for (int i = 0; i < m->nbad; i++) row(&r, "bad-row", -1, "  MALFORMED line: %s", m->bad[i]);
    }
    else if (ts && level == 0) { for (int i = 0; i < nspk; i++) row(&r, "row", i, "%s   (%d slots)", spokes[i].name, spokes[i].nsl); if (!nspk) row(&r, "row", -1, "(no spokes found under data/spokes)"); }
    else if (ts) { row(&r, "head-row", -1, "SPOKE %s  -  slots (spoke -> master):", spokes[sel].name); node_rows(&r, &spokes[sel]); }
    else if (tc && level == 0) {
        for (int i = 0; i < ncand; i++) { Cand *c = &cands[i];
            if (c->empty) row(&r, "bad-row", i, "%s   EMPTY file", c->file);
            else if (c->malformed) row(&r, "bad-row", i, "%s   MALFORMED: %.80s", c->file, c->bad);
            else row(&r, "row", i, "%s   %s  %s->%s  %s  [%s]%s%.40s", c->file, c->type, c->target, c->slot, c->delta, c->status, c->verdict[0] ? "  => " : "", c->verdict); }
        if (!ncand) row(&r, "row", -1, "(no candidates under data/candidates)");
    } else if (tc) {
        Cand *c = &cands[sel];
        row(&r, "head-row", -1, "CANDIDATE %s%s", c->file, c->n_edit > 1 ? "  (more than one EDIT line; first shown)" : "");
        if (c->empty) row(&r, "bad-row", -1, "  EMPTY file (no EDIT line)");
        else {
            if (c->malformed) row(&r, "bad-row", -1, "  MALFORMED: %s", c->bad);
            row(&r, "row", -1, "  id        %s", c->id); row(&r, "row", -1, "  type      %s", c->type);
            row(&r, "row", -1, "  target    %s", c->target); row(&r, "row", -1, "  slot      %s", c->slot);
            row(&r, "row", -1, "  delta     %s", c->delta); row(&r, "row", -1, "  status    %s", c->status);
            row(&r, "row", -1, "  proposer  %s", c->proposer); row(&r, "row", -1, "  reason    %.200s", c->reason);
        }
        row(&r, "head-row", -1, "validator: %s", c->verdict[0] ? c->verdict : "(not run - press validate)");
    }
    fclose(r.f);
    fprintf(f, "n_list=%d\n", r.n); fputs(r.buf, f); free(r.buf); fclose(f); rename(tmp, dst);
}

static void do_cmd(const char *cmd) {
    if (!strncmp(cmd, "TAB:", 4)) { snprintf(tab, sizeof(tab), "%s", cmd + 4); sanitize(tab);
        if (strcmp(tab, "masters") && strcmp(tab, "spokes") && strcmp(tab, "cands")) snprintf(tab, sizeof(tab), "masters");
        level = 0; sel = -1; load_bank(); }
    else if (!strncmp(cmd, "SEL:", 4)) { int i = atoi(cmd + 4); int lim = !strcmp(tab, "masters") ? nmas : !strcmp(tab, "spokes") ? nspk : ncand;
        if (level == 0 && i >= 0 && i < lim) { sel = i; level = 1; } }
    else if (!strcmp(cmd, "BACK")) { level = 0; sel = -1; }
    else if (!strcmp(cmd, "RELOAD")) { int keep = level, s = sel; char v[MAXN][320]; for (int i = 0; i < ncand; i++) snprintf(v[i], 320, "%s", cands[i].verdict);
        int n0 = ncand; load_bank(); for (int i = 0; i < ncand && i < n0; i++) snprintf(cands[i].verdict, 320, "%s", v[i]); level = keep; sel = s; }
    else if (!strcmp(cmd, "VALIDATE")) { if (!strcmp(tab, "cands") && level > 0) do_validate(sel); }
    write_ui();
}
static void clear_action_file(void) {
    char p[PL]; snprintf(p, sizeof(p), "%s/concept_bank_action.txt", pkg);
    FILE *f = fopen(p, "w"); if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); }
}
static void poll_action(int *last_seq) {
    char p[PL]; snprintf(p, sizeof(p), "%s/concept_bank_action.txt", pkg);
    FILE *f = fopen(p, "r"); if (!f) return;
    char buf[2048]; size_t nr = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[nr] = '\0';
    int seq = 0; char cmd[512] = "";
    for (char *ls = buf; *ls; ) {
        char *le = strchr(ls, '\n'); size_t ll = le ? (size_t)(le - ls) : strlen(ls);
        if (!strncmp(ls, "seq=", 4)) seq = atoi(ls + 4);
        else if (!strncmp(ls, "cmd=", 4)) { size_t cl = ll - 4; if (cl >= sizeof(cmd)) cl = sizeof(cmd) - 1; memcpy(cmd, ls + 4, cl); cmd[cl] = '\0'; }
        if (!le) break; ls = le + 1;
    }
    if (seq > *last_seq && cmd[0]) { *last_seq = seq; do_cmd(cmd); }
}
static void on_term(int sig) { (void)sig; if (val_pid > 0) kill(val_pid, SIGKILL); _exit(0); }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "Usage: %s <house_root> <package_dir> [arg3]\n", argv[0]); return 1; }
    snprintf(house, sizeof(house), "%s", argv[1]); snprintf(pkg, sizeof(pkg), "%s", argv[2]);
    snprintf(cfg_path, sizeof(cfg_path), "%s/concept_bank_hq_config.pdl", pkg);
    snprintf(state_dir, sizeof(state_dir), "%s/state", pkg); mkdir_p(state_dir);
    signal(SIGTERM, on_term); signal(SIGINT, on_term); signal(SIGHUP, on_term);
    load_config(); clear_action_file(); load_bank(); write_ui();
    int last_seq = 0;
    for (;;) { usleep(50000); poll_action(&last_seq); if (val_pid > 0) { val_poll(); if (val_pid <= 0) write_ui(); } }
}
