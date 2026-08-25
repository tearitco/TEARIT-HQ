/* egg_window.c - shaped GL window, drawing a real pet sprite and now
 * clipped to that sprite's own silhouette instead of a flat circle mask.
 * Base technique (X11 Shape Extension + GLX, borderless
 * override_redirect, hand-rolled drag) is unchanged from mutaclsym's
 * !.shape=on.0.0Ⓜ️/shape-a0.c; the mask itself is now built per-pixel from
 * the sprite's alpha channel (see build_shape_mask) instead of XFillArc,
 * per the "we wont show circle, just shape of pet" direction in
 * #.wussup🥚️.txt. Falls back to the original circle only if no pet_id is
 * given or its sprite.csv can't be read (e.g. run standalone for a quick
 * check: ./system/egg_window).
 *
 * The window is exactly one cell of an invisible desktop grid
 * (GRID_CELL_PX square) - pets move cell-by-cell, not in free pixels,
 * per the same direction doc: "it will move on an invisible grid on the
 * users screen (and can face left or right)". ops/tick_pets.c is the
 * sole owner of a pet's absolute grid_x/grid_y/z (so its position is
 * always displayable in the terminal even with no window open) - this
 * process only reads that position, defensively re-clamps it to the
 * real screen's cell bounds (it's the only process that knows the
 * display size), converts it to pixels, and moves the window there.
 * z (altitude - negative for digging/aquatic species, positive for
 * flying ones, see tick_pets.c) is rendered as a simple vertical pixel
 * offset (Z_PIXEL_OFFSET per unit) - a flying pet's window sits visibly
 * higher on screen, a burrowing one sits lower. Polls the pet's own
 * state.txt every ~300ms (same "renderer polls a file" pattern
 * system/renderer.c already uses for current_frame.txt); on every new
 * tick_seq, also mirrors the sprite (and rebuilds the shape mask to
 * match) when facing flips, retints the background by hunger/asleep
 * state, and overlays a small pre-rendered icon (pieces/registry/icons/
 * {poop,sleep}.csv, generated once by scripts/gen_icons.sh) when
 * applicable. All of that is a decision tick_pets.c already made - this
 * process only reads the resulting numbers and draws/moves accordingly.
 *
 * Dragging with the mouse is still free-pixel while the button is held,
 * then snaps to the nearest grid cell on release - "dropping" the pet
 * onto a tile rather than leaving it at an arbitrary pixel offset. The
 * dropped grid_x/grid_y is written back to state.txt (z untouched) so
 * tick_pets.c's next tick continues from where the user actually put it
 * - the one deliberate exception to "never write pet state here", since
 * this is recording user input rather than deciding pet behavior (the
 * same spot the header below already earmarks for click/drag events).
 *
 * This window is meant to genuinely outlive its terminal session, not
 * die with it - once opened, a pet keeps running (and recording data)
 * from its own process until something explicitly closes it (right-click
 * Close, a keypress, or scripts/kill_pets.sh), matching real desktop-pet
 * behavior. That means ticking can't be terminal-driven either (a
 * terminal-only tick would leave an outlived window frozen), so this
 * process self-ticks its own pet directly: every tick_interval_sec (from
 * pieces/system/tick_config.txt, read once at startup - see
 * read_tick_config below), if self_tick=1 there, it shells out to
 * `ops/+x/tick_pets.+x <pet_id>` for just its own pet, independent of any
 * terminal. This is also allowed to coexist with pal/main_loop.pal's own
 * "world tick" sweep of every pet (gated by that same config's
 * world_tick key) without double-ticking anything - see ops/tick_pets.c's
 * own header comment for the shared last_tick_ts mechanism that makes
 * the two compatible instead of mutually exclusive. Window-open/close is
 * logged to pieces/system/master_ledger.txt (append_window_ledger) as
 * the continuous process-lifecycle record.
 *
 * This process must stay a dumb renderer + input relay per egg-pals.txt
 * - do not put pet decision-making in here (the tick_pets.+x invocation
 * above is dispatch, not decision - identical in spirit to every other
 * op-shelling-out already in this codebase), only drawing + relaying
 * input (the position write-back above, and later, appending raw click
 * events to a history file for prisc+x to read).
 *
 * Two backends behind the same sprite/grid/vitals-polling contract:
 * X11 Shape Extension + GLX (Linux/Mac, Mac via XQuartz) or native Win32
 * windowing + WGL (Windows) - chosen at compile time via _WIN32. The
 * event-driven shape (X11's manual select()-timeout poll loop vs.
 * Win32's WM_TIMER-driven window procedure) is different enough between
 * the two that they're kept as two separate implementations below rather
 * than forced into one shared loop; everything that's pure sprite/state
 * logic with no windowing-API calls (vitals parsing, position write-back,
 * texture upload, the actual GL draw calls) stays shared. */
#define _DEFAULT_SOURCE /* glibc gates M_PI in math.h behind this under -std=c11 */
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <sys/select.h>
#include <sys/time.h>
#include <X11/Xatom.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#ifndef M_PI /* mingw's math.h gates this behind _USE_MATH_DEFINES; glibc's behind _DEFAULT_SOURCE (already set above) */
#define M_PI 3.14159265358979323846
#endif

#define GRID_CELL_PX 80
#define WIDTH GRID_CELL_PX
#define HEIGHT GRID_CELL_PX
#define Z_PIXEL_OFFSET 20 /* screen pixels per z unit - purely cosmetic altitude cue */
#define PROJ_MAX_PATH 4096
#define PATH_BUF (PROJ_MAX_PATH + 256)
#define POLL_INTERVAL_USEC 300000

static char project_root[PROJ_MAX_PATH] = ".";
static GLuint g_texture = 0;
static GLuint g_poop_texture = 0;
static GLuint g_sleep_texture = 0;
static int g_has_texture = 0;
static int g_has_poop_icon = 0;
static int g_has_sleep_icon = 0;
static unsigned char *g_sprite_pixels = NULL; /* kept around (not freed after upload) so the shape mask can be rebuilt when facing flips */
static int g_sprite_res = 0;

