/* khtpm_ui_common.c - text-included by BOTH khtpm_core_render.c (the HQ /
 * .xhtpm window engine) and khtpm_entity.c (the desktop pal process), same
 * house convention as khtpm_render_core.c / khtpm_draw_core.c: one canonical
 * copy compiled into each binary, no linking and no shared process. It has
 * only what both genuinely use: the hq_ui.pdl + theme loaders, the UI-scale
 * math, the meta.pdl METHOD reader, the entity-menu launcher and the globals
 * those touch. Include it after the system headers and PATH_BUF.
 * Unfactor piece 5 (2026-09-20). */
#ifndef KHTPM_UI_COMMON_C
#define KHTPM_UI_COMMON_C

#include "khtpm_ui_scale.c" /* pure screen-scale + reference-space position math, shared with the placer ops */

/* ---- types ---- */
#define TP_PATH_BUF 4352

/* action widened 2026-08-04, direct instruction (fo-menu-sys.md's real
 * convention: VALUE is a real, directly-executable command, e.g. a full
 * "gnome-terminal -- \"<long absolute house path>/chat.sh\"" line, not a
 * short keyword) - 64 bytes silently truncated/dropped real rows built
 * from this house's own long, emoji-heavy absolute paths.
 *
 * Moved up here (2026-08-05) from its original spot right before
 * load_methods() further down - nav_claim_rows() below needs the real
 * type, not just a forward declaration. */
typedef struct { char label[64]; char action[TP_PATH_BUF]; } MethodItem;

/* REAL FIX 2026-08-05: asa/ava's methods.pdl was already AT this cap
 * (8: Chat/Events/Events(mock)/Play/Stop/Ledger/Close/Cancel) before
 * adding "Events (ez)" - load_methods() silently drops anything past
 * MAX_METHODS with no error, so a 9th row would have been invisible
 * with zero warning. Bumped with real headroom, not just +1. */
#define MAX_METHODS 12

/* ---- globals ---- */
static char g_package_dir[PATH_BUF];
static char g_house_root[PATH_BUF];

/* REAL, NEW 2026-09-01 - the @ z-order toggle's managed half. House rule:
 * behavior comes from #.desktop/livedesk_override_redirect.pdl (true =
 * always-on-top override_redirect window, false = WM-managed so the WM
 * decides z-order and the "normal" mode can sink these below native
 * apps). Absent file / unrecognized value = default true (existing
 * behavior, every real window unaffected). override_redirect is fixed at
 * XCreateWindow time, so the taskbar's toggle kills + relaunches each
 * living window via /proc (ktb_toggle_zorder_respawn in
 * khtpm_strip_parser.c) - this loader only has to be right at startup. */
static int g_override_redirect = 1;

static char g_theme_bg[16] = "#1c1c1c";
static char g_theme_fg[16] = "#cccccc";
/* REAL, NEW 2026-09-15, direct live report ("the accent hue sounds
 * like a good idea if u know a good place to implement that... without
 * much fuss") - the house-wide focus-halo/armed-badge accent color was
 * hardcoded "#ff8c00" (orange) at 7 real draw sites in khtpm_draw_
 * core.c/khtpm_core_render.c, totally independent of the user's own
 * theme pick. A flat shade of g_theme_fg itself would often be too
 * close to body text to read as a distinct "this is focused" signal
 * (e.g. a lighter purple next to purple text), so this is a real hue
 * ROTATION (not a shade) - genuinely different-looking, but still
 * DERIVED from the user's own color, not a second hardcoded constant.
 * Computed once in load_theme_colors() below, not per-draw-call. */
static char g_theme_accent[16] = "#ff8c00";

/* Coalescing repaint flag for the generic (non-marker-pilot) window. N
 * repaint requests inside one event-loop iteration collapse to a single
 * redraw() at the tick boundary - the tpmos marker/dirty model
 * (chtpm_parser.c: triggers set the flag, compose_frame() runs once per
 * loop). Consumed at the bottom of hq_run_event_loop(). */
