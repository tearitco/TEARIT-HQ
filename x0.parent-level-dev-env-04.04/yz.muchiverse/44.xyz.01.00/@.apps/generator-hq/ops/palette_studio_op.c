/* palette_studio_op: one front door for the generators (D33 step 3). Pick a TEMPLATE (a sheet type), turn KNOBS for the whole sheet or a SELECTION, save the result as a "sheet" or a "crop"
 * together with a reproducing .studio.pdl, replay that file later and prove the bytes are the same.
 *   templates <templates.pdl>                                          list the templates, their kind and their knobs
 *   render    <templates.pdl> <id> <out.png> [knob=name:delta]... [select=a,b,c,d] [crop=a,b,c,d]
 *   save      <templates.pdl> <id> <outdir> <name> sheet|crop [knob=..]... [select=..] [crop=..]   writes <name>.png + <name>.studio.pdl (crop mode needs crop=, sheet mode refuses it)
 *   replay    <name.studio.pdl> <out.png>                              re-renders from the recorded rows; REPLAY ok / REPLAY mismatch (exit 1)
 * The studio never draws anything itself: it starts the real op (generator_voxel_op objsheet for kind objects, generator_ground_op for kind ground) with fork + exec, so the knobs, the selection
 * and the Concept Bank weights stay owned by those ops. Rows ('|' separated, '#' comments), paths are house relative, or start with "root:" for the repository root:
 *   TEMPLATE | id | kind objects|ground | label | knobs (comma list) | file1 | file2 ...      objects: cells.pdl, recipes.pdl...     ground: ground.pdl, source sheet.png */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#define MAXT 32