/* Real Xdnd (XSetSelectionOwner/XdndEnter-Position-Drop/SelectionRequest)
 * used to live here - see system/xdnd_source.c/.h (extracted, not
 * linked into this binary) and 0.a-z-pets-plan/a-z-fix.txt for why: even
 * with all 6 bugs in drag-drop-bugs.txt fixed, it never worked end-to-
 * end (WM reparenting broke gl_mirror's own window self-lookup, and
 * gl_mirror's Xdnd idle poll had no CPU throttle at all - the same bug
 * that crashed the machine once already in the gl-canvas/ prototype
 * this fix is ported from). Replaced with a coordinate check against
 * gl_mirror's on-screen rect + a plain drop-queue file - both this
 * process and gl_mirror are ours, so no real OS drag-and-drop protocol
 * is needed between them. See find_mirror_rect()/check_drop_on_release()
 * below. */
#ifndef _WIN32
#define MIRROR_WINDOW_TITLE "mutaclsym RGB mirror"
#endif

typedef struct {
    int hunger, energy, poop_count, asleep, grid_x, grid_y, z, tick_seq;
    char facing[8];
} PetVitals;

/* Reads just the handful of metabolism fields egg_window cares about -
 * defaults match tick_pets.c's own hatch-time defaults so a pet ticked
 * zero times still renders sanely. grid_x/grid_y/z are read as absolute
 * position (owned by tick_pets.c), not deltas. */
static int read_pet_vitals(const char *path, PetVitals *v) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    v->hunger = 0; v->energy = 100; v->poop_count = 0; v->asleep = 0;
    v->grid_x = 0; v->grid_y = 0; v->z = 0; v->tick_seq = 0;
    snprintf(v->facing, sizeof(v->facing), "right");
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, "facing") == 0) {
            char val[8];
            snprintf(val, sizeof(val), "%s", eq + 1);
            val[strcspn(val, "\r\n")] = '\0'; /* CRLF-safe - a Windows-touched state.txt can have \r\n endings */
            snprintf(v->facing, sizeof(v->facing), "%s", val);
            continue;
        }
        int val = atoi(eq + 1);
        if (strcmp(line, "hunger") == 0) v->hunger = val;
        else if (strcmp(line, "energy") == 0) v->energy = val;
        else if (strcmp(line, "poop_count") == 0) v->poop_count = val;
        else if (strcmp(line, "asleep") == 0) v->asleep = val;
        else if (strcmp(line, "grid_x") == 0) v->grid_x = val;
        else if (strcmp(line, "grid_y") == 0) v->grid_y = val;
        else if (strcmp(line, "z") == 0) v->z = val;
        else if (strcmp(line, "tick_seq") == 0) v->tick_seq = val;
    }
    fclose(f);
    return 1;
}

/* Writes a dropped pet's grid_x/grid_y back to its own state.txt (z left
 * untouched - dragging doesn't change altitude) - see this file's header
 * comment for why this one write-back is not a "decision". Single
 * read-modify-write pass, same shape as the ops-directory C files' own
 * convention. */
static void write_pet_position(const char *path, int grid_x, int grid_y) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char lines[64][256];
    int nlines = 0;
    while (nlines < 64 && fgets(lines[nlines], sizeof(lines[0]), f)) nlines++;
    fclose(f);

    f = fopen(path, "w");
    if (!f) return;
    int seen_x = 0, seen_y = 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "grid_x") == 0) { fprintf(f, "grid_x=%d\n", grid_x); seen_x = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "grid_y") == 0) { fprintf(f, "grid_y=%d\n", grid_y); seen_y = 1; *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!seen_x) fprintf(f, "grid_x=%d\n", grid_x);
    if (!seen_y) fprintf(f, "grid_y=%d\n", grid_y);
    fclose(f);
}

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

#ifndef _WIN32
/* Same derivation style as ops/pet_export.c's own resolve_exchange_root()
 * - climb to this project's own parent directory and append the sibling
 * project's own folder name. PRISC_MIRROR_ROOT overrides it if the two
 * projects ever aren't laid out as siblings. */
static void resolve_mirror_root(char *out, size_t out_sz) {
    const char *env = getenv("PRISC_MIRROR_ROOT");
    if (env && env[0]) { snprintf(out, out_sz, "%s", env); return; }
    char parent[PATH_BUF];
    snprintf(parent, sizeof(parent), "%s", project_root);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    snprintf(out, out_sz, "%s/101.mutaclsym🧟‍♂️️+18.01", parent);
}

/* WM reparenting (mutter wraps client windows in a decoration frame)
 * means a titled window is often not a direct child of root - search
 * recursively, not just one level deep. Same fix already applied in
 * gl-canvas/system/pet_purely.c and gl-canvas/system/gl_canvas.c
 * tonight. */
static Window find_window_by_name(Display *d, Window start, const char *target) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren)) return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        char *name = NULL;
        if (XFetchName(d, children[i], &name) && name) {
            if (strcmp(name, target) == 0) found = children[i];
            XFree(name);
        }
        if (!found) found = find_window_by_name(d, children[i], target);
    }
    if (children) XFree(children);
    return found;
}

/* Absolute (root-relative) on-screen rect for a window found by title.
 * XGetGeometry's x/y are relative to the window's PARENT, not the
 * screen - translate through XTranslateCoordinates to get true screen
 * coordinates regardless of how many WM frames it's nested inside (the
 * second real bug gl-canvas's tk_drag_sim.c had tonight). */
