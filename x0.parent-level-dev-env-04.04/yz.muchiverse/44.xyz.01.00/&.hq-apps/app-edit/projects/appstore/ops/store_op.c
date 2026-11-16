/* store_op - the App Store's engine and projector (design: BOOK 16.game/PLUS-PHONE-APP-STORE-AND-APPROVED-REPO-DESIGN-2026-11-16.md). Stage 1: the catalog is the LOCAL app-edit projects folder.
 *   module (the window runs it): store_op <house> <package dir>  with env KHTPM_ARG3 = the phone instance dir; republishes <inst>/.hq_manager/ui.txt when marker.txt grew or every 5 s; ends with its parent. [--once <inst>] = one projection.
 *   verbs:  store_op ui_install <id> <inst>   store_op ui_remove <id> <inst>   (both call phone_op, fork/exec, no shell string; install then builds the app's ops with its own ops/build_<id>.sh through cpu_gate)
 *           store_op catalog <house>          prints  id|title|installed|check   (check = OK or the first app_check failure; the harness reads this)
 * State: none of its own beyond <inst>/.hq_manager/last.txt (the last result line, a projection). What is installed is the folder apps/<id>/ (phone_op owns it). System apps settings and appstore are never offered. Footprint: no resident work except the module's 0.4 s poll. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#define P 4096
#define MAXE 32
static char HOUSE[P], HQD[P], INST[P];
static int word(const char *s) { if (!*s || strlen(s) > 40) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '-' || *s == '_')) return 0; return 1; }
static int sys_app(const char *id) { return !strcmp(id, "settings") || !strcmp(id, "appstore"); }
static int house_of(const char *inst) { const char *h = strstr(inst, "/&.hq-apps/plus-phone/instances/"); if (!h) return 0; snprintf(HOUSE, sizeof HOUSE, "%.*s", (int)(h - inst), inst); return 1; }
/* fork/exec with an output file and a 600 s watchdog; returns the exit code, -1 on a signal or timeout */
static int run_out(char *const av[], const char *outf) { pid_t c = fork(); if (c < 0) return -1; if (c == 0) { int fd = open(outf, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) dup2(fd, 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 2); dup2(nul, 0); } execv(av[0], av); _exit(127); }
    int st = 0; for (int i = 0; i < 6000; i++) { pid_t r = waitpid(c, &st, WNOHANG); if (r == c) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(100000); } kill(c, SIGKILL); waitpid(c, &st, 0); return -1; }
