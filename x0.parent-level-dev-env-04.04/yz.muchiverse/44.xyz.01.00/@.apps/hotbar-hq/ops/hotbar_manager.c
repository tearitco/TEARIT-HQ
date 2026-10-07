/* hotbar_manager - publishes the hotbar's projection (manager owns projection).
 *
 * The hotbar is an ordinary khtpm HQ window (hotbar-<mode>.xhtpm) drawing the
 * ui.txt this process keeps current; all hotbar logic lives here and in the
 * shared khtpm_inventory.c, none in the renderer. Design:
 * 18.pc-hq/CURSWORD-POSSESSION-DESIGN.md sections 4, 5b, 5e.
 *
 *   hotbar_manager desk|pchq <house_root> <package_dir>
 * pchq draws inside the pc-hq board window: the board's own <module> starts this with package_dir = the
 * board's dir, so the feed lands in <board>/state/pchq/ui.txt with hb_ keys and the board loads it as a second
 * vars file (vars="state/ui.txt state/pchq/ui.txt"). The window's own <module> lifetime ends this process.
 * (the order <module src="...hotbar_manager.+x desk"/> produces: the renderer appends
 * house_root and package_dir; state goes to <package_dir>/state/<mode>/)
 *
 * Whose inventory (the "holder" - psx_effective() in the design, until
 * khtpm_possess.c exists):
 *   desk : cursword (the desk's possessor; possession is not built on the desk).
 *   pchq : the entity the xelector possesses (xelector_01/state.txt possessed_id),
 *          else the xelector itself (unpossessed = its own inventory).
 * Writes <state_dir>/ui.txt (tmp + rename, only when the content changed) and
 * <state_dir>/holder.txt (the holder's entity dir, read by hotbar_action.sh so a
 * slot click selects in the right inventory).
 *
 * ui.txt keys (renderer <repeat count="${slots}" bind="s">):
 *   title, holder, count, slots=9 (a fixed Minecraft-style row; empty slots are
 *   shown empty), s_<i>_text, s_<i>_cls ("hb-sel" for the selected slot).
 * HOOK (vitals, not built): heart= / hunger= lines are added here by the vitals
 * reader when khtpm_possess.c exists; the layout shows whatever keys are present.
 *
 * Build: see build_hotbar_manager.sh */
#define _GNU_SOURCE
#include "khtpm_inventory.c"
#include <glob.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#define HB_SLOTS 9

static int read_kv(const char *path, const char *key, char *out, size_t n) {
    FILE *f = fopen(path, "r");
    char line[512];
    size_t kl = strlen(key);
    out[0] = '\0';
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, key, kl) && line[kl] == '=') {
            snprintf(out, n, "%s", line + kl + 1);
            out[strcspn(out, "\r\n")] = '\0';
            fclose(f);
            return out[0] != '\0';
        }
    }
    fclose(f);
    return 0;
}

