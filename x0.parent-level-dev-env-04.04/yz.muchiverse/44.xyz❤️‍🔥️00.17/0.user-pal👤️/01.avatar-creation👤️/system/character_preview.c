/* character_preview — mutaclysm-style RGB GL window for character manager.
 *
 * Shows the selected clone's 2D sprite as a textured billboard in a simple
 * 3D scene with orbit-style camera controls (same key family as
 * 101.mutaclsym ops/camera_control.c free/third-person look):
 *
 *   q/e  yaw left/right
 *   r/t  pitch up/down
 *   w/s  zoom in/out
 *   f    reset camera
 *   1    third-person preset (default)
 *   2    closer portrait
 *   3    free orbit (same keys, wider range)
 *   mouse drag = orbit yaw/pitch
 *
 * Desktop pet (system/avatar_window) stays separate — 2D shaped window
 * for "Open Desktop Window". This preview is the manager RGB view.
 *
 * Usage: system/character_preview <avatar_uuid>
 * Env:   PRISC_PROJECT_ROOT
 */
#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define WIN_W 420
#define WIN_H 420
#define POLL_MS 200

static char project_root[MAX_PATH] = ".";
static char avatar_uuid[128] = "";
static char state_path[PATH_BUF];
static char sprite_path[PATH_BUF];
static char title[256] = "Character Preview";

static GLuint g_tex = 0;
static int g_has_tex = 0;
static unsigned char *g_pixels = NULL;
static int g_res = 0;
static long g_sprite_mtime = 0;
static long g_state_mtime = 0;

/* Camera — muta free-roam family (degrees + distance). */
static double cam_yaw = 25.0;
static double cam_pitch = 18.0;
static double cam_dist = 3.2;
static int camera_mode = 1; /* 1 third, 2 portrait, 3 free */

static int mouse_down = 0;
static int last_mx = 0, last_my = 0;
static int g_win = 0;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            char *v = line + kl + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static long file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (long)st.st_mtime;
}

static int load_sprite(const char *csv_path, unsigned char **out_pixels, int *out_res) {
    FILE *f = fopen(csv_path, "r");
    if (!f) return 0;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return 0; }
    unsigned char *pixels = (unsigned char *)malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return 0; }
    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            pixels[count * 4 + 0] = (unsigned char)r;
            pixels[count * 4 + 1] = (unsigned char)g;
            pixels[count * 4 + 2] = (unsigned char)b;
            pixels[count * 4 + 3] = (unsigned char)a;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return 0; }
    *out_pixels = pixels;
    *out_res = res;
    return 1;
}

/* Best-effort: build sprite.csv from skin_emoji via emoji_gen_atlas (same as generate_clone). */
static void try_build_sprite(void) {
    char emoji[32] = "👤";
    read_kv(state_path, "skin_emoji", emoji, sizeof(emoji));
    if (!emoji[0]) snprintf(emoji, sizeof(emoji), "👤");
    char dir[PATH_BUF], atlas[PATH_BUF], cmd[PATH_BUF * 3];
    snprintf(dir, sizeof(dir), "%s/pieces/world_01/map_lobby/%s", project_root, avatar_uuid);
    snprintf(atlas, sizeof(atlas), "%s/atlas.png", dir);
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && ./system/emoji_gen_atlas '%s' '%s' >/dev/null 2>&1 && "
             "./system/emoji_xtract '%s' '%s' >/dev/null 2>&1",
             project_root, emoji, atlas, atlas, sprite_path);
    system(cmd);
}

static void upload_tex(void) {
    if (!g_pixels || g_res <= 0) { g_has_tex = 0; return; }
    if (g_tex) glDeleteTextures(1, &g_tex);
    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_res, g_res, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_pixels);
    g_has_tex = 1;
}

static void reload_sprite_if_needed(int force) {
    long sm = file_mtime(sprite_path);
    long st = file_mtime(state_path);
    if (!force && sm == g_sprite_mtime && st == g_state_mtime) return;

    /* If state changed emoji, rebuild sprite when csv missing or force. */
    if (force || sm == 0 || st != g_state_mtime) {
        try_build_sprite();
        sm = file_mtime(sprite_path);
    }
    free(g_pixels);
    g_pixels = NULL;
    g_res = 0;
    g_has_tex = 0;
    if (load_sprite(sprite_path, &g_pixels, &g_res)) {
        upload_tex();
        g_sprite_mtime = sm;
    } else {
        g_sprite_mtime = 0;
    }
    g_state_mtime = st;

    char name[64] = "Clone";
    read_kv(state_path, "name", name, sizeof(name));
    snprintf(title, sizeof(title), "Preview: %s", name[0] ? name : avatar_uuid);
    if (g_win) glutSetWindowTitle(title);
}