static void rd1(const char *f, char *o, size_t cap) { o[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return; if (fgets(o, (int)cap, fp)) o[strcspn(o, "\r\n")] = 0; fclose(fp); }
static void wr(const char *f, const char *t) { char tmp[P]; snprintf(tmp, sizeof tmp, "%s.tmp", f); FILE *w = fopen(tmp, "w"); if (!w) return; fputs(t, w); fclose(w); rename(tmp, f); }
static void bumpq(void) { char f[P]; snprintf(f, sizeof f, "%s/marker.txt", HQD); struct stat sb; if (!stat(f, &sb) && sb.st_size > 4096) unlink(f); FILE *m = fopen(f, "a"); if (m) { fputc('x', m); fclose(m); } }
static long msizeq(void) { char f[P]; struct stat sb; snprintf(f, sizeof f, "%s/marker.txt", HQD); return stat(f, &sb) ? 0 : (long)sb.st_size; }
static int title_of(const char *mf, char *t, size_t cap) { FILE *fp = fopen(mf, "r"); char ln[400]; t[0] = 0; while (fp && fgets(ln, sizeof ln, fp)) if (!strncmp(ln, "TITLE", 5)) { char *b = strchr(ln, '|'); if (b) { b++; while (*b == ' ') b++; b[strcspn(b, "\r\n")] = 0; snprintf(t, cap, "%s", b); } } if (fp) fclose(fp); return t[0] != 0; }
static struct { char id[48], title[64], ver[24], hash[65]; int inst; } E[MAXE]; static int NE; static int GIT;
static int store_pref(const char *key, char f[4][300]); static int tree_hash(const char *root, char *hex); static char TREASON[200];
static int cmp_id(const void *a, const void *b) { return strcmp(((const typeof(E[0]) *)a)->id, ((const typeof(E[0]) *)b)->id); }
static void scan_git(void);
static void scan(void) { char gf[4][300]; GIT = store_pref("STORE_SOURCE", gf) && !strcmp(gf[1], "git"); if (GIT) { scan_git(); return; } char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/app-edit/projects", HOUSE); NE = 0; DIR *dp = opendir(d); if (!dp) return; struct dirent *e;
    while ((e = readdir(dp)) && NE < MAXE) { if (e->d_name[0] == '.' || !word(e->d_name) || sys_app(e->d_name)) continue; char mf[P]; snprintf(mf, sizeof mf, "%s/%s/manifest.pdl", d, e->d_name); if (access(mf, R_OK)) continue;
        snprintf(E[NE].id, sizeof E[NE].id, "%s", e->d_name); if (!title_of(mf, E[NE].title, sizeof E[NE].title)) snprintf(E[NE].title, sizeof E[NE].title, "%s", e->d_name);
        char im[P]; snprintf(im, sizeof im, "%s/&.hq-apps/plus-phone/apps/%s/manifest.pdl", HOUSE, e->d_name); E[NE].inst = !access(im, R_OK); NE++; } closedir(dp); qsort(E, NE, sizeof E[0], cmp_id); }
/* git mode: the catalog is exactly the OK rows of store_cache/verified.txt (written by `sync`); a package that was refused or never synced is not offered */
static void scan_git(void) { char vf[P], ln[400]; snprintf(vf, sizeof vf, "%s/&.hq-apps/plus-phone/store_cache/verified.txt", HOUSE); NE = 0; FILE *fp = fopen(vf, "r");
    while (fp && fgets(ln, sizeof ln, fp) && NE < MAXE) { ln[strcspn(ln, "\r\n")] = 0; if (strncmp(ln, "OK|", 3)) continue; char *f[4]; int n = 0; char *p = ln; while (n < 4) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } if (n < 4 || !word(f[1]) || sys_app(f[1])) continue;
        snprintf(E[NE].id, sizeof E[NE].id, "%s", f[1]); snprintf(E[NE].ver, sizeof E[NE].ver, "%s", f[2]); snprintf(E[NE].hash, sizeof E[NE].hash, "%s", f[3]); char mf[P]; snprintf(mf, sizeof mf, "%s/&.hq-apps/plus-phone/store_cache/repo/%s/manifest.pdl", HOUSE, f[1]); if (!title_of(mf, E[NE].title, sizeof E[NE].title)) snprintf(E[NE].title, sizeof E[NE].title, "%s", f[1]);
        char im[P]; snprintf(im, sizeof im, "%s/&.hq-apps/plus-phone/apps/%s/manifest.pdl", HOUSE, f[1]); E[NE].inst = !access(im, R_OK); NE++; } if (fp) fclose(fp); qsort(E, NE, sizeof E[0], cmp_id); }
