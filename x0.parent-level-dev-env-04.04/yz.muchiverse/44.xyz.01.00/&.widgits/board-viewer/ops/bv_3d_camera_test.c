/* bv_3d_camera_test - the 3D free camera (owner 2026-10-27: "arrows move the xelector, not the camera ... same for 3D"). Scratch project in argv[1]: a plain 40 x 8 board, a xelector, bv_state with render_mode=1 camera_mode=3. The arrows go through the REAL input op
 * (env BV2D_INPUT_BIN) and the frame/camera through the REAL engine (env BV3D_BIN, default ops/+x/bv_render_3d.+x): the camera position is read from pieces/display/scene_receipt.pdl (cam_eye_x / cam_eye_z), the xelector's visibility from the pixel
 * difference of the frame with and without the xelector file. Checks: arrows leave the camera alone while the xelector stays in view, the camera pans when it would leave and the xelector stays visible, ';' puts the base back on the xelector, and mode 2
 * (the follow camera) is unchanged. Exit 0 = all held, 1 = a check failed, 2 = setup/run problem. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#define W 640
#define H 480
static char dir[600]; static int bad = 0; static unsigned char fa[W * H * 4], fb[W * H * 4];
static void wr(const char *rel, const char *txt) { char p[800]; snprintf(p, sizeof p, "%s/%s", dir, rel); FILE *f = fopen(p, "w"); if (!f) { perror(p); exit(2); } fputs(txt, f); fclose(f); }
static void md(const char *rel) { char p[800]; snprintf(p, sizeof p, "%s/%s", dir, rel); mkdir(p, 0755); }
static void setup(void) { md(""); md("pieces"); md("pieces/system"); md("pieces/display"); md("pieces/world_01"); md("pieces/xelector_01"); wr("pieces/world_01/state.txt", "desk_id=d1\nmap_id=m\n"); char rows[800] = ""; for (int i = 0; i < 8; i++) strcat(rows, "........................................\n"); wr("pieces/system/board.txt", rows); }
static void state(int mode, int sx, int sy, const char *extra) { char b[900]; snprintf(b, sizeof b, "focused_project_root=%s\nrender_mode=1\ncamera_mode=%d\nselector_x=%d\nselector_y=%d\n%s", dir, mode, sx, sy, extra); wr("pieces/system/bv_state.txt", b); }
static void xel(int x, int y) { char t[200]; snprintf(t, sizeof t, "entity_type=xelector\npos_x=%d\npos_y=%d\npos_z=1\n", x, y); wr("pieces/xelector_01/state.txt", t); }
static int run_engine(unsigned char *frame, double *ex, double *ez) {
    const char *bin = getenv("BV3D_BIN"); if (!bin) bin = "ops/+x/bv_render_3d.+x"; char abs[800]; if (bin[0] != '/') { char cw[512]; if (!getcwd(cw, sizeof cw)) return 2; snprintf(abs, sizeof abs, "%s/%s", cw, bin); } else snprintf(abs, sizeof abs, "%s", bin);
    char cmd[1700]; snprintf(cmd, sizeof cmd, "cd '%s' && timeout 60 '%s' >/dev/null 2>&1", dir, abs); if (system(cmd) != 0) return 2;
    char p[800], l[200]; snprintf(p, sizeof p, "%s/pieces/display/scene_receipt.pdl", dir); FILE *f = fopen(p, "r"); if (!f) return 2; *ex = *ez = -9999; while (fgets(l, sizeof l, f)) { if (!strncmp(l, "cam_eye_x=", 10)) *ex = atof(l + 10); if (!strncmp(l, "cam_eye_z=", 10)) *ez = atof(l + 10); } fclose(f);
    if (frame) { snprintf(p, sizeof p, "%s/pieces/display/rgb_frame_3d_overlay.raw", dir); f = fopen(p, "rb"); if (!f) return 2; size_t n = fread(frame, 1, W * H * 4, f); fclose(f); if (n != (size_t)W * H * 4) return 2; } return 0; }
static int keys(const char *k) { const char *bin = getenv("BV2D_INPUT_BIN"); if (!bin) bin = "ops/+x/bv_menu_input.+x"; char abs[800]; if (bin[0] != '/') { char cw[512]; if (!getcwd(cw, sizeof cw)) return 2; snprintf(abs, sizeof abs, "%s/%s", cw, bin); } else snprintf(abs, sizeof abs, "%s", bin);
    char cmd[1700]; snprintf(cmd, sizeof cmd, "cd '%s' && PRISC_PROJECT_ROOT='%s' timeout 20 '%s' %s >/dev/null 2>&1", dir, dir, abs, k); return system(cmd) == 0 ? 0 : 2; }
static int state_int(const char *rel, const char *key) { char p[800], l[200]; snprintf(p, sizeof p, "%s/%s", dir, rel); FILE *f = fopen(p, "r"); if (!f) return -9999; size_t kl = strlen(key); int v = -9999; while (fgets(l, sizeof l, f)) if (!strncmp(l, key, kl) && l[kl] == '=') v = atoi(l + kl + 1); fclose(f); return v; }
static void check(int ok, const char *what) { printf("%s: %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad = 1; }
static int diffpx(const unsigned char *a, const unsigned char *b) { int n = 0; for (int i = 0; i < W * H; i++) { const unsigned char *p = a + (size_t)i * 4, *q = b + (size_t)i * 4; if (abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2]) > 40) n++; } return n; }
/* how many pixels the xelector marker changes: the frame with the xelector file minus the frame without it, same camera state */
static int marker_px(void) { char a[800], b[800]; snprintf(a, sizeof a, "%s/pieces/xelector_01/state.txt", dir); snprintf(b, sizeof b, "%s/pieces/xelector_01/state.off", dir); double ex, ez; if (run_engine(fa, &ex, &ez)) return -1; rename(a, b); int r = run_engine(fb, &ex, &ez); rename(b, a); if (r) return -1; return diffpx(fa, fb); }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]); return 2; } snprintf(dir, sizeof dir, "%s", argv[1]); double x0, z0, x1, z1;
    setup(); state(3, 4, 4, ""); xel(4, 4); if (run_engine(NULL, &x0, &z0)) { fprintf(stderr, "engine run failed\n"); return 2; } printf("camera before: eye %.2f %.2f\n", x0, z0);
    if (keys("1001 1001")) return 2; if (run_engine(NULL, &x1, &z1)) return 2; printf("after 2 x ARROW_RIGHT: eye %.2f %.2f, xelector x %d\n", x1, z1, state_int("pieces/xelector_01/state.txt", "pos_x"));
    check(state_int("pieces/xelector_01/state.txt", "pos_x") == 6, "arrows: the xelector moved two cells"); check(fabs(x1 - x0) < 0.01 && fabs(z1 - z0) < 0.01, "arrows: the CAMERA did not move (mode 3, xelector still in view)");
    int mk = marker_px(); printf("xelector marker pixels: %d\n", mk); check(mk > 20, "arrows: the xelector is visible in the frame");
    if (keys("1001 1001 1001 1001 1001 1001 1001 1001 1001 1001 1001 1001 1001 1001 1001 1001 1001 1001")) return 2; if (run_engine(NULL, &x1, &z1)) return 2; mk = marker_px(); printf("after 18 more: xelector x %d, eye %.2f %.2f, marker px %d\n", state_int("pieces/xelector_01/state.txt", "pos_x"), x1, z1, mk);
    check(x1 > x0 + 2.0, "arrows: past the edge of the view the camera pans after the xelector"); check(mk > 20, "arrows: ... and the xelector is still visible");
    if (keys("59")) return 2; int bx = state_int("pieces/system/bv_state.txt", "cam_base_x"), sxx = state_int("pieces/system/bv_state.txt", "selector_x"); printf("after ; : cam_base_x %d selector_x %d\n", bx, sxx); check(bx == sxx, "; puts the camera base back on the xelector");
    setup(); state(2, 4, 4, ""); xel(4, 4); if (run_engine(NULL, &x0, &z0)) return 2; if (keys("1001 1001")) return 2; if (run_engine(NULL, &x1, &z1)) return 2; printf("mode 2: eye %.2f -> %.2f\n", x0, x1); check(fabs(x1 - x0) > 0.5, "mode 2 (the follow camera) is unchanged: the camera follows the xelector");
    return bad; }