static int g_frame_dirty = 0;

/* REAL, NEW 2026-08-29 (direct instruction: "make it optional from
 * .pdl, can open immediately, or wait for second click") - runtime-
 * configurable, not hardcoded, per this house's own standing rule
 * (real PDL config beats a baked-in constant - see hq_ui.pdl's own
 * font_scale/focus_grab keys, same file, same real key=value parser
 * shape, reused not reinvented). Default is the new two-step behavior
 * (1); set `click_two_step=0` in #.desktop/hq_ui.pdl to restore the
 * old single-click-activates "auto" behavior house-wide. */
static int g_click_two_step = 1;

/* UI scale (2026-09-09, LIVEDESK-UI-SCALE.md). Percent; 100 = 1.0x.
 * Loaded from #.desktop/hq_ui.pdl's `font_scale` key (which already
 * shipped 1.25 and was read by nothing) in desktop_load_click_two_step()
 * and live-reloaded via hq_ui_pdl_reload_if_changed(). Every font-size
 * and layout-box call site that goes through scaled() (font_for()'s CSS
 * font-size, row heights, paddings) picks this up for free. Settings
 * 'Size -'/'Size +' step it via the UI_SCALE_MINUS/PLUS verbs. */
static int g_ui_scale_pct = 100;

/* Screen-relative auto scale (2026-09-19, BUG-LOG "UI does not scale to
 * the monitor"). g_ui_scale_pct = g_ui_user_pct (hq_ui.pdl font_scale,
 * what the Settings Size -/+ buttons edit) * g_ui_auto_pct / 100, where
 * g_ui_auto_pct = min(screen_w/ui_ref_width, screen_h/ui_ref_height)
 * clamped 50..300, or ui_scale*100 when the ui_scale override is > 0.
 * The reference (hq_ui.pdl ui_ref_width/ui_ref_height, default 2496x1664)
 * is the screen the layouts were tuned on, so auto = 100 there and
 * nothing changes on that machine. Always recomputed from these bases,
 * never accumulated, so repeated layout passes are idempotent. */
static int g_ui_user_pct = 100;
static int g_ui_auto_pct = 100;
static int g_ui_screen_w = 0, g_ui_screen_h = 0; /* 0 = not probed yet -> auto 100 */
static int g_ui_ref_w = 2496, g_ui_ref_h = 1664;
static int g_ui_override_pct = 0; /* hq_ui.pdl ui_scale (0 = auto) */

/* REAL, NEW 2026-09-10, direct instruction ("when should we add font
 * picker to settings") - house-wide DEFAULT font family, same real
 * role font_scale already plays for size. Any window/CSS that sets its
 * own font-family (e.g. pchq-board.css's "Ubuntu") keeps winning - this
 * is only the fallback the Xft spec builder used to hard-literal to
 * "DejaVu Sans" (see that function's own header comment for the exact
 * site). Loaded by desktop_load_click_two_step() below, same file/same
 * function font_scale already uses - one settings reload path, not two. */
static char g_ui_font_family[64] = "DejaVu Sans";

/* REAL, NEW 2026-08-30, direct instruction ("we actually want to have
 * a .pdl file that decides how to render emoji sprits in top down.
 * (from top or front as usual) lets view from front for now but later
 * will change when doing more camera stuff") - a real, live-editable
 * `emoji_sprite_view` key in this same shared hq_ui.pdl (same real
 * home as click_two_step/cursword_move_mode - a house-wide UI toggle,
 * not buried in cursword's own pal-scoped config). "front" (default)
 * is a straight-on yaw=0 camera - the classic real "topdown map, but
 * sprites/objects render front-facing" convention most real top-down
 * games actually use, and directly answers the earlier live report
 * that the previous fixed yaw=45 diagonal corner view looked "melted"/
 * unreasonable. "top" is the original diagonal corner view, kept as a
 * real, named alternative for later camera work, not deleted. */
