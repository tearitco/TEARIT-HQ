/* terms_hq_manager.c — db-hq Terms tab's real MANAGER binary
 * (2026-08-28, TPMOS-compliant, matches khtpm_hq_manager.c's pattern).
 *
 * Reads Terms data from RPG Maker MV's System.json (real game database),
 * extracts 5 real term categories (basic, commands, params, equipTypes, messages),
 * and publishes them to a state file for the renderer's existing generic
 * sidebar-injection path to consume (same shape as db-hq's common-events or bookmarks).
 *
 * Real data sources:
 *   - System.json path discovered via RMMV-ASSET-SOURCE-LOCATION.pdl
 *   - If file exists, parse JSON and extract "terms" object + equipTypes at top level
 *   - If file doesn't exist or parse fails, publish empty state (graceful)
 *
 * State file format: one term per line, with category headers for readability
 *   [Basic Terms]
 *   Level
 *   Lv
 *   HP
 *   ...
 *   [Commands]
 *   Fight
 *   Escape
 *   ...
 *   [Equipment Types]
 *   Weapon
 *   Shield
 *   ...
 *   [Messages]
 *   There was no effect on %1!
 *   %1 took %2 damage!
 *   ...
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define PATH_BUF 4096
#define MAX_TERMS 512
#define JSON_BUF_SIZE (1024 * 1024)  /* 1MB for System.json */

static char g_house_root[PATH_BUF];
static char g_package_dir[PATH_BUF];
static char g_state_path[PATH_BUF];
static char g_system_json_path[PATH_BUF];
static time_t g_system_json_mtime = 0;

/* Simple string parsing to extract a JSON string value.
 * Finds the next quote, reads until closing quote, handles basic escapes.
 * Returns pointer past the closing quote, or NULL if parsing fails. */
static const char *json_read_string(const char *src, char *out, size_t outsz) {
    if (*src != '"') return NULL;
    src++;  /* skip opening quote */
    size_t n = 0;
    while (n < outsz - 1 && *src && *src != '"') {
        if (*src == '\\' && src[1]) {
            /* Basic escape handling: \n, \t, \", \\, and pass through others */
            src++;
            if (*src == 'n') out[n++] = '\n';
            else if (*src == 't') out[n++] = '\t';
            else if (*src == '"') out[n++] = '"';
            else if (*src == '\\') out[n++] = '\\';
            else out[n++] = *src;  /* pass through unrecognized escapes */
        } else {
            out[n++] = *src;
        }
        src++;
    }
    out[n] = '\0';
    if (*src == '"') src++;  /* skip closing quote */
    return src;
}

/* Find and parse a JSON array of strings (e.g., ["a","b","c"]).
 * Returns count of strings extracted. */
static int json_read_string_array(const char *src, char **out, int max_items) {
    const char *p = strchr(src, '[');
    if (!p) return 0;
    p++;  /* skip '[' */

    int count = 0;
    while (count < max_items && *p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
        if (*p == ']') break;  /* end of array */
        if (*p == '"') {
            char buf[256];
            const char *next = json_read_string(p, buf, sizeof(buf));
            if (next) {
                out[count] = malloc(strlen(buf) + 1);
                if (out[count]) {
                    strcpy(out[count], buf);
                    count++;
                }
                p = next;
            } else {
                break;
            }
        } else if (*p == 'n' && strncmp(p, "null", 4) == 0) {
            /* Skip null entries in arrays */
            p += 4;
        } else {
            p++;
        }
    }
    return count;
}

/* Parse a JSON object with string values (e.g., {"key1":"val1","key2":"val2"}).
 * Returns count of values extracted (ignores keys, extracts just values).
 * Used for terms.messages and similar object-based term categories. */
static int json_read_string_object_values(const char *src, char **out, int max_items) {
    const char *p = strchr(src, '{');
    if (!p) return 0;
    p++;  /* skip '{' */

    int count = 0;
    while (count < max_items && *p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',' || *p == ':') p++;
        if (*p == '}') break;  /* end of object */
        if (*p == '"') {
            /* First quote is the key - skip it */
            char key[256];
            const char *next = json_read_string(p, key, sizeof(key));
            if (!next) break;
            p = next;

            /* Skip to colon and then value */
            while (*p && *p != ':') p++;
            if (*p != ':') break;
            p++;  /* skip ':' */
            while (*p == ' ' || *p == '\t') p++;

            /* Now read the value string */
            if (*p == '"') {
                char buf[512];  /* Messages can be longer */
                const char *val_next = json_read_string(p, buf, sizeof(buf));
                if (val_next) {
                    /* Only add non-empty values */
                    if (strlen(buf) > 0) {
                        out[count] = malloc(strlen(buf) + 1);
                        if (out[count]) {
                            strcpy(out[count], buf);
                            count++;
                        }
                    }
                    p = val_next;
                } else {
                    break;
                }
            } else {
                /* Non-string value, skip it */
                while (*p && *p != ',' && *p != '}') p++;
            }
        } else {
            p++;
        }
    }
    return count;
}