static int find_mirror_rect(Display *d, int *out_x, int *out_y, int *out_w, int *out_h) {
    Window root = RootWindow(d, DefaultScreen(d));
    Window w = find_window_by_name(d, root, MIRROR_WINDOW_TITLE);
    if (!w) return 0;

    Window junk;
    int rel_x, rel_y;
    unsigned int ww, wh, wb, wdepth;
    if (!XGetGeometry(d, w, &junk, &rel_x, &rel_y, &ww, &wh, &wb, &wdepth)) return 0;

    int abs_x, abs_y;
    Window child_ret;
    XTranslateCoordinates(d, w, root, 0, 0, &abs_x, &abs_y, &child_ret);

    *out_x = abs_x; *out_y = abs_y; *out_w = (int)ww; *out_h = (int)wh;
    return 1;
}

/* Writes pet_id to the drop-queue file gl_mirror polls for, in
 * MUTACLSYM's own project root (not this project's). Write-to-temp
 * then rename so gl_mirror's idle-driven poll never sees a half-
 * written file - same pattern as gl-canvas/system/pet_purely.c. */
static void write_mirror_drop_file(const char *pet_id) {
    char mirror_root[PATH_BUF];
    resolve_mirror_root(mirror_root, sizeof(mirror_root));

    char tmp_path[PATH_BUF + 64], final_path[PATH_BUF + 64];
    snprintf(final_path, sizeof(final_path), "%s/incoming_drop.txt", mirror_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/incoming_drop.txt.tmp", mirror_root);

    FILE *f = fopen(tmp_path, "w");
    if (!f) { fprintf(stderr, "egg_window: cannot write mirror drop file at %s\n", tmp_path); return; }
    fprintf(f, "%s\n", pet_id);
    fclose(f);
    rename(tmp_path, final_path);
}

/* Checks the ButtonRelease point against gl_mirror's current on-screen
 * rect. If it lands inside: runs pet_export (the MISSING wire found
 * while writing a-z-fix.txt - ops/pet_export.c already exists and
 * correctly moves the pet's real directory into exchange/<pet_id>/, but
 * nothing ever called it) synchronously (not backgrounded - gl_mirror's
 * own pet_import needs exchange/<pet_id>/ to already exist by the time
 * it reads the drop file we're about to write), then hands off the
 * pet_id via write_mirror_drop_file(). Does NOT close this window
 * itself - egg_window already has a working close_request poll further
 * down in the main loop that gl_mirror's own trigger_pet_import()
 * already writes to after a successful import; reuse that instead of
 * duplicating a self-close path here. */
static void check_drop_on_release(Display *d, Window self, int root_x, int root_y, const char *pet_id) {
    int mx, my, mw, mh;
    if (!find_mirror_rect(d, &mx, &my, &mw, &mh)) return;
    (void)self;
    if (root_x >= mx && root_x < mx + mw && root_y >= my && root_y < my + mh) {
        char cmd[PATH_BUF + 128];
        snprintf(cmd, sizeof(cmd), "cd \"%s\" && ./ops/pet_export '%s' 2>&1", project_root, pet_id);
        system(cmd);
        write_mirror_drop_file(pet_id);
    }
}
#endif

#ifndef _WIN32
/* Read test harness config file and update window position */
static int read_test_config(const char *pet_id, int *x, int *y) {
    char config_path[1024];
    snprintf(config_path, sizeof(config_path),
             "%s/drag_drop_test.pdl", project_root);
    
    FILE *f = fopen(config_path, "r");
    if (!f) return 0;
    
    char line[256];
    int found_x = 0, found_y = 0;
    int in_window_section = 0;
    
    while (fgets(line, sizeof(line), f)) {
        /* Check for section headers */
        if (strstr(line, "SECTION")) {
            in_window_section = strstr(line, "WINDOW") != NULL;
            continue;
        }
        
        if (!in_window_section) continue;
        
        /* Parse key|value pairs */
        char *bar = strchr(line, '|');
        if (!bar) continue;
        *bar = '\0';
        char *key = line;
        char *value = bar + 1;
        
        /* Trim whitespace */
        while (*key == ' ') key++;
        while (*value == ' ') value++;
        value[strcspn(value, "\n")] = '\0';
        
        /* Look for egg_window position */
        if (strcmp(key, "egg_window_x") == 0) {
            *x = atoi(value);
            found_x = 1;
        } else if (strcmp(key, "egg_window_y") == 0) {
            *y = atoi(value);
            found_y = 1;
        }
        
        if (found_x && found_y) break;
    }
    
    fclose(f);
    return found_x && found_y;
}
#endif

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int g_self_tick_enabled = 1;
static int g_tick_interval_sec = 3;

/* pieces/system/tick_config.txt - see this file's own header comment and
 * ops/tick_pets.c's for how self_tick/tick_interval_sec make world-tick
 * and self-tick compatible instead of mutually exclusive. Missing file/
 * keys just keep the defaults above. */
static void read_tick_config(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/tick_config.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        int val = atoi(eq + 1);
        if (strcmp(line, "self_tick") == 0) g_self_tick_enabled = val;
        else if (strcmp(line, "tick_interval_sec") == 0 && val > 0) g_tick_interval_sec = val;
    }
    fclose(f);
}

/* Window-open/window-close as the continuous process-lifecycle record
 * requested alongside the per-pet state-change entries tick_pets.c
 * already logs here - same file, same format, one more event kind. */
static void append_window_ledger(const char *pet_id, const char *event) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/master_ledger.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] ProcessEvent: %s %s | Trigger: egg_window\n", ts, pet_id, event);
    fclose(f);
}

/* Shells out to tick_pets.+x for just this one pet - dispatch, not
 * decision, same as every other op-invocation in this codebase (see this
 * file's header comment). ops/+x/tick_pets.+x is the binary's actual name
 * on every platform (its name already has a dot, so Windows builds never
 * rename it to a separate .exe - see button.sh's own comment on this). */
