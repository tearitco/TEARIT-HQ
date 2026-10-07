/* dsr_sim_query_op - READ-ONLY, independent checks over a DSR scratch game root (the harness cannot do arithmetic; this reads the FILES, never the tick op's own bookkeeping).
 *   dsr_sim_query_op <game_root> total              -> `total=<n> sink=<n> loans_out=<n> loans_owed=<n> day=<n>`
 *        total = sum(store cash + bank reserves + castle treasury + population cash) + world sink (the conserved quantity); loans_out = sum(bank loans_out);
 *        loans_owed = sum(store loan_balance)
 *   dsr_sim_query_op <game_root> money              -> just the conserved total (for an exact before/after comparison)
 *   dsr_sim_query_op <game_root> rows               -> `min=<n> max=<n> entities=<n>`  (count of `DAY|` rows in each entity's dsr_history.txt)
 *   dsr_sim_query_op <game_root> get <entity> <key> -> the integer value in the entity's variables.txt (or `dsr_world` for the world file)
 *   dsr_sim_query_op <game_root> distinct <entity> <col> -> number of distinct values in column <col> (0 = the literal DAY, 1 = day, 2.. = values) of its history
 *   dsr_sim_query_op <game_root> atleast <entity> <col> <n> -> `ok` if distinct >= n, else `no distinct=<k>`
 *   dsr_sim_query_op <game_root> below|above <entity> <key> <n> -> `ok` if the variable is < n (below) / > n (above), else `no value=<v>`
 *   dsr_sim_query_op <game_root> debtbalanced -> `ok` if sum(bank loans_out) == sum(store loan_balance), else `no out=<a> owed=<b>`
 *   dsr_sim_query_op <game_root> ledgerconst <col> -> `ok rows=<n>` if every `DAY|` row in world_ledger.txt has the same value in <col> as the START row (col 2 = total cash), else `drift row=<day>`
 *   dsr_sim_query_op <game_root> daysseq <entity> -> `ok` if the entity's history day column is exactly 1,2,3.. with no gap or repeat, else `bad at <line>`
 *   dsr_sim_query_op <game_root> positions -> one `name x y` line per entity (sorted), from desktop_pos.txt
 *   dsr_sim_query_op <game_root> nodupcells -> `ok` if no two entities share (x,y), else `overlap <a> <b>`
 * Exit 0 ok, 1 usage/missing. Self-contained. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define P 4096
static char G[P];

static int kv(const char *path, const char *key, long *out) {
    FILE *f = fopen(path, "r"); char ln[256]; size_t kl = strlen(key); if (!f) return -1;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { *out = atol(ln + kl + 1); fclose(f); return 0; }
    fclose(f); *out = 0; return 0;
}
static int cmpstr(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static int list(char names[][64], int max) {
    char path[P]; snprintf(path, sizeof path, "%s/pals", G);
    DIR *d = opendir(path); struct dirent *e; int n = 0; char *ptr[256];
    if (!d) return 0;
    static char tmp[256][64];
    while ((e = readdir(d)) && n < max && n < 256) if (!strncmp(e->d_name, "dsrtest_", 8)) { snprintf(tmp[n], 64, "%s", e->d_name); ptr[n] = tmp[n]; n++; }
    closedir(d); qsort(ptr, (size_t)n, sizeof(char *), cmpstr);
    for (int i = 0; i < n; i++) snprintf(names[i], 64, "%s", ptr[i]);
    return n;
}
static long col_val(const char *row, int col, char *txt, size_t tn) {   /* col 0 is the 'DAY' token */
    char b[512]; snprintf(b, sizeof b, "%s", row); b[strcspn(b, "\r\n")] = 0;
    char *save = NULL, *t = strtok_r(b, "|", &save);
    for (int i = 0; t && i < col; i++) t = strtok_r(NULL, "|", &save);
    if (!t) { txt[0] = 0; return -1; }
    snprintf(txt, tn, "%s", t); return atol(t);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: dsr_sim_query_op <game_root> <cmd> ...\n"); return 1; }
    snprintf(G, sizeof G, "%s", argv[1]); const char *cmd = argv[2];
    static char names[256][64]; int n = list(names, 256);
    char path[P + 128], ln[512];
    if (!strcmp(cmd, "total") || !strcmp(cmd, "money")) {
        long total = 0, lo = 0, lw = 0, v, sink = 0, day = 0;
        snprintf(path, sizeof path, "%s/dsr_world/variables.txt", G); kv(path, "sink", &sink); kv(path, "day", &day); total += sink;
        for (int i = 0; i < n; i++) {
            snprintf(path, sizeof path, "%s/pals/%s/variables.txt", G, names[i]);
            if (strstr(names[i], "_store_")) { kv(path, "cash", &v); total += v; kv(path, "loan_balance", &v); lw += v; }
            else if (strstr(names[i], "_population_")) { kv(path, "cash", &v); total += v; }
            else if (strstr(names[i], "_bank_")) { kv(path, "reserves", &v); total += v; kv(path, "loans_out", &v); lo += v; }
            else if (strstr(names[i], "_castle_")) { kv(path, "treasury", &v); total += v; }
        }
        if (!strcmp(cmd, "money")) printf("%ld\n", total);
        else printf("total=%ld sink=%ld loans_out=%ld loans_owed=%ld day=%ld\n", total, sink, lo, lw, day);
        return 0;
    }
    if (!strcmp(cmd, "debtbalanced")) {
        long lo = 0, lw = 0, v;
        for (int i = 0; i < n; i++) { snprintf(path, sizeof path, "%s/pals/%s/variables.txt", G, names[i]);
            if (strstr(names[i], "_store_")) { kv(path, "loan_balance", &v); lw += v; } else if (strstr(names[i], "_bank_")) { kv(path, "loans_out", &v); lo += v; } }
        if (lo == lw) printf("ok\n"); else printf("no out=%ld owed=%ld\n", lo, lw);
        return 0;
    }
    if (!strcmp(cmd, "rows")) {
        long mn = -1, mx = 0;
        for (int i = 0; i < n; i++) {
            long c = 0; snprintf(path, sizeof path, "%s/pals/%s/dsr_history.txt", G, names[i]);
            FILE *f = fopen(path, "r"); if (f) { while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "DAY|", 4)) c++; fclose(f); }
            if (mn < 0 || c < mn) mn = c; if (c > mx) mx = c;
        }
        printf("min=%ld max=%ld entities=%d\n", mn < 0 ? 0 : mn, mx, n); return 0;
    }
    if (!strcmp(cmd, "get") && argc >= 5) {
        long v; if (!strcmp(argv[3], "dsr_world")) snprintf(path, sizeof path, "%s/dsr_world/variables.txt", G); else snprintf(path, sizeof path, "%s/pals/%s/variables.txt", G, argv[3]);
        if (kv(path, argv[4], &v)) { fprintf(stderr, "missing %s\n", path); return 1; }
        printf("%ld\n", v); return 0;
    }
    if ((!strcmp(cmd, "distinct") && argc >= 5) || (!strcmp(cmd, "atleast") && argc >= 6)) {
        int col = atoi(argv[4]); static char seen[1024][32]; int ns = 0;
        snprintf(path, sizeof path, "%s/pals/%s/dsr_history.txt", G, argv[3]);
        FILE *f = fopen(path, "r"); if (!f) { fprintf(stderr, "missing %s\n", path); return 1; }
        while (fgets(ln, sizeof ln, f)) {
            if (strncmp(ln, "DAY|", 4)) continue;
            char t[32]; col_val(ln, col, t, sizeof t); int dup = 0;
            for (int i = 0; i < ns; i++) if (!strcmp(seen[i], t)) { dup = 1; break; }
            if (!dup && ns < 1024) snprintf(seen[ns++], 32, "%s", t);
        }
        fclose(f);
        if (!strcmp(cmd, "distinct")) printf("%d\n", ns);
        else if (ns >= atoi(argv[5])) printf("ok\n"); else printf("no distinct=%d\n", ns);
        return 0;
    }
    if ((!strcmp(cmd, "below") || !strcmp(cmd, "above")) && argc >= 6) {
        long v; if (!strcmp(argv[3], "dsr_world")) snprintf(path, sizeof path, "%s/dsr_world/variables.txt", G); else snprintf(path, sizeof path, "%s/pals/%s/variables.txt", G, argv[3]);
        if (kv(path, argv[4], &v)) { fprintf(stderr, "missing %s\n", path); return 1; }
        long n0 = atol(argv[5]); int okk = !strcmp(cmd, "below") ? v < n0 : v > n0;
        if (okk) printf("ok\n"); else printf("no value=%ld\n", v);
        return 0;
    }
    if (!strcmp(cmd, "ledgerconst") && argc >= 4) {
        int col = atoi(argv[3]); long start = -1, rows = 0; char t[32];
        snprintf(path, sizeof path, "%s/dsr_world/world_ledger.txt", G);
        FILE *f = fopen(path, "r"); if (!f) return 1;
        while (fgets(ln, sizeof ln, f)) {
            if (!strncmp(ln, "START|", 6)) start = col_val(ln, col, t, sizeof t);
            else if (!strncmp(ln, "DAY|", 4)) { rows++; long v = col_val(ln, col, t, sizeof t); if (v != start) { printf("drift row=%ld\n", rows); fclose(f); return 0; } }
        }
        fclose(f); printf("ok rows=%ld\n", rows); return 0;
    }
    if (!strcmp(cmd, "daysseq") && argc >= 4) {
        long expect = 1, line = 0; char t[32];
        snprintf(path, sizeof path, "%s/pals/%s/dsr_history.txt", G, argv[3]);
        FILE *f = fopen(path, "r"); if (!f) { printf("bad missing\n"); return 0; }
        while (fgets(ln, sizeof ln, f)) { line++; if (strncmp(ln, "DAY|", 4)) { printf("bad at %ld\n", line); fclose(f); return 0; }
            if (col_val(ln, 1, t, sizeof t) != expect) { printf("bad at %ld\n", line); fclose(f); return 0; } expect++; }
        fclose(f); printf("ok\n"); return 0;
    }
    if (!strcmp(cmd, "positions") || !strcmp(cmd, "nodupcells")) {
        static long X[256], Y[256];
        for (int i = 0; i < n; i++) {
            snprintf(path, sizeof path, "%s/pals/%s/desktop_pos.txt", G, names[i]); kv(path, "x", &X[i]); kv(path, "y", &Y[i]);
            if (!strcmp(cmd, "positions")) printf("%s %ld %ld\n", names[i], X[i], Y[i]);
        }
        if (!strcmp(cmd, "nodupcells")) {
            for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (X[i] == X[j] && Y[i] == Y[j]) { printf("overlap %s %s\n", names[i], names[j]); return 0; }
            printf("ok\n");
        }
        return 0;
    }
    fprintf(stderr, "unknown command\n"); return 1;
}
