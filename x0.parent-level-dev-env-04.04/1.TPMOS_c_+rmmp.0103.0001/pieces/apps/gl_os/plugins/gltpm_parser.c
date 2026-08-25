/*
 * gltpm_parser.c - minimal GLTPM scene loader
 * Included directly by gl_desktop.c for the first vertical slice.
 */

#define MAX_GLTPM_VARS 256
#define MAX_GLTPM_VALUE 65536
#define MAX_GLTPM_LEGEND 128
#define MAX_GLTPM_BUFFER 1048576

typedef struct {
    char name[64];
    char value[MAX_GLTPM_VALUE];
} GLTPMVar;

typedef struct {
    char glyph;
    char type[16]; /* "tile" or "sprite" */
    char asset_id[64];
} GLTPMLegend;

static void gltpm_scene_reset(GLTPMScene *scene) {
    if (!scene) return;
    memset(scene, 0, sizeof(*scene));
    strcpy(scene->title, "GLTPM Window");
    scene->camera_mode = 4;
    scene->bg_color[0] = 0.08f;
    scene->bg_color[1] = 0.10f;
    scene->bg_color[2] = 0.16f;
}

static char* gltpm_trim(char *s) {
    char *end = NULL;
    if (!s) return s;

    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (*s == '\0') return s;

    end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end-- = '\0';
    }
    return s;
}

static void gltpm_load_vars_from_file(const char *path, GLTPMVar vars[], int *var_count) {
    FILE *f = NULL;
    char line[MAX_LINE];

    if (!path || !vars || !var_count) return;
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f) && *var_count < MAX_GLTPM_VARS) {
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        strncpy(vars[*var_count].name, gltpm_trim(line), sizeof(vars[*var_count].name) - 1);
        strncpy(vars[*var_count].value, gltpm_trim(eq + 1), MAX_GLTPM_VALUE - 1);
        vars[*var_count].name[sizeof(vars[*var_count].name) - 1] = '\0';
        vars[*var_count].value[MAX_GLTPM_VALUE - 1] = '\0';
        (*var_count)++;
    }

    fclose(f);
}

static const char* gltpm_get_var(GLTPMVar vars[], int var_count, const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) return vars[i].value;
    }
    return "";
}

static void gltpm_substitute_vars(const char *src, char *dst, size_t dst_size,
                                  GLTPMVar vars[], int var_count) {
    const char *p = src;
    char *out = dst;
    size_t remaining = dst_size;

    if (!src || !dst || dst_size == 0) return;
    dst[0] = '\0';

    while (*p && remaining > 1) {
        if (p[0] == '$' && p[1] == '{') {
            const char *end = strchr(p, '}');
            if (end) {
                char key[64];
                size_t len = (size_t)(end - (p + 2));
                const char *value = NULL;

                if (len >= sizeof(key)) len = sizeof(key) - 1;
                memcpy(key, p + 2, len);
                key[len] = '\0';
                value = gltpm_get_var(vars, var_count, key);

                while (*value && remaining > 1) {
                    if (*value == '\\' && *(value+1) == 'n') {
                        *out++ = '\n';
                        value += 2;
                    } else {
                        *out++ = *value++;
                    }
                    remaining--;
                }
                p = end + 1;
                continue;
            }
        }

        *out++ = *p++;
        remaining--;
    }

    *out = '\0';
}

static int gltpm_extract_attr(const char *line, const char *name, char *dst, size_t dst_size) {
    char needle[64];
    const char *start = NULL;
    const char *end = NULL;
    size_t len = 0;

    if (!line || !name || !dst || dst_size == 0) return 0;
    snprintf(needle, sizeof(needle), "%s=\"", name);
    start = strstr(line, needle);
    if (!start) return 0;

    start += strlen(needle);
    end = strchr(start, '"');
    if (!end) return 0;

    len = (size_t)(end - start);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
    return 1;
}

