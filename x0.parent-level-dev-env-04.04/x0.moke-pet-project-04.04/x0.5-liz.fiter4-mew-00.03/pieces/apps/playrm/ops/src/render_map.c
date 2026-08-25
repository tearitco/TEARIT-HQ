/*
 * Op: render_map
 * Usage: ./+x/render_map.+x
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAP_ROWS 10
#define MAP_COLS 20
#define MAX_ENTITIES 100
#define MAX_PATH 4096
#define MAX_CMD 16384
#define MAX_LINE 1024

typedef struct {
    char id[64];
    int x, y, z;
    char icon[8]; /* UTF-8 string */
} Entity;

char terrain[MAP_ROWS][MAP_COLS][8]; /* Grid of UTF-8 strings */
char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";
char app_title[256] = "PIECEMARK PLAYER";
char active_map_file[MAX_LINE] = "map_01.txt";
int current_z = 0;
int map_offset_x = 0;
int map_offset_y = 0;
int emoji_mode = 0;  /* 0 = ASCII, 1 = Emoji */

/* ASCII <-> Emoji conversion table */
typedef struct {
    const char *ascii;
    const char *emoji;
} GlyphMap;

static const GlyphMap glyph_conversion[] = {
    {"#", "🧱"},  /* Wall */
    {".", "⬛"},  /* Empty */
    {"R", "🧟"},  /* Zombie/Resource */
    {"T", "🎯"},  /* Target/Tree */
    {"@", "🐶"},  /* Player/Pet */
    {"&", "🐱"},  /* NPC */
    {"Z", "🧟"},  /* Zombie */
    {"X", "🎯"},  /* Xlector/Selector */
    {"?", "❓"},  /* Unknown */
    {"!", "🔥"},  /* Alert */
    {"$", "💰"},  /* Money */
    {"H", "🏠"},  /* Home */
    {"C", "🏰"},  /* Castle */
    {"G", "🟩"},  /* Grass/Green */
    {NULL, NULL}
};

/* Convert a single glyph based on emoji_mode */
const char* convert_glyph(const char *ascii_glyph) {
    if (!emoji_mode) return ascii_glyph;
    
    for (int i = 0; glyph_conversion[i].ascii != NULL; i++) {
        if (strcmp(ascii_glyph, glyph_conversion[i].ascii) == 0) {
            return glyph_conversion[i].emoji;
        }
    }
    return ascii_glyph;  /* No conversion found, return original */
}

char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line), *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) strncpy(project_root, v, MAX_PATH-1);
            }
        }
        fclose(kvp);
    }

    /* 1. Get Project ID from global manager state first */
    char *p_mgr_path = NULL;
    if (asprintf(&p_mgr_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(p_mgr_path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "project_id") == 0) strncpy(current_project, trim_str(eq+1), sizeof(current_project)-1);
                }
            }
            fclose(f);
        }
        free(p_mgr_path);
    }

    /* 2. Load Project Defaults (PDL) */
    char *pdl_path = NULL;
    if (asprintf(&pdl_path, "%s/projects/%s/project.pdl", project_root, current_project) != -1) {
        FILE *pf = fopen(pdl_path, "r");
        if (pf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), pf)) {
                if (strstr(line, "starting_map")) {
                    char *pipe = strrchr(line, '|');
                    if (pipe) snprintf(active_map_file, sizeof(active_map_file), "%s.txt", trim_str(pipe + 1));
                }
                else if (strstr(line, "title")) {
                    char *pipe = strrchr(line, '|');
                    if (pipe) strncpy(app_title, trim_str(pipe + 1), 255);
                }
            }
            fclose(pf);
        }
        free(pdl_path);
    }

    /* 3. FINAL OVERRIDE: Active session state from Manager (includes scrolling and specific map) */
    if (asprintf(&p_mgr_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(p_mgr_path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line), *v = trim_str(eq + 1);
                    if (strcmp(k, "map_offset_x") == 0) map_offset_x = atoi(v);
                    else if (strcmp(k, "map_offset_y") == 0) map_offset_y = atoi(v);
                    else if (strcmp(k, "current_map") == 0) strncpy(active_map_file, v, sizeof(active_map_file)-1);
                }
            }
            fclose(f);
        }
        free(p_mgr_path);
    }
    
    /* Z-LEVEL: Read pos_z directly from xlector state (like fuzzpet_app) */
    char *sel_path = NULL;
    if (asprintf(&sel_path, "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project) != -1) {
        FILE *f = fopen(sel_path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "pos_z") == 0) { current_z = atoi(trim_str(eq + 1)); break; }
                }
            }
            fclose(f);
        }
        free(sel_path);
    }
    if (current_z < 0) current_z = 0;

    /* EMOJI MODE: Discover from active project's state (TPM Direct Mirror Access)
     * Check multiple standard locations in order of precedence:
     * 1. Project's app-specific state (e.g., projects/op-ed/pieces/emoji_state.txt)
     * 2. Project's manager state (e.g., projects/{project}/pieces/emoji_mode.txt)
     * 3. Player app manager state (global fallback)
     */
    
    /* Try project-specific emoji state file */
    char *emoji_path = NULL;
    if (asprintf(&emoji_path, "%s/projects/%s/pieces/emoji_mode.txt", project_root, current_project) != -1) {
        FILE *f = fopen(emoji_path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "emoji_mode") == 0) { emoji_mode = atoi(trim_str(eq + 1)); break; }
                }
            }
            fclose(f);
        }
        free(emoji_path);
    }
    
    /* Fallback: Check manager state for emoji_mode field */
    if (emoji_mode == 0) {
        char *mgr_path = NULL;
        if (asprintf(&mgr_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
            FILE *f = fopen(mgr_path, "r");
            if (f) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), f)) {
                    char *eq = strchr(line, '=');
                    if (eq) {
                        *eq = '\0';
                        if (strcmp(trim_str(line), "emoji_mode") == 0) { emoji_mode = atoi(trim_str(eq + 1)); break; }
                    }
                }
                fclose(f);
            }
            free(mgr_path);
        }
    }
}

