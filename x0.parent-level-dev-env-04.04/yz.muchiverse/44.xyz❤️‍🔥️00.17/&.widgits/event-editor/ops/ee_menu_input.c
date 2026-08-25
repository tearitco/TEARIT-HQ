/* ee_menu_input - Event Editor method side-effects
 * CHTPM owns focus [>] and multi-digit jump (digit_accum).
 * This op handles KEY:n side effects that change state:
 *   5  = toggle Commands|Scratch
 *   7  = next page
 *   9-12 = set page
 *   29 OK / 30 Cancel stubs
 *   idle 0 = no-op
 *
 * Usage: ee_menu_input.+x <keycode>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int n = 0;
    if (f) {
        while (n < 64 && fgets(lines[n], MAX_LINE, f)) n++;
        fclose(f);
    }
    size_t klen = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(lines[i], key, klen) == 0 && lines[i][klen] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void bump(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/ee_screen_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static void set_msg(const char *m) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/ee_state.txt", project_root);
    write_kv(path, "last_message", m);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    char state[PATH_BUF];
    snprintf(state, sizeof(state), "%s/pieces/system/ee_state.txt", project_root);

    if (key == 0) return 0;

    /* Digit keys '1'-'9' and KEY:n from chtpm arrive as ASCII or as
     * multi-digit already resolved to focus — side effects on Enter
     * or explicit KEY: for methods 1-8 as single digits when fired. */

    /* Single-digit method keys (ASCII) */
    if (key >= '1' && key <= '8') {
        int n = key - '0';
        if (n == 5) {
            char vm[64];
            read_kv(state, "view_mode", vm, sizeof(vm));
            if (strcmp(vm, "scratch") == 0) {
                write_kv(state, "view_mode", "commands");
                set_msg("View: COMMANDS (RMMV list) — same graph as Scratch");
            } else {
                write_kv(state, "view_mode", "scratch");
                set_msg("View: SCRATCH blocks — same graph as Commands");
            }
            bump();
            return 0;
        }
        if (n == 1) { set_msg("Save: stub (would flush package to desktop)"); bump(); return 0; }
        if (n == 2) { set_msg("Load: stub (would open desktop package)"); bump(); return 0; }
        if (n == 3) { set_msg("Import→Muta: stub (ee_import_to_world)"); bump(); return 0; }
        if (n == 4) { set_msg("Export→Desktop: stub (ee_export_entity)"); bump(); return 0; }
        if (n == 6) { set_msg("Edit .pal: stub (open pal buffer INTERACT later)"); bump(); return 0; }
        if (n == 7) {
            char page[32];
            read_kv(state, "page", page, sizeof(page));
            int p = atoi(page);
            if (p < 1) p = 1;
            p = (p % 4) + 1;
            char ps[8];
            snprintf(ps, sizeof(ps), "%d", p);
            write_kv(state, "page", ps);
            set_msg("New/next page");
            bump();
            return 0;
        }
        if (n == 8) {
            set_msg("Help: digits multi-accumulate (17=content1). Tab/KEY:5 toggle view.");
            bump();
            return 0;
        }
    }

    /* Page keys 9 and KEY for higher numbers: chtpm may inject multi-digit
     * as separate keys; also accept keycode as raw method index if > 9
     * (some paths pass the n from KEY:n as decimal string only — we get
     * keycode). Handle '9' and pages via KEY injection of digit only. */
    if (key == '9') {
        write_kv(state, "page", "1");
        set_msg("Page 1");
        bump();
        return 0;
    }

    /* Enter: no special buffer typing in v1 */
    if (key == 10 || key == 13) {
        set_msg("Enter: CHTPM commits focused button (onClick)");
        bump();
        return 0;
    }

    return 0;
}
