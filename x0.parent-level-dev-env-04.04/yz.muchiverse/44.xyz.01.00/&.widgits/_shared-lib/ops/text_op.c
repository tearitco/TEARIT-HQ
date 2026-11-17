/* text_op - the core of the +phone Texting app (BOOK 16.game/PLUS-PHONE-TEXTING-APP-DESIGN-2026-11-16.md): contacts, send, threads, one thread, search, the address book.
 * No screens here: the app screens and the entity auto-reply op call these verbs. Pure file I/O + fork/exec of two existing ops; no network, no shell.
 *
 * Usage: text_op <house> <verb> [args]
 *   me                          my number, label and phone folder (config `MYPHONE | <number>` in &.hq-apps/plus-phone/texting.pdl, else the first phones.index row labelled `self`)
 *   contacts                    CONTACT|<number>|<name>|<owned|other>|<index label>   owned entities first, then the others; my own number is left out; a name from the address book wins over the label
 *   send <to> <text...>         appends ONE `say` line to MY outbox through phone_send_op (the sender number comes from my own phone.pdl); the router delivers it
 *   threads                     THREAD|<peer>|<name>|<last_ms>|<in|out>|<unread>|<last text>   newest first
 *   thread <peer> [--read]      LINE|<ms>|<in|out>|<text>   oldest first; --read records the thread as read
 *   search <query>              HIT|contact|<number>|<name>  and  HIT|msg|<peer>|<ms>|<in|out>|<text>   case-insensitive substring over MY contacts, notes and MY phone's lines only (caps: 200 hits)
 *   contact add <number> <name> | contact name <number> | contact note <number> <text> | contact publish <number>     the address book is a trunk_op trunk + branch beside my phone (trunk contacts.trunk.txt, branch contacts.txt, owner `me`); notes are private rows
 * Only MY phone's inbox/outbox are ever read: the phone folder must resolve under <house>/xyzfs/users/ and end in /zz.phone. Another user's phone is never opened (the router delivers into it).
 * Exit: 0 ok, 1 refused (one line `REFUSED ...` on stdout, nothing written), 2 usage. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define PN 4352
static char HOUSE[PN], MYNUM[20], MYLABEL[96], MYPHONE[PN], MYENT[PN];

static int number_ok(const char *n) { if (strlen(n) != 13) return 0; for (int i = 0; i < 13; i++) { if (i == 3 || i == 8) { if (n[i] != '-') return 0; } else if (n[i] < '0' || n[i] > '9') return 0; } return 1; }
/* fork/exec with stdout+stderr captured (no shell) */
static int run(char *const av[], char *out, size_t cap) {
    int p[2]; if (pipe(p)) return -1; pid_t c = fork(); if (c < 0) return -1;
    if (c == 0) { close(p[0]); dup2(p[1], 1); dup2(p[1], 2); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t r; while (n + 1 < cap && (r = read(p[0], out + n, cap - 1 - n)) > 0) n += (size_t)r; out[n] = 0; close(p[0]);
    int st = 0; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static void chomp(char *s) { size_t n = strlen(s); while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = 0; }
static int under_users(const char *dir) { char rp[PN], base[PN]; if (!realpath(dir, rp)) return 0; snprintf(base, sizeof base, "%s/xyzfs/users/", HOUSE); char rb[PN]; if (!realpath(HOUSE, rb)) return 0; snprintf(base, sizeof base, "%s/xyzfs/users/", rb); size_t L = strlen(rp); return !strncmp(rp, base, strlen(base)) && L > 9 && !strcmp(rp + L - 9, "/zz.phone"); }

/* ---- the index ---- */
typedef struct { char number[20], uid[80], label[96], ent[PN], phone[PN]; } Ix;
static Ix *IX; static int NIX;
static int load_index(void) {
    char f[PN]; snprintf(f, sizeof f, "%s/^.hai-server/phones.index", HOUSE); FILE *fp = fopen(f, "r"); if (!fp) return 0;
    static char ln[PN * 2 + 400]; int cap = 0;
    while (fgets(ln, sizeof ln, fp)) {
        chomp(ln); char *p[5]; int k = 0; p[k++] = ln; for (char *c = ln; *c && k < 5; c++) if (*c == '|') { *c = 0; p[k++] = c + 1; } if (k < 5 || !number_ok(p[0])) continue;
        if (NIX == cap) { cap = cap ? cap * 2 : 128; IX = realloc(IX, (size_t)cap * sizeof *IX); if (!IX) exit(2); }
        Ix *x = &IX[NIX++]; snprintf(x->number, sizeof x->number, "%s", p[0]); snprintf(x->uid, sizeof x->uid, "%s", p[1]); snprintf(x->label, sizeof x->label, "%s", p[2]); snprintf(x->ent, sizeof x->ent, "%s", p[3]); snprintf(x->phone, sizeof x->phone, "%s", p[4]);
    }
    fclose(fp); return 1;
}
static Ix *ix_find(const char *num) { for (int i = 0; i < NIX; i++) if (!strcmp(IX[i].number, num)) return &IX[i]; return NULL; }
static int resolve_me(void) {
    char cf[PN], want[20] = ""; snprintf(cf, sizeof cf, "%s/&.hq-apps/plus-phone/texting.pdl", HOUSE); FILE *fp = fopen(cf, "r");
    if (fp) { char ln[300]; while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, "MYPHONE", 7)) continue; char *b = strchr(ln, '|'); if (!b) continue; b++; while (*b == ' ') b++; chomp(b); size_t n = strlen(b); while (n && b[n - 1] == ' ') b[--n] = 0; snprintf(want, sizeof want, "%s", b); } fclose(fp); }
    Ix *me = NULL; if (want[0]) me = ix_find(want); else for (int i = 0; i < NIX; i++) if (!strcmp(IX[i].label, "self")) { me = &IX[i]; break; }
    if (!me) { printf("REFUSED no phone of mine: add `MYPHONE | <number>` to &.hq-apps/plus-phone/texting.pdl or a `self` row to phones.index\n"); return 0; }
    if (!under_users(me->phone)) { printf("REFUSED my phone folder is not under xyzfs/users/.../zz.phone\n"); return 0; }
    snprintf(MYNUM, sizeof MYNUM, "%s", me->number); snprintf(MYLABEL, sizeof MYLABEL, "%s", me->label); snprintf(MYPHONE, sizeof MYPHONE, "%s", me->phone); snprintf(MYENT, sizeof MYENT, "%s", me->ent);
    if (MYPHONE[0] != '/') { char t[PN]; snprintf(t, sizeof t, "%s/%s", HOUSE, MYPHONE); snprintf(MYPHONE, sizeof MYPHONE, "%s", t); }
    return 1;
}
/* "owned": the contact's entity lives under the same user folder as my phone (xyzfs/users/<uid>/) */
static int owned(const Ix *x) { const char *a = strstr(MYPHONE, "/xyzfs/users/"); if (!a) return 0; a += 13; const char *e = strchr(a, '/'); if (!e) return 0; size_t L = (size_t)(e - a); const char *b = strstr(x->phone, "/xyzfs/users/"); if (!b) { b = strstr(x->phone, "xyzfs/users/"); if (!b) return 0; b += 12; } else b += 13; return !strncmp(a, b, L) && b[L] == '/'; }