static int g_emoji_sprite_view_top = 0; /* 0 = front (default), 1 = top */

/* Forward declaration - set by handle_shutdown_signal() (defined later
 * in this file, in the section just above tp_main's use of it). The
 * default/HQ-mode loops below honor it so a SIGTERM/SIGINT breaks the
 * event loop and runs the shared cleanup path. */
static volatile sig_atomic_t g_shutdown_requested;

static char g_khtpm_menu_pkg_dir[TP_PATH_BUF] = "";
static char g_khtpm_menu_house_root[TP_PATH_BUF] = "";
static pid_t g_khtpm_menu_pid = -1;

/* ---- funcs ---- */
static void load_override_redirect(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_override_redirect.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';
        if (strcmp(line, "override_redirect") == 0)
            g_override_redirect = (strcmp(val, "true") == 0);
    }
    fclose(f);
}

static void set_window_opacity(Display *d, Window w, double opacity) {
    if (opacity < 0.0) opacity = 0.0;
    if (opacity > 1.0) opacity = 1.0;
    Atom opacity_atom = XInternAtom(d, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long val = (unsigned long)(opacity * (double)0xFFFFFFFFUL);
    XChangeProperty(d, w, opacity_atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&val, 1);
}

/* Nudge a "#rrggbb" toward white (delta>0) or black (delta<0) by delta
 * per channel, clamped. Used for the small chrome-strip accent over the
 * themed base fill so it tracks the theme instead of a hardcoded grey.
 * Returns a pointer to a static buffer - one call per use site. */
static const char *kh_shade_hex(const char *hex, int delta) {
    static char out[8];
    int r = 0, g = 0, b = 0;
    if (!hex || sscanf(hex, "#%2x%2x%2x", &r, &g, &b) != 3) return hex ? hex : "#2a2a2a";
    r += delta; g += delta; b += delta;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    snprintf(out, sizeof(out), "#%02x%02x%02x", r, g, b);
    return out;
}

/* REAL, NEW 2026-09-15, direct live report ("the accent hue sounds
 * like a good idea") - rotates a "#rrggbb" hex's HUE by `deg` degrees
 * (0-360) around the HSL color wheel, keeping saturation/lightness
 * (boosted to a real minimum so a near-grey theme fg still produces a
 * visibly colored accent, not another shade of grey). Used once, at
 * theme-load, to derive g_theme_accent from g_theme_fg - a genuinely
 * different-looking color still DERIVED from the user's own pick,
 * unlike a plain shade (kh_shade_hex) which stays the same hue. */
static const char *kh_hue_rotate_hex(const char *hex, double deg) {
    static char out[8];
    int ri = 0, gi = 0, bi = 0;
    if (!hex || sscanf(hex, "#%2x%2x%2x", &ri, &gi, &bi) != 3) return hex ? hex : "#ff8c00";
    double r = ri / 255.0, g = gi / 255.0, b = bi / 255.0;
    double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    double mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    double l = (mx + mn) / 2.0, h = 0.0, s = 0.0;
    double d = mx - mn;
    if (d > 0.0001) {
        s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
        if (mx == r) h = fmod((g - b) / d + (g < b ? 6.0 : 0.0), 6.0);
        else if (mx == g) h = (b - r) / d + 2.0;
        else h = (r - g) / d + 4.0;
        h *= 60.0;
    }
    h = fmod(h + deg, 360.0); if (h < 0) h += 360.0;
    if (s < 0.5) s = 0.5;          /* real minimum so a near-grey fg still reads as a real color */
    if (l < 0.35) l = 0.35; if (l > 0.65) l = 0.65; /* keep it visible on both light and dark bg */
    double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = l - c / 2.0;
    double r2, g2, b2;
    if (h < 60)       { r2 = c; g2 = x; b2 = 0; }
    else if (h < 120) { r2 = x; g2 = c; b2 = 0; }
    else if (h < 180) { r2 = 0; g2 = c; b2 = x; }
    else if (h < 240) { r2 = 0; g2 = x; b2 = c; }
    else if (h < 300) { r2 = x; g2 = 0; b2 = c; }
    else              { r2 = c; g2 = 0; b2 = x; }
    int ro = (int)((r2 + m) * 255.0 + 0.5), go = (int)((g2 + m) * 255.0 + 0.5), bo = (int)((b2 + m) * 255.0 + 0.5);
    if (ro < 0) ro = 0; if (ro > 255) ro = 255;
    if (go < 0) go = 0; if (go > 255) go = 255;
    if (bo < 0) bo = 0; if (bo > 255) bo = 255;
    snprintf(out, sizeof(out), "#%02x%02x%02x", ro, go, bo);
    return out;
}

static void load_theme_colors(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "COLOR", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *key_end = end;
        while (key_end > p && key_end[-1] == ' ') key_end--;
        char key[16];
        size_t klen = (size_t)(key_end - p);
        if (klen == 0 || klen >= sizeof(key)) continue;
        memcpy(key, p, klen);
        key[klen] = '\0';
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        if (v[0] != '#') continue;
        if (strcmp(key, "bg") == 0) snprintf(g_theme_bg, sizeof(g_theme_bg), "%s", v);
        else if (strcmp(key, "fg") == 0) snprintf(g_theme_fg, sizeof(g_theme_fg), "%s", v);
    }
    fclose(f);
    /* g_theme_accent tracks g_theme_fg (a +150deg hue rotation - roughly
     * opposite-ish on the wheel without landing exactly on the
     * mathematical complement, which for some hues reads as muddy) -
     * recomputed every time the theme reloads, same as bg/fg. */
    snprintf(g_theme_accent, sizeof(g_theme_accent), "%s", kh_hue_rotate_hex(g_theme_fg, 150.0));
}