/* The holder's entity dir; returns 1 if it exists. */
static int resolve_holder(const char *house, const char *mode, char *dir, size_t n) {
    struct stat st;
    if (!strcmp(mode, "pchq")) {
        char sp[INV_PATH], pid[128] = "";
        snprintf(sp, sizeof(sp), "%s/@.apps/piececraft-hq/pieces/xelector_01/state.txt", house);
        read_kv(sp, "possessed_id", pid, sizeof(pid));
        if (!pid[0] || !strcmp(pid, "none")) snprintf(pid, sizeof(pid), "xelector_01");
        snprintf(dir, n, "%s/@.apps/piececraft-hq/pieces/%s", house, pid);
    } else {
        glob_t g;
        char pat[INV_PATH];
        dir[0] = '\0';
        snprintf(pat, sizeof(pat), "%s/xyzfs/users/*/home/livedesk/pals/cursword", house);
        if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc > 0) snprintf(dir, n, "%s", g.gl_pathv[0]);
        globfree(&g);
        if (!dir[0]) return 0;
    }
    return stat(dir, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Where the window should sit, published as anchor_cx (centre x) and anchor_bottom (bottom
 * edge y); the renderer (class "vars-positioned") centres and bottom-aligns its own size on
 * them. desk: centred on the bottom taskbar, just above it (dock stack base.txt x|y|w|h,
 * published by the dock, design 5c/5g). pchq: centred in the pc-hq board window, above its
 * footer (that window's registry line). Returns 0 if the anchor source does not exist yet. */
static int anchor_for(const char *house, const char *mode, int *cx, int *bottom) {
    char path[INV_PATH], line[600];
    FILE *f;
    int x, y, w, h;
    if (!strcmp(mode, "pchq")) {
        glob_t g;
        int found = 0;
        size_t i;
        snprintf(path, sizeof(path), "%s/#.desktop/livedesk_hq_windows_*.txt", house);
        if (glob(path, 0, NULL, &g) == 0) {
            for (i = 0; i < g.gl_pathc && !found; i++) {
                if (!(f = fopen(g.gl_pathv[i], "r"))) continue;
                while (fgets(line, sizeof(line), f)) {
                    char *t = strstr(line, "title=piececraft-hq board|");
                    if (t && (t = strstr(line, "|x=")) && sscanf(t, "|x=%d|y=%d|w=%d|h=%d", &x, &y, &w, &h) == 4) {
                        *cx = x + w / 2; *bottom = y + h - 56; found = 1; break;
                    }
                }
                fclose(f);
            }
        }
        globfree(&g);
        return found;
    }
    snprintf(path, sizeof(path), "%s/#.desktop/dock_stack/base.txt", house);
    if (!(f = fopen(path, "r"))) return 0;
    x = y = w = h = -1;
    if (fgets(line, sizeof(line), f)) sscanf(line, "%d|%d|%d|%d", &x, &y, &w, &h);
    fclose(f);
    if (w <= 0 || y <= 0) return 0;
    *cx = x + w / 2; *bottom = y - 4;
    return 1;
}

static void publish(const char *house, const char *mode, const char *state_dir, const char *holder_dir, char *last, size_t lastn) {
    char names[INV_MAX][INV_NAME], buf[8192], path[INV_PATH + 16], tmp[INV_PATH + 24];
    int n = inv_list(holder_dir, names, INV_MAX), sel = inv_slot_get(holder_dir, n), i, off = 0;
    FILE *f;
    off += snprintf(buf + off, sizeof(buf) - off, "title=Hotbar - %s\nholder=%s\ncount=%d\nn_slots=%d\n",
                    inv_base(holder_dir), inv_base(holder_dir), n, HB_SLOTS);
    {   /* the holder's OWN visual (owner todo 2026-10-06: the hotbar showed only the holder's name): the entity dir, drawn by the renderer from its sprite.csv
         * like every slot picture. Empty when the holder has no sprite.csv, so a template gated with show="${holder_sprite}" simply hides the cell. */
        char hsp[INV_PATH + 32]; struct stat hst;
        snprintf(hsp, sizeof(hsp), "%s/sprite.csv", holder_dir);
        off += snprintf(buf + off, sizeof(buf) - off, "holder_sprite=%s\n", stat(hsp, &hst) == 0 ? holder_dir : "");
    }
    {
        int cx, bt;
        if (anchor_for(house, mode, &cx, &bt))
            off += snprintf(buf + off, sizeof(buf) - off, "anchor_cx=%d\nanchor_bottom=%d\n", cx, bt);
    }
    {
        /* visible: <state_dir>/visible.txt ("1"/"0", default 1), flipped by hotbar_toggle.sh - the pc-hq
         * board hides/shows its hotbar overlay with it (show="${hb_visible}"). */
        char vp[INV_PATH + 16], vv[16] = "";
        snprintf(vp, sizeof(vp), "%s/visible.txt", state_dir);
        read_kv(vp, "visible", vv, sizeof(vv));
        off += snprintf(buf + off, sizeof(buf) - off, "visible=%s\n", vv[0] == '0' ? "0" : "1");
    }
    off += snprintf(buf + off, sizeof(buf) - off, "sel_name=%s\n", sel < n && n > 0 ? names[sel] : "(empty)");
    for (i = 0; i < HB_SLOTS; i++) {
        if (i < n) {
            char gp[INV_PATH], glyph[64] = "";
            FILE *g;
            snprintf(gp, sizeof(gp), "%s/inventory/%s/glyph.txt", holder_dir, names[i]);
            if ((g = fopen(gp, "r"))) {
                if (fgets(glyph, sizeof(glyph), g)) glyph[strcspn(glyph, "\r\n")] = '\0';
                fclose(g);
            }
            /* s_<i>_sprite: the item's entity dir - the renderer draws that entity's own
             * sprite.csv image (what the taskbar cells do), crisp at any size, instead of
             * the emoji glyph, which the font may not have (drawn as an empty box). The
             * glyph stays as the label fallback for an item with no sprite. */
            {   /* an item with a sprite.csv shows its picture (label blank); one without shows its glyph (emoji) as the label */
                char spp[INV_PATH + 64]; struct stat sst;
                int has_spr;
                snprintf(spp, sizeof(spp), "%s/inventory/%s/sprite.csv", holder_dir, names[i]);
                has_spr = stat(spp, &sst) == 0;
                off += snprintf(buf + off, sizeof(buf) - off, "s_%d_text=%s\ns_%d_glyph=%s\ns_%d_sprite=%s%s%s\ns_%d_cls=%s\n",
                                i, has_spr ? " " : (glyph[0] ? glyph : "?"), i, glyph[0] ? glyph : "?",
                                i, has_spr ? holder_dir : "", has_spr ? "/inventory/" : "", has_spr ? names[i] : "",
                                i, i == sel ? "hb-sel" : "hb-full theme-2");
            }
        } else {
            off += snprintf(buf + off, sizeof(buf) - off, "s_%d_text=.\ns_%d_cls=hb-empty theme-2\n", i, i);
        }
        if (off > (int)sizeof(buf) - 256) break;
    }
    if (!strcmp(buf, last)) return; /* unchanged: do not touch the file (renderer reparses on change) */
    snprintf(last, lastn, "%s", buf);
    snprintf(path, sizeof(path), "%s/ui.txt", state_dir);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if ((f = fopen(tmp, "w"))) {
        /* pchq: the feed is a SECOND vars file of the pc-hq board window, whose own ui.txt has its own keys,
         * so every key is prefixed hb_ to rule out a collision. */
        if (!strcmp(mode, "pchq")) {
            char *ln = buf, *nl;
            while (*ln) { nl = strchr(ln, '\n'); if (!nl) break; fprintf(f, "hb_%.*s\n", (int)(nl - ln), ln); ln = nl + 1; }
        } else fputs(buf, f);
        fclose(f);
        rename(tmp, path);
    }
}

int main(int argc, char **argv) {
    char last[8192] = "", holder[INV_PATH] = "", prev_holder[INV_PATH] = "", hp[INV_PATH + 16], htmp[INV_PATH + 24];
    const char *house, *mode, *state_dir;
    struct timespec ts = {0, 80 * 1000 * 1000};   /* 80 ms tick: visible.txt is checked every tick, the full publish only every 5th (400 ms) - see main() */
    char sdir[INV_PATH];
    if (argc < 4) { fprintf(stderr, "usage: hotbar_manager desk|pchq <house_root> <package_dir>\n"); return 2; }
    /* the renderer resolves bare tokens against the house ("desk" -> "<house>/desk"): keep the basename */
    mode = strrchr(argv[1], '/') ? strrchr(argv[1], '/') + 1 : argv[1];
    house = argv[2];
    snprintf(sdir, sizeof(sdir), "%s/state", argv[3]);
    mkdir(sdir, 0755);
    snprintf(sdir, sizeof(sdir), "%s/state/%s", argv[3], mode);
    state_dir = sdir;
    mkdir(state_dir, 0755);
    {
        int tick = 0; char vlast[16] = "";
    for (;;) {
        /* minimize / restore must feel instant: the owner re-clicked "_" within ~280 ms because the old 400 ms poll + board reparse showed nothing, and a toggle cancelled itself
         * (2026-10-06, from the human-input log). So the tiny visible.txt is read every tick and a change publishes at once; everything else stays at 400 ms. */
        char vp[INV_PATH + 16], vv[16] = "";
        int vchanged;
        snprintf(vp, sizeof(vp), "%s/visible.txt", state_dir);
        read_kv(vp, "visible", vv, sizeof(vv));
        vchanged = strcmp(vv, vlast) != 0;
        if (tick++ % 5 != 0 && !vchanged) { nanosleep(&ts, NULL); continue; }
        snprintf(vlast, sizeof(vlast), "%s", vv);
        if (resolve_holder(house, mode, holder, sizeof(holder))) {
            if (strcmp(holder, prev_holder)) {
                FILE *f;
                snprintf(hp, sizeof(hp), "%s/holder.txt", state_dir);
                snprintf(htmp, sizeof(htmp), "%s.tmp", hp);
                if ((f = fopen(htmp, "w"))) { fprintf(f, "%s\n", holder); fclose(f); rename(htmp, hp); }
                snprintf(prev_holder, sizeof(prev_holder), "%s", holder);
            }
            publish(house, mode, state_dir, holder, last, sizeof(last));
        }
        nanosleep(&ts, NULL);
    }
    }
    return 0;
}