#define MAXF 8
#define MAXA 40
typedef struct { char id[40], kind[16], label[100], knobs[80], file[MAXF][500]; int nf; } Template;
static Template T[MAXT]; static int nt;
static char g_house[2048]; /* well under PATH_MAX so a house path plus any suffix below always fits */
static char *fld(char **p) {
    char *s = *p, *e; while (*s == ' ') s++;
    e = strchr(s, '|'); if (e) { *e = 0; *p = e + 1; } else { e = s + strlen(s); *p = e; }
    while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
    return s;
}
static int find_house(void) { /* .../<house>/@.apps/generator-hq/ops/+x/palette_studio_op.+x : five components up */
    char buf[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1); int i; if (n >= (ssize_t)sizeof g_house) return 2;
    if (n <= 0) return 2;
    buf[n] = 0;
    for (i = 0; i < 5; i++) { char *s = strrchr(buf, '/'); if (!s) return 2; *s = 0; }
    snprintf(g_house, sizeof g_house, "%.2040s", buf); return 0;
}
static void resolve(const char *rel, char *out, size_t cap) { /* house relative, root: = repository root (three levels above the house) */
    int n = !strncmp(rel, "root:", 5) ? snprintf(out, cap, "%s/../../../%s", g_house, rel + 5) : snprintf(out, cap, "%s/%s", g_house, rel);
    if (n < 0 || (size_t)n >= cap) out[0] = 0; /* too long: an empty path, the op then reports it cannot open it */
}
static int read_templates(const char *path) {
    FILE *f = fopen(path, "r"); char line[1200];
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *p = line, *k, *a; Template *t;
        if (line[0] == '#' || line[0] == '\n') continue;
        k = fld(&p); if (strcmp(k, "TEMPLATE")) continue;
        if (nt >= MAXT) { fprintf(stderr, "too many templates\n"); fclose(f); return 2; }
        t = &T[nt]; memset(t, 0, sizeof *t);
        snprintf(t->id, sizeof t->id, "%s", fld(&p)); snprintf(t->kind, sizeof t->kind, "%s", fld(&p)); snprintf(t->label, sizeof t->label, "%s", fld(&p)); snprintf(t->knobs, sizeof t->knobs, "%s", fld(&p));
        while (*p && t->nf < MAXF) { a = fld(&p); if (a[0]) snprintf(t->file[t->nf++], sizeof t->file[0], "%s", a); }
        if ((strcmp(t->kind, "objects") && strcmp(t->kind, "ground")) || (!strcmp(t->kind, "objects") && t->nf < 2) || (!strcmp(t->kind, "ground") && t->nf != 2)) { fprintf(stderr, "bad TEMPLATE %s\n", t->id); fclose(f); return 2; }
        nt++;
    }
    fclose(f); return 0;
}
static Template *find_t(const char *id) { int i; for (i = 0; i < nt; i++) if (!strcmp(T[i].id, id)) return &T[i]; return NULL; }
static int run_argv(char **av) { /* fork + exec, wait; the child's own output passes through */
    pid_t pid = fork(); int st;
    if (pid < 0) return 2;
    if (pid == 0) { execv(av[0], av); _exit(127); }
    if (waitpid(pid, &st, 0) < 0) return 2;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 2;
}
/* start the real op for template t; opts = knob=/select=/crop= strings */
static int render_t(const Template *t, const char *out, char **opts, int no) {
    char *av[MAXA + MAXF + 12]; char buf[MAXF + 4][PATH_MAX]; int n = 0, i, b = 0; char bank[PATH_MAX];
    snprintf(bank, sizeof bank, "bank=%s/&.widgits/concept-bank", g_house);
    if (!strcmp(t->kind, "objects")) {
        snprintf(buf[b], PATH_MAX, "%s/@.apps/generator-hq/ops/+x/generator_voxel_op.+x", g_house); av[n++] = buf[b++]; av[n++] = "objsheet"; av[n++] = (char *)out;
        for (i = 0; i < t->nf; i++) { resolve(t->file[i], buf[b], PATH_MAX); av[n++] = buf[b++]; }
    } else {
        snprintf(buf[b], PATH_MAX, "%s/@.apps/generator-hq/ops/+x/generator_ground_op.+x", g_house); av[n++] = buf[b++]; av[n++] = "ground";
        for (i = 0; i < 2; i++) { resolve(t->file[i], buf[b], PATH_MAX); av[n++] = buf[b++]; }
        av[n++] = (char *)out;
    }
    av[n++] = bank;
    for (i = 0; i < no && n < MAXA + MAXF + 10; i++) av[n++] = opts[i];
    av[n] = NULL;
    return run_argv(av);
}
/* ---- sha256 (the proof that a replay is the same file) ---- */
typedef struct { unsigned h[8]; unsigned long long len; unsigned char buf[64]; int fill; } Sha;
static const unsigned K[64] = {0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(Sha *s, const unsigned char *p) {
    unsigned w[64], a, b, c, d, e, f, g, h, i;
    for (i = 0; i < 16; i++) w[i] = (unsigned)p[i * 4] << 24 | (unsigned)p[i * 4 + 1] << 16 | (unsigned)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (i = 16; i < 64; i++) { unsigned s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10); w[i] = w[i - 16] + s0 + w[i - 7] + s1; }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        unsigned t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i], t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static int sha_file(const char *path, char *hex, long *size) {
    FILE *f = fopen(path, "rb"); Sha s; unsigned char chunk[4096]; size_t n; int i; unsigned long long bits;
    if (!f) return 1;
    s.h[0] = 0x6a09e667; s.h[1] = 0xbb67ae85; s.h[2] = 0x3c6ef372; s.h[3] = 0xa54ff53a; s.h[4] = 0x510e527f; s.h[5] = 0x9b05688c; s.h[6] = 0x1f83d9ab; s.h[7] = 0x5be0cd19; s.len = 0; s.fill = 0;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) { size_t j; s.len += n; for (j = 0; j < n; j++) { s.buf[s.fill++] = chunk[j]; if (s.fill == 64) { sha_block(&s, s.buf); s.fill = 0; } } }
    fclose(f); bits = s.len * 8; *size = (long)s.len;
    s.buf[s.fill++] = 0x80; if (s.fill > 56) { while (s.fill < 64) s.buf[s.fill++] = 0; sha_block(&s, s.buf); s.fill = 0; }
    while (s.fill < 56) s.buf[s.fill++] = 0;
    for (i = 0; i < 8; i++) s.buf[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
    sha_block(&s, s.buf);
    for (i = 0; i < 8; i++) snprintf(hex + i * 8, 9, "%08x", s.h[i]);
    return 0;
}
static int is_opt(const char *a) { return !strncmp(a, "knob=", 5) || !strncmp(a, "select=", 7) || !strncmp(a, "crop=", 5); }
static int cmd_render(int argc, char **argv) { /* render <templates> <id> <out> [opts] */
    Template *t; if (argc < 5) return 2;
    if (read_templates(argv[2])) return 2;
    t = find_t(argv[3]); if (!t) { fprintf(stderr, "unknown template %s\n", argv[3]); return 2; }
    return render_t(t, argv[4], argv + 5, argc - 5);
}
static int cmd_save(int argc, char **argv) { /* save <templates> <id> <outdir> <name> sheet|crop [opts] */
    Template *t; char png[PATH_MAX], rep[PATH_MAX], hex[65], rel[PATH_MAX]; long size; int i, has_crop = 0, rc; FILE *f; char *tp;
    if (argc < 7) return 2;
    if (read_templates(argv[2])) return 2;
    t = find_t(argv[3]); if (!t) { fprintf(stderr, "unknown template %s\n", argv[3]); return 2; }
    for (i = 7; i < argc; i++) if (!strncmp(argv[i], "crop=", 5)) has_crop = 1;
    if (strcmp(argv[6], "sheet") && strcmp(argv[6], "crop")) { fprintf(stderr, "mode must be sheet or crop\n"); return 2; }
    if (!strcmp(argv[6], "crop") && !has_crop) { fprintf(stderr, "crop mode needs crop=a,b,c,d\n"); return 2; }
    if (!strcmp(argv[6], "sheet") && has_crop) { fprintf(stderr, "sheet mode refuses crop= (use crop mode)\n"); return 2; }
    snprintf(png, sizeof png, "%s/%s.png", argv[4], argv[5]); snprintf(rep, sizeof rep, "%s/%s.studio.pdl", argv[4], argv[5]);
    rc = render_t(t, png, argv + 7, argc - 7); if (rc) return rc;
    if (sha_file(png, hex, &size)) return 2;
    f = fopen(rep, "w"); if (!f) { fprintf(stderr, "cannot write %s\n", rep); return 2; }
    tp = argv[2]; if (!strncmp(tp, g_house, strlen(g_house)) && tp[strlen(g_house)] == '/') tp += strlen(g_house) + 1; snprintf(rel, sizeof rel, "%s", tp);
    fprintf(f, "# palette-studio save (D33): replay with  palette_studio_op replay <this file> <out.png>\nTEMPLATES | %s\nTEMPLATE | %s\nMODE | %s\n", rel, t->id, argv[6]);
    for (i = 7; i < argc; i++) fprintf(f, "OPT | %s\n", argv[i]);
    fprintf(f, "RESULT | bytes=%ld | sha256=%s\n", size, hex); fclose(f);
    printf("STUDIO saved %s mode=%s bytes=%ld sha256=%s\n", argv[5], argv[6], size, hex); return 0;
}
static int cmd_replay(int argc, char **argv) { /* replay <repro> <out> */
    FILE *f; char line[1200], tpl[PATH_MAX] = "", id[40] = "", want[65] = "", hex[65], tpath[PATH_MAX]; char *opts[MAXA]; static char store[MAXA][200]; int no = 0; long size; Template *t; int rc;
    if (argc != 4) return 2;
    f = fopen(argv[2], "r"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *p = line, *k; if (line[0] == '#' || line[0] == '\n') continue; k = fld(&p);
        if (!strcmp(k, "TEMPLATES")) snprintf(tpl, sizeof tpl, "%s", fld(&p));
        else if (!strcmp(k, "TEMPLATE")) snprintf(id, sizeof id, "%s", fld(&p));
        else if (!strcmp(k, "OPT") && no < MAXA) { snprintf(store[no], sizeof store[no], "%s", fld(&p)); opts[no] = store[no]; no++; }
        else if (!strcmp(k, "RESULT")) { char *a; fld(&p); a = fld(&p); if (!strncmp(a, "sha256=", 7)) snprintf(want, sizeof want, "%s", a + 7); }
    }
    fclose(f);
    if (!tpl[0] || !id[0] || strlen(want) != 64) { fprintf(stderr, "bad repro file\n"); return 2; }
    resolve(tpl, tpath, sizeof tpath); if (read_templates(tpath)) return 2;
    t = find_t(id); if (!t) { fprintf(stderr, "unknown template %s\n", id); return 2; }
    rc = render_t(t, argv[3], opts, no); if (rc) return rc;
    if (sha_file(argv[3], hex, &size)) return 2;
    if (strcmp(hex, want)) { printf("REPLAY mismatch want=%s got=%s\n", want, hex); return 1; }
    printf("REPLAY ok bytes=%ld sha256=%s\n", size, hex); return 0;
}
static int cmd_list(const char *path) {
    int i, j;
    if (read_templates(path)) return 2;
    for (i = 0; i < nt; i++) { printf("TEMPLATE %s kind=%s knobs=%s | %s |", T[i].id, T[i].kind, T[i].knobs, T[i].label); for (j = 0; j < T[i].nf; j++) printf(" %s", T[i].file[j]); printf("\n"); }
    printf("TEMPLATES count=%d\n", nt); return 0;
}
int main(int argc, char **argv) {
    if (find_house()) { fprintf(stderr, "cannot find the house root\n"); return 2; }
    if (argc >= 3 && !strcmp(argv[1], "templates")) return cmd_list(argv[2]);
    if (argc >= 5 && !strcmp(argv[1], "render")) { int i; for (i = 5; i < argc; i++) if (!is_opt(argv[i])) { fprintf(stderr, "bad option %s\n", argv[i]); return 2; } return cmd_render(argc, argv); }
    if (argc >= 7 && !strcmp(argv[1], "save")) { int i; for (i = 7; i < argc; i++) if (!is_opt(argv[i])) { fprintf(stderr, "bad option %s\n", argv[i]); return 2; } return cmd_save(argc, argv); }
    if (argc == 4 && !strcmp(argv[1], "replay")) return cmd_replay(argc, argv);
    fprintf(stderr, "usage: templates <templates.pdl> | render <templates.pdl> <id> <out.png> [opts] | save <templates.pdl> <id> <outdir> <name> sheet|crop [opts] | replay <name.studio.pdl> <out.png>\n");
    return 2;
}
