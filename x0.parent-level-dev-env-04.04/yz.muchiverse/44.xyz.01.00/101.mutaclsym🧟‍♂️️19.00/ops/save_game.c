/* save_game - one verb, one binary, no shared headers.
 * Outer-bar action: copies the ENTIRE live pieces/world_01/ tree into a
 * freshly auto-numbered pieces/saves/save_N/world_01/ folder via
 * system("cp -r ...") - the same op-to-op/shell-out precedent already
 * established by choice.c calling other ops, and the same mechanism
 * real 1.TPMOS's op-ed uses for its own save_game_to_path(): mkdir -p
 * + cp -r into a named folder under games/ (see platform-vision.txt
 * and dox/01-cdda-architecture.md for the full citation of op-ed's
 * "Sovereign Architecture" - one self-contained folder per save).
 *
 * Auto-names save_N via an incrementing counter file (pieces/system/
 * save_serial_counter.txt, same pattern as item_serial_counter.txt),
 * matching op-ed's OWN auto-increment precedent for map files
 * (map_%04d.txt) - deliberately NOT op-ed's raw user-typed save path,
 * which had zero sanitization in the real source (a genuine
 * path-injection risk if a save name were ever attacker-controlled).
 * This project has no free-text input mechanism anywhere yet anyway,
 * so auto-naming is also just the only thing actually buildable today.
 *
 * Also writes save_meta.txt (turn count, unix timestamp) inside the
 * save folder, for the title screen's load list to display before the
 * player commits to a choice - op-ed itself has no such manifest (its
 * own metadata file, project.pdl, is only read AFTER loading, not
 * before - a raw file browser doesn't need a preview), but a numbered
 * picker menu does need something to show up front. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void log_message(const char *msg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/message_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", msg);
    fclose(f);
}

static int next_save_serial(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/save_serial_counter.txt", project_root);
    int serial = 0;
    FILE *f = fopen(path, "r");
    if (f) { if (fscanf(f, "%d", &serial) != 1) serial = 0; fclose(f); }
    serial++;
    f = fopen(path, "w");
    if (f) { fprintf(f, "%d\n", serial); fclose(f); }
    return serial;
}

int main(void) {
    resolve_root();

    int serial = next_save_serial();
    char save_dir[PATH_BUF];
    snprintf(save_dir, sizeof(save_dir), "%s/pieces/saves/save_%d", project_root, serial);
    mkdir(save_dir, 0755); /* mkdir -p equivalent isn't needed - pieces/saves/ already exists in the repo */

    char world_src[PATH_BUF], world_dst[PATH_BUF + 32];
    snprintf(world_src, sizeof(world_src), "%s/pieces/world_01", project_root);
    snprintf(world_dst, sizeof(world_dst), "%s/world_01", save_dir);

    char cmd[(PATH_BUF + 32) * 2 + 16];
    snprintf(cmd, sizeof(cmd), "cp -r '%s' '%s'", world_src, world_dst);
    int rc = system(cmd);

    char turn_path[PATH_BUF];
    snprintf(turn_path, sizeof(turn_path), "%s/pieces/world_01/map_start/state.txt", project_root);
    int turn = read_kv_int(turn_path, "turn", 0);

    char meta_path[PATH_BUF + 32];
    snprintf(meta_path, sizeof(meta_path), "%s/save_meta.txt", save_dir);
    FILE *mf = fopen(meta_path, "w");
    if (mf) {
        fprintf(mf, "turn=%d\n", turn);
        fprintf(mf, "saved_at=%ld\n", (long)time(NULL));
        fclose(mf);
    }

    char msg[128];
    if (rc == 0) snprintf(msg, sizeof(msg), "Saved (save_%d, turn %d).", serial, turn);
    else snprintf(msg, sizeof(msg), "Save failed.");
    log_message(msg);
    return 0;
}