static void say(const char *t) { char f[P]; snprintf(f, sizeof f, "%s/last.txt", HQD); wr(f, t); bumpq(); }
static int ui_verb(const char *verb, const char *id, const char *inst) {
    snprintf(INST, sizeof INST, "%s", inst); snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); if (!house_of(inst) || !word(id) || sys_app(id)) return 2;
    char pm[P], pkg[P], o[P], line[300]; snprintf(pm, sizeof pm, "%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x", HOUSE); snprintf(o, sizeof o, "%s/store_run.txt", HQD); snprintf(pkg, sizeof pkg, "%s/&.hq-apps/app-edit/projects/%s", HOUSE, id);
    if (!strcmp(verb, "ui_sync")) { char self[P], r[300] = "", ln[300]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n <= 0) return 2; self[n] = 0; char *av[] = { self, "sync", HOUSE, NULL }; int rc = run_out(av, o); char l2[300]; FILE *fp = fopen(o, "r"); while (fp && fgets(l2, sizeof l2, fp)) { l2[strcspn(l2, "\r\n")] = 0; if (l2[0]) snprintf(r, sizeof r, "%s", l2); } if (fp) fclose(fp); if (!strncmp(r, "SYNCED", 6)) { char *v = strstr(r, "verified="); snprintf(ln, sizeof ln, "synced the approved apps (%s)", v ? v : "done"); rc = 0; } else snprintf(ln, sizeof ln, "sync did not run: %s", r[0] ? r : "no answer"); say(ln); return rc == 0 ? 0 : 1; }
    if (!strcmp(verb, "ui_remove")) { char *av[] = { pm, "uninstall", HOUSE, (char *)id, NULL }; int rc = run_out(av, o); char r[200]; rd1(o, r, sizeof r); snprintf(line, sizeof line, "%s: %s", id, rc == 0 ? r : "could not remove"); say(line); return rc == 0 ? 0 : 1; }
    scan(); if (GIT) { snprintf(pkg, sizeof pkg, "%s/&.hq-apps/plus-phone/store_cache/repo/%s", HOUSE, id); int fi = -1; for (int i = 0; i < NE; i++) if (!strcmp(E[i].id, id)) fi = i; char hx[65]; if (fi < 0) { snprintf(line, sizeof line, "%s: not in the verified catalog (sync first)", id); say(line); return 1; } if (!tree_hash(pkg, hx) || strcmp(hx, E[fi].hash)) { snprintf(line, sizeof line, "%s: NOT installed, the files changed since they were verified", id); say(line); return 1; } }
    if (strcmp(verb, "ui_install")) return 2; if (access(pkg, R_OK)) { snprintf(line, sizeof line, "%s: no such package in the catalog", id); say(line); return 1; }
    { char *av[] = { pm, "install", HOUSE, pkg, NULL }; int rc = run_out(av, o); FILE *fp = fopen(o, "r"); char l[300], last[300] = ""; while (fp && fgets(l, sizeof l, fp)) { l[strcspn(l, "\r\n")] = 0; if (l[0]) snprintf(last, sizeof last, "%s", l); } if (fp) fclose(fp);
        if (rc != 0) { snprintf(line, sizeof line, "%s not installed: %s", id, last[0] ? last : "refused"); say(line); return 1; } }
    char bs[P]; snprintf(bs, sizeof bs, "%s/&.hq-apps/plus-phone/apps/%s/ops/build_%s.sh", HOUSE, id, id);
    if (!access(bs, R_OK)) { snprintf(line, sizeof line, "%s installed; building it...", id); say(line); char gate[P]; snprintf(gate, sizeof gate, "%s/&.widgits/_shared-lib/ops/+x/cpu_gate.+x", HOUSE); char lab[80]; snprintf(lab, sizeof lab, "store-build-%s", id);
        char *av[] = { gate, HOUSE, "run", lab, "--wait", "300", "--", "/bin/sh", bs, NULL }; int rc = run_out(av, o); if (rc != 0) { snprintf(line, sizeof line, "%s installed, but its build FAILED (exit %d): it will not run", id, rc); say(line); return 1; } }
    if (GIT) { char lf[P], cm[P], cmt[100] = ""; snprintf(lf, sizeof lf, "%s/&.hq-apps/plus-phone/store_cache/install.ledger", HOUSE); snprintf(cm, sizeof cm, "%s/&.hq-apps/plus-phone/store_cache/commit.txt", HOUSE); rd1(cm, cmt, sizeof cmt); FILE *lw = fopen(lf, "a"); if (lw) { for (int i = 0; i < NE; i++) if (!strcmp(E[i].id, id)) fprintf(lw, "INSTALL|%s|%s|%s|%s|%ld\n", id, E[i].ver, cmt, E[i].hash, (long)time(NULL)); fclose(lw); } }
    snprintf(line, sizeof line, "%s installed", id); say(line); return 0; }