/* Read System.json and extract terms, publishing to state file */
static void publish_terms(void) {
    struct stat st;
    if (stat(g_system_json_path, &st) != 0) {
        /* File doesn't exist - publish empty state */
        FILE *f = fopen(g_state_path, "w");
        if (f) fclose(f);
        return;
    }

    /* Only re-parse if file has changed */
    if (st.st_mtime == g_system_json_mtime) return;
    g_system_json_mtime = st.st_mtime;

    /* Read entire file */
    FILE *f = fopen(g_system_json_path, "r");
    if (!f) return;
    char *json_buf = malloc(JSON_BUF_SIZE);
    if (!json_buf) { fclose(f); return; }
    size_t json_len = fread(json_buf, 1, JSON_BUF_SIZE - 1, f);
    fclose(f);
    json_buf[json_len] = '\0';

    /* Find "terms":{...} object */
    const char *terms_start = strstr(json_buf, "\"terms\":");
    if (!terms_start) {
        free(json_buf);
        /* No terms found - publish empty state */
        FILE *out = fopen(g_state_path, "w");
        if (out) fclose(out);
        return;
    }

    /* Publish to state file with category headers */
    FILE *out = fopen(g_state_path, "w");
    if (!out) { free(json_buf); return; }

    /* Extract and publish "basic" array */
    const char *basic_p = strstr(terms_start, "\"basic\":");
    if (basic_p) {
        fprintf(out, "[Basic Terms]\n");
        char *basic_items[128];
        int n_basic = json_read_string_array(basic_p, basic_items, 128);
        for (int i = 0; i < n_basic; i++) {
            fprintf(out, "%s\n", basic_items[i]);
            free(basic_items[i]);
        }
    }

    /* Extract and publish "commands" array */
    const char *cmds_p = strstr(terms_start, "\"commands\":");
    if (cmds_p) {
        fprintf(out, "[Commands]\n");
        char *cmd_items[128];
        int n_cmds = json_read_string_array(cmds_p, cmd_items, 128);
        for (int i = 0; i < n_cmds; i++) {
            if (cmd_items[i] && strlen(cmd_items[i]) > 0) {  /* skip nulls/empty */
                fprintf(out, "%s\n", cmd_items[i]);
            }
            if (cmd_items[i]) free(cmd_items[i]);
        }
    }

    /* Extract and publish "params" array */
    const char *params_p = strstr(terms_start, "\"params\":");
    if (params_p) {
        fprintf(out, "[Parameters]\n");
        char *param_items[128];
        int n_params = json_read_string_array(params_p, param_items, 128);
        for (int i = 0; i < n_params; i++) {
            if (param_items[i] && strlen(param_items[i]) > 0) {
                fprintf(out, "%s\n", param_items[i]);
            }
            if (param_items[i]) free(param_items[i]);
        }
    }

    /* Extract and publish "messages" object (key→value strings) */
    const char *msgs_p = strstr(terms_start, "\"messages\":");
    if (msgs_p) {
        fprintf(out, "[Messages]\n");
        char *msg_items[256];
        int n_msgs = json_read_string_object_values(msgs_p, msg_items, 256);
        for (int i = 0; i < n_msgs; i++) {
            if (msg_items[i]) {
                fprintf(out, "%s\n", msg_items[i]);
                free(msg_items[i]);
            }
        }
    }

    /* Extract and publish "equipTypes" array (top-level key in System.json) */
    const char *equip_p = strstr(json_buf, "\"equipTypes\":");
    if (equip_p) {
        fprintf(out, "[Equipment Types]\n");
        char *equip_items[128];
        int n_equip = json_read_string_array(equip_p, equip_items, 128);
        for (int i = 0; i < n_equip; i++) {
            if (equip_items[i] && strlen(equip_items[i]) > 0) {
                fprintf(out, "%s\n", equip_items[i]);
            }
            if (equip_items[i]) free(equip_items[i]);
        }
    }

    fclose(out);
    free(json_buf);
}

/* Discover System.json path - real, in-house copy checked FIRST
 * (2026-08-28, same convention as &.widgits/palettes/tilesets/rmmv/'s
 * own copied PNGs - never depend on an external drive staying mounted
 * for something this house can just keep a real local copy of). Only
 * falls through to the external-mount discovery below if the local
 * copy genuinely isn't there. */