static void gltpm_parse_rgb(const char *value, float rgb[3], float fallback_r,
                            float fallback_g, float fallback_b) {
    rgb[0] = fallback_r;
    rgb[1] = fallback_g;
    rgb[2] = fallback_b;

    if (!value || strlen(value) == 0) return;

    if (value[0] == '#') {
        /* Hex format: #RRGGBB */
        if (strlen(value) >= 7) {
            int r, g, b;
            if (sscanf(value + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
                rgb[0] = (float)r / 255.0f;
                rgb[1] = (float)g / 255.0f;
                rgb[2] = (float)b / 255.0f;
            }
        }
    } else {
        int r = 0, g = 0, b = 0;
        if (sscanf(value, "%d,%d,%d", &r, &g, &b) == 3) {
            rgb[0] = (float)r / 255.0f;
            rgb[1] = (float)g / 255.0f;
            rgb[2] = (float)b / 255.0f;
        }
    }
}

static void gltpm_build_abs_path(char *dst, size_t dst_size,
                                 const char *root, const char *rel) {
    if (!dst || dst_size == 0) return;
    if (!root || !rel) {
        dst[0] = '\0';
        return;
    }

    if (rel[0] == '/') {
        strncpy(dst, rel, dst_size - 1);
        dst[dst_size - 1] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s/%s", root, rel);
}

static void gltpm_load_tile_metadata(const char *project_root, const char *project_id,
                                     GLTPMTile *tile) {
    char path[MAX_PATH];
    FILE *f = NULL;
    char line[MAX_LINE];

    if (!project_root || !project_id || !tile) return;

    snprintf(path, sizeof(path), "%s/projects/%s/assets/tiles/%s.tile.txt",
             project_root, project_id, tile->tile_id);

    strcpy(tile->ascii, "?");
    strcpy(tile->unicode, "?");
    tile->extrude = 1.0f;
    tile->color[0] = 0.35f;
    tile->color[1] = 0.45f;
    tile->color[2] = 0.65f;

    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        char *key = NULL;
        char *value = NULL;
        if (!eq) continue;
        *eq = '\0';
        key = gltpm_trim(line);
        value = gltpm_trim(eq + 1);

        if (strcmp(key, "ascii") == 0) {
            strncpy(tile->ascii, value, sizeof(tile->ascii) - 1);
        } else if (strcmp(key, "unicode") == 0) {
            strncpy(tile->unicode, value, sizeof(tile->unicode) - 1);
        } else if (strcmp(key, "rgb_top") == 0) {
            gltpm_parse_rgb(value, tile->color, tile->color[0], tile->color[1], tile->color[2]);
        } else if (strcmp(key, "extrude") == 0) {
            tile->extrude = (float)atof(value);
            if (tile->extrude <= 0.0f) tile->extrude = 1.0f;
        }
    }

    fclose(f);
}

static void gltpm_load_artifact(const char *path, unsigned char mask[8][8]) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = gltpm_trim(line);
        char *val = gltpm_trim(eq + 1);
        if (key[0] == 'z' && isdigit(key[1])) {
            int z = atoi(key + 1);
            if (z >= 0 && z < 8) {
                /* Parse 8 hex bytes: FF,81,00... */
                char *saveptr;
                char *token = strtok_r(val, ",", &saveptr);
                for (int i = 0; i < 8 && token; i++) {
                    mask[z][i] = (unsigned char)strtol(token, NULL, 16);
                    token = strtok_r(NULL, ",", &saveptr);
                }
            }
        }
    }
    fclose(f);
}