static void self_tick_pet(const char *pet_id) {
    char exe_path[PATH_BUF];
    snprintf(exe_path, sizeof(exe_path), "%s/ops/+x/tick_pets.+x", project_root);
#ifdef _WIN32
    /* Plain double-quote wrap, no backslash-escaping - unlike
     * menu_input.c's win_quote_arg (which has to handle arbitrary
     * user-supplied paths), project_root/pet_id here are never expected
     * to contain embedded quotes in practice. */
    char cmdline[PATH_BUF * 2];
    snprintf(cmdline, sizeof(cmdline), "\"%s\" \"%s\"", exe_path, pet_id);
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
#else
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' > /dev/null 2>&1", exe_path, pet_id);
    int rc = system(cmd);
    (void)rc; /* fire-and-forget - tick_pets.c prints nothing on success anyway */
#endif
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

    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
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

static void upload_texture(const unsigned char *pixels, int res, GLuint *tex_out) {
    glGenTextures(1, tex_out);
    glBindTexture(GL_TEXTURE_2D, *tex_out);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res, res, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

/* Loads pieces/registry/icons/<name>.csv (pre-rendered once by
 * scripts/gen_icons.sh) the same way the main sprite is loaded - shared
 * across all pets, not per-pet. */
static void load_icon(const char *name, GLuint *tex_out, int *has_out) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/icons/%s.csv", project_root, name);
    unsigned char *pixels = NULL;
    int res = 0;
    if (load_sprite(path, &pixels, &res)) {
        upload_texture(pixels, res, tex_out);
        free(pixels);
        *has_out = 1;
    }
}

static void draw_circle(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(0.2f, 0.6f, 0.9f); // Solid blue color
    glVertex2f(0.0f, 0.0f);      // Center
    for (int i = 0; i <= 100; i++) {
        float angle = i * 2.0f * (float)M_PI / 100;
        glVertex2f(cosf(angle), sinf(angle));
    }
    glEnd();
}

static void draw_sprite(int flip) {
    float u0 = flip ? 1.0f : 0.0f;
    float u1 = flip ? 0.0f : 1.0f;
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, 1.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(u1, 1.0f); glVertex2f(1.0f, -1.0f);
    glTexCoord2f(u1, 0.0f); glVertex2f(1.0f, 1.0f);
    glTexCoord2f(u0, 0.0f); glVertex2f(-1.0f, 1.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static void draw_icon_quad(GLuint tex, float x0, float y0, float x1, float y1) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y1);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y1);
    glEnd();
}

/* Small corner overlays reflecting state tick_pets.c already decided -
 * this process just draws what the numbers say. Poop in the bottom-left,
 * sleep in the bottom-right, both a fifth of the window's width. */
static void draw_overlays(int poop_count, int asleep) {
    glEnable(GL_TEXTURE_2D);
    if (g_has_poop_icon && poop_count > 0) draw_icon_quad(g_poop_texture, -1.0f, -1.0f, -0.6f, -0.6f);
    if (g_has_sleep_icon && asleep) draw_icon_quad(g_sleep_texture, 0.6f, -1.0f, 1.0f, -0.6f);
    glDisable(GL_TEXTURE_2D);
}

#ifdef _WIN32

static HDC g_hdc;
static const char *g_pet_id = NULL;
static char g_pet_state_path[PATH_BUF] = "";
static int g_facing_left = 0;
static int g_dragging = 0;
static POINT g_drag_last;
static int g_win_start_x = 3 * GRID_CELL_PX, g_win_start_y = 3 * GRID_CELL_PX;
static int g_grid_x = 3, g_grid_y = 3, g_last_z = 0;
static int g_max_col = 0, g_max_row = 0;
static int g_last_tick_seq = -1;
static int g_cur_poop_count = 0, g_cur_asleep = 0;
static time_t g_last_self_tick = 0;

/* Builds the window's clip shape from the sprite's own alpha channel
 * (upscaled nearest-neighbor to the window's pixel size, same technique
 * ops/export_card.c's blit_sprite uses) instead of a fixed circle - "just
 * shape of pet", per #.wussup🥚️.txt. Sampled with the same horizontal
 * flip as draw_sprite() so the clip region always matches what's drawn.
 * Falls back to the original circle if no sprite was loaded. Builds one
 * HRGN rectangle per contiguous run of opaque pixels in a scanline
 * (rather than one per pixel) - cheaper, and still only runs once per
 * facing flip, not per frame. SetWindowRgn takes ownership of the region
 * handle on success, so it must not be deleted afterward in that case. */
static void build_shape_mask(HWND hwnd, int flip) {
    HRGN region = CreateRectRgn(0, 0, 0, 0);
    if (g_sprite_pixels) {
        for (int y = 0; y < HEIGHT; y++) {
            int sy = (y * g_sprite_res) / HEIGHT;
            if (sy >= g_sprite_res) sy = g_sprite_res - 1;
            int x = 0;
            while (x < WIDTH) {
                int sample_x = flip ? (WIDTH - 1 - x) : x;
                int sx = (sample_x * g_sprite_res) / WIDTH;
                if (sx >= g_sprite_res) sx = g_sprite_res - 1;
                if (g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] <= 127) { x++; continue; }
                int run_start = x;
                while (x < WIDTH) {
                    sample_x = flip ? (WIDTH - 1 - x) : x;
                    sx = (sample_x * g_sprite_res) / WIDTH;
                    if (sx >= g_sprite_res) sx = g_sprite_res - 1;
                    if (g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] <= 127) break;
                    x++;
                }
                HRGN run = CreateRectRgn(run_start, y, x, y + 1);
                CombineRgn(region, region, run, RGN_OR);
                DeleteObject(run);
            }
        }
    } else {
        HRGN circle = CreateEllipticRgn(0, 0, WIDTH, HEIGHT);
        CombineRgn(region, region, circle, RGN_OR);
        DeleteObject(circle);
    }
    if (!SetWindowRgn(hwnd, region, TRUE)) DeleteObject(region);
}

static void render_frame(void) {
    glViewport(0, 0, WIDTH, HEIGHT);
    if (g_has_texture) draw_sprite(g_facing_left);
    else draw_circle();
    draw_overlays(g_cur_poop_count, g_cur_asleep);
    SwapBuffers(g_hdc);
}

