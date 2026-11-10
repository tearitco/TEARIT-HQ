/* crew_op — the first autonomous build FSM (2026-11-10).
 * It reads JOB rows from a bp_op projection and nothing else.
 * One pet. States, in order, copied from df-hq's dig loop
 * (pick, CLAIM, then do the work): idle, claim, place.
 * There is no map walk yet, so "nearest" is file order.
 * A place that is not in the job list is refused.
 *
 *   crew_op step  <jobs.pdl> <ledger>
 *   crew_op run   <jobs.pdl> <ledger>
 *   crew_op audit <jobs.pdl> <ledger>
 *
 * df-hq's live tick still only digs DESIGNATE rows in jobs.txt
 * (2D, cap 64). `mineplan` writes those rows for the stone cells at
 * the lowest z, at most 64, so the existing dig loop can mine them.
 * It does not start a pet.
 *
 * Talking, trading, buying and building houses stay Teropoly's verbs
 * (`talk`, `offer`, `accept`, `buy`, `build` in deskopoly/verbs.pdl).
 * This op does not grow a second copy of them. Farming is the plant
 * job once a row says so. Mining is `mineplan` into the dig loop.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXJ 8000
typedef struct { int x, y, z; char mat[16]; int done; } Job;
static Job jobs[MAXJ];
static int nj;

static int load_jobs(const char *path) {
    FILE *f = fopen(path, "r"); char line[240];
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 2; }
    nj = 0;
    while (fgets(line, sizeof line, f)) {
        int x, y, z; char mat[16];
        if (sscanf(line, "JOB | place | %d | %d | %d | %15s", &x, &y, &z, mat) != 4) continue;
        if (nj >= MAXJ) { fprintf(stderr, "too many jobs\n"); fclose(f); return 2; }
        jobs[nj].x = x; jobs[nj].y = y; jobs[nj].z = z; snprintf(jobs[nj].mat, 16, "%s", mat); jobs[nj].done = 0; nj++;
    }
    fclose(f);
    if (!nj) { fprintf(stderr, "no JOB rows\n"); return 2; }
    return 0;
}

static int find_job(int x, int y, int z, const char *mat) {
    for (int i = 0; i < nj; i++)
        if (jobs[i].x == x && jobs[i].y == y && jobs[i].z == z && !strcmp(jobs[i].mat, mat)) return i;
    return -1;
}

/* Replay the life ledger. One grammar, the S1 event row:
 * EVENT|round|actor|verb|target|at|result
 * A world place uses target=material and at=x,y,z. Anything else is not a life row. */
static int replay(const char *path, int *open_claim, char *cfsm, size_t cfsm_n) {
    FILE *f = fopen(path, "r");
    *open_claim = -1; snprintf(cfsm, cfsm_n, "idle");
    if (!f) return 0;
    char line[240]; int ln = 0;
    while (fgets(line, sizeof line, f)) {
        ln++;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (!strncmp(line, "CLAIM ", 6) || !strncmp(line, "PLACE ", 6) || !strncmp(line, "CALL ", 5) || !strncmp(line, "FSM ", 4)) {
            fprintf(stderr, "line %d: not a life row\n", ln); fclose(f); return 2;
        }
        int round, x, y, z; char actor[16], verb[16], mat[16], result[16];
        if (sscanf(line, "EVENT|%d|%15[^|]|%15[^|]|%15[^|]|%d,%d,%d|%15s", &round, actor, verb, mat, &x, &y, &z, result) != 8) continue;
        (void)round; (void)actor;
        int k = find_job(x, y, z, mat);
        if (!strcmp(verb, "claim")) {
            if (k < 0) { fprintf(stderr, "line %d: claim is not a job\n", ln); fclose(f); return 2; }
            if (jobs[k].done) { fprintf(stderr, "line %d: claim after the cell was placed\n", ln); fclose(f); return 2; }
            *open_claim = k; snprintf(cfsm, cfsm_n, "claim");
        } else if (!strcmp(verb, "place")) {
            if (k < 0) { fprintf(stderr, "line %d: place is not a job\n", ln); fclose(f); return 2; }
            if (*open_claim != k) { fprintf(stderr, "line %d: place without that claim\n", ln); fclose(f); return 2; }
            jobs[k].done = 1; *open_claim = -1; snprintf(cfsm, cfsm_n, "place");
        }
    }
    fclose(f);
    return 0;
}

static int append(const char *path, const char *line) {
    FILE *f = fopen(path, "a"); if (!f) return 2;
    fputs(line, f); fputc('\n', f); fclose(f); return 0;
}