static void apply_camera_preset(int mode) {
    camera_mode = mode;
    if (mode == 1) { /* third person default */
        cam_yaw = 25.0; cam_pitch = 18.0; cam_dist = 3.2;
    } else if (mode == 2) { /* portrait closer */
        cam_yaw = 10.0; cam_pitch = 8.0; cam_dist = 2.2;
    } else { /* free orbit defaults */
        cam_yaw = 40.0; cam_pitch = 25.0; cam_dist = 4.0;
    }
}

static void clamp_camera(void) {
    if (cam_pitch > 85.0) cam_pitch = 85.0;
    if (cam_pitch < -20.0) cam_pitch = -20.0;
    if (cam_dist < 1.4) cam_dist = 1.4;
    if (cam_dist > 8.0) cam_dist = 8.0;
    while (cam_yaw > 360.0) cam_yaw -= 360.0;
    while (cam_yaw < 0.0) cam_yaw += 360.0;
}


/* --- Minecraft-ish blocky avatar (box head/torso/arms/legs) --- */
static void hex_approx(const char *name, float *r, float *g, float *b) {
    /* rough palette for DNA color names + skin emoji index fallbacks */
    *r = 0.7f; *g = 0.55f; *b = 0.4f;
    if (!name || !name[0]) return;
    if (strstr(name, "black")) { *r=0.15f;*g=0.12f;*b=0.1f; }
    else if (strstr(name, "brown")) { *r=0.45f;*g=0.28f;*b=0.12f; }
    else if (strstr(name, "blonde") || strstr(name, "yellow")) { *r=0.9f;*g=0.8f;*b=0.35f; }
    else if (strstr(name, "red")) { *r=0.75f;*g=0.2f;*b=0.15f; }
    else if (strstr(name, "gray") || strstr(name, "grey")) { *r=0.55f;*g=0.55f;*b=0.55f; }
    else if (strstr(name, "blue")) { *r=0.25f;*g=0.4f;*b=0.85f; }
    else if (strstr(name, "pink")) { *r=0.9f;*g=0.5f;*b=0.65f; }
    else if (strstr(name, "green")) { *r=0.25f;*g=0.65f;*b=0.3f; }
    else if (strstr(name, "white")) { *r=0.92f;*g=0.92f;*b=0.95f; }
    else if (strstr(name, "purple")) { *r=0.55f;*g=0.3f;*b=0.75f; }
    else if (strstr(name, "orange")) { *r=0.9f;*g=0.5f;*b=0.15f; }
    else if (strstr(name, "khaki")) { *r=0.7f;*g=0.65f;*b=0.4f; }
}

static void skin_tone_rgb(int idx, float *r, float *g, float *b) {
    /* approximate Unicode Fitzpatrick-ish tones */
    switch (idx) {
    case 0: *r=0.96f; *g=0.80f; *b=0.69f; break; /* default */
    case 1: *r=0.98f; *g=0.87f; *b=0.76f; break; /* light */
    case 2: *r=0.90f; *g=0.72f; *b=0.58f; break;
    case 3: *r=0.76f; *g=0.55f; *b=0.40f; break;
    case 4: *r=0.55f; *g=0.36f; *b=0.25f; break;
    case 5: *r=0.35f; *g=0.22f; *b=0.15f; break;
    default: *r=0.85f; *g=0.65f; *b=0.5f; break;
    }
}

static void draw_box(float x0, float y0, float z0, float x1, float y1, float z1,
                     float r, float g, float b) {
    glColor3f(r, g, b);
    /* 6 faces */
    glBegin(GL_QUADS);
    /* +Z */
    glVertex3f(x0,y0,z1); glVertex3f(x1,y0,z1); glVertex3f(x1,y1,z1); glVertex3f(x0,y1,z1);
    /* -Z darker */
    glColor3f(r*0.75f,g*0.75f,b*0.75f);
    glVertex3f(x1,y0,z0); glVertex3f(x0,y0,z0); glVertex3f(x0,y1,z0); glVertex3f(x1,y1,z0);
    /* +X */
    glColor3f(r*0.9f,g*0.9f,b*0.9f);
    glVertex3f(x1,y0,z1); glVertex3f(x1,y0,z0); glVertex3f(x1,y1,z0); glVertex3f(x1,y1,z1);
    /* -X */
    glColor3f(r*0.85f,g*0.85f,b*0.85f);
    glVertex3f(x0,y0,z0); glVertex3f(x0,y0,z1); glVertex3f(x0,y1,z1); glVertex3f(x0,y1,z0);
    /* +Y top */
    glColor3f(r*1.05f > 1?1:r*1.05f, g*1.05f>1?1:g*1.05f, b*1.05f>1?1:b*1.05f);
    glVertex3f(x0,y1,z1); glVertex3f(x1,y1,z1); glVertex3f(x1,y1,z0); glVertex3f(x0,y1,z0);
    /* -Y */
    glColor3f(r*0.7f,g*0.7f,b*0.7f);
    glVertex3f(x0,y0,z0); glVertex3f(x1,y0,z0); glVertex3f(x1,y0,z1); glVertex3f(x0,y0,z1);
    glEnd();
}