static void kh_ui_apply_scale(void) {
    int a = kps_auto_pct(g_ui_override_pct, g_ui_screen_w, g_ui_screen_h, g_ui_ref_w, g_ui_ref_h);
    g_ui_auto_pct = a;
    int p = (g_ui_user_pct * a + 50) / 100;
    if (p < 25) p = 25;
    if (p > 400) p = 400;
    g_ui_scale_pct = p;
}

/* Saved entity positions (desktop_pos.txt) are REFERENCE px - see
 * khtpm_ui_scale.c. g_grid_cell_base = desk_grid.pdl cell_px (unscaled; the
 * entity sets it next to GRID_CELL_PX). Identity when auto == 100. */
static int g_grid_cell_base = 80;
static KPS_UNUSED int kh_pos_ref_to_screen(int ref) { return kps_ref_to_screen(ref, g_grid_cell_base, g_ui_auto_pct); }
static KPS_UNUSED int kh_pos_screen_to_ref(int scr) { return kps_screen_to_ref(scr, g_grid_cell_base, g_ui_auto_pct); }

/* Screen-relative px (entity grid/window sizes, which font_scale must not
 * change): base * auto / 100, min 1. */
static int kh_auto_px(int base_px) {
    if (g_ui_auto_pct == 100) return base_px;
    int v = (base_px * g_ui_auto_pct + 50) / 100;
    return (base_px > 0 && v < 1) ? 1 : v;
}

static int scaled(int base_px) {
    if (g_ui_scale_pct == 100) return base_px;
    /* round to nearest; keep a 1px floor for anything that was >=1 */
    int v = (base_px * g_ui_scale_pct + 50) / 100;
    if (base_px > 0 && v < 1) v = 1;
    return v;
}

