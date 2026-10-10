/* pet_world - the village's rules: where the trainer can walk, which door teleports, where the creatures are, who is next to whom.
 *
 *   pet_world init  <state_dir> <party_file>        create world.st from world_map.txt (trainer on P, one creature per N spot, in party order)
 *   pet_world walk  <state_dir> <up|down|left|right> <active_id>
 *        prints one line: `moved X Y` | `blocked` | `talk <id>` (a creature is on that tile) | `door home` (the player's door: teleport into the house) | `door locked` (another house)
 *   pet_world npcstep <state_dir> <active_id>       every creature may take one random step (the active pet follows the trainer instead)
 *   pet_world adjacent <state_dir> <active_id>      prints the ids of creatures on the 4 tiles around the trainer
 *   pet_world exit  <state_dir>                     put the trainer on the tile below the player's door (coming back out of the house)
 *   pet_world show  <state_dir>                     print world.st
 * Map legend (world_map.txt next to the app): . grass  , flowers  = path  T tree  W water  ^ roof  # wall  d door (locked)  D the player's door  P trainer start  N creature spot.
 * Walkable: . , = P N. A door is entered by walking into it. world.st keys: tx ty dir fx fy (the active pet follows one tile behind) n_npc npc_<i>_id/x/y.
 * Build: gcc -std=c11 -O2 -Wall -Wextra -D_DEFAULT_SOURCE -o ops/+x/pet_world.+x ops/pet_world.c */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MW 64
#define MH 64
#define MAXN 12
static char map[MH][MW + 2]; static int mw, mh;
static int tx, ty, fx, fy, n; static char dir[8] = "down";
static char nid[MAXN][32]; static int nx_[MAXN], ny_[MAXN];

