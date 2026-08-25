/* pdl_reader - SHARED OP (yz.muchiverse/2.muchi-verse/shared-ops/, see
 * shared-ops-refactor-plan.txt for the why). Reads a piece's own
 * piece.pdl METHOD table - ported from the real 1.TPMOS
 * pieces/system/pdl/pdl_reader.c (same CLI shape, and genuinely the
 * SAME kind of shared-across-projects service there: confirmed by
 * direct read of real 1.TPMOS's own `fuzz-op_manager.c` line 716,
 * which calls `'%s/pieces/system/pdl/+x/pdl_reader.+x' %s
 * list_methods` rather than keeping its own private copy under
 * `projects/fuzz-op/` - this file's whole existence at the
 * muchi-verse-family level mirrors that real structure, not a new
 * invention). Originally written for mutaclsym, adopted unchanged by
 * zoo_0000 (confirmed byte-identical via diff before this file
 * replaced both per-project copies), now also serving
 * muchipal-editor-0.0's own former `piece_viewer.c` use case (reading
 * an EXTERNAL project's piece.pdl by absolute path - see the
 * resolution rule below).
 *
 * Meant to be called by other ops (e.g. a project's own input
 * dispatcher) via popen/exec, same as any other op-to-op call in this
 * family - not wired directly into a pal script itself.
 *
 * Path resolution rule: if <piece_id_or_path> is itself an absolute
 * path (starts with '/') and a file exists there, it's used DIRECTLY
 * as the piece.pdl path - no project/world/map resolution at all. This
 * is what lets this same binary serve muchipal-editor-0.0 (which reads
 * piece.pdl files belonging to whatever EXTERNAL project the user has
 * open, not its own pieces/ tree, so it has no PRISC_PROJECT_ROOT-
 * relative piece_id to resolve in the first place). Otherwise, resolve
 * <piece_id> the normal way: scan world_NN/map_NN/<piece_id>/piece.pdl
 * under $PRISC_PROJECT_ROOT (matching system/prisc+x.c's own
 * resolve_piece_state_path() pattern), falling back to a flat
 * pieces/<piece_id>/ layout.
 *
 * Usage: pdl_reader.+x <piece_id_or_absolute_path> list_methods
 *        pdl_reader.+x <piece_id_or_absolute_path> list_methods_full
 *        pdl_reader.+x <piece_id_or_absolute_path> get_method <name>
 *        pdl_reader.+x <piece_id_or_absolute_path> has_method <name>
 * list_methods prints one method NAME per line, in file order (index 0
 * first). list_methods_full prints "name|handler" per line (the exact
 * shape muchipal-editor-0.0's own former piece_viewer.c produced).
 * get_method prints that name's handler path. has_method prints "1" or
 * "0". */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Same resolution order as system/prisc+x.c's resolve_piece_state_path():
 * scan world_NN/map_NN/<piece_id>/ first, fall back to a flat
 * pieces/<piece_id>/ layout. */
static int resolve_piece_pdl_path(const char *piece_id, char *out, size_t out_sz) {
    char pieces_dir[PATH_BUF];
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/pieces", project_root);

    DIR *worlds = opendir(pieces_dir);
    if (worlds) {
        struct dirent *w;
        while ((w = readdir(worlds)) != NULL) {
            if (strncmp(w->d_name, "world_", 6) != 0) continue;
            char maps_dir[PATH_BUF + 256];
            snprintf(maps_dir, sizeof(maps_dir), "%s/%s", pieces_dir, w->d_name);
            DIR *maps = opendir(maps_dir);
            if (!maps) continue;
            struct dirent *m;
            while ((m = readdir(maps)) != NULL) {
                if (strncmp(m->d_name, "map_", 4) != 0) continue;
                char candidate[PATH_BUF + 512];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(candidate, sizeof(candidate), "%s/%s/%s/piece.pdl", maps_dir, m->d_name, piece_id);
#pragma GCC diagnostic pop
                struct stat st;
                if (stat(candidate, &st) == 0) {
                    closedir(maps);
                    closedir(worlds);
                    snprintf(out, out_sz, "%s", candidate);
                    return 1;
                }
            }
            closedir(maps);
        }
        closedir(worlds);
    }

    snprintf(out, out_sz, "%s/pieces/%s/piece.pdl", project_root, piece_id);
    return 0;
}