/* REAL 2026-08-07, direct-caught bug ("muchi 4TSG has no Close
 * button"): m8's objects.pdl began with a UTF-8 BOM (EF BB BF), so its
 * first line "PAGE | main" failed strncmp(line,"PAGE",4) and the whole
 * main page (the one carrying Feed/Menu/Play/Close/Cancel) silently
 * vanished - the menu instead showed the NEXT page (activities), which
 * has no Close. Every PDL reader here opens package data files that are
 * often saved by Windows editors (which attach a BOM). This helper eats
 * a leading BOM right after fopen so the very first line parses like
 * any other. */
static FILE *pdl_open(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    int c1 = fgetc(f), c2 = fgetc(f), c3 = fgetc(f);
    if (!(c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)) {
        if (c3 != EOF) ungetc(c3, f);
        if (c2 != EOF) ungetc(c2, f);
        if (c1 != EOF) ungetc(c1, f);
    }
    return f;
}

static int load_methods(const char *package_dir, MethodItem *items, int max) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 0;
    char line[TP_PATH_BUF];
    int n = 0;
    /* REAL, NEW 2026-09-22 (task 2 - per-entity Cli-io on/off) - the
     * Cli-io row itself (below, action="CLI_IO") was already fully
     * real and wired (khtpm_entity.c's real armed/typed/committed
     * handling at every RUN_METHOD/ACTIVATE_NAV/Enter dispatch site,
     * grep 'CLI_IO' in that file) - it was just unconditionally
     * auto-appended to EVERY entity's context menu with no opt-out.
     * REAL FIX 2026-09-22 (direct correction, "not just dsr, but all
     * entities would get the cli-io"): flipped from opt-in to
     * opt-OUT. Default is now ON for every entity (absent field =
     * on); a real META row lets ONE entity turn it off:
     *   META | cli_io_default | off
     * Same real SECTION|KEY|VALUE shape this file's own header row
     * already documents (matches the META|piece_id|... row every
     * real meta.pdl already has - not a new format). */
    int cli_io_default_on = 1;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "META", 4) == 0 && strncmp(line, "METHOD", 6) != 0) {
            char *mp = strchr(line, '|');
            if (mp) {
                mp++;
                while (*mp == ' ') mp++;
                char *mend = strchr(mp, '|');
                if (mend) {
                    char *klabel_end = mend;
                    while (klabel_end > mp && klabel_end[-1] == ' ') klabel_end--;
                    size_t klen = (size_t)(klabel_end - mp);
                    if (klen == 14 && strncmp(mp, "cli_io_default", 14) == 0) {
                        char *va = mend + 1;
                        while (*va == ' ') va++;
                        char *va_end = va + strcspn(va, "\r\n");
                        while (va_end > va && va_end[-1] == ' ') va_end--;
                        size_t vlen = (size_t)(va_end - va);
                        if (vlen == 3 && strncmp(va, "off", 3) == 0) cli_io_default_on = 0;
                    }
                }
            }
            continue;
        }
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t llen = (size_t)(label_end - p);
        if (llen == 0 || llen >= sizeof(items[0].label)) continue;
        memcpy(items[n].label, p, llen);
        items[n].label[llen] = '\0';

        char *a = end + 1;
        while (*a == ' ') a++;
        char *a_end = a + strcspn(a, "\r\n");
        while (a_end > a && a_end[-1] == ' ') a_end--;
        size_t alen = (size_t)(a_end - a);
        if (alen == 0 || alen >= sizeof(items[0].action)) continue;
        memcpy(items[n].action, a, alen);
        items[n].action[alen] = '\0';
        n++;
    }
    fclose(f);
    if (cli_io_default_on && n < max) {
        int has = 0, j;
        for (j = 0; j < n; j++)
            if (!strcmp(items[j].action, "CLI_IO") || !strcmp(items[j].label, "Cli-io")) has = 1;
        if (!has) {
            snprintf(items[n].label, sizeof(items[n].label), "Cli-io");
            snprintf(items[n].action, sizeof(items[n].action), "CLI_IO");
            n++;
        }
    }
    return n;
}