/* STOPGAP (copy of tsc_op's book_vars; remove when phone_projector publishes the phone bar's book:page variables): the book name is the BOOK row of phone_prefs.pdl (default phone); the pages are home (the user's name) plus every installed app; the current page is this app's name. */
static int pref_book(const char *house, char *out, size_t cap) { char f[4096], ln[300]; snprintf(f, sizeof f, "%s/&.hq-apps/plus-phone/phone_prefs.pdl", house); FILE *fp = fopen(f, "r"); out[0] = 0; while (fp && fgets(ln, sizeof ln, fp)) if (!strncmp(ln, "BOOK", 4)) { char *b = strchr(ln, '|'); if (b) { b++; while (*b == ' ') b++; b[strcspn(b, "\r\n")] = 0; snprintf(out, cap, "%s", b); } } if (fp) fclose(fp); return out[0] != 0; }
static int book_vars(char *UI, size_t cap, const char *inst, const char *house, const char *page) {
    char b[80] = "", un[80] = "", ln[300]; int n = 0; if (!pref_book(house, b, sizeof b)) snprintf(b, sizeof b, "phone"); n += snprintf(UI + n, cap - n, "book_label=%s\npage_label=%s\n", b, page);
    { char sf[4096]; snprintf(sf, sizeof sf, "%s/#.desktop/strip_ui.txt", house); FILE *pf = fopen(sf, "r"); if (pf) { while (fgets(ln, sizeof ln, pf)) if (!strncmp(ln, "username=", 9)) { ln[strcspn(ln, "\r\n")] = 0; snprintf(un, sizeof un, "%s", ln + 9); break; } fclose(pf); } if (!un[0]) snprintf(un, sizeof un, "home"); }
    char ad[4096]; snprintf(ad, sizeof ad, "%s/&.hq-apps/plus-phone/apps", house); n += snprintf(UI + n, cap - n, "pg_0_label=%s\npg_0_act='%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' 'home' '%s'\n", un, house, inst); int k = 1;
    DIR *dp = opendir(ad); if (dp) { struct dirent *e; while ((e = readdir(dp)) && k < 12) { if (e->d_name[0] == '.') continue; char mf[4096]; snprintf(mf, sizeof mf, "%s/%s/manifest.pdl", ad, e->d_name); if (access(mf, R_OK)) continue; n += snprintf(UI + n, cap - n, "pg_%d_label=%s\npg_%d_act='%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' 'open' '%s' '%s'\n", k, e->d_name, k, house, inst, e->d_name); k++; } closedir(dp); }
    n += snprintf(UI + n, cap - n, "n_pages=%d\n", k); return n; }
static void publish(void) {
    char last[300]; char f[P]; snprintf(f, sizeof f, "%s/last.txt", HQD); rd1(f, last, sizeof last); scan(); static char UI[16384]; int n = 0; n += snprintf(UI + n, sizeof UI - n, "window_title=App Store\nstatus=%s\nshow_sync=%d\nn_apps=%d\n", last[0] ? last : (GIT && NE == 0 ? "Tap sync to fetch the approved apps." : "Pick an app to install or remove."), GIT, NE);
    n += book_vars(UI + n, sizeof UI - n, INST, HOUSE, "App Store");
    for (int i = 0; i < NE; i++) n += snprintf(UI + n, sizeof UI - n, "app_%d_id=%s\napp_%d_verb=%s\napp_%d_cls=%s\napp_%d_label=  %s  %s  \n", i, E[i].id, i, E[i].inst ? "ui_remove" : "ui_install", i, E[i].inst ? "dimbtn" : "btn", i, E[i].inst ? "remove" : "install", E[i].title);
    char uf[P], t[P], old[16384] = ""; snprintf(uf, sizeof uf, "%s/ui.txt", HQD); FILE *fp = fopen(uf, "r"); if (fp) { size_t r = fread(old, 1, sizeof old - 1, fp); old[r] = 0; fclose(fp); } if (strcmp(old, UI)) { snprintf(t, sizeof t, "%s/ui.txt.tmp", HQD); fp = fopen(t, "w"); if (fp) { fputs(UI, fp); fclose(fp); rename(t, uf); } } }