static void gltpm_load_sprite_metadata(const char *project_root, const char *project_id,
                                       GLTPMSprite *sprite) {
    char path[MAX_PATH];
    FILE *f = NULL;
    char line[MAX_LINE];

    if (!project_root || !project_id || !sprite) return;

    snprintf(path, sizeof(path), "%s/projects/%s/assets/sprites/%s.sprite.txt",
             project_root, project_id, sprite->sprite_id);

    strcpy(sprite->ascii, "@");
    strcpy(sprite->unicode, "@");
    if (sprite->label[0] == '\0') strcpy(sprite->label, "Sprite");
    sprite->color[0] = 0.95f;
    sprite->color[1] = 0.85f;
    sprite->color[2] = 0.25f;
    memset(sprite->artifact_mask, 0, 64);

    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            char *key = NULL;
            char *value = NULL;
            if (!eq) continue;
            *eq = '\0';
            key = gltpm_trim(line);
            value = gltpm_trim(eq + 1);

            if (strcmp(key, "ascii") == 0) {
                strncpy(sprite->ascii, value, sizeof(sprite->ascii) - 1);
            } else if (strcmp(key, "unicode") == 0) {
                strncpy(sprite->unicode, value, sizeof(sprite->unicode) - 1);
            } else if (strcmp(key, "label") == 0) {
                strncpy(sprite->label, value, sizeof(sprite->label) - 1);
            } else if (strcmp(key, "rgb") == 0) {
                gltpm_parse_rgb(value, sprite->color, sprite->color[0], sprite->color[1], sprite->color[2]);
            }
        }
        fclose(f);
    }

    /* Load Artifact Mask from piece directory */
    char artifact_path[MAX_PATH];
    snprintf(artifact_path, sizeof(artifact_path), "%s/projects/%s/pieces/%s/artifact.txt",
             project_root, project_id, sprite->sprite_id);
    if (access(artifact_path, F_OK) != 0) {
        /* Fallback to asset directory if piece doesn't have custom artifact */
        snprintf(artifact_path, sizeof(artifact_path), "%s/projects/%s/assets/sprites/%s.artifact.txt",
                 project_root, project_id, sprite->sprite_id);
    }
    gltpm_load_artifact(artifact_path, sprite->artifact_mask);

    /* Load Face Colors from piece state.txt */
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/projects/%s/pieces/%s/state.txt",
             project_root, project_id, sprite->sprite_id);
    FILE *sf = fopen(state_path, "r");
    if (sf) {
        char line[MAX_LINE];
        sprite->has_face_colors = 1;
        /* Default all faces to sprite base color first */
        for(int i=0; i<6; i++) {
            sprite->face_colors[i][0] = sprite->color[0];
            sprite->face_colors[i][1] = sprite->color[1];
            sprite->face_colors[i][2] = sprite->color[2];
        }
        while (fgets(line, sizeof(line), sf)) {
            char *eq = strchr(line, '='); if (!eq) continue;
            *eq = '\0'; char *k = gltpm_trim(line); char *v = gltpm_trim(eq + 1);
            if (strcmp(k, "face_top_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[0], 1, 1, 1);
            else if (strcmp(k, "face_bottom_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[1], 1, 1, 1);
            else if (strcmp(k, "face_front_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[2], 1, 1, 1);
            else if (strcmp(k, "face_back_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[3], 1, 1, 1);
            else if (strcmp(k, "face_right_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[4], 1, 1, 1);
            else if (strcmp(k, "face_left_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[5], 1, 1, 1);
        }
        fclose(sf);
    }
}

static GLTPMLegend* gltpm_lookup_legend(GLTPMLegend legend[], int legend_count, char glyph) {
    for (int i = 0; i < legend_count; i++) {
        if (legend[i].glyph == glyph) return &legend[i];
    }
    return NULL;
}

static void gltpm_load_legend(const char *path, GLTPMLegend legend[], int *legend_count) {
    FILE *f = NULL;
    char line[MAX_LINE];

    if (!path || !legend || !legend_count) return;
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f) && *legend_count < MAX_GLTPM_LEGEND) {
        char *eq = strchr(line, '=');
        char *value = NULL;
        char *trimmed = gltpm_trim(line);
        if (!eq || trimmed[0] == '#' || trimmed[0] == '\0') continue;

        *eq = '\0';
        value = gltpm_trim(eq + 1);
        legend[*legend_count].glyph = gltpm_trim(line)[0];
        
        if (strncmp(value, "tile:", 5) == 0) {
            strcpy(legend[*legend_count].type, "tile");
            strncpy(legend[*legend_count].asset_id, value + 5, sizeof(legend[*legend_count].asset_id) - 1);
        } else if (strncmp(value, "sprite:", 7) == 0) {
            strcpy(legend[*legend_count].type, "sprite");
            strncpy(legend[*legend_count].asset_id, value + 7, sizeof(legend[*legend_count].asset_id) - 1);
        } else {
            strcpy(legend[*legend_count].type, "tile");
            strncpy(legend[*legend_count].asset_id, value, sizeof(legend[*legend_count].asset_id) - 1);
        }
        
        legend[*legend_count].asset_id[sizeof(legend[*legend_count].asset_id) - 1] = '\0';
        (*legend_count)++;
    }

    fclose(f);
}

static void gltpm_add_tile(GLTPMScene *scene, const char *project_root, const char *project_id,
                           const char *tile_id, float x, float y, float z) {
    GLTPMTile *tile = NULL;
    if (!scene || !tile_id || scene->tile_count >= MAX_GLTPM_TILES) return;

    tile = &scene->tiles[scene->tile_count++];
    memset(tile, 0, sizeof(*tile));
    tile->parent = -1; /* Default to root */
    tile->x = x;
    tile->y = y;
    tile->z = z;
    strncpy(tile->tile_id, tile_id, sizeof(tile->tile_id) - 1);
    gltpm_load_tile_metadata(project_root, project_id, tile);
}

static void gltpm_add_sprite(GLTPMScene *scene, const char *project_root, const char *project_id,
                             const char *sprite_id, float x, float y, float z) {
    GLTPMSprite *sprite = NULL;
    if (!scene || !sprite_id || scene->sprite_count >= MAX_GLTPM_SPRITES) return;

    sprite = &scene->sprites[scene->sprite_count++];
    memset(sprite, 0, sizeof(*sprite));
    sprite->parent = -1; /* Default to root */
    sprite->x = x;
    sprite->y = y;
    sprite->z = z;
    strncpy(sprite->sprite_id, sprite_id, sizeof(sprite->sprite_id) - 1);
    gltpm_load_sprite_metadata(project_root, project_id, sprite);
}

/* Updated Fix 1 & 3: Persistent row tracking to prevent overlaps */
static int gltpm_process_canvas_block(GLTPMScene *scene, const char *project_root, const char *project_id,
                                      const char *block, float origin_x, float origin_y, int start_row, float z_level) {
    GLTPMLegend legend[MAX_GLTPM_LEGEND];
    int legend_count = 0;
    char legend_path[MAX_PATH];
    int rows_processed = 0;
    
    snprintf(legend_path, sizeof(legend_path), "%s/projects/%s/assets/tiles/registry.txt", project_root, project_id);
    gltpm_load_legend(legend_path, legend, &legend_count);

    char *copy = strdup(block);
    char *saveptr;
    char *line = strtok_r(copy, "\n", &saveptr);
    
    while (line) {
        /* Skip ASCII border lines and Z-level status lines */
        if (strstr(line, "+====") != NULL || 
            strstr(line, "----") != NULL || 
            strstr(line, "[Z-LEVEL]") != NULL ||
            strstr(line, "LAYOUT ID:") != NULL) { 
            line = strtok_r(NULL, "\n", &saveptr); 
            continue; 
        }

        const char *p = line;
        /* Strip leading sidebar '| ' if present */
        if (*p == '|') {
            p++;
            while (*p == ' ') p++;
        }

        for (int col = 0; p[col] != '\0'; col++) {
            /* Stop at trailing sidebar ' |' or end of map block */
            if (p[col] == '|' || p[col] == '\r') break;
            if (p[col] == ' ') continue;
            
            /* SKIP XELECTOR: The yellow wireframe is drawn by the Host, 
               don't render a generic cube for the ASCII 'X' */
            if (p[col] == 'X') continue;

            GLTPMLegend *l = gltpm_lookup_legend(legend, legend_count, p[col]);
            if (l) {
                if (strcmp(l->type, "tile") == 0) {
                    gltpm_add_tile(scene, project_root, project_id, l->asset_id,
                                   origin_x + (float)col, origin_y + (float)(start_row + rows_processed), z_level);
                } else if (strcmp(l->type, "sprite") == 0) {
                    /* SOVEREIGN FIX: Automatically place a base tile (grass) under sprites 
                       so they don't leave holes in the floor when using ASCII glyphs */
                    gltpm_add_tile(scene, project_root, project_id, "grass",
                                   origin_x + (float)col, origin_y + (float)(start_row + rows_processed), z_level);

                    gltpm_add_sprite(scene, project_root, project_id, l->asset_id,
                                     origin_x + (float)col, origin_y + (float)(start_row + rows_processed), z_level);
                }
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
        rows_processed++;
    }
    free(copy);
    return rows_processed;
}

static int gltpm_load_scene(GLTPMScene *scene, const char *project_root,
                            const char *project_id, const char *layout_path) {
    GLTPMVar *vars = NULL;
    int var_count = 0;
    char state_path[MAX_PATH];
    char gui_state_path[MAX_PATH];
    char view_path[MAX_PATH];
    char layout_abs[MAX_PATH];
    char *layout_raw = NULL;
    char *layout_sub = NULL;
    int canvas_row_total = 0;

    if (!scene || !project_root || !project_id || !layout_path) return 0;

    vars = malloc(sizeof(GLTPMVar) * MAX_GLTPM_VARS);
    if (!vars) return 0;

    gltpm_scene_reset(scene);
    memset(vars, 0, sizeof(GLTPMVar) * MAX_GLTPM_VARS);

    snprintf(state_path, sizeof(state_path), "%s/projects/%s/session/state.txt",
             project_root, project_id);
    snprintf(gui_state_path, sizeof(gui_state_path), "%s/projects/%s/manager/gui_state.txt",
             project_root, project_id);
    
    char camera_state_path[MAX_PATH];
    snprintf(camera_state_path, sizeof(camera_state_path), "%s/projects/%s/pieces/camera/state.txt",
             project_root, project_id);

    gltpm_load_vars_from_file(state_path, vars, &var_count);
    gltpm_load_vars_from_file(gui_state_path, vars, &var_count);
    gltpm_load_vars_from_file(camera_state_path, vars, &var_count);
    
    /* Dynamic Methods (${piece_methods}) - PDL-Driven */
    const char *active_id = gltpm_get_var(vars, var_count, "active_target_id");
    if (!active_id || strlen(active_id) == 0) active_id = gltpm_get_var(vars, var_count, "active_piece");
    if (active_id && strlen(active_id) > 0) {
        char pdl_path[MAX_PATH];
        /* Search list for PDL */
        int found_pdl = 0;
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/pieces/%s/piece.pdl", project_root, project_id, active_id);
        if (access(pdl_path, F_OK) == 0) found_pdl = 1;
        else {
            snprintf(pdl_path, sizeof(pdl_path), "%s/pieces/apps/%s/pieces/%s/piece.pdl", project_root, project_id, active_id);
            if (access(pdl_path, F_OK) == 0) found_pdl = 1;
        }

        if (found_pdl) {
            FILE *pf = fopen(pdl_path, "r");
            if (pf) {
                char *methods_buf = malloc(MAX_GLTPM_VALUE);
                if (methods_buf) {
                    methods_buf[0] = '\0';
                    char line[MAX_LINE];
                    int method_idx = 2;
                    while (fgets(line, sizeof(line), pf)) {
                        if (strncmp(line, "METHOD", 6) == 0) {
                            char *key_start = strchr(line, '|'); if (!key_start) continue;
                            key_start++; char *val_start = strchr(key_start, '|'); if (!val_start) continue;
                            *val_start = '\0';
                            char *trimmed_key = gltpm_trim(key_start);
                            if (strcmp(trimmed_key, "move") == 0 || strcmp(trimmed_key, "select") == 0) continue;
                            char btn[256];
                            snprintf(btn, sizeof(btn), "<button label=\"%s\" onClick=\"KEY:%d\" />\n", trimmed_key, method_idx++);
                            if (strlen(methods_buf) + strlen(btn) < MAX_GLTPM_VALUE - 1) strcat(methods_buf, btn);
                        }
                    }
                    strncpy(vars[var_count].name, "piece_methods", 63);
                    strncpy(vars[var_count].value, methods_buf, MAX_GLTPM_VALUE - 1);
                    var_count++;
                    free(methods_buf);
                }
                fclose(pf);
            }
        }
    }

    /* PROJECT-FIRST VIEW LOADING */
    int view_loaded = 0;
    char *v_paths[] = {
        "projects/%s/manager/view.txt",
        "pieces/apps/%s/manager/view.txt",
        "pieces/apps/%s/session/view.txt"
    };
    for (int i = 0; i < 3; i++) {
        snprintf(view_path, sizeof(view_path), v_paths[i], project_id);
        char abs_v[MAX_PATH]; snprintf(abs_v, sizeof(abs_v), "%s/%s", project_root, view_path);
        if (access(abs_v, F_OK) == 0) {
            FILE *vf = fopen(abs_v, "r");
            if (vf) {
                char *map_buf = malloc(MAX_GLTPM_VALUE);
                if (map_buf) {
                    map_buf[0] = '\0';
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), vf)) {
                        if (strlen(map_buf) + strlen(line) < MAX_GLTPM_VALUE - 1) strcat(map_buf, line);
                    }
                    strncpy(vars[var_count].name, "game_map", 63);
                    strncpy(vars[var_count].value, map_buf, MAX_GLTPM_VALUE - 1);
                    var_count++;
                    free(map_buf);
                    view_loaded = 1;
                }
                fclose(vf);
                break;
            }
        }
    }

    /* Fallback to global view only if project doesn't have one */
    if (!view_loaded) {
        snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
        FILE *vf = fopen(view_path, "r");
        if (vf) {
            char *map_buf = malloc(MAX_GLTPM_VALUE);
            if (map_buf) {
                map_buf[0] = '\0';
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), vf)) {
                    if (strlen(map_buf) + strlen(line) < MAX_GLTPM_VALUE - 1) strcat(map_buf, line);
                }
                strncpy(vars[var_count].name, "game_map", 63);
                strncpy(vars[var_count].value, map_buf, MAX_GLTPM_VALUE - 1);
                var_count++;
                free(map_buf);
            }
            fclose(vf);
        }
    }

    strncpy(vars[var_count].name, "project_id", 63);
    strncpy(vars[var_count].value, project_id, MAX_GLTPM_VALUE - 1);
    var_count++;

    /* Fixed 4: Extract Dynamic Camera/Xelector state */
    strncpy(scene->last_key, gltpm_get_var(vars, var_count, "last_key"), 31);
    strncpy(scene->active_target_id, gltpm_get_var(vars, var_count, "active_target_id"), 63);
    scene->is_map_control = atoi(gltpm_get_var(vars, var_count, "is_map_control"));
    scene->camera_mode = atoi(gltpm_get_var(vars, var_count, "camera_mode"));
    if (scene->camera_mode <= 0) scene->camera_mode = 4;

    scene->cam_pos[0] = (float)atof(gltpm_get_var(vars, var_count, "cam_x"));
    scene->cam_pos[1] = (float)atof(gltpm_get_var(vars, var_count, "cam_y"));
    scene->cam_pos[2] = (float)atof(gltpm_get_var(vars, var_count, "cam_z"));
    scene->cam_rot[0] = (float)atof(gltpm_get_var(vars, var_count, "cam_pitch"));
    scene->cam_rot[1] = (float)atof(gltpm_get_var(vars, var_count, "cam_yaw"));
    scene->xelector_pos[0] = atoi(gltpm_get_var(vars, var_count, "xelector_pos_x"));
    scene->xelector_pos[1] = atoi(gltpm_get_var(vars, var_count, "xelector_pos_y"));
    scene->xelector_pos[2] = atoi(gltpm_get_var(vars, var_count, "current_z"));

    int parent_stack[16];
    int stack_top = -1;
    float cur_offset_x = 0;
    float cur_offset_y = 0;

    gltpm_build_abs_path(layout_abs, sizeof(layout_abs), project_root, layout_path);
    FILE *f = fopen(layout_abs, "r");
    if (!f) { free(vars); return 0; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    layout_raw = malloc(sz + 1); fread(layout_raw, 1, sz, f); layout_raw[sz] = '\0';
    fclose(f);

    layout_sub = malloc(MAX_GLTPM_BUFFER);
    gltpm_substitute_vars(layout_raw, layout_sub, MAX_GLTPM_BUFFER, vars, var_count);
    free(layout_raw);

    char *p = layout_sub;
    while (*p) {
        char *line_end = strchr(p, '\n');
        if (line_end) *line_end = '\0';

        char *trimmed = gltpm_trim(p);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            if (!line_end) break;
            p = line_end + 1;
            continue;
        }

        if (strncmp(trimmed, "<scene", 6) == 0) {
            char title[128] = ""; char bg[64] = ""; char camera[32] = ""; char ui_mode[32] = "";
            if (gltpm_extract_attr(trimmed, "title", title, sizeof(title))) strncpy(scene->title, title, MAX_WIN_TITLE - 1); scene->title[MAX_WIN_TITLE-1] = '\0';
            if (gltpm_extract_attr(trimmed, "bg", bg, sizeof(bg))) gltpm_parse_rgb(bg, scene->bg_color, scene->bg_color[0], scene->bg_color[1], scene->bg_color[2]);
            if (gltpm_extract_attr(trimmed, "camera_mode", camera, sizeof(camera))) {
                scene->camera_mode = atoi(camera); if (scene->camera_mode <= 0) scene->camera_mode = 4;
            }
            if (gltpm_extract_attr(trimmed, "ui_mode", ui_mode, sizeof(ui_mode))) {
                if (strcmp(ui_mode, "SOVEREIGN") == 0) scene->ui_mode = 1;
            }
        } else if (strncmp(trimmed, "<camera", 7) == 0) {
            char mode[32] = "", x[32] = "", y[32] = "", z[32] = "", pitch[32] = "", yaw[32] = "";
            if (gltpm_extract_attr(trimmed, "mode", mode, sizeof(mode))) scene->camera_mode = atoi(mode);
            if (gltpm_extract_attr(trimmed, "x", x, sizeof(x))) scene->cam_pos[0] = (float)atof(x);
            if (gltpm_extract_attr(trimmed, "y", y, sizeof(y))) scene->cam_pos[1] = (float)atof(y);
            if (gltpm_extract_attr(trimmed, "z", z, sizeof(z))) scene->cam_pos[2] = (float)atof(z);
            if (gltpm_extract_attr(trimmed, "pitch", pitch, sizeof(pitch))) scene->cam_rot[0] = (float)atof(pitch);
            if (gltpm_extract_attr(trimmed, "yaw", yaw, sizeof(yaw))) scene->cam_rot[1] = (float)atof(yaw);
        } else if (strncmp(trimmed, "<panel", 6) == 0 || strncmp(trimmed, "<div", 4) == 0 || 
                   strncmp(trimmed, "<window", 7) == 0 || strncmp(trimmed, "<header", 7) == 0 ||
                   strncmp(trimmed, "<menu", 5) == 0 || strncmp(trimmed, "<ul", 3) == 0 ||
                   strncmp(trimmed, "<canvas", 7) == 0) {
            if (scene->node_count < MAX_GLTPM_NODES) {
                int node_idx = scene->node_count++;
                GLTPMNode *node = &scene->nodes[node_idx];
                memset(node, 0, sizeof(*node));

                char id[64], x[32], y[32], w[32], h[32], color[64], title[128];
                if (trimmed[1] == 'p') strcpy(node->tag, "panel"); 
                else if (trimmed[1] == 'd') strcpy(node->tag, "div");
                else if (trimmed[1] == 'w') {
                    strcpy(node->tag, "window");
                    if (gltpm_extract_attr(trimmed, "title", title, sizeof(title))) {
                        strncpy(scene->title, title, MAX_WIN_TITLE - 1);
                        scene->title[MAX_WIN_TITLE - 1] = '\0';
                    }
                    if (gltpm_extract_attr(trimmed, "width", w, sizeof(w))) scene->w = (float)atof(w);
                    if (gltpm_extract_attr(trimmed, "height", h, sizeof(h))) scene->h = (float)atof(h);
                }
                else if (trimmed[1] == 'h') strcpy(node->tag, "header");
                else if (trimmed[1] == 'm') strcpy(node->tag, "menu");
                else if (trimmed[1] == 'u') strcpy(node->tag, "ul");
                else strcpy(node->tag, "canvas");

                if (gltpm_extract_attr(trimmed, "id", id, sizeof(id))) strcpy(node->id, id);
                if (gltpm_extract_attr(trimmed, "x", x, sizeof(x))) node->x = (float)atof(x);
                if (gltpm_extract_attr(trimmed, "y", y, sizeof(y))) node->y = (float)atof(y);
                if (gltpm_extract_attr(trimmed, "width", w, sizeof(w))) node->w = (float)atof(w);
                else if (gltpm_extract_attr(trimmed, "w", w, sizeof(w))) node->w = (float)atof(w);
                if (gltpm_extract_attr(trimmed, "height", h, sizeof(h))) node->h = (float)atof(h);
                else if (gltpm_extract_attr(trimmed, "h", h, sizeof(h))) node->h = (float)atof(h);

                if (gltpm_extract_attr(trimmed, "color", color, sizeof(color))) {
                    float rgb[3];
                    gltpm_parse_rgb(color, rgb, 0.2f, 0.2f, 0.3f);
                    node->color[0] = rgb[0]; node->color[1] = rgb[1]; node->color[2] = rgb[2]; node->color[3] = 1.0f;
                }

                /* Special Case: Header/Menu with labels */
                char lbl[128];
                if (gltpm_extract_attr(trimmed, "label", lbl, sizeof(lbl))) {
                    if (scene->text_count < MAX_GLTPM_TEXTS) {
                        GLTPMText *txt = &scene->texts[scene->text_count++];
                        strcpy(txt->label, lbl);
                        txt->x = 10; txt->y = 10;
                        txt->parent = node_idx;
                    }
                }

                node->parent = (stack_top >= 0) ? parent_stack[stack_top] : -1;
                if (stack_top < 15) parent_stack[++stack_top] = node_idx;
            }
        } else if (strncmp(trimmed, "</panel", 7) == 0 || strncmp(trimmed, "</div>", 6) == 0 ||
                   strncmp(trimmed, "</window", 8) == 0 || strncmp(trimmed, "</header", 8) == 0 ||
                   strncmp(trimmed, "</menu", 6) == 0 || strncmp(trimmed, "</ul>", 4) == 0 ||
                   strncmp(trimmed, "</canvas", 8) == 0) {
            if (stack_top >= 0) stack_top--;
        } else if (strncmp(trimmed, "<sprite", 7) == 0) {
            char id[64] = "", x[32] = "", y[32] = "", z[32] = "";
            if (gltpm_extract_attr(trimmed, "id", id, sizeof(id))) {
                float sx = (float)atof(gltpm_extract_attr(trimmed, "x", x, sizeof(x)) ? x : "0");
                float sy = (float)atof(gltpm_extract_attr(trimmed, "y", y, sizeof(y)) ? y : "0");
                float sz = (float)atof(gltpm_extract_attr(trimmed, "z", z, sizeof(z)) ? z : "0");

                int spr_idx = scene->sprite_count;
                gltpm_add_sprite(scene, project_root, project_id, id, sx, sy, sz);
                if (scene->sprite_count > spr_idx) {
                    scene->sprites[spr_idx].parent = (stack_top >= 0) ? parent_stack[stack_top] : -1;
                }
            }
        } else if (strncmp(trimmed, "<interact", 9) == 0) {
            char src[MAX_PATH];
            if (gltpm_extract_attr(trimmed, "src", src, sizeof(src))) strncpy(scene->interact_src, src, MAX_PATH-1);
        } else if (strncmp(trimmed, "<menuitem", 9) == 0 || strncmp(trimmed, "<li", 3) == 0 || 
                   strncmp(trimmed, "<button", 7) == 0 || strncmp(trimmed, "<checkbox", 9) == 0 ||
                   strncmp(trimmed, "<slider", 7) == 0) {
            if (scene->button_count < MAX_GLTPM_BUTTONS) {
                GLTPMButton *btn = &scene->buttons[scene->button_count++];
                char id[128], lbl[128], clk[128], x[32], y[32], w[32], h[32];
                if (gltpm_extract_attr(trimmed, "id", id, 127)) strncpy(btn->id, id, 63);
                if (gltpm_extract_attr(trimmed, "label", lbl, 127)) strncpy(btn->label, lbl, 127);
                if (gltpm_extract_attr(trimmed, "onClick", clk, 127)) strncpy(btn->onClick, clk, 127);

                /* Auto-layout for interactive elements inside a parent */
                float local_offset_y = 0;
                if (stack_top >= 0) {
                    int sibling_count = 0;
                    for (int b = 0; b < scene->button_count - 1; b++) {
                        if (scene->buttons[b].parent == parent_stack[stack_top]) sibling_count++;
                    }
                    /* Also count text siblings for combined offset */
                    for (int t = 0; t < scene->text_count; t++) {
                        if (scene->texts[t].parent == parent_stack[stack_top]) sibling_count++;
                    }
                    local_offset_y = sibling_count * 35.0f;
                    }

                if (gltpm_extract_attr(trimmed, "x", x, sizeof(x))) btn->x = (float)atof(x);
                else btn->x = (stack_top >= 0) ? 10.0f : -1.0f;

                if (gltpm_extract_attr(trimmed, "y", y, sizeof(y))) btn->y = (float)atof(y);
                else btn->y = (stack_top >= 0) ? local_offset_y + 10.0f : -1.0f;

                btn->w = (float)atof(gltpm_extract_attr(trimmed, "width", w, sizeof(w)) ? w : "180");
                btn->h = (float)atof(gltpm_extract_attr(trimmed, "height", h, sizeof(h)) ? h : "30");

                btn->parent = (stack_top >= 0) ? parent_stack[stack_top] : -1;
            }
        } else if (strstr(trimmed, "<text") != NULL) {
            if (scene->text_count < MAX_GLTPM_TEXTS) {
                GLTPMText *txt = &scene->texts[scene->text_count++];
                char lbl[256], x[32], y[32];
                if (gltpm_extract_attr(trimmed, "label", lbl, 255)) strncpy(txt->label, lbl, 255);

                /* Auto-layout for text elements inside a parent */
                float local_offset_y = 0;
                if (stack_top >= 0) {
                    int sibling_count = 0;
                    for (int b = 0; b < scene->button_count; b++) {
                        if (scene->buttons[b].parent == parent_stack[stack_top]) sibling_count++;
                    }
                    for (int t = 0; t < scene->text_count - 1; t++) {
                        if (scene->texts[t].parent == parent_stack[stack_top]) sibling_count++;
                    }
                    local_offset_y = sibling_count * 25.0f; /* Slightly tighter spacing for text */
                }

                if (gltpm_extract_attr(trimmed, "x", x, sizeof(x))) txt->x = (float)atof(x);
                else txt->x = (stack_top >= 0) ? 10.0f : -1.0f;

                if (gltpm_extract_attr(trimmed, "y", y, sizeof(y))) txt->y = (float)atof(y);
                else txt->y = (stack_top >= 0) ? local_offset_y + 10.0f : -1.0f;

                txt->parent = (stack_top >= 0) ? parent_stack[stack_top] : -1;
            }
        } else if (strncmp(trimmed, "<img", 4) == 0) {
            /* Placeholder for image - render as colored box or textured quad if supported later */
            if (scene->node_count < MAX_GLTPM_NODES) {
                int node_idx = scene->node_count++;
                GLTPMNode *node = &scene->nodes[node_idx];
                memset(node, 0, sizeof(*node));
                strcpy(node->tag, "img");
                char id[64], x[32], y[32], w[32], h[32], src[MAX_PATH];
                if (gltpm_extract_attr(trimmed, "id", id, sizeof(id))) strcpy(node->id, id);
                if (gltpm_extract_attr(trimmed, "x", x, sizeof(x))) node->x = (float)atof(x);
                if (gltpm_extract_attr(trimmed, "y", y, sizeof(y))) node->y = (float)atof(y);
                if (gltpm_extract_attr(trimmed, "width", w, sizeof(w))) node->w = (float)atof(w);
                if (gltpm_extract_attr(trimmed, "height", h, sizeof(h))) node->h = (float)atof(h);
                
                if (gltpm_extract_attr(trimmed, "src", src, sizeof(src))) {
                    char abs_src[MAX_PATH];
                    int found = 0;
                    
                    if (src[0] == '/') {
                        strncpy(abs_src, src, MAX_PATH-1);
                        found = 1;
                    } else {
                        /* 1. Try projects/%s/%s */
                        snprintf(abs_src, MAX_PATH, "%s/projects/%s/%s", project_root, project_id, src);
                        if (access(abs_src, F_OK) == 0) found = 1;
                        
                        if (!found) {
                            /* 2. Try pieces/apps/%s/%s */
                            snprintf(abs_src, MAX_PATH, "%s/pieces/apps/%s/%s", project_root, project_id, src);
                            if (access(abs_src, F_OK) == 0) found = 1;
                        }
                        
                        if (!found) {
                            /* 3. Fallback to project_root/%s */
                            snprintf(abs_src, MAX_PATH, "%s/%s", project_root, src);
                        }
                    }
                    
                    node->texture_id = gltpm_load_texture(abs_src);
                    node->has_texture = (node->texture_id != 0);
                    if (!node->has_texture) {
                        printf("[GLTPM] Failed to load image: %s\n", abs_src);
                    } else {
                        printf("[GLTPM] Loaded image: %s (ID: %u)\n", abs_src, node->texture_id);
                    }
                }

                node->color[0] = 0.5f; node->color[1] = 0.5f; node->color[2] = 0.5f; node->color[3] = 1.0f;
                node->parent = (stack_top >= 0) ? parent_stack[stack_top] : -1;
            }
        } else if (trimmed[0] == '<') {
            /* skip unknown tags */
        } else {
            /* Raw block - process as map canvas with persistent row tracking */
            int tile_idx = scene->tile_count;
            int sprite_idx = scene->sprite_count;
            int rows = gltpm_process_canvas_block(scene, project_root, project_id, trimmed, -10.0f, -5.0f, canvas_row_total, 0.0f);
            
            /* Assign parent to newly added tiles and sprites */
            if (stack_top >= 0) {
                for (int i = tile_idx; i < scene->tile_count; i++) {
                    scene->tiles[i].parent = parent_stack[stack_top];
                }
                for (int i = sprite_idx; i < scene->sprite_count; i++) {
                    scene->sprites[i].parent = parent_stack[stack_top];
                }
            }
            canvas_row_total += rows;
        }

        if (!line_end) break;
        p = line_end + 1;
    }

    free(layout_sub);
    free(vars);
    scene->loaded = 1;
    return 1;
}