static void launch_khtpm_menu(int px, int py) {
    /* kill-then-relaunch, same real single-instance convention every
     * khtpm app's own button.sh already uses - a page-nav GOTO could
     * call open_context_menu() again while a prior instance is still
     * up (real for objects.pdl-style multi-page menus, not exercised
     * by ava's own single-page menu.chtpm yet, but correct to guard
     * for now rather than after it's hit live). */
    if (g_khtpm_menu_pid > 0) {
        kill(g_khtpm_menu_pid, SIGTERM);
        waitpid(g_khtpm_menu_pid, NULL, WNOHANG);
        g_khtpm_menu_pid = -1;
    }
    char bin_path[TP_PATH_BUF];
    snprintf(bin_path, sizeof(bin_path), "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x", g_khtpm_menu_house_root);
    /* REAL Stage 5 step 3/4 (2026-08-16, khtpm-merge-how2.md §5d.3) -
     * real, unified <house_root> <chtpm_path> [x] [y] contract (was
     * <package_dir> <house_root> [x] [y]) - khtpm_core_render's
     * own main() now derives package_dir from dirname(chtpm_path)
     * itself, so this caller just needs to build the real chtpm path
     * once instead of passing the bare dir. */
    char chtpm_path[TP_PATH_BUF];
    snprintf(chtpm_path, sizeof(chtpm_path), "%s/menu.chtpm", g_khtpm_menu_pkg_dir);
    char px_str[16], py_str[16];
    snprintf(px_str, sizeof(px_str), "%d", px);
    snprintf(py_str, sizeof(py_str), "%d", py);
#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        execl(bin_path, bin_path, g_khtpm_menu_house_root, chtpm_path, px_str, py_str, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        g_khtpm_menu_pid = pid;
    }
#else
    (void)bin_path;
    (void)chtpm_path;
    (void)px_str;
    (void)py_str;
    /* Entity-menu CHTPM renderer is a later Win pass; keep legacy popup. */
    g_khtpm_menu_pid = -1;
#endif
}

/* ---- hq_ui.pdl loader: keys both processes read; `extra` gets the rest ---- */
static void kh_ui_pdl_load(const char *house_root, int (*extra)(const char *key, const char *val)) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        if (strcmp(line, "click_two_step") == 0) g_click_two_step = atoi(val) != 0;
        else if (strcmp(line, "emoji_sprite_view") == 0) g_emoji_sprite_view_top = (strcmp(val, "top") == 0);
        else if (strcmp(line, "font_scale") == 0) {
            int p = (int)(atof(val) * 100.0 + 0.5);
            if (p < 50) p = 50;      /* the hq_ui.pdl comment's own 0.5-3.0 range */
            if (p > 300) p = 300;
            g_ui_user_pct = p;
            kh_ui_apply_scale();
        }
        /* ui_scale: manual screen-scale override (0 = auto from the screen
         * size); ui_ref_width/ui_ref_height: the screen the layouts were
         * tuned on. See kh_ui_apply_scale(). */
        else if (strcmp(line, "ui_scale") == 0) {
            int p = (int)(atof(val) * 100.0 + 0.5);
            g_ui_override_pct = p > 0 ? p : 0;
            kh_ui_apply_scale();
        }
        else if (strcmp(line, "ui_ref_width") == 0 && atoi(val) > 0) { g_ui_ref_w = atoi(val); kh_ui_apply_scale(); }
        else if (strcmp(line, "ui_ref_height") == 0 && atoi(val) > 0) { g_ui_ref_h = atoi(val); kh_ui_apply_scale(); }
        else if (strcmp(line, "font_family") == 0 && val[0]) {
            snprintf(g_ui_font_family, sizeof(g_ui_font_family), "%s", val);
        }
        else if (extra) (void)extra(line, val);
    }
    fclose(f);
}

#endif /* KHTPM_UI_COMMON_C */
