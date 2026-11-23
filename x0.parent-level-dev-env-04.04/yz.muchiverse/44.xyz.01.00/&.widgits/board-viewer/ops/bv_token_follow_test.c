/* bv_token_follow_test (D118, 2026-11-22): the xelector FOLLOWS a possessed game-driven token. A desk token is a row of the shown desk's events.txt ("x y r g b <name> ..."), moved by the game's driver after the
 * keys the input op forwarded; before the fix the xelector (outline cube, HUD pos) stayed on the old cell (RTA joe/car 0,11 -> 3,11 left the cursor at 0,10,1). Scratch project in argv[1], the REAL engine
 * (env BV3D_BIN): xelector state names possessed_id=joe at 0,10, the token row says 3,11. Checks: after one engine frame the xelector state file AND the scene receipt say 3,11; the token moves again -> follows again;
 * possessed_id=none -> the xelector stays where it is (the token is not a magnet); an ENGINE piece of the same name is left to the piece rules. Exit 0 = all held, 1 = a check failed, 2 = setup/run problem. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
static char dir[600]; static int bad = 0;
static void wr(const char *rel, const char *txt) { char p[800]; snprintf(p, sizeof p, "%s/%s", dir, rel); FILE *f = fopen(p, "w"); if (!f) { perror(p); exit(2); } fputs(txt, f); fclose(f); }
static void md(const char *rel) { char p[800]; snprintf(p, sizeof p, "%s/%s", dir, rel); mkdir(p, 0755); }
static void mdp(const char *abs) { char c[900]; snprintf(c, sizeof c, "mkdir -p '%s'", abs); if (system(c)) exit(2); }
static void check(int ok, const char *what) { printf("%s: %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad = 1; }
static int kv(const char *rel, const char *key) { char p[800], l[200]; snprintf(p, sizeof p, "%s/%s", dir, rel); FILE *f = fopen(p, "r"); if (!f) return -9999; size_t kl = strlen(key); int v = -9999; while (fgets(l, sizeof l, f)) if (!strncmp(l, key, kl) && l[kl] == '=') v = atoi(l + kl + 1); fclose(f); return v; }
static void token(int x, int y) { char b[900], row[200]; snprintf(b, sizeof b, "%s/pieces/system/maps/m/d1", dir); mdp(b); snprintf(row, sizeof row, "%d %d 200 60 60 joe 0 0 0 0 0\n", x, y); char p[900]; snprintf(p, sizeof p, "%s/events.txt", b); FILE *f = fopen(p, "w"); if (!f) exit(2); fputs(row, f); fclose(f); }
static void setup(int xx, int xy, const char *poss) {
    md(""); md("pieces"); md("pieces/system"); md("pieces/display"); md("pieces/world_01"); md("pieces/xelector_01"); wr("pieces/world_01/state.txt", "desk_id=d1\nmap_id=m\n");
    char rows[800] = ""; for (int i = 0; i < 16; i++) strcat(rows, "........................................\n"); wr("pieces/system/board.txt", rows);
    char b[900]; snprintf(b, sizeof b, "focused_project_root=%s\nrender_mode=1\ncamera_mode=2\nselector_x=%d\nselector_y=%d\n", dir, xx, xy); wr("pieces/system/bv_state.txt", b);
    snprintf(b, sizeof b, "entity_type=xelector\npos_x=%d\npos_y=%d\npos_z=1\npossessed_id=%s\n", xx, xy, poss); wr("pieces/xelector_01/state.txt", b); }
static int frame(void) { const char *bin = getenv("BV3D_BIN"); if (!bin) bin = "ops/+x/bv_render_3d.+x"; char cmd[1900]; snprintf(cmd, sizeof cmd, "cd '%s' && timeout 60 '%s' >/dev/null 2>&1", dir, bin); return system(cmd) == 0 ? 0 : 2; }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]); return 2; } snprintf(dir, sizeof dir, "%s", argv[1]);
    setup(0, 10, "joe"); token(3, 11); if (frame()) { fprintf(stderr, "engine run failed\n"); return 2; }
    printf("after frame 1: xelector %d,%d receipt %d,%d\n", kv("pieces/xelector_01/state.txt", "pos_x"), kv("pieces/xelector_01/state.txt", "pos_y"), kv("pieces/display/scene_receipt.pdl", "xelector_x"), kv("pieces/display/scene_receipt.pdl", "xelector_y"));
    check(kv("pieces/xelector_01/state.txt", "pos_x") == 3 && kv("pieces/xelector_01/state.txt", "pos_y") == 11, "possessed token moved to 3,11: the xelector state follows it");
    check(kv("pieces/display/scene_receipt.pdl", "xelector_x") == 3 && kv("pieces/display/scene_receipt.pdl", "xelector_y") == 11, "the frame (HUD pos, outline cube) shows the xelector on 3,11");
    check(kv("pieces/system/bv_state.txt", "selector_x") == 3 && kv("pieces/system/bv_state.txt", "selector_y") == 11, "the HUD source (bv_state selector) follows too: pos 3,11");
    token(5, 11); if (frame()) return 2; check(kv("pieces/xelector_01/state.txt", "pos_x") == 5, "the token moves again to 5,11: the xelector follows again");
    setup(0, 10, "none"); token(3, 11); if (frame()) return 2; check(kv("pieces/xelector_01/state.txt", "pos_x") == 0 && kv("pieces/xelector_01/state.txt", "pos_y") == 10, "possessed_id=none: the token does not pull the xelector");
    setup(0, 10, "joe"); token(3, 11); md("pieces/joe"); wr("pieces/joe/state.txt", "pos_x=0\npos_y=10\npos_z=1\n"); if (frame()) return 2; check(kv("pieces/xelector_01/state.txt", "pos_x") == 0, "an ENGINE piece named joe is not a desk token: the piece rules own it");
    return bad; }