static int module_main(const char *inst, int once) { snprintf(INST, sizeof INST, "%s", inst); snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); if (!house_of(inst)) return 2; if (once) { publish(); return 0; }
    long seen = -1; time_t last = 0; for (;;) { long m = msizeq(); time_t now = time(NULL); if (m != seen || now - last >= 5) { publish(); seen = m; last = now; } if (getppid() == 1) return 0; usleep(400000); } }
/* ---- the GIT source (2026-11-16; design section 4): a public repo of approved packages at a PINNED commit. `store_op sync <house>` reads the STORE_SOURCE row of phone_prefs.pdl (STORE_SOURCE | git | <url> | <full 40-hex commit>),
 * fetches exactly that commit into <house>/&.hq-apps/plus-phone/store_cache/repo with git (fork/exec, no shell, no credentials, GIT_TERMINAL_PROMPT=0), then for every ENTRY of the pinned catalog.pdl
 * (ENTRY | id | KIND | phone-app | VERSION | n | SHA256 | tree hash) it checks the package folder: no symlink or special file, no ops/+x folder, no ELF binary (source only), the tree hash equals the catalog's. Result: store_cache/verified.txt
 * (OK|id|version|hash  or  REFUSED|id|reason): ONLY the OK ids are offered by the store. Tree hash = sha256 of the lines "<sha256 of file>  <relative path>\n" over every file in bytewise path order (the same as
 * find . -type f | sort | xargs sha256sum | sha256sum). Nothing is ever built or run here. ---- */
typedef struct { unsigned int h[8]; unsigned long long len; unsigned char buf[64]; int n; } Sha;
static const unsigned int K256[64] = { 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(Sha *s, const unsigned char *p) { unsigned int w[64], a, b, c, d, e, f, g, h; for (int i = 0; i < 16; i++) w[i] = (unsigned)p[4*i] << 24 | (unsigned)p[4*i+1] << 16 | (unsigned)p[4*i+2] << 8 | p[4*i+3];
    for (int i = 16; i < 64; i++) { unsigned s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3), s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10); w[i] = w[i-16] + s0 + w[i-7] + s1; }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; i++) { unsigned S1 = ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25), ch = (e & f) ^ (~e & g), t1 = h + S1 + ch + K256[i] + w[i], S0 = ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22), mj = (a & b) ^ (a & c) ^ (b & c), t2 = S0 + mj; h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2; }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h; }
