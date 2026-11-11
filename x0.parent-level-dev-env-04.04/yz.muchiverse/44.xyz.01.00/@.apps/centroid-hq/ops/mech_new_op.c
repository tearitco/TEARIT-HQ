/* mech_new_op — one loop for the next skyland slice.
 *   mech_new_op <blueprint.pdl> <slice> <state-dir> <page-dir>
 * slice is a BUILD id (bridge) or under:<island>.
 * It runs the bp_op and crew_op that sit next to it (./bp_op and ./crew_op,
 * or the same directory as this binary). gen writes the jobs. raise places
 * them. The page map is a projection of those jobs, not a second plan.
 * Grass is '.'. A cell that was placed is 'W'. The world origin is written
 * in projection.txt. The life ledger in the state dir stays the authority.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define PW 32
#define PH 20
#define OX 8
#define OY 6

static int run(char *const av[]) {
    pid_t p = fork();
    if (p < 0) return -1;
    if (p == 0) { execv(av[0], av); _exit(127); }
    int st = 0;
    if (waitpid(p, &st, 0) < 0) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static void tool(char *dst, size_t n, const char *argv0, const char *name) {
    if (!access(name, X_OK)) { snprintf(dst, n, "%s", name); return; }
    snprintf(dst, n, "%s", argv0);
    char *sl = strrchr(dst, '/');
    if (sl) snprintf(sl + 1, n - (size_t)(sl + 1 - dst), "%s", name);
    else snprintf(dst, n, "%s", name);
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: mech_new_op <blueprint.pdl> <slice> <state-dir> <page-dir>\n");
        return 2;
    }
    char bp[512], crew[512], jobs[560], life[560];
    tool(bp, sizeof bp, argv[0], "bp_op");
    tool(crew, sizeof crew, argv[0], "crew_op");
    mkdir(argv[3], 0755);
    mkdir(argv[4], 0755);
    snprintf(jobs, sizeof jobs, "%s/jobs.pdl", argv[3]);
    snprintf(life, sizeof life, "%s/life.txt", argv[3]);
    unlink(life);
    char *gargv[] = { bp, "gen", argv[1], argv[2], jobs, NULL };
    int rc = run(gargv);
    if (rc != 0) { fprintf(stderr, "gen failed (%d)\n", rc); return 2; }
    char *rargv[] = { crew, "raise", argv[3], NULL };
    rc = run(rargv);
    if (rc != 0) { fprintf(stderr, "raise failed (%d)\n", rc); return 2; }

    int xy[400][2], nxy = 0, minx = 0, miny = 0, jobs_n = 0, have = 0;
    FILE *jf = fopen(jobs, "r");
    if (!jf) { fprintf(stderr, "no jobs\n"); return 2; }
    char line[200];
    while (fgets(line, sizeof line, jf)) {
        int x, y, z;
        if (sscanf(line, "JOB | place | %d | %d | %d", &x, &y, &z) != 3) continue;
        jobs_n++;
        if (!have || x < minx) minx = x;
        if (!have || y < miny) miny = y;
        have = 1;
        int seen = 0;
        for (int i = 0; i < nxy; i++) if (xy[i][0] == x && xy[i][1] == y) seen = 1;
        if (!seen && nxy < 400) { xy[nxy][0] = x; xy[nxy][1] = y; nxy++; }
    }
    fclose(jf);
    if (!have) { fprintf(stderr, "no place jobs\n"); return 2; }

    char map[PH][PW + 1];
    for (int r = 0; r < PH; r++) { memset(map[r], '.', PW); map[r][PW] = 0; }
    int clipped = 0;
    for (int i = 0; i < nxy; i++) {
        int c = OX + (xy[i][0] - minx);
        int r = OY + (xy[i][1] - miny);
        if (c < 0 || r < 0 || c >= PW || r >= PH) { clipped++; continue; }
        map[r][c] = 'W';
    }
    char mp[560];
    snprintf(mp, sizeof mp, "%s/map.txt", argv[4]);
    FILE *mf = fopen(mp, "w");
    if (!mf) { perror(mp); return 2; }
    for (int r = 0; r < PH; r++) fprintf(mf, "%s\n", map[r]);
    fclose(mf);
    char pr[560];
    snprintf(pr, sizeof pr, "%s/projection.txt", argv[4]);
    FILE *pf = fopen(pr, "w");
    if (!pf) { perror(pr); return 2; }
    fprintf(pf, "PROJECTION | %s | world %d,%d | page %d,%d | jobs %d | cells %d | clipped %d\n",
            argv[2], minx, miny, OX, OY, jobs_n, nxy, clipped);
    fprintf(pf, "kind | PROJECTION | regenerate_with | mech_new_op\n");
    fclose(pf);
    printf("MECH slice=%s jobs=%d cells=%d world=%d,%d page=%d,%d clipped=%d\n",
           argv[2], jobs_n, nxy, minx, miny, OX, OY, clipped);
    return clipped ? 2 : 0;
}
