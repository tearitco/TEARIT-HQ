/* crew_op — the first autonomous build FSM (2026-11-10).
 * It reads JOB rows from a bp_op projection and nothing else.
 * One pet. States, in order, copied from df-hq's dig loop
 * (pick, CLAIM, then do the work): idle, claim, place.
 * `walk` and `place` move one scratch pet on the job list (file order).
 * `step` and `run` still claim and place with no walk.
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
#include <unistd.h>

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

static int state_dir(int argc, char **argv, char *dir, size_t n) {
    if (argc >= 3) { snprintf(dir, n, "%s", argv[2]); return 0; }
    char exe[512]; ssize_t k = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (k < 0) return 2;
    exe[k] = 0;
    /* .../centroid-hq/ops/+x/crew_op.+x -> .../centroid-hq/state */
    char *s = strrchr(exe, '/'); if (!s) return 2; *s = 0;
    s = strrchr(exe, '/'); if (!s) return 2; *s = 0;
    s = strrchr(exe, '/'); if (!s) return 2; *s = 0;
    snprintf(dir, n, "%s/state", exe);
    return 0;
}

static int read_pos(const char *path, int *x, int *y) {
    *x = *y = 0; FILE *f = fopen(path, "r"); if (!f) return 0;
    if (fscanf(f, "%d %d", x, y) != 2) { fclose(f); return 2; }
    fclose(f); return 0;
}

/* Top-down plan of every job, one row of cells per z. Open cells are red.
 * Placed stone is gray, a door is brown, a roof is green. The pet is yellow.
 * The frame is the job box, so a building at 41,40 still fits. */