static int step(const char *jobs_path, const char *ledger) {
    int rc = load_jobs(jobs_path); if (rc) return rc;
    int claim = -1; char fsm[16]; int tick = 0;
    rc = replay(ledger, &claim, fsm, sizeof fsm); if (rc) return rc;
    FILE *f = fopen(ledger, "r"); if (f) { char b[80]; while (fgets(b, sizeof b, f)) if (!strncmp(b, "EVENT|", 6)) tick++; fclose(f); }
    int left = 0; for (int i = 0; i < nj; i++) if (!jobs[i].done) left++;
    char line[160];
    if (claim >= 0) {
        Job *j = &jobs[claim];
        snprintf(line, sizeof line, "EVENT|%d|p1|place|%s|%d,%d,%d|ok", tick, j->mat, j->x, j->y, j->z);
        if (append(ledger, line)) return 2;
        printf("CREW fsm=place cell=%d,%d,%d %s left=%d\n", j->x, j->y, j->z, j->mat, left - 1);
        return 0;
    }
    int next = -1; for (int i = 0; i < nj; i++) if (!jobs[i].done) { next = i; break; }
    if (next < 0) { printf("CREW fsm=idle placed=%d left=0\n", nj); return 0; }
    Job *j = &jobs[next];
    snprintf(line, sizeof line, "EVENT|%d|p1|claim|%s|%d,%d,%d|ok", tick, j->mat, j->x, j->y, j->z);
    if (append(ledger, line)) return 2;
    printf("CREW fsm=claim cell=%d,%d,%d %s left=%d\n", j->x, j->y, j->z, j->mat, left);
    return 0;
}

#define MAXR 16
typedef struct { char life[16], kind[12], app[24], verb[16], check[24]; } Route;
static Route routes[MAXR];
static int nr;

static int load_routes(const char *path) {
    FILE *f = fopen(path, "r"); char line[240];
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 2; }
    nr = 0;
    while (fgets(line, sizeof line, f)) {
        char life[16], kind[12], app[24], verb[16], check[24];
        if (sscanf(line, "ROUTE | %15s | %11s | %23s | %15s | %23s", life, kind, app, verb, check) != 5) continue;
        if (nr >= MAXR) { fclose(f); fprintf(stderr, "too many routes\n"); return 2; }
        snprintf(routes[nr].life, 16, "%s", life); snprintf(routes[nr].kind, 12, "%s", kind);
        snprintf(routes[nr].app, 24, "%s", app); snprintf(routes[nr].verb, 16, "%s", verb);
        snprintf(routes[nr].check, 24, "%s", check); nr++;
    }
    fclose(f);
    if (!nr) { fprintf(stderr, "no ROUTE rows\n"); return 2; }
    return 0;
}

static Route *find_route(const char *life) {
    for (int i = 0; i < nr; i++) if (!strcmp(routes[i].life, life)) return &routes[i];
    return NULL;
}

static int file_has_verb(const char *path, const char *verb) {
    FILE *f = fopen(path, "r"); char line[240], needle[40];
    if (!f) return 0;
    snprintf(needle, sizeof needle, "VERB | %s", verb);
    int ok = 0;
    while (fgets(line, sizeof line, f)) if (strstr(line, needle)) { ok = 1; break; }
    fclose(f);
    return ok;
}

/* bind: the route table must name a verb the owner file already has. OPEN stays open. */
static int cmd_bind(const char *routes_path, const char *house) {
    int rc = load_routes(routes_path); if (rc) return rc;
    for (int i = 0; i < nr; i++) {
        if (strcmp(routes[i].kind, "world") && strcmp(routes[i].kind, "table")) {
            fprintf(stderr, "BIND %s kind is world or table\n", routes[i].life); return 2;
        }
        if (!strcmp(routes[i].kind, "table") && (!strcmp(routes[i].app, "crew") || !strcmp(routes[i].app, "df-hq") || !strcmp(routes[i].app, "OPEN"))) {
            fprintf(stderr, "table verb cannot be owned by the crew\n"); return 2;
        }
        if (!strcmp(routes[i].kind, "world") && !strcmp(routes[i].app, "deskopoly")) {
            fprintf(stderr, "world verb cannot be owned by the table\n"); return 2;
        }
        if (!strcmp(routes[i].app, "OPEN") && strcmp(routes[i].kind, "world")) {
            fprintf(stderr, "OPEN is only for a world verb\n"); return 2;
        }
        printf("KIND %s=%s\n", routes[i].life, routes[i].kind);
        if (!strcmp(routes[i].app, "OPEN")) { printf("BIND %s=OPEN\n", routes[i].life); continue; }
        if (!strcmp(routes[i].check, "-")) { printf("BIND %s=%s %s\n", routes[i].life, routes[i].app, routes[i].verb); continue; }
        char path[512];
        snprintf(path, sizeof path, "%s/@.apps/%s/%s", house, routes[i].app, routes[i].check);
        if (!file_has_verb(path, routes[i].verb)) {
            fprintf(stderr, "BIND %s missing VERB %s in %s\n", routes[i].life, routes[i].verb, path);
            return 2;
        }
        printf("BIND %s=%s %s\n", routes[i].life, routes[i].app, routes[i].verb);
    }
    return 0;
}