static int discover_system_json_path(void) {
    snprintf(g_system_json_path, sizeof(g_system_json_path), "%s/&.widgits/db-hq/data/System.json", g_house_root);
    if (access(g_system_json_path, F_OK) == 0) return 1;

    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/#.#.✅️.cal-user-sum/1.^V-hq/RMMV-ASSET-SOURCE-LOCATION.pdl", g_house_root);

    char mount_point[PATH_BUF] = "";
    char project_root[PATH_BUF] = "";
    char line[PATH_BUF];

    FILE *f = fopen(pdl_path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            /* Parse PDL format: SECTION | KEY | VALUE (with spaces around |) */
            char *val = strstr(line, " | ");
            if (!val) continue;
            *val = '\0';
            val += 3;

            /* Trim whitespace from value */
            while (*val == ' ') val++;
            size_t vlen = strlen(val);
            while (vlen > 0 && (val[vlen-1] == ' ' || val[vlen-1] == '\n' || val[vlen-1] == '\r')) {
                val[--vlen] = '\0';
            }

            if (strstr(line, "mount_point")) {
                snprintf(mount_point, sizeof(mount_point), "%s", val);
            } else if (strstr(line, "project_root")) {
                if (!strstr(val, "<")) {  /* skip template references */
                    snprintf(project_root, sizeof(project_root), "%s", val);
                }
            }
        }
        fclose(f);
    }

    /* Try PDL project_root first */
    if (project_root[0] != '\0') {
        snprintf(g_system_json_path, sizeof(g_system_json_path), "%s/data/System.json", project_root);
        if (access(g_system_json_path, F_OK) == 0) {
            return 1;
        }
    }

    /* Fallback: search for System.json in mount point recursively */
    if (mount_point[0] != '\0' && access(mount_point, F_OK) == 0) {
        /* Try finding System.json by walking the directory tree (generous search depth) */
        char search_cmd[PATH_BUF * 2];
        FILE *fp;
        snprintf(search_cmd, sizeof(search_cmd),
            "find \"%s\" -maxdepth 20 -name 'System.json' -type f 2>/dev/null | head -1",
            mount_point);
        fp = popen(search_cmd, "r");
        if (fp) {
            if (fgets(g_system_json_path, sizeof(g_system_json_path), fp)) {
                /* Remove trailing newline */
                size_t len = strlen(g_system_json_path);
                if (len > 0 && g_system_json_path[len-1] == '\n') {
                    g_system_json_path[--len] = '\0';
                }
                pclose(fp);
                if (access(g_system_json_path, F_OK) == 0) {
                    return 1;
                }
            }
            pclose(fp);
        }
    }

    g_system_json_path[0] = '\0';
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "terms_hq_manager: usage: <house_root> [package_dir] [system_json_path]\n");
        return 1;
    }

    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_package_dir, sizeof(g_package_dir), "%s", argc >= 3 ? argv[2] : "#.desktop");
    /* REAL FIX 2026-08-28 (found live during verification) - g_package_dir
     * is already a real, ABSOLUTE path when the renderer launches a
     * module (dbhq_launch_module() passes its own real g_package_dir
     * straight through, see khtpm_hq_manager.c/palettes_manager.c's own
     * identical convention: "%s/rmmv_active.txt", g_package_dir - no
     * house_root prefix). Concatenating house_root+"/"+package_dir here
     * produced a garbage nested path that never resolved to a writable
     * directory, so the state file silently never got created - exactly
     * the "empty output" symptom found in first testing. Only join with
     * house_root for the literal "#.desktop" default (house-root-
     * relative by convention); an absolute package_dir is used as-is. */
    if (g_package_dir[0] == '/')
        snprintf(g_state_path, sizeof(g_state_path), "%s/db_hq_terms.state.txt", g_package_dir);
    else
        snprintf(g_state_path, sizeof(g_state_path), "%s/%s/db_hq_terms.state.txt", g_house_root, g_package_dir);

    /* If System.json path provided directly (for testing), use it */
    if (argc >= 4) {
        snprintf(g_system_json_path, sizeof(g_system_json_path), "%s", argv[3]);
        if (access(g_system_json_path, F_OK) != 0) {
            /* Path doesn't exist - publish empty state and exit */
            FILE *f = fopen(g_state_path, "w");
            if (f) fclose(f);
            return 0;
        }
    } else if (!discover_system_json_path()) {
        /* No System.json found - publish empty state and exit */
        FILE *f = fopen(g_state_path, "w");
        if (f) fclose(f);
        return 0;
    }

    /* Poll and publish terms periodically */
    for (;;) {
        publish_terms();
        usleep(400000);  /* 400ms poll - matches db-hq manager pattern */
    }
    return 0;
}
