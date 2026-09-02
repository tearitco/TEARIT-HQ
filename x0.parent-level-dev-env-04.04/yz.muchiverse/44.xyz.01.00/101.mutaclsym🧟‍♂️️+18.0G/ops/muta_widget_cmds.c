/* muta_widget_cmds - drain widget cmd inbox for mutaclysm world IO.
 *
 * Env: PRISC_PROJECT_ROOT, MUTA_SAVES_ROOT, optional MUTA_TEMPLATE_WORLD,
 *      PRISC_INSTALL_ROOT (for muta_world_io.+x path).
 *
 * Commands:
 *   PING | SEED_DEMO | NEW_GAME | SAVE_GAME_AS:<slot> | LOAD_GAME:<slot>
 *   LIST_MAPS | SWITCH_MAP:<map_id> | SWITCH_MAP:<map_id>:<x>:<y>
 *   PLACE_TILE:<map_id>:<x>:<y>:<glyph>
 *   SET_BRUSH:<glyph>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define PATH_BUF 4352
#define MAX_LINE 2048
#define MAX_PATH 4096

static char project_root[MAX_PATH] = ".";
static char saves_root[MAX_PATH] = "";
static char live_world[PATH_BUF];
static char template_world[PATH_BUF];
static char io_bin[PATH_BUF];
static char map_bin[PATH_BUF];
static char place_bin[PATH_BUF];
static char brush_glyph[8] = "T";

static void resolve_bin(char *out, size_t out_sz, const char *name) {
    snprintf(out, out_sz, "%s/ops/+x/%s", project_root, name);
    if (access(out, X_OK) != 0) {
        const char *inst = getenv("PRISC_INSTALL_ROOT");
        if (inst && inst[0])
            snprintf(out, out_sz, "%s/ops/+x/%s", inst, name);
    }
}

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    const char *sr = getenv("MUTA_SAVES_ROOT");
    if (sr && sr[0]) snprintf(saves_root, sizeof(saves_root), "%s", sr);
    snprintf(live_world, sizeof(live_world), "%s/pieces/world_01", project_root);
    const char *tw = getenv("MUTA_TEMPLATE_WORLD");
    if (tw && tw[0])
        snprintf(template_world, sizeof(template_world), "%s", tw);
    else
        snprintf(template_world, sizeof(template_world),
                 "%s/pieces/world_01_template", project_root);
    resolve_bin(io_bin, sizeof(io_bin), "muta_world_io.+x");
    resolve_bin(map_bin, sizeof(map_bin), "muta_map_io.+x");
    resolve_bin(place_bin, sizeof(place_bin), "muta_place_tile.+x");
    /* restore brush from session if present */
    {
        char bp[PATH_BUF], g[8];
        snprintf(bp, sizeof(bp), "%s/pieces/system/paint_brush.txt", project_root);
        FILE *bf = fopen(bp, "r");
        if (bf) {
            if (fgets(g, sizeof(g), bf) && g[0] >= 32 && g[0] <= 126)
                brush_glyph[0] = g[0], brush_glyph[1] = 0;
            fclose(bf);
        }
    }
}

static void ensure_dirs(void) {
    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/pieces/system/widget_cmds'", project_root);
    system(cmd);
}

static void set_status(const char *cmdn, const char *result, const char *msg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/widget_cmds/status.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "last_cmd=%s\n", cmdn ? cmdn : "");
    fprintf(f, "result=%s\n", result ? result : "");
    fprintf(f, "message=%s\n", msg ? msg : "");
    fprintf(f, "at=%ld\n", (long)time(NULL));
    fclose(f);
}