/* One social or physical job that is not a voxel place. The rule stays in the owner app. */
static int cmd_call(const char *routes_path, const char *jobs_path, const char *ledger) {
    int rc = load_routes(routes_path); if (rc) return rc;
    FILE *f = fopen(jobs_path, "r"); char line[240];
    if (!f) { fprintf(stderr, "cannot read jobs\n"); return 2; }
    char life[16] = "", rest[64] = ""; int got = 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "JOB | %15s | %*d | %*d | %*d | %63s", life, rest) == 2) { got = 1; break; }
    }
    fclose(f);
    if (!got) { fprintf(stderr, "no JOB row\n"); return 2; }
    Route *r = find_route(life);
    if (!r) { fprintf(stderr, "no route for %s\n", life); return 2; }
    if (!strcmp(r->app, "OPEN")) { fprintf(stderr, "%s has no owner yet\n", life); return 2; }
    if (!strcmp(life, "place")) { fprintf(stderr, "place stays on crew_op run\n"); return 2; }
    if (!strcmp(r->kind, "table")) {
        /* The trade stays in the table's own ledger. This row only records the handoff. */
        char out[160];
        snprintf(out, sizeof out, "EVENT|0|p1|%s|%s|-|handed-off", r->verb, rest);
        if (append(ledger, out)) return 2;
        printf("LIFE %s handed-off\n", r->verb);
        return 0;
    }
    char out[160];
    snprintf(out, sizeof out, "EVENT|0|p1|%s|%s|-|ok", r->verb, rest);
    if (append(ledger, out)) return 2;
    printf("LIFE %s %s\n", r->kind, r->verb);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "bind")) return cmd_bind(argv[2], argv[3]);
    if (argc == 5 && !strcmp(argv[1], "call")) return cmd_call(argv[2], argv[3], argv[4]);
    if (argc != 4) { fprintf(stderr, "usage: crew_op step|run|audit|mineplan <jobs> <out> | bind <routes> <house> | call <routes> <jobs> <ledger>\n"); return 2; }
    if (!strcmp(argv[1], "step")) return step(argv[2], argv[3]);
    if (!strcmp(argv[1], "run")) {
        for (int n = 0; n < MAXJ * 2 + 2; n++) {
            int rc = step(argv[2], argv[3]); if (rc) return rc;
            int claim = -1; char fsm[16];
            if (load_jobs(argv[2])) return 2;
            if (replay(argv[3], &claim, fsm, sizeof fsm)) return 2;
            int left = 0; for (int i = 0; i < nj; i++) if (!jobs[i].done) left++;
            if (!left && claim < 0) { printf("CREW done placed=%d\n", nj); return 0; }
        }
        fprintf(stderr, "run did not finish\n"); return 2;
    }
    if (!strcmp(argv[1], "mineplan")) {
        int rc = load_jobs(argv[2]); if (rc) return rc;
        int z0 = jobs[0].z; for (int i = 1; i < nj; i++) if (jobs[i].z < z0) z0 = jobs[i].z;
        FILE *o = fopen(argv[3], "w"); if (!o) return 2;
        int wrote = 0;
        for (int i = 0; i < nj && wrote < 64; i++) {
            if (jobs[i].z != z0 || strcmp(jobs[i].mat, "stone")) continue;
            fprintf(o, "DESIGNATE|0|chapel|%d|%d\n", jobs[i].x, jobs[i].y);
            wrote++;
        }
        fclose(o);
        printf("MINEPLAN wrote=%d cap=64 z=%d\n", wrote, z0);
        return wrote ? 0 : 2;
    }
    if (!strcmp(argv[1], "audit")) {
        int claim = -1; char fsm[16];
        int rc = load_jobs(argv[2]); if (rc) return rc;
        rc = replay(argv[3], &claim, fsm, sizeof fsm); if (rc) return rc;
        int done = 0; for (int i = 0; i < nj; i++) done += jobs[i].done;
        if (claim >= 0) { fprintf(stderr, "open claim left\n"); return 2; }
        if (done != nj) { fprintf(stderr, "placed %d of %d\n", done, nj); return 2; }
        printf("AUDIT placed=%d fsm=%s\n", done, fsm);
        return 0;
    }
    fprintf(stderr, "usage: crew_op step|run|audit <jobs.pdl> <ledger>\n");
    return 2;
}