static void draw_mc_character(void) {
    char si[16]="", hair[32]="", shirt[32]="", pants[32]="";
    read_kv(state_path, "skin_index", si, sizeof(si));
    read_kv(state_path, "hair_color", hair, sizeof(hair));
    read_kv(state_path, "shirt_color", shirt, sizeof(shirt));
    read_kv(state_path, "pants_color", pants, sizeof(pants));
    float sr,sg,sb, hr,hg,hb, tr,tg,tb, pr,pg,pb;
    skin_tone_rgb(atoi(si), &sr,&sg,&sb);
    hex_approx(hair, &hr,&hg,&hb);
    hex_approx(shirt, &tr,&tg,&tb);
    hex_approx(pants, &pr,&pg,&pb);

    glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    /* units roughly like MC: head 0.5 cube, body 0.5x0.75x0.25, etc. */
    /* legs */
    draw_box(-0.22f, 0.0f, -0.10f, -0.02f, 0.55f, 0.10f, pr,pg,pb);
    draw_box( 0.02f, 0.0f, -0.10f,  0.22f, 0.55f, 0.10f, pr*0.92f,pg*0.92f,pb*0.92f);
    /* torso */
    draw_box(-0.25f, 0.55f, -0.12f, 0.25f, 1.15f, 0.12f, tr,tg,tb);
    /* arms */
    draw_box(-0.42f, 0.55f, -0.10f, -0.25f, 1.15f, 0.10f, sr,sg,sb);
    draw_box( 0.25f, 0.55f, -0.10f,  0.42f, 1.15f, 0.10f, sr*0.95f,sg*0.95f,sb*0.95f);
    /* sleeves tint on upper arm strip */
    draw_box(-0.42f, 0.95f, -0.105f, -0.25f, 1.15f, 0.105f, tr*0.9f,tg*0.9f,tb*0.9f);
    draw_box( 0.25f, 0.95f, -0.105f,  0.42f, 1.15f, 0.105f, tr*0.9f,tg*0.9f,tb*0.9f);
    /* head */
    draw_box(-0.22f, 1.15f, -0.22f, 0.22f, 1.59f, 0.22f, sr,sg,sb);
    /* hair top + bangs */
    draw_box(-0.24f, 1.48f, -0.24f, 0.24f, 1.62f, 0.24f, hr,hg,hb);
    draw_box(-0.24f, 1.40f, 0.18f, 0.24f, 1.55f, 0.26f, hr,hg,hb);
    /* simple face eyes */
    glColor3f(0.05f,0.05f,0.08f);
    glBegin(GL_QUADS);
    glVertex3f(-0.12f,1.38f,0.221f); glVertex3f(-0.04f,1.38f,0.221f);
    glVertex3f(-0.04f,1.44f,0.221f); glVertex3f(-0.12f,1.44f,0.221f);
    glVertex3f( 0.04f,1.38f,0.221f); glVertex3f( 0.12f,1.38f,0.221f);
    glVertex3f( 0.12f,1.44f,0.221f); glVertex3f( 0.04f,1.44f,0.221f);
    glEnd();
    glPopMatrix();
}