static void sha_init(Sha *s) { static const unsigned int i0[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 }; memcpy(s->h, i0, sizeof i0); s->len = 0; s->n = 0; }
static void sha_add(Sha *s, const void *d, size_t l) { const unsigned char *p = d; s->len += l; while (l--) { s->buf[s->n++] = *p++; if (s->n == 64) { sha_block(s, s->buf); s->n = 0; } } }
static void sha_hex(Sha *s, char *out) { unsigned long long bits = s->len * 8; unsigned char pad = 0x80; sha_add(s, &pad, 1); pad = 0; while (s->n != 56) sha_add(s, &pad, 1); unsigned char lb[8]; for (int i = 0; i < 8; i++) lb[i] = (unsigned char)(bits >> (56 - 8 * i)); sha_add(s, lb, 8); for (int i = 0; i < 8; i++) snprintf(out + 8 * i, 9, "%08x", s->h[i]); }
static int file_sha(const char *path, char *hex, int *is_elf) { FILE *fp = fopen(path, "rb"); if (!fp) return 0; Sha s; sha_init(&s); unsigned char b[4096]; size_t n; int first = 1; *is_elf = 0; while ((n = fread(b, 1, sizeof b, fp)) > 0) { if (first && n >= 4 && b[0] == 0x7f && b[1] == 'E' && b[2] == 'L' && b[3] == 'F') *is_elf = 1; first = 0; sha_add(&s, b, n); } fclose(fp); sha_hex(&s, hex); return 1; }
#define MAXF 600
static char *TF[MAXF]; static int NTF;
static int walk(const char *root, const char *rel, int depth) { if (depth > 8) { snprintf(TREASON, sizeof TREASON, "too deep"); return 0; } char dir[P]; snprintf(dir, sizeof dir, "%s%s%s", root, rel[0] ? "/" : "", rel); DIR *dp = opendir(dir); if (!dp) { snprintf(TREASON, sizeof TREASON, "cannot read a folder"); return 0; } struct dirent *e; int ok = 1;
    while (ok && (e = readdir(dp))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; char r[P], full[P]; snprintf(r, sizeof r, "%s%s%s", rel, rel[0] ? "/" : "", e->d_name); snprintf(full, sizeof full, "%s/%s", root, r); struct stat sb; if (lstat(full, &sb)) { ok = 0; break; }
        if (S_ISLNK(sb.st_mode)) { snprintf(TREASON, sizeof TREASON, "symlink %s", r); ok = 0; } else if (S_ISDIR(sb.st_mode)) { if (!strcmp(e->d_name, "+x") || !strcmp(e->d_name, ".git")) { snprintf(TREASON, sizeof TREASON, "contains a %s folder (source only)", e->d_name); ok = 0; } else ok = walk(root, r, depth + 1); }
        else if (S_ISREG(sb.st_mode)) { if (NTF >= MAXF) { snprintf(TREASON, sizeof TREASON, "too many files"); ok = 0; } else TF[NTF++] = strdup(r); } else { snprintf(TREASON, sizeof TREASON, "special file %s", r); ok = 0; } } closedir(dp); return ok; }