void get_piece_state(const char* piece_id, const char* key, char* out_val) {
    strcpy(out_val, "0");
    if (!piece_id || !key) return;
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id) == -1) return;
    if (access(path, F_OK) != 0) {
        free(path);
        if (asprintf(&path, "%s/pieces/world/map_01/%s/state.txt", project_root, piece_id) == -1) return;
    }
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return; }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(trim_str(line), key) == 0) { strcpy(out_val, trim_str(eq + 1)); break; }
    }
    fclose(f); free(path);
}

int get_state_from_file(const char* state_path, const char* key, char* out_val, size_t out_sz) {
    if (!state_path || !key || !out_val || out_sz == 0) return 0;
    out_val[0] = '\0';
    FILE *f = fopen(state_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(trim_str(line), key) == 0) {
            snprintf(out_val, out_sz, "%s", trim_str(eq + 1));
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int get_piece_state_from_dir(const char* piece_dir, const char* key, char* out_val, size_t out_sz) {
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", piece_dir);
    return get_state_from_file(state_path, key, out_val, out_sz);
}

int is_directory_path(const char* path) {
    struct stat st;
    if (!path) return 0;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

void normalize_map_token(const char* in, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!in) return;
    snprintf(out, out_sz, "%s", in);
    char *dot = strstr(out, ".txt");
    if (dot) *dot = '\0';
    char *z = strstr(out, "_z");
    if (z) {
        char *p = z + 2;
        int digits = 0;
        while (*p && isdigit((unsigned char)*p)) { digits = 1; p++; }
        if (digits && *p == '\0') *z = '\0';
    }
}

int map_names_match(const char* a, const char* b) {
    if (!a || !b || !*a || !*b) return 0;
    if (strcmp(a, b) == 0) return 1;
    char na[MAX_LINE], nb[MAX_LINE];
    normalize_map_token(a, na, sizeof(na));
    normalize_map_token(b, nb, sizeof(nb));
    return (na[0] && nb[0] && strcmp(na, nb) == 0);
}

int find_entity_by_id(Entity entities[], int count, const char* id) {
    for (int i = 0; i < count; i++) {
        if (strcmp(entities[i].id, id) == 0) return i;
    }
    return -1;
}

void infer_icon_from_type(const char* type, char* out_icon, size_t out_sz) {
    if (!type || !*type) { snprintf(out_icon, out_sz, "?"); return; }
    if (strcmp(type, "player") == 0 || strcmp(type, "pet") == 0) snprintf(out_icon, out_sz, "@");
    else if (strcmp(type, "xlector") == 0) snprintf(out_icon, out_sz, "X");
    else if (strcmp(type, "npc") == 0) snprintf(out_icon, out_sz, "&");
    else if (strcmp(type, "chest") == 0) snprintf(out_icon, out_sz, "T");
    else if (strcmp(type, "zombie") == 0) snprintf(out_icon, out_sz, "Z");
    else snprintf(out_icon, out_sz, "?");
}

int add_or_replace_entity(Entity entities[], int* p_count, const char* piece_id, const char* piece_dir, int prefer_replace) {
    if (!p_count || !piece_id || !piece_dir) return 0;

    char val[MAX_LINE];
    if (!get_piece_state_from_dir(piece_dir, "on_map", val, sizeof(val))) snprintf(val, sizeof(val), "0");
    if (atoi(val) != 1 && strcmp(piece_id, "xlector") != 0) return 0;

    if (get_piece_state_from_dir(piece_dir, "pos_z", val, sizeof(val))) {
        if (atoi(val) != current_z) return 0;
    } else if (current_z != 0) {
        return 0;
    }

    if (strcmp(piece_id, "xlector") != 0) {
        char map_id[MAX_LINE] = "";
        if (get_piece_state_from_dir(piece_dir, "map_id", map_id, sizeof(map_id))) {
            if (strlen(map_id) > 0 && !map_names_match(map_id, active_map_file)) return 0;
        }
    }

    if (!get_piece_state_from_dir(piece_dir, "pos_x", val, sizeof(val))) return 0;
    int ex = atoi(val);
    if (!get_piece_state_from_dir(piece_dir, "pos_y", val, sizeof(val))) return 0;
    int ey = atoi(val);

    if (!(ex >= map_offset_x && ex < map_offset_x + MAP_COLS && ey >= map_offset_y && ey < map_offset_y + MAP_ROWS)) {
        return 0;
    }

    int idx = find_entity_by_id(entities, *p_count, piece_id);
    if (idx >= 0 && !prefer_replace) return 0;
    if (idx < 0) {
        if (*p_count >= MAX_ENTITIES) return 0;
        idx = *p_count;
        (*p_count)++;
    }

    snprintf(entities[idx].id, sizeof(entities[idx].id), "%s", piece_id);
    entities[idx].x = ex - map_offset_x;
    entities[idx].y = ey - map_offset_y;

    if (get_piece_state_from_dir(piece_dir, "icon", val, sizeof(val)) && strlen(val) > 0) {
        strncpy(entities[idx].icon, val, sizeof(entities[idx].icon) - 1);
        entities[idx].icon[sizeof(entities[idx].icon) - 1] = '\0';
    } else {
        char type[MAX_LINE] = "";
        get_piece_state_from_dir(piece_dir, "type", type, sizeof(type));
        infer_icon_from_type(type, entities[idx].icon, sizeof(entities[idx].icon));
    }
    return 1;
}

void set_state_field(const char* key, const char* val) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/state.txt", project_root) == -1) return;
    char lines[200][MAX_LINE];
    int line_count = 0, found = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[line_count], sizeof(lines[0]), f) && line_count < 199) {
            char *eq = strchr(lines[line_count], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(lines[line_count]), key) == 0) {
                    snprintf(lines[line_count], sizeof(lines[0]), "%s=%s\n", key, val);
                    found = 1;
                } else *eq = '=';
            }
            line_count++;
        }
        fclose(f);
    }
    if (!found && line_count < 200) snprintf(lines[line_count++], sizeof(lines[0]), "%s=%s\n", key, val);
    f = fopen(path, "w");
    if (f) { for (int i = 0; i < line_count; i++) fputs(lines[i], f); fclose(f); }
    free(path);
}