/* Parses one "METHOD | name | handler" line. Returns 1 and fills
 * name/handler if this line is a METHOD row, 0 otherwise. */
static int parse_method_line(const char *line, char *name, size_t name_sz, char *handler, size_t handler_sz) {
    if (strncmp(line, "METHOD", 6) != 0) return 0;
    const char *p = line + 6;
    while (*p == ' ' || *p == '|') p++;
    const char *bar = strchr(p, '|');
    if (!bar) return 0;
    const char *name_end = bar;
    while (name_end > p && name_end[-1] == ' ') name_end--;
    snprintf(name, name_sz, "%.*s", (int)(name_end - p), p);

    p = bar + 1;
    while (*p == ' ') p++;
    char handler_buf[MAX_LINE];
    snprintf(handler_buf, sizeof(handler_buf), "%s", p);
    handler_buf[strcspn(handler_buf, "\r\n")] = '\0';
    size_t hlen = strlen(handler_buf);
    while (hlen > 0 && handler_buf[hlen - 1] == ' ') handler_buf[--hlen] = '\0';
    snprintf(handler, handler_sz, "%s", handler_buf);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <piece_id_or_absolute_path> <list_methods|list_methods_full|get_method|has_method> [name]\n", argv[0]);
        return 1;
    }
    const char *piece_id = argv[1];
    const char *action = argv[2];
    resolve_root();

    char pdl_path[PATH_BUF + 512];
    /* Absolute-path escape hatch - see this file's own header comment.
     * A real existing file at that exact path always wins over the
     * project-relative piece_id scan below. */
    if (piece_id[0] == '/') {
        struct stat st;
        if (stat(piece_id, &st) == 0) {
            snprintf(pdl_path, sizeof(pdl_path), "%s", piece_id);
        } else {
            resolve_piece_pdl_path(piece_id, pdl_path, sizeof(pdl_path));
        }
    } else {
        resolve_piece_pdl_path(piece_id, pdl_path, sizeof(pdl_path));
    }

    FILE *f = fopen(pdl_path, "r");
    if (!f) {
        if (strcmp(action, "has_method") == 0) printf("0\n");
        return 1;
    }

    if (strcmp(action, "list_methods") == 0) {
        char line[MAX_LINE], name[64], handler[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (parse_method_line(line, name, sizeof(name), handler, sizeof(handler))) {
                printf("%s\n", name);
            }
        }
        fclose(f);
        return 0;
    }

    if (strcmp(action, "list_methods_full") == 0) {
        char line[MAX_LINE], name[64], handler[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (parse_method_line(line, name, sizeof(name), handler, sizeof(handler))) {
                printf("%s|%s\n", name, handler);
            }
        }
        fclose(f);
        return 0;
    }

    if (argc < 4) {
        fprintf(stderr, "Usage: %s %s %s <name>\n", argv[0], piece_id, action);
        fclose(f);
        return 1;
    }
    const char *target_name = argv[3];

    char line[MAX_LINE], name[64], handler[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!parse_method_line(line, name, sizeof(name), handler, sizeof(handler))) continue;
        if (strcmp(name, target_name) == 0) { found = 1; break; }
    }
    fclose(f);

    if (strcmp(action, "has_method") == 0) {
        printf("%d\n", found);
        return 0;
    }
    if (strcmp(action, "get_method") == 0) {
        if (found) printf("%s\n", handler);
        return found ? 0 : 1;
    }

    fprintf(stderr, "Unknown action: %s\n", action);
    return 1;
}