static void publish_bridge(void) {
    ensure_dirs();
    char path[PATH_BUF], inbox[PATH_BUF], status[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/widget_bridge.txt", project_root);
    snprintf(inbox, sizeof(inbox), "%s/pieces/system/widget_cmds/inbox.txt", project_root);
    snprintf(status, sizeof(status), "%s/pieces/system/widget_cmds/status.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "session_root=%s\n", project_root);
    fprintf(f, "project_id=mutaclysm\n");
    fprintf(f, "kind=game_world\n");
    fprintf(f, "capabilities=game_world,mutaclysm_saves\n");
    fprintf(f, "live_world=%s\n", live_world);
    fprintf(f, "template_world=%s\n", template_world);
    fprintf(f, "saves_root=%s\n", saves_root[0] ? saves_root : "");
    fprintf(f, "inbox_path=%s\n", inbox);
    fprintf(f, "status_path=%s\n", status);
    fprintf(f, "display_name=mutaclysm\n");
    fclose(f);
}

static int process_line(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    line[strcspn(line, "\r\n")] = '\0';
    if (!line[0] || line[0] == '#') return 0;

    char cmd[PATH_BUF * 3];

    if (strcmp(line, "PING") == 0) {
        publish_bridge();
        set_status("PING", "ok", "pong");
        return 0;
    }
    if (strcmp(line, "SEED_DEMO") == 0) {
        if (!saves_root[0]) {
            set_status("SEED_DEMO", "error", "MUTA_SAVES_ROOT unset");
            return -1;
        }
        struct stat st;
        const char *src = (stat(live_world, &st) == 0) ? live_world : template_world;
        snprintf(cmd, sizeof(cmd), "'%s' seed-demo '%s' '%s'", io_bin, src, saves_root);
        if (system(cmd) != 0) {
            set_status("SEED_DEMO", "error", "seed failed");
            return -1;
        }
        set_status("SEED_DEMO", "ok", "demo-project ready");
        return 0;
    }
    if (strcmp(line, "NEW_GAME") == 0) {
        snprintf(cmd, sizeof(cmd), "'%s' new-game '%s' '%s'",
                 io_bin, live_world, template_world);
        if (system(cmd) != 0) {
            set_status("NEW_GAME", "error", "new-game failed");
            return -1;
        }
        set_status("NEW_GAME", "ok", "live reset from template");
        return 0;
    }
    if (strncmp(line, "SAVE_GAME_AS:", 13) == 0) {
        const char *slot = line + 13;
        if (!saves_root[0] || !slot[0]) {
            set_status("SAVE_GAME_AS", "error", "missing saves_root or slot");
            return -1;
        }
        snprintf(cmd, sizeof(cmd), "'%s' save '%s' '%s' '%s'",
                 io_bin, live_world, saves_root, slot);
        if (system(cmd) != 0) {
            set_status("SAVE_GAME_AS", "error", "save failed");
            return -1;
        }
        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "saved slot %s", slot);
        set_status("SAVE_GAME_AS", "ok", msg);
        return 0;
    }
    if (strncmp(line, "LOAD_GAME:", 10) == 0) {
        const char *slot = line + 10;
        if (!saves_root[0] || !slot[0]) {
            set_status("LOAD_GAME", "error", "missing saves_root or slot");
            return -1;
        }
        snprintf(cmd, sizeof(cmd), "'%s' load '%s' '%s' '%s'",
                 io_bin, live_world, saves_root, slot);
        if (system(cmd) != 0) {
            set_status("LOAD_GAME", "error", "load failed");
            return -1;
        }
        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "loaded slot %s", slot);
        set_status("LOAD_GAME", "ok", msg);
        return 0;
    }
    if (strcmp(line, "LIST_MAPS") == 0) {
        char outp[PATH_BUF];
        snprintf(outp, sizeof(outp), "%s/pieces/system/widget_cmds/map_list.txt", project_root);
        snprintf(cmd, sizeof(cmd), "'%s' list '%s' > '%s'", map_bin, live_world, outp);
        if (system(cmd) != 0) {
            set_status("LIST_MAPS", "error", "list failed");
            return -1;
        }
        set_status("LIST_MAPS", "ok", outp);
        return 0;
    }
    if (strncmp(line, "SET_BRUSH:", 10) == 0) {
        char g = line[10];
        if (g < 32 || g > 126) {
            set_status("SET_BRUSH", "error", "need printable ASCII glyph");
            return -1;
        }
        brush_glyph[0] = g;
        brush_glyph[1] = 0;
        char bp[PATH_BUF];
        snprintf(bp, sizeof(bp), "%s/pieces/system/paint_brush.txt", project_root);
        {
            char cmdmk[PATH_BUF];
            snprintf(cmdmk, sizeof(cmdmk), "mkdir -p '%s/pieces/system'", project_root);
            system(cmdmk);
        }
        FILE *bf = fopen(bp, "w");
        if (bf) { fprintf(bf, "%c\n", g); fclose(bf); }
        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "brush=%c", g);
        set_status("SET_BRUSH", "ok", msg);
        return 0;
    }
    if (strncmp(line, "PLACE_TILE:", 11) == 0) {
        /* PLACE_TILE:map:x:y:glyph  or PLACE_TILE:map:x:y (use brush) */
        char map_id[128] = "";
        int x = 0, y = 0;
        char glyph = brush_glyph[0];
        char tmp[MAX_LINE];
        snprintf(tmp, sizeof(tmp), "%s", line + 11);
        char *a = tmp;
        char *b = strchr(a, ':');
        if (!b) { set_status("PLACE_TILE", "error", "bad format"); return -1; }
        *b = '\0';
        snprintf(map_id, sizeof(map_id), "%s", a);
        a = b + 1;
        b = strchr(a, ':');
        if (!b) { set_status("PLACE_TILE", "error", "bad format"); return -1; }
        *b = '\0';
        x = atoi(a);
        a = b + 1;
        b = strchr(a, ':');
        if (!b) {
            y = atoi(a);
        } else {
            *b = '\0';
            y = atoi(a);
            if (b[1] >= 32 && b[1] <= 126) glyph = b[1];
        }
        if (!map_id[0]) {
            set_status("PLACE_TILE", "error", "empty map_id");
            return -1;
        }
        snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s' %d %d '%c'",
                 place_bin, project_root, map_id, x, y, glyph);
        if (system(cmd) != 0) {
            set_status("PLACE_TILE", "error", "place failed");
            return -1;
        }
        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "placed %c at %s(%d,%d)", glyph, map_id, x, y);
        set_status("PLACE_TILE", "ok", msg);
        return 0;
    }
    if (strncmp(line, "SWITCH_MAP:", 11) == 0) {
        const char *rest = line + 11;
        char map_id[128] = "";
        int x = 5, y = 4;
        /* SWITCH_MAP:id or SWITCH_MAP:id:x:y */
        {
            char tmp[MAX_LINE];
            snprintf(tmp, sizeof(tmp), "%s", rest);
            char *p1 = strchr(tmp, ':');
            if (!p1) {
                snprintf(map_id, sizeof(map_id), "%s", tmp);
            } else {
                *p1 = '\0';
                snprintf(map_id, sizeof(map_id), "%s", tmp);
                char *p2 = strchr(p1 + 1, ':');
                if (p2) {
                    *p2 = '\0';
                    x = atoi(p1 + 1);
                    y = atoi(p2 + 1);
                } else {
                    x = atoi(p1 + 1);
                }
            }
        }
        if (!map_id[0]) {
            set_status("SWITCH_MAP", "error", "empty map_id");
            return -1;
        }
        snprintf(cmd, sizeof(cmd), "'%s' switch '%s' '%s' %d %d",
                 map_bin, project_root, map_id, x, y);
        if (system(cmd) != 0) {
            set_status("SWITCH_MAP", "error", "switch failed");
            return -1;
        }
        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "switched to %s @ %d,%d", map_id, x, y);
        set_status("SWITCH_MAP", "ok", msg);
        return 0;
    }
    set_status(line, "error", "unknown command");
    return -1;
}

int main(void) {
    resolve_root();
    ensure_dirs();
    publish_bridge();

    char inbox[PATH_BUF];
    snprintf(inbox, sizeof(inbox), "%s/pieces/system/widget_cmds/inbox.txt", project_root);
    FILE *f = fopen(inbox, "r");
    if (!f) return 0;

    char lines[64][MAX_LINE];
    int n = 0;
    while (n < 64 && fgets(lines[n], MAX_LINE, f)) n++;
    fclose(f);

    int i = 0, processed = 0;
    for (; i < n && processed < 32; i++) {
        if (lines[i][0] == '\0' || lines[i][0] == '\n') continue;
        process_line(lines[i]);
        processed++;
    }
    f = fopen(inbox, "w");
    if (f) {
        for (; i < n; i++) fputs(lines[i], f);
        fclose(f);
    }
    (void)processed;
    return 0;
}