static void display(void) {
    glClearColor(0.12f, 0.14f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    double aspect = (double)WIN_W / (double)WIN_H;
    /* Simple perspective (no glu dependency required if we use frustum). */
    {
        double fovy = 45.0 * M_PI / 180.0;
        double f = 1.0 / tan(fovy / 2.0);
        double near = 0.1, far = 40.0;
        double m[16] = {0};
        m[0] = f / aspect;
        m[5] = f;
        m[10] = (far + near) / (near - far);
        m[11] = -1.0;
        m[14] = (2.0 * far * near) / (near - far);
        glLoadMatrixd(m);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Orbit eye around origin (character at origin). */
    double yr = cam_yaw * M_PI / 180.0;
    double pr = cam_pitch * M_PI / 180.0;
    double ex = cam_dist * cos(pr) * sin(yr);
    double ey = cam_dist * sin(pr) + 0.6;
    double ez = cam_dist * cos(pr) * cos(yr);

    /* Look-at origin, up = Y. Manual gluLookAt. */
    {
        double fx = -ex, fy = 0.6 - ey, fz = -ez;
        double fl = sqrt(fx*fx + fy*fy + fz*fz);
        if (fl < 1e-9) fl = 1.0;
        fx /= fl; fy /= fl; fz /= fl;
        double ux = 0, uy = 1, uz = 0;
        double sx = fy*uz - fz*uy, sy = fz*ux - fx*uz, sz = fx*uy - fy*ux;
        double sl = sqrt(sx*sx + sy*sy + sz*sz);
        if (sl < 1e-9) { sx = 1; sy = 0; sz = 0; sl = 1; }
        sx /= sl; sy /= sl; sz /= sl;
        ux = sy*fz - sz*fy; uy = sz*fx - sx*fz; uz = sx*fy - sy*fx;
        double m[16] = {
            sx, ux, -fx, 0,
            sy, uy, -fy, 0,
            sz, uz, -fz, 0,
            0, 0, 0, 1
        };
        glLoadMatrixd(m);
        glTranslated(-ex, -ey, -ez);
    }

    /* Ground grid (muta-ish board cue). */
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.22f, 0.25f, 0.30f);
    glBegin(GL_QUADS);
    glVertex3f(-2.5f, 0.0f, -2.5f);
    glVertex3f( 2.5f, 0.0f, -2.5f);
    glVertex3f( 2.5f, 0.0f,  2.5f);
    glVertex3f(-2.5f, 0.0f,  2.5f);
    glEnd();
    glColor3f(0.30f, 0.34f, 0.40f);
    glBegin(GL_LINES);
    for (int i = -5; i <= 5; i++) {
        float t = (float)i * 0.5f;
        glVertex3f(t, 0.01f, -2.5f); glVertex3f(t, 0.01f, 2.5f);
        glVertex3f(-2.5f, 0.01f, t); glVertex3f(2.5f, 0.01f, t);
    }
    glEnd();

    /* Minecraft-style blocky character (DNA colors from state.txt). */
    draw_mc_character();

    /* HUD strip */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIN_W, WIN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3f(0.75f, 0.78f, 0.85f);
    const char *help = "q/e yaw  r/t pitch  w/s zoom  f reset  1/2/3 presets  drag=orbit";
    glRasterPos2i(8, 16);
    for (const char *p = help; *p; p++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
    char line2[128];
    snprintf(line2, sizeof(line2), "cam yaw=%.0f pitch=%.0f dist=%.1f mode=%d",
             cam_yaw, cam_pitch, cam_dist, camera_mode);
    glRasterPos2i(8, 32);
    for (const char *p = line2; *p; p++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);

    glutSwapBuffers();
}

static void reshape(int w, int h) {
    (void)w; (void)h;
    glViewport(0, 0, WIN_W, WIN_H);
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    int changed = 1;
    if (key == 'q' || key == 'Q') cam_yaw -= 10.0;
    else if (key == 'e' || key == 'E') cam_yaw += 10.0;
    else if (key == 'r' || key == 'R') cam_pitch += 10.0;
    else if (key == 't' || key == 'T') cam_pitch -= 10.0;
    else if (key == 'w' || key == 'W') cam_dist -= 0.25;
    else if (key == 's' || key == 'S') cam_dist += 0.25;
    else if (key == 'f' || key == 'F') apply_camera_preset(camera_mode);
    else if (key == '1') apply_camera_preset(1);
    else if (key == '2') apply_camera_preset(2);
    else if (key == '3') apply_camera_preset(3);
    else if (key == 27) exit(0); /* Esc */
    else changed = 0;
    if (changed) {
        clamp_camera();
        glutPostRedisplay();
    }
}

static void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        mouse_down = (state == GLUT_DOWN);
        last_mx = x; last_my = y;
    }
}

static void motion(int x, int y) {
    if (!mouse_down) return;
    cam_yaw += (x - last_mx) * 0.4;
    cam_pitch += (last_my - y) * 0.4;
    last_mx = x; last_my = y;
    clamp_camera();
    glutPostRedisplay();
}

static void timer(int v) {
    (void)v;
    reload_sprite_if_needed(0);
    glutPostRedisplay();
    glutTimerFunc(POLL_MS, timer, 0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: character_preview <avatar_uuid>\n");
        return 1;
    }
    resolve_root();
    snprintf(avatar_uuid, sizeof(avatar_uuid), "%s", argv[1]);
    snprintf(state_path, sizeof(state_path),
             "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, avatar_uuid);
    snprintf(sprite_path, sizeof(sprite_path),
             "%s/pieces/world_01/map_lobby/%s/sprite.csv", project_root, avatar_uuid);
    if (access(state_path, R_OK) != 0) {
        fprintf(stderr, "character_preview: no state for %s\n", avatar_uuid);
        return 1;
    }

    apply_camera_preset(1);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(WIN_W, WIN_H);
    g_win = glutCreateWindow(title);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    reload_sprite_if_needed(1);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutTimerFunc(POLL_MS, timer, 0);
    glutMainLoop();
    return 0;
}