static void write_plan(const char *path, int px, int py) {
    if (!nj) return;
    int x0 = jobs[0].x, x1 = jobs[0].x, y0 = jobs[0].y, y1 = jobs[0].y, z0 = jobs[0].z, z1 = jobs[0].z;
    for (int i = 1; i < nj; i++) {
        if (jobs[i].x < x0) x0 = jobs[i].x; if (jobs[i].x > x1) x1 = jobs[i].x;
        if (jobs[i].y < y0) y0 = jobs[i].y; if (jobs[i].y > y1) y1 = jobs[i].y;
        if (jobs[i].z < z0) z0 = jobs[i].z; if (jobs[i].z > z1) z1 = jobs[i].z;
    }
    const int sc = 8;
    int gw = x1 - x0 + 1, gh = y1 - y0 + 1, gz = z1 - z0 + 1;
    FILE *f = fopen(path, "wb"); if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", gw * sc, gz * gh * sc);
    for (int pyx = 0; pyx < gz * gh * sc; pyx++) for (int pxx = 0; pxx < gw * sc; pxx++) {
        int cx = x0 + pxx / sc;
        int band = pyx / (gh * sc);          /* 0 is the highest z */
        int cy = y0 + (pyx % (gh * sc)) / sc;
        int z = z1 - band;
        unsigned char rgb[3] = { 20, 24, 28 };
        for (int i = 0; i < nj; i++) if (jobs[i].x == cx && jobs[i].y == cy && jobs[i].z == z) {
            if (!jobs[i].done) { rgb[0] = 210; rgb[1] = 60; rgb[2] = 50; }
            else if (!strcmp(jobs[i].mat, "door")) { rgb[0] = 140; rgb[1] = 90; rgb[2] = 40; }
            else if (!strcmp(jobs[i].mat, "roof")) { rgb[0] = 70; rgb[1] = 140; rgb[2] = 80; }
            else if (!strcmp(jobs[i].mat, "earth")) { rgb[0] = 120; rgb[1] = 85; rgb[2] = 50; }
            else { rgb[0] = 170; rgb[1] = 170; rgb[2] = 160; }
        }
        if (cx == px && cy == py) {
            int lx = pxx % sc, ly = (pyx % (gh * sc)) % sc;
            if (lx < 2 || ly < 2 || lx >= sc - 2 || ly >= sc - 2) { rgb[0] = 230; rgb[1] = 230; rgb[2] = 80; }
        }
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void write_ppm(const char *path, int px, int py, int jx, int jy) {
    FILE *f = fopen(path, "wb"); if (!f) return;
    const int sc = 8, n = 8; fprintf(f, "P6\n%d %d\n255\n", n * sc, n * sc);
    for (int y = 0; y < n * sc; y++) for (int x = 0; x < n * sc; x++) {
        int cx = x / sc, cy = y / sc; unsigned char rgb[3] = { 20, 24, 28 };
        if (cx == jx && cy == jy) { rgb[0] = 210; rgb[1] = 60; rgb[2] = 50; }
        if (cx == px && cy == py) { rgb[0] = 230; rgb[1] = 230; rgb[2] = 80; }
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* One step toward the next open job, then place only from beside it or on it. */
static int cmd_move(const char *which, int argc, char **argv) {
    char dir[512], jpath[560], life[560], pos[560], ppm[560];
    if (state_dir(argc, argv, dir, sizeof dir)) return 2;
    snprintf(jpath, sizeof jpath, "%s/jobs.pdl", dir);
    snprintf(life, sizeof life, "%s/life.txt", dir);
    snprintf(pos, sizeof pos, "%s/pet.txt", dir);
    snprintf(ppm, sizeof ppm, "%s/pet.ppm", dir);
    int rc = load_jobs(jpath); if (rc) return rc;
    int claim = -1; char fsm[16];
    rc = replay(life, &claim, fsm, sizeof fsm); if (rc) return rc;
    int px, py; if (read_pos(pos, &px, &py)) return 2;
    int next = -1; for (int i = 0; i < nj; i++) if (!jobs[i].done) { next = i; break; }
    if (next < 0) { printf("PET idle placed=%d\n", nj); return 0; }
    Job *j = &jobs[next];
    if (!strcmp(which, "walk")) {
        if (px < j->x) px++; else if (px > j->x) px--;
        else if (py < j->y) py++; else if (py > j->y) py--;
        FILE *pf = fopen(pos, "w"); if (!pf) return 2; fprintf(pf, "%d %d\n", px, py); fclose(pf);
        int tick = 0; FILE *lf = fopen(life, "r"); if (lf) { char b[80]; while (fgets(b, sizeof b, lf)) if (!strncmp(b, "EVENT|", 6)) tick++; fclose(lf); }
        char line[160]; snprintf(line, sizeof line, "EVENT|%d|p1|walk|%s|%d,%d,%d|ok", tick, j->mat, px, py, j->z);
        if (append(life, line)) return 2;
        write_ppm(ppm, px, py, j->x, j->y);
        printf("WALK %d,%d goal=%d,%d\n", px, py, j->x, j->y);
        return 0;
    }
    if (abs(px - j->x) + abs(py - j->y) > 1) { fprintf(stderr, "not beside the cell\n"); return 2; }
    int tick = 0; FILE *lf = fopen(life, "r"); if (lf) { char b[80]; while (fgets(b, sizeof b, lf)) if (!strncmp(b, "EVENT|", 6)) tick++; fclose(lf); }
    char line[160];
    snprintf(line, sizeof line, "EVENT|%d|p1|claim|%s|%d,%d,%d|ok", tick, j->mat, j->x, j->y, j->z);
    if (append(life, line)) return 2;
    snprintf(line, sizeof line, "EVENT|%d|p1|place|%s|%d,%d,%d|ok", tick + 1, j->mat, j->x, j->y, j->z);
    if (append(life, line)) return 2;
    write_ppm(ppm, px, py, j->x, j->y);
    printf("PLACE %d,%d,%d %s\n", j->x, j->y, j->z, j->mat);
    return 0;
}

/* Walk and place every open job. Same steps as walk and place, one building. */
static int cmd_raise(int argc, char **argv) {
    for (int n = 0; n < 20000; n++) {
        char dir[512], jpath[560], life[560], pos[560], plan[560];
        if (state_dir(argc, argv, dir, sizeof dir)) return 2;
        snprintf(jpath, sizeof jpath, "%s/jobs.pdl", dir);
        snprintf(life, sizeof life, "%s/life.txt", dir);
        snprintf(pos, sizeof pos, "%s/pet.txt", dir);
        snprintf(plan, sizeof plan, "%s/plan.ppm", dir);
        int rc = load_jobs(jpath); if (rc) return rc;
        int claim = -1; char fsm[16];
        rc = replay(life, &claim, fsm, sizeof fsm); if (rc) return rc;
        int px, py; if (read_pos(pos, &px, &py)) return 2;
        int next = -1; for (int i = 0; i < nj; i++) if (!jobs[i].done) { next = i; break; }
        if (next < 0) {
            write_plan(plan, px, py);
            printf("RAISE placed=%d\n", nj);
            return 0;
        }
        Job *j = &jobs[next];
        if (abs(px - j->x) + abs(py - j->y) > 1) {
            if (cmd_move("walk", argc, argv)) return 2;
        } else if (cmd_move("place", argc, argv)) return 2;
    }
    fprintf(stderr, "raise did not finish\n");
    return 2;
}

int main(int argc, char **argv) {
    if (argc >= 2 && (!strcmp(argv[1], "walk") || !strcmp(argv[1], "place"))) return cmd_move(argv[1], argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "raise")) return cmd_raise(argc, argv);
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