int main() {
    resolve_paths();
    for (int y = 0; y < MAP_ROWS; y++) { for (int x = 0; x < MAP_COLS; x++) strcpy(terrain[y][x], "."); }
    char *map_path = NULL; FILE *mf = NULL;
    
    /* Z-LEVEL SUPPORT: Load map_{base}_z{current_z}.txt */
    char map_with_z[MAX_LINE];
    char *base = strdup(active_map_file);
    /* Remove existing _zN suffix if present */
    char *underscore_z = strstr(base, "_z");
    if (underscore_z) *underscore_z = '\0';
    snprintf(map_with_z, sizeof(map_with_z), "%s_z%d.txt", base, current_z);
    free(base);
    
    if (asprintf(&map_path, "%s/projects/%s/maps/%s", project_root, current_project, map_with_z) != -1) {
        mf = fopen(map_path, "r");
        if (!mf) { free(map_path); map_path = NULL; }
    }
    /* Fallback to original map name */
    if (!mf && asprintf(&map_path, "%s/projects/%s/maps/%s", project_root, current_project, active_map_file) != -1) {
        mf = fopen(map_path, "r");
        if (!mf) { free(map_path); map_path = NULL; }
    }
    
    if (mf) {
        char line[MAX_LINE];
        for (int i = 0; i < map_offset_y && fgets(line, sizeof(line), mf); i++);
        for (int y = 0; y < MAP_ROWS && fgets(line, sizeof(line), mf); y++) {
            char *p = line; int skip_x = map_offset_x;
            while (skip_x > 0 && *p && *p != '\n' && *p != '\r') {
                int len = ((*p & 0x80) == 0) ? 1 : ((*p & 0xE0) == 0xC0) ? 2 : ((*p & 0xF0) == 0xE0) ? 3 : 4;
                p += len; skip_x--;
            }
            for (int x = 0; x < MAP_COLS && *p && *p != '\n' && *p != '\r'; x++) {
                int len = ((*p & 0x80) == 0) ? 1 : ((*p & 0xE0) == 0xC0) ? 2 : ((*p & 0xF0) == 0xE0) ? 3 : 4;
                strncpy(terrain[y][x], p, len); terrain[y][x][len] = '\0'; p += len;
            }
        }
        fclose(mf);
    }
    if (map_path) free(map_path);
    
    Entity entities[MAX_ENTITIES];
    int entity_count = 0;
    int xlector_idx = -1;
    char *pieces_dir = NULL;
    if (asprintf(&pieces_dir, "%s/projects/%s/pieces", project_root, current_project) != -1) {
        DIR *root = opendir(pieces_dir);
        if (root) {
            struct dirent *wentry;
            while ((wentry = readdir(root)) != NULL) {
                if (wentry->d_name[0] == '.') continue;
                if (strncmp(wentry->d_name, "world_", 6) != 0) continue;

                char world_path[MAX_PATH];
                snprintf(world_path, sizeof(world_path), "%s/%s", pieces_dir, wentry->d_name);
                if (!is_directory_path(world_path)) continue;

                DIR *wdir = opendir(world_path);
                if (!wdir) continue;
                struct dirent *mentry;
                while ((mentry = readdir(wdir)) != NULL) {
                    if (mentry->d_name[0] == '.') continue;
                    if (strncmp(mentry->d_name, "map_", 4) != 0) continue;
                    if (!map_names_match(mentry->d_name, active_map_file)) continue;

                    char map_dir[MAX_PATH];
                    snprintf(map_dir, sizeof(map_dir), "%.*s/%.*s", 2000, world_path, 2000, mentry->d_name);
                    if (!is_directory_path(map_dir)) continue;

                    DIR *mdir = opendir(map_dir);
                    if (!mdir) continue;
                    struct dirent *pentry;
                    while ((pentry = readdir(mdir)) != NULL && entity_count < MAX_ENTITIES) {
                        if (pentry->d_name[0] == '.') continue;
                        char piece_path[MAX_PATH];
                        snprintf(piece_path, sizeof(piece_path), "%.*s/%.*s", 2000, map_dir, 2000, pentry->d_name);
                        if (!is_directory_path(piece_path)) continue;
                        add_or_replace_entity(entities, &entity_count, pentry->d_name, piece_path, 1);
                    }
                    closedir(mdir);
                }
                closedir(wdir);
            }
            closedir(root);
        }

        root = opendir(pieces_dir);
        if (root) {
            struct dirent *entry;
            while ((entry = readdir(root)) != NULL && entity_count < MAX_ENTITIES) {
                if (entry->d_name[0] == '.') continue;
                if (strncmp(entry->d_name, "world_", 6) == 0) continue;
                char piece_path[MAX_PATH];
                snprintf(piece_path, sizeof(piece_path), "%s/%s", pieces_dir, entry->d_name);
                if (!is_directory_path(piece_path)) continue;
                add_or_replace_entity(entities, &entity_count, entry->d_name, piece_path, 0);
            }
            closedir(root);
        }
        free(pieces_dir);
    }
    
    /* Move xlector to end of entity list (renders behind other entities) */
    xlector_idx = find_entity_by_id(entities, entity_count, "xlector");
    if (xlector_idx >= 0 && xlector_idx < entity_count - 1) {
        Entity temp = entities[xlector_idx];
        for (int i = xlector_idx; i < entity_count - 1; i++) {
            entities[i] = entities[i + 1];
        }
        entities[entity_count - 1] = temp;
    }
    
    /* CPU-SAFE: Use asprintf for path construction to avoid truncation warnings */
    char *view_path = NULL;
    if (asprintf(&view_path, "%s/pieces/apps/player_app/view.txt", project_root) == -1) return 1;
    char *tmp_path = NULL;
    if (asprintf(&tmp_path, "%s.tmp", view_path) == -1) { free(view_path); return 1; }
    
    /* Build new view content in memory first */
    char new_view[4096] = "";
    char line_buf[256];
    
    /* Header with Z-level */
    snprintf(line_buf, sizeof(line_buf), "+=========================================================+\n");
    strcat(new_view, line_buf);
    snprintf(line_buf, sizeof(line_buf), "| [Z-LEVEL]: %-3d                                             |\n", current_z);
    strcat(new_view, line_buf);
    snprintf(line_buf, sizeof(line_buf), "+=========================================================+\n");
    strcat(new_view, line_buf);

    for (int y = 0; y < MAP_ROWS; y++) {
        snprintf(line_buf, sizeof(line_buf), "|  ");
        for (int x = 0; x < MAP_COLS; x++) {
            int found = -1;
            int xlector_found = -1;
            /* Find entity at this position, prefer non-xlector */
            for (int i = 0; i < entity_count; i++) {
                if (entities[i].x == x && entities[i].y == y) {
                    if (strcmp(entities[i].id, "xlector") == 0) {
                        xlector_found = i;
                    } else {
                        found = i;
                        break;  /* Found non-xlector, use it */
                    }
                }
            }
            /* If only xlector at this position, use it */
            if (found == -1 && xlector_found >= 0) found = xlector_found;

            if (found != -1) {
                /* Entity icon - apply emoji conversion */
                const char *icon = convert_glyph(entities[found].icon);
                snprintf(line_buf + strlen(line_buf), sizeof(line_buf) - strlen(line_buf), "%s", icon);
            } else {
                /* Terrain - apply emoji conversion */
                const char *tile = convert_glyph(terrain[y][x]);
                snprintf(line_buf + strlen(line_buf), sizeof(line_buf) - strlen(line_buf), "%s", tile);
            }
        }
        snprintf(line_buf + strlen(line_buf), sizeof(line_buf) - strlen(line_buf), "                                     |\n");
        strcat(new_view, line_buf);
    }
    
    /* DEDUPLICATION: Only write if view changed */
    int should_write = 1;
    FILE *old_view = fopen(view_path, "r");
    if (old_view) {
        char old_view_content[4096] = "";
        char old_line[256];
        while (fgets(old_line, sizeof(old_line), old_view)) {
            strcat(old_view_content, old_line);
        }
        fclose(old_view);
        if (strcmp(old_view_content, new_view) == 0) {
            should_write = 0;  /* No change, skip write */
        }
    }
    
    if (should_write) {
        FILE *vp = fopen(tmp_path, "w");
        if (vp) {
            fputs(new_view, vp);
            fclose(vp);
            rename(tmp_path, view_path);
            /* TICK MARKER: Signal parser that frame changed.
             * This is the ONLY render trigger for game layouts.
             * The parser polls frame_changed.txt size — when it grows, compose_frame() fires once.
             * DO NOT add additional triggers (state_changed, view_changed, etc.) — they cause
             * redundant renders. If you need to force a render, write to this marker. */
            char *fc = NULL;
            if (asprintf(&fc, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
                FILE *fcf = fopen(fc, "a");
                if (fcf) { fprintf(fcf, "T\n"); fclose(fcf); }
                free(fc);
            }
        }
    }
    free(view_path);
    free(tmp_path);
    
    set_state_field("project_id", current_project); 
    set_state_field("app_title", app_title);
    set_state_field("emoji_mode", emoji_mode ? "ON" : "OFF");
    return 0;
}