static int load_map(const char *app) {      /* the town as built so far (state/town_all.txt: world_map.txt + the buildings the pets built) if it exists, else the shipped map */
    char p[PATH_MAX]; const char *sh = getenv("PET_SHARED"); FILE *f = NULL;
    if (sh && sh[0]) { snprintf(p, sizeof p, "%s/town_all.txt", sh); f = fopen(p, "r"); }
    if (!f) { snprintf(p, sizeof p, "%s/state/town_all.txt", app); f = fopen(p, "r"); }
    if (!f) { snprintf(p, sizeof p, "%s/world_map.txt", app); f = fopen(p, "r"); } if (!f) return -1;
    char l[256]; mh = 0; mw = 0;
    while (fgets(l, sizeof l, f) && mh < MH) { l[strcspn(l, "\r\n")] = 0; int k = (int)strlen(l); if (k > MW) k = MW; memcpy(map[mh], l, (size_t)k); map[mh][k] = 0; if (k > mw) mw = k; mh++; }
    fclose(f); return mh ? 0 : -1;
}
static char at(int x, int y) { return (x < 0 || y < 0 || y >= mh || x >= (int)strlen(map[y])) ? 'T' : map[y][x]; }
static int walkable(char c) { return c == '.' || c == ',' || c == '=' || c == 'P' || c == 'N' || c == 'f'; }
static int npc_at(int x, int y, const char *active) { for (int i = 0; i < n; i++) if (nx_[i] == x && ny_[i] == y && strcmp(nid[i], active)) return i; return -1; }
static void load_state(const char *sd) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/world.st", sd); FILE *f = fopen(p, "r"); n = 0; if (!f) return; char l[128];
    while (fgets(l, sizeof l, f)) {
        l[strcspn(l, "\r\n")] = 0; int i, v;
        if (!strncmp(l, "tx=", 3)) tx = atoi(l + 3); else if (!strncmp(l, "ty=", 3)) ty = atoi(l + 3); else if (!strncmp(l, "fx=", 3)) fx = atoi(l + 3); else if (!strncmp(l, "fy=", 3)) fy = atoi(l + 3);
        else if (!strncmp(l, "dir=", 4)) snprintf(dir, sizeof dir, "%s", l + 4); else if (!strncmp(l, "n_npc=", 6)) n = atoi(l + 6);
        else if (sscanf(l, "npc_%d_id=%31s", &i, nid[0]) == 2 && i >= 0 && i < MAXN) snprintf(nid[i], sizeof nid[i], "%s", strchr(l, '=') + 1);
        else if (sscanf(l, "npc_%d_x=%d", &i, &v) == 2 && i >= 0 && i < MAXN) nx_[i] = v; else if (sscanf(l, "npc_%d_y=%d", &i, &v) == 2 && i >= 0 && i < MAXN) ny_[i] = v;
    }
    fclose(f);
}
static void save_state(const char *sd) {
    char p[PATH_MAX], t[PATH_MAX]; snprintf(p, sizeof p, "%s/world.st", sd); snprintf(t, sizeof t, "%s.tmp", p); FILE *f = fopen(t, "w"); if (!f) return;
    fprintf(f, "tx=%d\nty=%d\nfx=%d\nfy=%d\ndir=%s\nn_npc=%d\n", tx, ty, fx, fy, dir, n);
    for (int i = 0; i < n; i++) fprintf(f, "npc_%d_id=%s\nnpc_%d_x=%d\nnpc_%d_y=%d\n", i, nid[i], i, nx_[i], i, ny_[i]);
    fclose(f); rename(t, p);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: pet_world init|walk|npcstep|adjacent|exit|show <state_dir> ...\n"); return 2; }
    char self[PATH_MAX]; if (!realpath(argv[0], self)) return 1; char app[PATH_MAX]; snprintf(app, sizeof app, "%s", self); for (int i = 0; i < 3; i++) { char *s = strrchr(app, '/'); if (s) *s = 0; }
    const char *cmd = argv[1], *sd = argv[2];
    if (load_map(app)) { fprintf(stderr, "pet_world: no world_map.txt in %s\n", app); return 1; }
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    if (!strcmp(cmd, "init")) {
        const char *pf = argc > 3 ? argv[3] : ""; char ids[MAXN][32]; int ni = 0; FILE *f = fopen(pf, "r");
        if (f) { char l[128]; while (fgets(l, sizeof l, f) && ni < MAXN) { char id[32]; if (sscanf(l, "%31s", id) == 1) snprintf(ids[ni++], 32, "%s", id); } fclose(f); }
        tx = ty = 1; n = 0; int k = 0;
        for (int y = 0; y < mh; y++) for (int x = 0; x < (int)strlen(map[y]); x++) { if (map[y][x] == 'P') { tx = x; ty = y; } else if (map[y][x] == 'N' && k < ni) { snprintf(nid[k], 32, "%s", ids[k]); nx_[k] = x; ny_[k] = y; k++; } }
        n = k; fx = tx; fy = ty + 1; snprintf(dir, sizeof dir, "down"); save_state(sd); printf("init %d creatures\n", n); return 0;
    }
    load_state(sd);
    if (!strcmp(cmd, "show")) { printf("tx=%d ty=%d dir=%s n=%d\n", tx, ty, dir, n); return 0; }
    const char *active = argc > 4 ? argv[4] : (argc > 3 ? argv[3] : "");
    if (!strcmp(cmd, "walk")) {
        const char *d = argc > 3 ? argv[3] : ""; active = argc > 4 ? argv[4] : "";
        int dx = 0, dy = 0; if (!strcmp(d, "up")) dy = -1; else if (!strcmp(d, "down")) dy = 1; else if (!strcmp(d, "left")) dx = -1; else if (!strcmp(d, "right")) dx = 1; else { puts("blocked"); return 0; }
        snprintf(dir, sizeof dir, "%s", d); int x = tx + dx, y = ty + dy; char c = at(x, y);
        if (c == 'D') { puts("door home"); save_state(sd); return 0; }
        if (c == 'd') { puts("door locked"); save_state(sd); return 0; }
        int i = npc_at(x, y, active); if (i >= 0) { printf("talk %s\n", nid[i]); save_state(sd); return 0; }
        if (!walkable(c) || (x == fx && y == fy && 0)) { puts("blocked"); save_state(sd); return 0; }
        fx = tx; fy = ty; tx = x; ty = y; save_state(sd); printf("moved %d %d\n", tx, ty); return 0;
    }
    if (!strcmp(cmd, "npcpos")) { for (int i = 0; i < n; i++) if (argc > 3 && !strcmp(nid[i], argv[3])) { printf("%d %d\n", nx_[i], ny_[i]); return 0; } puts("none"); return 1; }
    if (!strcmp(cmd, "nearest")) {      /* nearest <sd> <id> <chars> [exclude_file]: nearest INTERIOR tile (not the map edge) of one of the chars, by walking distance to a tile beside it; prints "x y dist" or "none". exclude_file rows "x y" are skipped. */
        int me = -1; for (int i = 0; i < n; i++) if (argc > 3 && !strcmp(nid[i], argv[3])) me = i; if (me < 0 || argc < 5) { puts("none"); return 1; }
        static int ex[256], ey[256]; int nex = 0; if (argc > 5) { FILE *ef = fopen(argv[5], "r"); int a, b; while (ef && nex < 256 && fscanf(ef, "%d %d", &a, &b) == 2) { ex[nex] = a; ey[nex] = b; nex++; } if (ef) fclose(ef); }
        static int dist[MH][MW]; for (int y = 0; y < mh; y++) for (int x = 0; x < MW; x++) dist[y][x] = -1;
        static int qx[MH * MW], qy[MH * MW]; int h = 0, tl = 0; qx[tl] = nx_[me]; qy[tl] = ny_[me]; tl++; dist[ny_[me]][nx_[me]] = 0;
        int bx = -1, by = -1, bd = 1 << 30; char bc = 0;
        while (h < tl) { int x = qx[h], y = qy[h]; h++; static const int ox[4] = { 0, 0, -1, 1 }, oy[4] = { -1, 1, 0, 0 };
            for (int k = 0; k < 4; k++) { int a = x + ox[k], b = y + oy[k]; if (a < 0 || b < 0 || b >= mh || a >= (int)strlen(map[b])) continue;
                if (a > 0 && b > 0 && b < mh - 1 && a < (int)strlen(map[b]) - 1 && strchr(argv[4], map[b][a])) { int skip = 0; for (int e = 0; e < nex; e++) if (ex[e] == a && ey[e] == b) skip = 1; if (!skip && dist[y][x] < bd) { bd = dist[y][x]; bx = a; by = b; bc = map[b][a]; } }
                if (dist[b][a] >= 0 || !walkable(map[b][a])) continue; dist[b][a] = dist[y][x] + 1; qx[tl] = a; qy[tl] = b; tl++; } }
        if (bx < 0) { puts("none"); return 1; } printf("%d %d %d %c\n", bx, by, bd, bc); return 0;
    }
    if (!strcmp(cmd, "npcgo")) {      /* npcgo <sd> <id> <x> <y>: one step along the shortest walk to a tile beside (x,y); prints arrived / moved X Y / stuck */
        int me = -1; for (int i = 0; i < n; i++) if (argc > 3 && !strcmp(nid[i], argv[3])) me = i; if (me < 0 || argc < 6) { puts("stuck"); return 1; }
        int gx = atoi(argv[4]), gy = atoi(argv[5]); static int pv[MH][MW][2]; static char seen[MH][MW]; memset(seen, 0, sizeof seen);
        static int qx[MH * MW], qy[MH * MW]; int h = 0, tl = 0; qx[tl] = nx_[me]; qy[tl] = ny_[me]; tl++; seen[ny_[me]][nx_[me]] = 1; int fx2 = -1, fy2 = -1;
        while (h < tl) { int x = qx[h], y = qy[h]; h++; if (abs(x - gx) + abs(y - gy) <= 1) { fx2 = x; fy2 = y; break; } static const int ox[4] = { 0, 0, -1, 1 }, oy[4] = { -1, 1, 0, 0 };
            for (int k = 0; k < 4; k++) { int a = x + ox[k], b = y + oy[k]; if (a < 0 || b < 0 || b >= mh || a >= (int)strlen(map[b]) || seen[b][a] || !walkable(map[b][a])) continue;
                if ((a == tx && b == ty) || npc_at(a, b, "") >= 0) continue; seen[b][a] = 1; pv[b][a][0] = x; pv[b][a][1] = y; qx[tl] = a; qy[tl] = b; tl++; } }
        if (fx2 < 0) { puts("stuck"); return 1; } if (fx2 == nx_[me] && fy2 == ny_[me]) { puts("arrived"); return 0; }
        while (!(pv[fy2][fx2][0] == nx_[me] && pv[fy2][fx2][1] == ny_[me])) { int px = pv[fy2][fx2][0], py = pv[fy2][fx2][1]; fx2 = px; fy2 = py; }
        nx_[me] = fx2; ny_[me] = fy2; save_state(sd); printf("moved %d %d\n", fx2, fy2); return 0;
    }
    if (!strcmp(cmd, "npcstep")) {
        active = argc > 3 ? argv[3] : "";
        for (int i = 0; i < n; i++) {
            if (!strcmp(nid[i], active) || rand() % 100 >= 35) continue;
            { char gp[PATH_MAX]; snprintf(gp, sizeof gp, "%s/pets/%s/ai_goal.txt", sd, nid[i]); if (access(gp, F_OK) == 0) continue; }      /* a pet with a goal walks to it (ai_step), not at random */
            int r = rand() % 4, dx = r == 0 ? 1 : r == 1 ? -1 : 0, dy = r == 2 ? 1 : r == 3 ? -1 : 0, x = nx_[i] + dx, y = ny_[i] + dy;
            if (!walkable(at(x, y)) || (x == tx && y == ty) || npc_at(x, y, active) >= 0) continue; nx_[i] = x; ny_[i] = y;
        }
        save_state(sd); return 0;
    }
    if (!strcmp(cmd, "adjacent")) {
        active = argc > 3 ? argv[3] : ""; static const int ox[4] = { 0, 0, -1, 1 }, oy[4] = { -1, 1, 0, 0 };
        for (int k = 0; k < 4; k++) { int i = npc_at(tx + ox[k], ty + oy[k], active); if (i >= 0) printf("%s\n", nid[i]); }
        return 0;
    }
    if (!strcmp(cmd, "exit")) {
        for (int y = 0; y < mh; y++) for (int x = 0; x < (int)strlen(map[y]); x++) if (map[y][x] == 'D') { tx = x; ty = y + 1; fx = x; fy = y + 2; }
        snprintf(dir, sizeof dir, "down"); save_state(sd); printf("moved %d %d\n", tx, ty); return 0;
    }
    return 2;
}