/* Diagnostic only: a plain crash here (access violation, etc.) exits
 * silently with no console attached and no error dialog anyone would
 * see, which is exactly what "the window just disappeared" looks like
 * from outside. Logs the exception code/address to a file next to the
 * project root instead of guessing blind next time it happens. */
static LONG WINAPI crash_handler(EXCEPTION_POINTERS *ep) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/egg_window_crash.log", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "exception code=0x%08lX at address=%p\n",
                (unsigned long)ep->ExceptionRecord->ExceptionCode,
                ep->ExceptionRecord->ExceptionAddress);
        fclose(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        g_dragging = 1;
        GetCursorPos(&g_drag_last);
        return 0;
    case WM_MOUSEMOVE:
        if (g_dragging) {
            POINT cur;
            GetCursorPos(&cur);
            g_win_start_x += cur.x - g_drag_last.x;
            g_win_start_y += cur.y - g_drag_last.y;
            SetWindowPos(hwnd, NULL, g_win_start_x, g_win_start_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            g_drag_last = cur;
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_dragging) {
            ReleaseCapture();
            g_dragging = 0;
            /* Drop onto the nearest grid cell instead of leaving the
             * pet at an arbitrary pixel offset. */
            g_grid_x = clampi((g_win_start_x + GRID_CELL_PX / 2) / GRID_CELL_PX, 0, g_max_col);
            g_grid_y = clampi((g_win_start_y + GRID_CELL_PX / 2) / GRID_CELL_PX, 0, g_max_row);
            g_win_start_x = g_grid_x * GRID_CELL_PX;
            g_win_start_y = g_grid_y * GRID_CELL_PX - g_last_z * Z_PIXEL_OFFSET;
            SetWindowPos(hwnd, NULL, g_win_start_x, g_win_start_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            if (g_pet_id) write_pet_position(g_pet_state_path, g_grid_x, g_grid_y);
        }
        return 0;
    case WM_KEYDOWN:
        DestroyWindow(hwnd); // Press any key to close
        return 0;
    case WM_RBUTTONUP: {
        /* This window is meant to outlive its terminal session (see this
         * file's own header comment), so nothing else will ever close it
         * automatically - a manual way to close one is needed. */
        POINT pt;
        GetCursorPos(&pt);
        HMENU hmenu = CreatePopupMenu();
        AppendMenuA(hmenu, MF_STRING, 1, "Close");
        SetForegroundWindow(hwnd); /* required for the popup to dismiss correctly on click-away */
        TrackPopupMenu(hmenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hmenu);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1) DestroyWindow(hwnd); /* "Close" */
        return 0;
    case WM_TIMER:
        /* Self-tick: independent of any terminal session - see this
         * file's own header comment. */
        if (g_pet_id && g_self_tick_enabled) {
            time_t now = time(NULL);
            if (now - g_last_self_tick >= g_tick_interval_sec) {
                self_tick_pet(g_pet_id);
                g_last_self_tick = now;
            }
        }
        if (g_pet_id) {
            PetVitals v;
            if (read_pet_vitals(g_pet_state_path, &v) && v.tick_seq != g_last_tick_seq) {
                g_last_tick_seq = v.tick_seq;
                g_cur_poop_count = v.poop_count;
                g_cur_asleep = v.asleep;
                g_last_z = v.z;

                int new_facing_left = (strcmp(v.facing, "left") == 0);
                if (new_facing_left != g_facing_left) {
                    g_facing_left = new_facing_left;
                    build_shape_mask(hwnd, g_facing_left);
                }

                if (!g_dragging) {
                    /* tick_pets.c owns absolute position - just re-read
                     * and re-clamp to this screen's real cell bounds,
                     * not accumulate deltas. */
                    g_grid_x = clampi(v.grid_x, 0, g_max_col);
                    g_grid_y = clampi(v.grid_y, 0, g_max_row);
                    g_win_start_x = g_grid_x * GRID_CELL_PX;
                    g_win_start_y = g_grid_y * GRID_CELL_PX - v.z * Z_PIXEL_OFFSET;
                    SetWindowPos(hwnd, NULL, g_win_start_x, g_win_start_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                }

                if (v.asleep) glClearColor(0.55f, 0.60f, 0.75f, 1.0f);
                else if (v.hunger >= 70) glClearColor(0.95f, 0.70f, 0.55f, 1.0f);
                else glClearColor(0.95f, 0.90f, 0.80f, 1.0f);

                render_frame();
            }
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        render_frame();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; /* OpenGL owns every pixel of this shaped window - avoid GDI flicker */
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main(int argc, char **argv) {
    const char *pet_id = (argc >= 2) ? argv[1] : NULL;
    resolve_root();
    read_tick_config();
    SetUnhandledExceptionFilter(crash_handler);
    g_pet_id = pet_id;

    HINSTANCE hinst = GetModuleHandle(NULL);
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.lpszClassName = "EggWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    /* WS_POPUP, no WS_CAPTION/WS_THICKFRAME - borderless, same intent as
     * the X11 build's override_redirect. WS_EX_TOOLWINDOW keeps it off
     * the taskbar/alt-tab list. */
    HWND hwnd = CreateWindowExA(WS_EX_TOOLWINDOW, "EggWindowClass", pet_id ? pet_id : "Round GL Window",
                                 WS_POPUP, 3 * GRID_CELL_PX, 3 * GRID_CELL_PX, WIDTH, HEIGHT,
                                 NULL, NULL, hinst, NULL);
    if (!hwnd) {
        fprintf(stderr, "egg_window: CreateWindow failed\n");
        return 1;
    }

    g_hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cAlphaBits = 8;
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf || !SetPixelFormat(g_hdc, pf, &pfd)) {
        fprintf(stderr, "egg_window: no suitable pixel format\n");
        return 1;
    }
    HGLRC glrc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, glrc);

    glClearColor(0.95f, 0.90f, 0.80f, 1.0f); /* eggshell background behind transparent sprite pixels */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (pet_id) {
        char sprite_path[PATH_BUF];
        snprintf(sprite_path, sizeof(sprite_path), "%s/pieces/world_01/map_lobby/%s/sprite.csv", project_root, pet_id);
        if (load_sprite(sprite_path, &g_sprite_pixels, &g_sprite_res)) {
            upload_texture(g_sprite_pixels, g_sprite_res, &g_texture);
            g_has_texture = 1;
            /* g_sprite_pixels is kept (not freed) - build_shape_mask needs
             * it again whenever facing flips, not just at startup. */
        } else {
            fprintf(stderr, "egg_window: could not load sprite for %s, falling back to circle\n", pet_id);
        }
    }
    /* Shared status-overlay icons, not per-pet - pre-rendered once by
     * scripts/gen_icons.sh. Missing files just mean no overlay is drawn. */
    load_icon("poop", &g_poop_texture, &g_has_poop_icon);
    load_icon("sleep", &g_sleep_texture, &g_has_sleep_icon);

    if (pet_id) snprintf(g_pet_state_path, sizeof(g_pet_state_path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, pet_id);

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    g_max_col = (screen_w / GRID_CELL_PX) - 1;
    g_max_row = (screen_h / GRID_CELL_PX) - 1;
    if (g_max_col < 0) g_max_col = 0;
    if (g_max_row < 0) g_max_row = 0;

    build_shape_mask(hwnd, g_facing_left);
    ShowWindow(hwnd, SW_SHOW);
    /* Nothing else forces this onto the foreground by default - it's an
     * 80x80px borderless window with no titlebar (WS_POPUP) and
     * deliberately excluded from the taskbar/alt-tab (WS_EX_TOOLWINDOW),
     * spawned as a detached background process, so without this it can
     * open successfully yet sit invisibly behind whatever window already
     * has focus (verified: process alive and rendering, just not seen).
     * SetForegroundWindow alone isn't reliable here - Windows' foreground-
     * lock protection can silently ignore it for a freshly-launched
     * background process - so also pin it topmost via SetWindowPos, which
     * has no such restriction and guarantees visibility even if it can't
     * steal keyboard focus. Matches the spirit of the X11 build's
     * override_redirect, which already tends to float these above the
     * desktop rather than being managed like a normal window. */
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(hwnd);
    render_frame();
    if (pet_id) append_window_ledger(pet_id, "window_opened");

    /* WM_TIMER is this build's equivalent of the X11 build's
     * select()-with-timeout poll loop - it's what lets the window also
     * animate on its own between input events. */
    SetTimer(hwnd, 1, POLL_INTERVAL_USEC / 1000, NULL);

    MSG msg;
    BOOL got;
    while ((got = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (got == -1) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (pet_id) append_window_ledger(pet_id, "window_closed");
    free(g_sprite_pixels);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(glrc);
    ReleaseDC(hwnd, g_hdc);
    return 0;
}

#else /* POSIX: X11 + GLX */

/* Builds the window's clip shape from the sprite's own alpha channel
 * (upscaled nearest-neighbor to the window's pixel size, same technique
 * ops/export_card.c's blit_sprite uses) instead of a fixed circle - "just
 * shape of pet", per #.wussup🥚️.txt. Sampled with the same horizontal
 * flip as draw_sprite() so the clip region always matches what's drawn.
 * Falls back to the original circle if no sprite was loaded. Rebuilding
 * this is cheap (one-time cost per facing change, not per frame), so it's
 * called again whenever facing flips, not just once at startup. */
static void build_shape_mask(Display *dpy, Window win, GC mask_gc, Pixmap mask, int flip) {
    XSetForeground(dpy, mask_gc, 0);
    XFillRectangle(dpy, mask, mask_gc, 0, 0, WIDTH, HEIGHT);
    XSetForeground(dpy, mask_gc, 1);
    if (g_sprite_pixels) {
        for (int y = 0; y < HEIGHT; y++) {
            int sy = (y * g_sprite_res) / HEIGHT;
            if (sy >= g_sprite_res) sy = g_sprite_res - 1;
            for (int x = 0; x < WIDTH; x++) {
                int sample_x = flip ? (WIDTH - 1 - x) : x;
                int sx = (sample_x * g_sprite_res) / WIDTH;
                if (sx >= g_sprite_res) sx = g_sprite_res - 1;
                if (g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] > 127) {
                    XFillRectangle(dpy, mask, mask_gc, x, y, 1, 1);
                }
            }
        }
    } else {
        XFillArc(dpy, mask, mask_gc, 0, 0, WIDTH, HEIGHT, 0, 360 * 64);
    }
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
}

#define POPUP_W 100
#define POPUP_H 32

/* Right-click "Close" context menu - the Linux/X11 counterpart to the
 * Windows build's WM_RBUTTONUP/TrackPopupMenu handler. This window is
 * meant to outlive its terminal session (see this file's own header
 * comment), so nothing else ever closes it automatically - a manual way
 * to close one is needed on every platform, not just Windows. Plain
 * core-Xlib 2D
 * drawing (XDrawRectangle/XDrawString), not GL - it's just a label, no
 * need to drag in a second GL context for it. Pointer is grabbed to this
 * popup window the moment it opens so any click anywhere (not just
 * clicks that land on it) dismisses or activates it, matching ordinary
 * popup-menu behavior. */
static Window open_context_menu(Display *dpy, GC popup_gc, int root_x, int root_y) {
    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = WhitePixel(dpy, DefaultScreen(dpy));
    swa.event_mask = ExposureMask | ButtonPressMask;
    Window popup = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                  root_x, root_y, POPUP_W, POPUP_H, 1,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XMapRaised(dpy, popup);
    XSetForeground(dpy, popup_gc, BlackPixel(dpy, DefaultScreen(dpy)));
    XGrabPointer(dpy, popup, True, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                 None, None, CurrentTime);
    return popup;
}

static void draw_context_menu(Display *dpy, Window popup, GC popup_gc) {
    XClearWindow(dpy, popup);
    XDrawRectangle(dpy, popup, popup_gc, 0, 0, POPUP_W - 1, POPUP_H - 1);
    XDrawString(dpy, popup, popup_gc, 14, POPUP_H / 2 + 4, "Close", 5);
}

static void close_context_menu(Display *dpy, Window popup) {
    XUngrabPointer(dpy, CurrentTime);
    XDestroyWindow(dpy, popup);
}

int main(int argc, char **argv) {
    const char *pet_id = (argc >= 2) ? argv[1] : NULL;
    resolve_root();
    read_tick_config();

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    // 1. Choose visual for OpenGL
    GLint att[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), att);

    // 2. Set window attributes
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask;
    swa.override_redirect = True; // Tells Window Manager to remove borders/titlebar

    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 3 * GRID_CELL_PX, 3 * GRID_CELL_PX, WIDTH, HEIGHT,
                              0, vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask | CWOverrideRedirect, &swa);

    XMapWindow(dpy, win);
    XStoreName(dpy, win, pet_id ? pet_id : "Round GL Window");
    if (pet_id) append_window_ledger(pet_id, "window_opened");

    // 3. Create the bounding mask pixmap/GC once - contents get rebuilt
    // by build_shape_mask() below (and again whenever facing flips).
    Pixmap mask = XCreatePixmap(dpy, win, WIDTH, HEIGHT, 1);
    GC gc = XCreateGC(dpy, mask, 0, NULL);

    // 4. Setup OpenGL Context
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);

    glClearColor(0.95f, 0.90f, 0.80f, 1.0f); /* eggshell background behind transparent sprite pixels */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (pet_id) {
        char sprite_path[PATH_BUF];
        snprintf(sprite_path, sizeof(sprite_path), "%s/pieces/world_01/map_lobby/%s/sprite.csv", project_root, pet_id);
        if (load_sprite(sprite_path, &g_sprite_pixels, &g_sprite_res)) {
            upload_texture(g_sprite_pixels, g_sprite_res, &g_texture);
            g_has_texture = 1;
            /* g_sprite_pixels is kept (not freed) - build_shape_mask needs
             * it again whenever facing flips, not just at startup. */
        } else {
            fprintf(stderr, "egg_window: could not load sprite for %s, falling back to circle\n", pet_id);
        }
    }
    /* Shared status-overlay icons, not per-pet - pre-rendered once by
     * scripts/gen_icons.sh. Missing files just mean no overlay is drawn. */
    load_icon("poop", &g_poop_texture, &g_has_poop_icon);
    load_icon("sleep", &g_sleep_texture, &g_has_sleep_icon);

    char pet_state_path[PATH_BUF] = "";
    if (pet_id) snprintf(pet_state_path, sizeof(pet_state_path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, pet_id);

    int screen_w = DisplayWidth(dpy, DefaultScreen(dpy));
    int screen_h = DisplayHeight(dpy, DefaultScreen(dpy));
    int max_col = (screen_w / GRID_CELL_PX) - 1;
    int max_row = (screen_h / GRID_CELL_PX) - 1;
    if (max_col < 0) max_col = 0;
    if (max_row < 0) max_row = 0;
    int xfd = ConnectionNumber(dpy);

    int facing_left = 0; /* mirrors draw_sprite() and build_shape_mask() together */
    build_shape_mask(dpy, win, gc, mask, facing_left);

    Window popup_win = 0; /* 0 = no context menu currently open */
    GC popup_gc = XCreateGC(dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, NULL);

    // 5. Main loop: poll the X connection with a timeout instead of
    // blocking forever on XNextEvent, so the window can also animate on
    // its own between input events (the "moving around the desktop" +
    // status-overlay behavior) - drawing/moving only, no decision-making,
    // matching this file's own header comment.
    XEvent xev;
    int drag_start_x = 0, drag_start_y = 0;
    /* Placeholder spawn position - overwritten immediately on the first
     * poll below (last_tick_seq starts at -1, guaranteeing that branch
     * runs at least once) with the pet's real grid_x/grid_y/z. */
    int win_start_x = 3 * GRID_CELL_PX, win_start_y = 3 * GRID_CELL_PX;
    int grid_x = 3, grid_y = 3, last_z = 0;
    int dragging = 0;
    int last_tick_seq = -1;
    int cur_poop_count = 0, cur_asleep = 0;
    int running = 1;
    time_t last_self_tick = 0;

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = POLL_INTERVAL_USEC;
        select(xfd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XNextEvent(dpy, &xev);

            if (xev.type == Expose && xev.xany.window == popup_win) {
                draw_context_menu(dpy, popup_win, popup_gc);
            }
            else if (xev.type == Expose) {
                glViewport(0, 0, WIDTH, HEIGHT);
                if (g_has_texture) draw_sprite(facing_left);
                else draw_circle();
                draw_overlays(cur_poop_count, cur_asleep);
                glXSwapBuffers(dpy, win);
            }
            else if (popup_win && xev.type == ButtonPress) {
                /* Pointer is grabbed to popup_win while it's open (see
                 * open_context_menu), so every button press anywhere
                 * arrives here regardless of which window it physically
                 * landed on - inside the popup's own rectangle activates
                 * "Close", anywhere else just dismisses it. */
                int inside = xev.xbutton.x >= 0 && xev.xbutton.x < POPUP_W &&
                             xev.xbutton.y >= 0 && xev.xbutton.y < POPUP_H;
                close_context_menu(dpy, popup_win);
                popup_win = 0;
                if (inside) { running = 0; break; }
            }
            else if (xev.type == ButtonPress && xev.xbutton.button == 3) {
                popup_win = open_context_menu(dpy, popup_gc, xev.xbutton.x_root, xev.xbutton.y_root);
            }
            else if (xev.type == ButtonPress && xev.xbutton.button == 1) {
                dragging = 1;
                drag_start_x = xev.xbutton.x_root;
                drag_start_y = xev.xbutton.y_root;
            }
            else if (xev.type == ButtonRelease && xev.xbutton.button == 1) {
                dragging = 0;
                /* Drop onto the nearest grid cell instead of leaving the
                 * pet at an arbitrary pixel offset. */
                grid_x = (win_start_x + GRID_CELL_PX / 2) / GRID_CELL_PX;
                grid_y = (win_start_y + GRID_CELL_PX / 2) / GRID_CELL_PX;
                if (grid_x < 0) grid_x = 0;
                if (grid_x > max_col) grid_x = max_col;
                if (grid_y < 0) grid_y = 0;
                if (grid_y > max_row) grid_y = max_row;
                win_start_x = grid_x * GRID_CELL_PX;
                win_start_y = grid_y * GRID_CELL_PX - last_z * Z_PIXEL_OFFSET;
                XMoveWindow(dpy, win, win_start_x, win_start_y);
                if (pet_id) write_pet_position(pet_state_path, grid_x, grid_y);
#ifndef _WIN32
                /* Coordinate+file handoff to gl_mirror instead of real
                 * Xdnd - see this file's header comment near
                 * MIRROR_WINDOW_TITLE and 0.a-z-pets-plan/a-z-fix.txt. */
                if (pet_id) check_drop_on_release(dpy, win, xev.xbutton.x_root, xev.xbutton.y_root, pet_id);
#endif
            }
            else if (xev.type == MotionNotify) {
                /* Free-pixel while dragging, matching the original
                 * hand-rolled drag feel - snapped to grid on release. */
                int dx = xev.xmotion.x_root - drag_start_x;
                int dy = xev.xmotion.y_root - drag_start_y;
                win_start_x += dx;
                win_start_y += dy;
                XMoveWindow(dpy, win, win_start_x, win_start_y);
                drag_start_x = xev.xmotion.x_root;
                drag_start_y = xev.xmotion.y_root;
            }
            else if (xev.type == KeyPress) {
                running = 0;
                break; // Press any key to close
            }
        }
        if (!running) break;

        /* Self-tick: independent of any terminal session - see this
         * file's own header comment. */
        if (pet_id && g_self_tick_enabled) {
            time_t now = time(NULL);
            if (now - last_self_tick >= g_tick_interval_sec) {
                self_tick_pet(pet_id);
                last_self_tick = now;
            }
        }

#ifndef _WIN32
        /* Check for close request from gl_mirror after successful drop */
        if (pet_id) {
            char close_request[1024];
            snprintf(close_request, sizeof(close_request),
                     "/tmp/egg_window_close_%s.txt", pet_id);
            FILE *f = fopen(close_request, "r");
            if (f) {
                fclose(f);
                remove(close_request); /* Clean up the request file */
                fprintf(stderr, "egg_window: received close request for %s\n", pet_id);
                running = 0;
                break;
            }
        }
        
        /* Poll test harness config for position updates */
        if (pet_id && !dragging) {
            static time_t last_config_poll = 0;
            time_t now = time(NULL);
            if (now - last_config_poll >= 1) { /* Poll every second */
                last_config_poll = now;
                int cfg_x, cfg_y;
                if (read_test_config(pet_id, &cfg_x, &cfg_y)) {
                    /* Update window position from config */
                    win_start_x = cfg_x;
                    win_start_y = cfg_y;
                    XMoveWindow(dpy, win, win_start_x, win_start_y);
                    /* Update grid position to match */
                    grid_x = win_start_x / GRID_CELL_PX;
                    grid_y = (win_start_y + last_z * Z_PIXEL_OFFSET) / GRID_CELL_PX;
                    if (grid_x < 0) grid_x = 0;
                    if (grid_x > max_col) grid_x = max_col;
                    if (grid_y < 0) grid_y = 0;
                    if (grid_y > max_row) grid_y = max_row;
                }
            }
        }
#endif

        if (pet_id) {
            PetVitals v;
            if (read_pet_vitals(pet_state_path, &v) && v.tick_seq != last_tick_seq) {
                last_tick_seq = v.tick_seq;
                cur_poop_count = v.poop_count;
                cur_asleep = v.asleep;
                last_z = v.z;

                int new_facing_left = (strcmp(v.facing, "left") == 0);
                if (new_facing_left != facing_left) {
                    facing_left = new_facing_left;
                    build_shape_mask(dpy, win, gc, mask, facing_left);
                }

                if (!dragging) {
                    /* tick_pets.c owns absolute position - just re-read
                     * and re-clamp to this screen's real cell bounds,
                     * not accumulate deltas. */
                    grid_x = clampi(v.grid_x, 0, max_col);
                    grid_y = clampi(v.grid_y, 0, max_row);
                    win_start_x = grid_x * GRID_CELL_PX;
                    win_start_y = grid_y * GRID_CELL_PX - v.z * Z_PIXEL_OFFSET;
                    XMoveWindow(dpy, win, win_start_x, win_start_y);
                }

                if (v.asleep) glClearColor(0.55f, 0.60f, 0.75f, 1.0f);
                else if (v.hunger >= 70) glClearColor(0.95f, 0.70f, 0.55f, 1.0f);
                else glClearColor(0.95f, 0.90f, 0.80f, 1.0f);

                glViewport(0, 0, WIDTH, HEIGHT);
                if (g_has_texture) draw_sprite(facing_left);
                else draw_circle();
                draw_overlays(cur_poop_count, cur_asleep);
                glXSwapBuffers(dpy, win);
            }
        }
    }

    // Cleanup
    if (pet_id) append_window_ledger(pet_id, "window_closed");
    if (popup_win) close_context_menu(dpy, popup_win); /* e.g. closed via keypress while the menu was still open */
    XFreeGC(dpy, popup_gc);
    free(g_sprite_pixels);
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

#endif