/* ---- the address book (trunk_op) ---- */
static char TRUNK[PN], BRANCH[PN], TOP[PN];
static void book_load(void);
static int book_ready(void) {
    snprintf(TOP, sizeof TOP, "%s/&.widgits/_shared-lib/ops/+x/trunk_op.+x", HOUSE); if (access(TOP, X_OK)) { printf("REFUSED trunk_op is not built\n"); return 0; }
    snprintf(TRUNK, sizeof TRUNK, "%s/contacts.trunk.txt", MYPHONE); snprintf(BRANCH, sizeof BRANCH, "%s/contacts.txt", MYPHONE); char out[400];
    if (access(TRUNK, F_OK)) { char *a[] = { TOP, TRUNK, "init-trunk", "--as", "me", NULL }; run(a, out, sizeof out); }
    if (access(BRANCH, F_OK)) { char *a[] = { TOP, BRANCH, "init-branch", TRUNK, "--as", "me", NULL }; run(a, out, sizeof out); }
    if (access(BRANCH, F_OK)) return 0;
    book_load(); return 1;
}
/* the address book is read ONCE per run with `trunk_op list` (one fork), then looked up in memory: a screen refresh must not fork per contact */
static char **BKEY, **BVAL; static int NB;
static void book_load(void) {
    static char out[400000]; char *a[] = { TOP, BRANCH, "list", NULL }; NB = 0; if (run(a, out, sizeof out) != 0) return;
    int cap = 0; char *save = NULL;
    for (char *ln = strtok_r(out, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
        char *b1 = strchr(ln, '|'); if (!b1) continue; char *b2 = strrchr(ln, '|'); if (b2 == b1) continue; *b1 = 0; *b2 = 0; char *val = b1 + 1;
        char *w = val; for (char *r = val; *r; r++) { if (*r == '\\' && r[1]) { r++; *w++ = (*r == 'p') ? '|' : (*r == 'n') ? '\n' : *r; } else *w++ = *r; } *w = 0;
        if (NB == cap) { cap = cap ? cap * 2 : 64; BKEY = realloc(BKEY, (size_t)cap * sizeof *BKEY); BVAL = realloc(BVAL, (size_t)cap * sizeof *BVAL); if (!BKEY || !BVAL) exit(2); }
        BKEY[NB] = strdup(ln); BVAL[NB] = strdup(val); NB++;
    }
}
static int book_get(const char *key, char *val, size_t cap) { for (int i = 0; i < NB; i++) if (!strcmp(BKEY[i], key)) { snprintf(val, cap, "%s", BVAL[i]); return 1; } return 0; }
static const char *name_of(const Ix *x, char *buf, size_t cap) { char key[40], v[200]; snprintf(key, sizeof key, "c.%s", x->number); if (book_get(key, v, sizeof v)) snprintf(buf, cap, "%s", v); else snprintf(buf, cap, "%s", x->label); return buf; }

static int v_me(void) { printf("ME|%s|%s|%s\n", MYNUM, MYLABEL, MYPHONE); return 0; }
static int v_contacts(void) {
    for (int pass = 0; pass < 2; pass++) for (int i = 0; i < NIX; i++) { Ix *x = &IX[i]; if (!strcmp(x->number, MYNUM)) continue; int o = owned(x); if ((pass == 0) != (o == 1)) continue; char nm[200]; name_of(x, nm, sizeof nm); printf("CONTACT|%s|%s|%s|%s\n", x->number, nm, o ? "owned" : "other", x->label); }
    return 0;
}
static int v_send(int argc, char **argv) {
    if (argc < 2) return 2; const char *to = argv[0]; char text[4100] = ""; for (int i = 1; i < argc; i++) { if (i > 1) strncat(text, " ", sizeof text - strlen(text) - 1); strncat(text, argv[i], sizeof text - strlen(text) - 1); }
    char op[PN], ref[40], out[600]; snprintf(op, sizeof op, "%s/&.widgits/_shared-lib/ops/+x/phone_send_op.+x", HOUSE); snprintf(ref, sizeof ref, "t:%s", to);
    if (!number_ok(to)) { printf("REFUSED %s is not a phone number (NNN-NNNN-NNNN)\n", to); return 1; }
    char *a[] = { op, MYPHONE, (char *)to, "say", ref, text, NULL }; int rc = run(a, out, sizeof out); chomp(out);
    if (rc != 0) { printf("REFUSED %s\n", out[0] ? out : "the send op refused"); return 1; } printf("SENT to %s\n", to); return 0;
}
/* ---- reading my phone's lines ---- */
typedef struct { long long ms; char peer[20]; int in; char text[4100]; } Line;
static Line *L; static int NL;
static void read_file(const char *name, int in) {
    char f[PN]; snprintf(f, sizeof f, "%s/%s", MYPHONE, name); FILE *fp = fopen(f, "r"); if (!fp) return; static char ln[9000]; static int cap = 0;
    while (fgets(ln, sizeof ln, fp)) {
        chomp(ln); if (ln[0] == '#' || !ln[0]) continue; char *p[6]; int k = 0; p[k++] = ln; for (char *c = ln; *c && k < 6; c++) if (*c == '|' && k < 6) { *c = 0; p[k++] = c + 1; if (k == 6) break; }
        if (k < 6 || strcmp(p[3], "say")) continue; const char *peer = in ? p[1] : p[2]; if (!number_ok(peer)) continue;
        if (NL == cap) { cap = cap ? cap * 2 : 256; L = realloc(L, (size_t)cap * sizeof *L); if (!L) exit(2); }
        Line *l = &L[NL++]; l->ms = atoll(p[0]); snprintf(l->peer, sizeof l->peer, "%s", peer); l->in = in; snprintf(l->text, sizeof l->text, "%s", p[5]);
    }
    fclose(fp);
}
static int cmp_line(const void *a, const void *b) { const Line *x = a, *y = b; return x->ms < y->ms ? -1 : x->ms > y->ms ? 1 : 0; }
static void load_lines(void) { read_file("inbox.txt", 1); read_file("outbox.txt", 0); if (NL > 1) qsort(L, (size_t)NL, sizeof *L, cmp_line); }
/* read cursors: <phone>/texting.read rows "<peer>|<ms>" (a view cache, safe to delete) */
static long long read_cursor(const char *peer) { char f[PN]; snprintf(f, sizeof f, "%s/texting.read", MYPHONE); FILE *fp = fopen(f, "r"); long long v = 0; if (!fp) return 0; char ln[100]; while (fgets(ln, sizeof ln, fp)) { chomp(ln); char *b = strchr(ln, '|'); if (b) { *b = 0; if (!strcmp(ln, peer)) v = atoll(b + 1); } } fclose(fp); return v; }
static void write_cursor(const char *peer, long long ms) {
    char f[PN], t[PN + 8]; snprintf(f, sizeof f, "%s/texting.read", MYPHONE); snprintf(t, sizeof t, "%s.tmp", f); FILE *o = fopen(t, "w"); if (!o) return; FILE *fp = fopen(f, "r"); char ln[100];
    if (fp) { while (fgets(ln, sizeof ln, fp)) { char c[100]; snprintf(c, sizeof c, "%s", ln); chomp(c); char *b = strchr(c, '|'); if (b) *b = 0; if (strcmp(c, peer)) fputs(ln, o); } fclose(fp); }
    fprintf(o, "%s|%lld\n", peer, ms); fclose(o); rename(t, f);
}
static int v_threads(void) {
    load_lines(); char seen[2000][20]; int ns = 0;
    for (int i = NL - 1; i >= 0; i--) {   /* newest first */
        int dup = 0; for (int j = 0; j < ns; j++) if (!strcmp(seen[j], L[i].peer)) { dup = 1; break; } if (dup || ns >= 2000) continue; snprintf(seen[ns++], 20, "%s", L[i].peer);
        long long cur = read_cursor(L[i].peer); int unread = 0; for (int k = 0; k < NL; k++) if (L[k].in && !strcmp(L[k].peer, L[i].peer) && L[k].ms > cur) unread++;
        Ix *x = ix_find(L[i].peer); char nm[200]; if (x) name_of(x, nm, sizeof nm); else snprintf(nm, sizeof nm, "%s", L[i].peer);
        printf("THREAD|%s|%s|%lld|%s|%d|%s\n", L[i].peer, nm, L[i].ms, L[i].in ? "in" : "out", unread, L[i].text);
    }
    return 0;
}
static int v_thread(int argc, char **argv) {
    if (argc < 1) return 2; if (!number_ok(argv[0])) { printf("REFUSED %s is not a phone number\n", argv[0]); return 1; } int rd = argc > 1 && !strcmp(argv[1], "--read"); load_lines(); long long last = 0;
    for (int i = 0; i < NL; i++) if (!strcmp(L[i].peer, argv[0])) { printf("LINE|%lld|%s|%s\n", L[i].ms, L[i].in ? "in" : "out", L[i].text); if (L[i].in && L[i].ms > last) last = L[i].ms; }
    if (rd && last) write_cursor(argv[0], last); return 0;
}
static const char *ci_find(const char *h, const char *n) { return strcasestr(h, n); }
static int v_search(int argc, char **argv) {
    if (argc < 1 || !argv[0][0]) return 2; const char *q = argv[0]; int hits = 0;
    for (int i = 0; i < NIX && hits < 200; i++) { Ix *x = &IX[i]; if (!strcmp(x->number, MYNUM)) continue; char nm[200], nt[4100] = "", key[40]; name_of(x, nm, sizeof nm); snprintf(key, sizeof key, "n.%s", x->number); book_get(key, nt, sizeof nt);
        if (ci_find(nm, q) || ci_find(x->number, q) || ci_find(x->label, q) || (nt[0] && ci_find(nt, q))) { printf("HIT|contact|%s|%s\n", x->number, nm); hits++; } }
    load_lines(); for (int i = NL - 1; i >= 0 && hits < 200; i--) if (ci_find(L[i].text, q)) { printf("HIT|msg|%s|%lld|%s|%s\n", L[i].peer, L[i].ms, L[i].in ? "in" : "out", L[i].text); hits++; }
    printf("SEARCH-END %d\n", hits); return 0;
}
static int v_contact(int argc, char **argv) {
    if (argc < 2) return 2; const char *what = argv[0], *num = argv[1]; if (!number_ok(num)) { printf("REFUSED %s is not a phone number (NNN-NNNN-NNNN)\n", num); return 1; } if (!book_ready()) return 1;
    char key[48], out[600], text[3000] = ""; for (int i = 2; i < argc; i++) { if (i > 2) strncat(text, " ", sizeof text - strlen(text) - 1); strncat(text, argv[i], sizeof text - strlen(text) - 1); }
    if (!strcmp(what, "add") || !strcmp(what, "name")) { if (!text[0]) return 2; snprintf(key, sizeof key, "c.%s", num); char *a[] = { TOP, BRANCH, "put", key, text, "--as", "me", NULL }; int rc = run(a, out, sizeof out); chomp(out); if (rc) { printf("REFUSED %s\n", out); return 1; } printf("CONTACT-SAVED %s %s\n", num, text); return 0; }
    if (!strcmp(what, "note")) { if (!text[0]) return 2; snprintf(key, sizeof key, "n.%s", num); char *a[] = { TOP, BRANCH, "put", key, text, "--private", "--as", "me", NULL }; int rc = run(a, out, sizeof out); chomp(out); if (rc) { printf("REFUSED %s\n", out); return 1; } printf("NOTE-SAVED %s\n", num); return 0; }
    if (!strcmp(what, "publish")) { snprintf(key, sizeof key, "c.%s", num); char *a[] = { TOP, BRANCH, "publish", key, "--as", "me", NULL }; int rc = run(a, out, sizeof out); chomp(out); if (rc) { printf("REFUSED %s\n", out); return 1; } printf("CONTACT-PUBLISHED %s\n", num); return 0; }
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: text_op <house> me|contacts|send <to> <text>|threads|thread <peer> [--read]|search <q>|contact add|name|note|publish <number> [text]\n"); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); const char *v = argv[2]; if (!load_index()) { printf("REFUSED phones.index is missing\n"); return 1; }
    if (!resolve_me()) return 1;
    int rc = 2;
    if (!strcmp(v, "me")) rc = v_me();
    else if (!strcmp(v, "contacts")) { book_ready(); rc = v_contacts(); }
    else if (!strcmp(v, "send")) rc = v_send(argc - 3, argv + 3);
    else if (!strcmp(v, "threads")) { book_ready(); rc = v_threads(); }
    else if (!strcmp(v, "thread")) rc = v_thread(argc - 3, argv + 3);
    else if (!strcmp(v, "search")) { book_ready(); rc = v_search(argc - 3, argv + 3); }
    else if (!strcmp(v, "contact")) rc = v_contact(argc - 3, argv + 3);
    return rc;
}