static int cmp_s(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
/* tree hash of a package folder; 0 and TREASON when it is not acceptable (symlink, special file, +x folder, ELF) */
static int tree_hash(const char *root, char *hex) { NTF = 0; TREASON[0] = 0; int ok = walk(root, "", 0); Sha t; sha_init(&t); if (ok) { qsort(TF, NTF, sizeof TF[0], cmp_s);
        for (int i = 0; i < NTF && ok; i++) { char full[P], fh[65], line[P + 80]; int elf; snprintf(full, sizeof full, "%s/%s", root, TF[i]); if (!file_sha(full, fh, &elf)) { snprintf(TREASON, sizeof TREASON, "cannot read %s", TF[i]); ok = 0; } else if (elf) { snprintf(TREASON, sizeof TREASON, "a compiled binary: %s (source only)", TF[i]); ok = 0; } else { int l = snprintf(line, sizeof line, "%s  %s\n", fh, TF[i]); sha_add(&t, line, (size_t)l); } } }
    for (int i = 0; i < NTF; i++) free(TF[i]); NTF = 0; if (ok) sha_hex(&t, hex); return ok; }
static int store_pref_file(const char *file, const char *key, char f[4][300]) { char pf[P], ln[800]; snprintf(pf, sizeof pf, "%s/&.hq-apps/plus-phone/%s", HOUSE, file); FILE *fp = fopen(pf, "r"); int found = 0; while (fp && fgets(ln, sizeof ln, fp)) { if (strncmp(ln, key, strlen(key))) continue; ln[strcspn(ln, "\r\n")] = 0; char *p = ln; int n = 0; while (n < 4) { char *b = strchr(p, '|'); if (b) *b = 0; while (*p == ' ') p++; char *e = p + strlen(p); while (e > p && e[-1] == ' ') *--e = 0; snprintf(f[n++], 300, "%s", p); if (!b) break; p = b + 1; } if (n >= 4) found = 1; } if (fp) fclose(fp); return found; }
/* the user's own row wins; else the row shipped with the install (store_sources.default.pdl: the owner's approved-apps repo and the commit this release trusts) */
static int store_pref(const char *key, char f[4][300]) { return store_pref_file("phone_prefs.pdl", key, f) || store_pref_file("store_sources.default.pdl", key, f); }
static int hex40(const char *s) { if (strlen(s) != 40) return 0; for (; *s; s++) if (!isxdigit((unsigned char)*s)) return 0; return 1; }
static int url_ok(const char *u) { return !strncmp(u, "https://", 8) || !strncmp(u, "file:///", 8) || (u[0] == '/' && !strstr(u, "..")); }
static int cmd_sync(void) {
    char f[4][300]; if (!store_pref("STORE_SOURCE", f) || strcmp(f[1], "git")) { printf("SYNC-SKIPPED no STORE_SOURCE | git | <url> | <commit> row in phone_prefs.pdl\n"); return 1; }
    if (!url_ok(f[2])) { printf("SYNC-REFUSED the source url must be https:// (or a local path)\n"); return 1; } if (!hex40(f[3])) { printf("SYNC-REFUSED the pin must be a full 40-hex commit id (never a branch)\n"); return 1; }
    char cache[P], repo[P], o[P], git[] = "/usr/bin/git"; snprintf(cache, sizeof cache, "%s/&.hq-apps/plus-phone/store_cache", HOUSE); snprintf(repo, sizeof repo, "%s/repo", cache); mkdir(cache, 0755); mkdir(repo, 0755); snprintf(o, sizeof o, "%s/sync.out", cache); setenv("GIT_TERMINAL_PROMPT", "0", 1);
    { char *rm[] = { "/bin/rm", "-rf", repo, NULL }; run_out(rm, o); mkdir(repo, 0755); }   /* the cache is exactly ONE pinned commit: wiped before every fetch (runtime-file cap), so it never grows */
    { char *av[] = { git, "-C", repo, "init", "-q", NULL }; if (run_out(av, o) != 0) { printf("SYNC-FAILED git init\n"); return 1; } }
    { char *av[] = { git, "-C", repo, "-c", "protocol.file.allow=always", "fetch", "-q", "--depth", "1", f[2], f[3], NULL }; if (run_out(av, o) != 0) { printf("SYNC-FAILED could not fetch that commit from the source\n"); return 1; } }
    { char head[100] = ""; char *av[] = { git, "-C", repo, "rev-parse", "FETCH_HEAD", NULL }; if (run_out(av, o) != 0) { printf("SYNC-FAILED rev-parse\n"); return 1; } rd1(o, head, sizeof head); if (strcmp(head, f[3])) { printf("SYNC-REFUSED fetched %s, not the pinned commit\n", head); return 1; } }
    { char *av[] = { git, "-C", repo, "checkout", "-q", "-f", "--detach", "FETCH_HEAD", NULL }; if (run_out(av, o) != 0) { printf("SYNC-FAILED checkout\n"); return 1; } }
    char cf[P], vf[P], vt[P]; snprintf(cf, sizeof cf, "%s/catalog.pdl", repo); snprintf(vf, sizeof vf, "%s/verified.txt", cache); snprintf(vt, sizeof vt, "%s/verified.txt.tmp", cache); FILE *cp = fopen(cf, "r"); if (!cp) { printf("SYNC-REFUSED the pinned commit has no catalog.pdl\n"); return 1; } FILE *vw = fopen(vt, "w"); if (!vw) { fclose(cp); return 1; }
    char ln[800]; int ok = 0, bad = 0; while (fgets(ln, sizeof ln, cp)) { if (strncmp(ln, "ENTRY", 5)) continue; ln[strcspn(ln, "\r\n")] = 0; char *fld[10]; int n = 0; char *p = ln; while (n < 10) { fld[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; }
        for (int i = 0; i < n; i++) { while (*fld[i] == ' ') fld[i]++; char *e = fld[i] + strlen(fld[i]); while (e > fld[i] && e[-1] == ' ') *--e = 0; } if (n < 8) { fprintf(vw, "REFUSED|?|malformed ENTRY\n"); bad++; continue; }
        const char *id = fld[1], *kind = fld[3], *ver = fld[5], *want = fld[7]; if (!word(id) || sys_app(id)) { fprintf(vw, "REFUSED|%s|bad or system id\n", id); bad++; continue; } if (strcmp(kind, "phone-app")) { fprintf(vw, "REFUSED|%s|kind %s is not supported yet\n", id, kind); bad++; continue; }
        char pk[P], got[65]; snprintf(pk, sizeof pk, "%s/%s", repo, id); if (!tree_hash(pk, got)) { fprintf(vw, "REFUSED|%s|%s\n", id, TREASON[0] ? TREASON : "unreadable"); bad++; continue; } if (strcmp(got, want)) { fprintf(vw, "REFUSED|%s|hash mismatch (the files are not what was approved)\n", id); bad++; continue; } fprintf(vw, "OK|%s|%s|%s\n", id, ver, got); ok++; }
    fclose(cp); fclose(vw); rename(vt, vf); char cm[P]; snprintf(cm, sizeof cm, "%s/commit.txt", cache); wr(cm, f[3]); printf("SYNCED %s verified=%d refused=%d\n", f[3], ok, bad); return bad ? 1 : 0; }
int main(int argc, char **argv) {
    { const char *ki = getenv("KHTPM_ARG3"); if (argc >= 3 && argv[1][0] == '/') { if (argc >= 5 && !strcmp(argv[3], "--once")) return module_main(argv[4], 1); if (!ki || !ki[0]) { fprintf(stderr, "store_op: no KHTPM_ARG3 (open it through the phone)\n"); return 2; } return module_main(ki, 0); } }
    if (argc >= 4 && !strncmp(argv[1], "ui_", 3)) return ui_verb(argv[1], argv[2], argv[3]);
    if (argc >= 3 && !strcmp(argv[1], "sync")) { snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); return cmd_sync(); }
    if (argc >= 3 && !strcmp(argv[1], "treehash")) { char hx[65]; if (!tree_hash(argv[2], hx)) { printf("TREE-REFUSED %s\n", TREASON); return 1; } printf("%s\n", hx); return 0; }
    if (argc >= 3 && !strcmp(argv[1], "catalog")) { snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); scan(); char chk[P]; snprintf(chk, sizeof chk, "%s/&.hq-apps/plus-phone/ops/+x/app_check.+x", HOUSE);
        for (int i = 0; i < NE; i++) { char pkg[P], o[P], first[300] = "OK"; snprintf(pkg, sizeof pkg, GIT ? "%s/&.hq-apps/plus-phone/store_cache/repo/%s" : "%s/&.hq-apps/app-edit/projects/%s", HOUSE, E[i].id); snprintf(o, sizeof o, "/tmp/store_chk_%d.txt", (int)getpid()); char *av[] = { chk, pkg, HOUSE, NULL }; int rc = run_out(av, o);
            if (rc != 0) { FILE *fp = fopen(o, "r"); char l[300]; first[0] = 0; while (fp && fgets(l, sizeof l, fp)) if (!strncmp(l, "CHECK-FAIL", 10)) { l[strcspn(l, "\r\n")] = 0; snprintf(first, sizeof first, "%s", l); break; } if (fp) fclose(fp); if (!first[0]) snprintf(first, sizeof first, "refused"); } unlink(o);
            printf("%s|%s|%d|%s\n", E[i].id, E[i].title, E[i].inst, first); } return 0; }
    fprintf(stderr, "usage: store_op ui_install|ui_remove <id> <inst> | catalog <house> | <house> <pkg> (module)\n"); return 2;
}
