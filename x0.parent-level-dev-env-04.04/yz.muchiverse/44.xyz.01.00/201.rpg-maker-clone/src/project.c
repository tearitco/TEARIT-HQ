/* project.c — load/save map.txt, events, switches.pdl (house-aligned) */
#include "rpg.h"

static int mkdir_p(const char *path) {
    char tmp[MAX_PATH];
    char *p;
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

static void rtrim(char *s) {
    char *e;
    if (!s || !*s) return;
    e = s + strlen(s) - 1;
    while (e >= s && (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n')) *e-- = 0;
}

void cmd_to_label(const Command *c, char *out, int n) {
    switch (c->type) {
    case CMD_SHOW_TEXT:
        snprintf(out, n, "Show Text : \"%s\"", c->a);
        break;
    case CMD_SET_SWITCH:
        snprintf(out, n, "Control Switches : %s = %s", c->a, c->b[0] ? c->b : "1");
        break;
    case CMD_IF_SWITCH:
        snprintf(out, n, "Conditional Branch : Switch %s is %s",
                 c->a, c->b[0] ? c->b : "1");
        break;
    case CMD_END:
        snprintf(out, n, "End");
        break;
    case CMD_TRANSFER:
        snprintf(out, n, "Transfer Player : %s (%s, %s)", c->a, c->b, c->c);
        break;
    case CMD_RET:
        snprintf(out, n, "Exit Event Processing");
        break;
    case CMD_COMMENT:
        snprintf(out, n, "Comment : %s", c->a);
        break;
    default:
        snprintf(out, n, "(empty — insert command)");
        break;
    }
}

void cmd_from_pal_line(Command *c, const char *line) {
    char buf[MAX_LINE];
    char *p;
    memset(c, 0, sizeof(*c));
    c->type = CMD_EMPTY;
    if (!line) return;
    while (*line == ' ' || *line == '\t') line++;
    if (!*line || *line == '#') return;
    snprintf(buf, sizeof(buf), "%s", line);
    rtrim(buf);
    if (!buf[0]) return;

    if (strncmp(buf, "show_text", 9) == 0) {
        c->type = CMD_SHOW_TEXT;
        p = buf + 9;
        while (*p == ' ') p++;
        snprintf(c->a, sizeof(c->a), "%s", p);
    } else if (strncmp(buf, "set_switch", 10) == 0) {
        c->type = CMD_SET_SWITCH;
        if (sscanf(buf + 10, "%63s %31s", c->a, c->b) < 1)
            snprintf(c->a, sizeof(c->a), "switch");
        if (!c->b[0]) snprintf(c->b, sizeof(c->b), "1");
    } else if (strncmp(buf, "if_switch", 9) == 0) {
        c->type = CMD_IF_SWITCH;
        if (sscanf(buf + 9, "%63s %31s", c->a, c->b) < 1)
            snprintf(c->a, sizeof(c->a), "switch");
        if (!c->b[0]) snprintf(c->b, sizeof(c->b), "1");
    } else if (strcmp(buf, "end") == 0) {
        c->type = CMD_END;
    } else if (strncmp(buf, "transfer", 8) == 0) {
        c->type = CMD_TRANSFER;
        sscanf(buf + 8, "%63s %31s %31s", c->a, c->b, c->c);
    } else if (strcmp(buf, "ret") == 0) {
        c->type = CMD_RET;
    } else if (strncmp(buf, "comment", 7) == 0) {
        c->type = CMD_COMMENT;
        p = buf + 7;
        while (*p == ' ') p++;
        snprintf(c->a, sizeof(c->a), "%s", p);
    } else {
        c->type = CMD_COMMENT;
        snprintf(c->a, sizeof(c->a), "%s", buf);
    }
}

static void cmd_to_pal(const Command *c, char *out, int n) {
    switch (c->type) {
    case CMD_SHOW_TEXT:  snprintf(out, n, "show_text %s", c->a); break;
    case CMD_SET_SWITCH: snprintf(out, n, "set_switch %s %s", c->a, c->b[0] ? c->b : "1"); break;
    case CMD_IF_SWITCH:  snprintf(out, n, "if_switch %s %s", c->a, c->b[0] ? c->b : "1"); break;
    case CMD_END:        snprintf(out, n, "end"); break;
    case CMD_TRANSFER:   snprintf(out, n, "transfer %s %s %s", c->a, c->b, c->c); break;
    case CMD_RET:        snprintf(out, n, "ret"); break;
    case CMD_COMMENT:    snprintf(out, n, "comment %s", c->a); break;
    default:             out[0] = 0; break;
    }
}

int switch_get(Project *p, const char *name) {
    int i;
    for (i = 0; i < p->n_switches; i++)
        if (strcmp(p->switches[i].name, name) == 0)
            return p->switches[i].value;
    return 0;
}

void switch_set(Project *p, const char *name, int val) {
    int i;
    for (i = 0; i < p->n_switches; i++) {
        if (strcmp(p->switches[i].name, name) == 0) {
            p->switches[i].value = val;
            return;
        }
    }
    if (p->n_switches < MAX_SWITCHES) {
        snprintf(p->switches[p->n_switches].name, MAX_NAME, "%s", name);
        p->switches[p->n_switches].value = val;
        p->n_switches++;
    }
}

Event *project_event_at(Project *p, int x, int y) {
    int i;
    for (i = 0; i < MAX_EVENTS; i++)
        if (p->events[i].used && p->events[i].x == x && p->events[i].y == y)
            return &p->events[i];
    return NULL;
}

int project_ensure_dirs(Project *p) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/maps/%s/events", p->root, p->map_id);
    return mkdir_p(path);
}

static int load_switches(Project *p) {
    char path[MAX_PATH], line[MAX_LINE];
    FILE *f;
    p->n_switches = 0;
    snprintf(path, sizeof(path), "%s/switches.pdl", p->root);
    f = fopen(path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char name[MAX_NAME];
        int val = 0;
        rtrim(line);
        if (!line[0] || line[0] == '#' || line[0] == '-') continue;
        if (strncmp(line, "SECTION", 7) == 0) continue;
        /* formats: "SWITCH | door_open | 0" or "door_open=0" or "door_open 0" */
        if (strstr(line, "|")) {
            char *a = strchr(line, '|');
            char *b;
            if (!a) continue;
            a++;
            while (*a == ' ') a++;
            b = strchr(a, '|');
            if (b) {
                *b = 0;
                rtrim(a);
                snprintf(name, sizeof(name), "%s", a);
                val = atoi(b + 1);
            } else continue;
        } else if (strchr(line, '=')) {
            char *eq = strchr(line, '=');
            *eq = 0;
            rtrim(line);
            snprintf(name, sizeof(name), "%s", line);
            val = atoi(eq + 1);
        } else if (sscanf(line, "%63s %d", name, &val) < 1) {
            continue;
        }
        if (strcmp(name, "KEY") == 0 || strcmp(name, "SWITCH") == 0) continue;
        switch_set(p, name, val);
    }
    fclose(f);
    return 0;
}

int project_save_switches(Project *p) {
    char path[MAX_PATH];
    FILE *f;
    int i;
    snprintf(path, sizeof(path), "%s/switches.pdl", p->root);
    f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    for (i = 0; i < p->n_switches; i++) {
        fprintf(f, "SWITCH       | %-18s | %d\n",
                p->switches[i].name, p->switches[i].value);
    }
    fclose(f);
    return 0;
}

int project_scan_maps(Project *p) {
    char path[MAX_PATH];
    DIR *d;
    struct dirent *e;
    p->n_maps = 0;
    snprintf(path, sizeof(path), "%s/maps", p->root);
    d = opendir(path);
    if (!d) return 0;
    while ((e = readdir(d)) != NULL && p->n_maps < MAX_MAPS) {
        char full[MAX_PATH];
        struct stat st;
        if (e->d_name[0] == '.') continue;
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(p->maps[p->n_maps].id, sizeof(p->maps[0].id), "%s", e->d_name);
        /* friendlier MZ-style tree labels */
        if (strcmp(e->d_name, "map_start") == 0)
            snprintf(p->maps[p->n_maps].label, sizeof(p->maps[0].label), "Factory 1");
        else if (strcmp(e->d_name, "factory_2") == 0)
            snprintf(p->maps[p->n_maps].label, sizeof(p->maps[0].label), "Factory 2");
        else if (strcmp(e->d_name, "world") == 0)
            snprintf(p->maps[p->n_maps].label, sizeof(p->maps[0].label), "World map");
        else
            snprintf(p->maps[p->n_maps].label, sizeof(p->maps[0].label), "%s", e->d_name);
        p->n_maps++;
    }
    closedir(d);
    return p->n_maps;
}

int project_load_map(Project *p, const char *map_id) {
    char path[MAX_PATH], line[MAX_LINE];
    FILE *f;
    int y = 0, x;
    snprintf(p->map_id, sizeof(p->map_id), "%s", map_id);
    p->map.w = MAP_W;
    p->map.h = MAP_H;
    memset(p->map.cells, 0, sizeof(p->map.cells));
    memset(p->map.objects, 0, sizeof(p->map.objects));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            p->map.cells[y][x] = '.';
            p->map.objects[y][x] = ' ';
        }
        p->map.cells[y][MAP_W] = 0;
        p->map.objects[y][MAP_W] = 0;
    }
    snprintf(path, sizeof(path), "%s/maps/%s/map.txt", p->root, map_id);
    f = fopen(path, "r");
    if (!f) return -1;
    y = 0;
    while (y < MAP_H && fgets(line, sizeof(line), f)) {
        int len;
        rtrim(line);
        if (!line[0]) continue;
        len = (int)strlen(line);
        for (x = 0; x < MAP_W && x < len; x++)
            p->map.cells[y][x] = line[x];
        for (; x < MAP_W; x++)
            p->map.cells[y][x] = '.';
        p->map.cells[y][MAP_W] = 0;
        y++;
    }
    fclose(f);
    /* optional upper layer */
    snprintf(path, sizeof(path), "%s/maps/%s/map_obj.txt", p->root, map_id);
    f = fopen(path, "r");
    if (f) {
        y = 0;
        while (y < MAP_H && fgets(line, sizeof(line), f)) {
            int len;
            rtrim(line);
            if (!line[0]) continue;
            len = (int)strlen(line);
            for (x = 0; x < MAP_W && x < len; x++)
                p->map.objects[y][x] = (line[x] == '.' ? ' ' : line[x]);
            for (; x < MAP_W; x++)
                p->map.objects[y][x] = ' ';
            p->map.objects[y][MAP_W] = 0;
            y++;
        }
        fclose(f);
    }
    return 0;
}

int project_save_map(Project *p) {
    char path[MAX_PATH];
    FILE *f;
    int y, x;
    project_ensure_dirs(p);
    snprintf(path, sizeof(path), "%s/maps/%s", p->root, p->map_id);
    mkdir_p(path);
    snprintf(path, sizeof(path), "%s/maps/%s/map.txt", p->root, p->map_id);
    f = fopen(path, "w");
    if (!f) return -1;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            char ch = p->map.cells[y][x];
            if (!ch) ch = '.';
            fputc(ch, f);
        }
        fputc('\n', f);
    }
    fclose(f);
    snprintf(path, sizeof(path), "%s/maps/%s/map_obj.txt", p->root, p->map_id);
    f = fopen(path, "w");
    if (f) {
        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                char ch = p->map.objects[y][x];
                if (!ch || ch == ' ') ch = '.';
                fputc(ch, f);
            }
            fputc('\n', f);
        }
        fclose(f);
    }
    return 0;
}

static int load_one_event(Project *p, const char *evdir) {
    char path[MAX_PATH], line[MAX_LINE];
    FILE *f;
    Event *e;
    int i, x = 0, y = 0;
    /* parse ev_X_Y */
    if (sscanf(evdir, "ev_%d_%d", &x, &y) != 2) return -1;
    if (p->n_events >= MAX_EVENTS) return -1;
    /* find free slot */
    for (i = 0; i < MAX_EVENTS; i++)
        if (!p->events[i].used) break;
    if (i >= MAX_EVENTS) return -1;
    e = &p->events[i];
    memset(e, 0, sizeof(*e));
    e->used = 1;
    e->x = x;
    e->y = y;
    e->sprite = '@';
    e->trigger = TR_ACTION;
    snprintf(e->name, sizeof(e->name), "event_%d_%d", x, y);
    snprintf(e->dir, sizeof(e->dir), "%s", evdir);

    snprintf(path, sizeof(path), "%s/maps/%s/events/%s/state.txt",
             p->root, p->map_id, evdir);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char key[64], val[MAX_TEXT];
            rtrim(line);
            if (sscanf(line, "%63[^=]=%159[^\n]", key, val) == 2) {
                rtrim(key); rtrim(val);
                if (strcmp(key, "name") == 0) snprintf(e->name, sizeof(e->name), "%s", val);
                else if (strcmp(key, "trigger") == 0) {
                    if (strcmp(val, "touch") == 0) e->trigger = TR_TOUCH;
                    else e->trigger = TR_ACTION;
                } else if (strcmp(key, "sprite") == 0 && val[0]) e->sprite = val[0];
                else if (strcmp(key, "x") == 0) e->x = atoi(val);
                else if (strcmp(key, "y") == 0) e->y = atoi(val);
            }
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/maps/%s/events/%s/event.pal",
             p->root, p->map_id, evdir);
    f = fopen(path, "r");
    e->n_cmds = 0;
    if (f) {
        while (fgets(line, sizeof(line), f) && e->n_cmds < MAX_CMDS) {
            Command c;
            rtrim(line);
            if (!line[0] || line[0] == '#') continue;
            cmd_from_pal_line(&c, line);
            if (c.type == CMD_EMPTY) continue;
            e->cmds[e->n_cmds++] = c;
        }
        fclose(f);
    }
    if (i >= p->n_events) p->n_events = i + 1;
    return 0;
}

static int load_events(Project *p) {
    char path[MAX_PATH];
    DIR *d;
    struct dirent *ent;
    int i;
    for (i = 0; i < MAX_EVENTS; i++)
        p->events[i].used = 0;
    p->n_events = 0;
    snprintf(path, sizeof(path), "%s/maps/%s/events", p->root, p->map_id);
    d = opendir(path);
    if (!d) return 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (strncmp(ent->d_name, "ev_", 3) != 0) continue;
        load_one_event(p, ent->d_name);
    }
    closedir(d);
    return 0;
}

int project_switch_map(Project *p, const char *map_id) {
    int i;
    if (!map_id || !map_id[0]) return -1;
    for (i = 0; i < MAX_EVENTS; i++)
        memset(&p->events[i], 0, sizeof(p->events[i]));
    p->n_events = 0;
    if (project_load_map(p, map_id) != 0) return -1;
    load_events(p);
    snprintf(p->map_id, sizeof(p->map_id), "%s", map_id);
    return 0;
}

int project_save_event(Project *p, int ei) {
    char path[MAX_PATH], dir[MAX_PATH];
    FILE *f;
    Event *e;
    int i;
    if (ei < 0 || ei >= MAX_EVENTS || !p->events[ei].used) return -1;
    e = &p->events[ei];
    snprintf(e->dir, sizeof(e->dir), "ev_%d_%d", e->x, e->y);
    snprintf(dir, sizeof(dir), "%s/maps/%s/events/%s", p->root, p->map_id, e->dir);
    mkdir_p(dir);

    snprintf(path, sizeof(path), "%s/state.txt", dir);
    f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "name=%s\n", e->name);
    fprintf(f, "trigger=%s\n", e->trigger == TR_TOUCH ? "touch" : "action");
    fprintf(f, "x=%d\n", e->x);
    fprintf(f, "y=%d\n", e->y);
    fprintf(f, "sprite=%c\n", e->sprite ? e->sprite : '@');
    fprintf(f, "map=%s\n", p->map_id);
    fclose(f);

    snprintf(path, sizeof(path), "%s/event.pal", dir);
    f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# event.pal — mini interpreter script\n");
    for (i = 0; i < e->n_cmds; i++) {
        char line[MAX_LINE];
        cmd_to_pal(&e->cmds[i], line, sizeof(line));
        if (line[0]) fprintf(f, "%s\n", line);
    }
    fprintf(f, "ret\n");
    fclose(f);

    /* optional structured IR stub */
    snprintf(path, sizeof(path), "%s/event.ir.pdl", dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | name               | %s\n", e->name);
        fprintf(f, "META         | trigger            | %s\n",
                e->trigger == TR_TOUCH ? "touch" : "action");
        fprintf(f, "META         | cmds               | %d\n", e->n_cmds);
        fclose(f);
    }
    return 0;
}

int project_add_event(Project *p, int x, int y) {
    int i;
    Event *e;
    if (project_event_at(p, x, y)) return -1;
    for (i = 0; i < MAX_EVENTS; i++)
        if (!p->events[i].used) break;
    if (i >= MAX_EVENTS) return -1;
    e = &p->events[i];
    memset(e, 0, sizeof(*e));
    e->used = 1;
    e->x = x;
    e->y = y;
    e->sprite = '@';
    e->trigger = TR_ACTION;
    snprintf(e->name, sizeof(e->name), "event_%d_%d", x, y);
    snprintf(e->dir, sizeof(e->dir), "ev_%d_%d", x, y);
    e->n_cmds = 1;
    e->cmds[0].type = CMD_SHOW_TEXT;
    snprintf(e->cmds[0].a, sizeof(e->cmds[0].a), "Hello!");
    if (i >= p->n_events) p->n_events = i + 1;
    p->dirty = 1;
    return i;
}

static int load_project_pdl(Project *p) {
    char path[MAX_PATH], line[MAX_LINE];
    FILE *f;
    snprintf(p->name, sizeof(p->name), "demo");
    snprintf(p->start_map, sizeof(p->start_map), "map_start");
    p->start_x = 2;
    p->start_y = 2;
    snprintf(path, sizeof(path), "%s/project.pdl", p->root);
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[MAX_TEXT];
        char *a, *b;
        rtrim(line);
        if (!line[0] || line[0] == '#' || line[0] == '-') continue;
        /* KEY | value  or  section | key | value */
        if (strchr(line, '|')) {
            a = strchr(line, '|');
            if (!a) continue;
            /* may be SECTION | KEY | VALUE */
            b = strchr(a + 1, '|');
            if (b) {
                char k2[64];
                *b = 0;
                while (*++a == ' ') ;
                rtrim(a);
                snprintf(k2, sizeof(k2), "%s", a);
                while (*++b == ' ') ;
                rtrim(b);
                if (strcmp(k2, "name") == 0 || strcmp(k2, "title") == 0)
                    snprintf(p->name, sizeof(p->name), "%s", b);
                else if (strcmp(k2, "start_map") == 0 || strcmp(k2, "starting_map") == 0)
                    snprintf(p->start_map, sizeof(p->start_map), "%s", b);
                else if (strcmp(k2, "start_x") == 0) p->start_x = atoi(b);
                else if (strcmp(k2, "start_y") == 0) p->start_y = atoi(b);
            }
        } else if (sscanf(line, "%63[^=]=%159[^\n]", key, val) == 2) {
            rtrim(key); rtrim(val);
            if (strcmp(key, "name") == 0) snprintf(p->name, sizeof(p->name), "%s", val);
            else if (strcmp(key, "start_map") == 0) snprintf(p->start_map, sizeof(p->start_map), "%s", val);
            else if (strcmp(key, "start_x") == 0) p->start_x = atoi(val);
            else if (strcmp(key, "start_y") == 0) p->start_y = atoi(val);
        }
    }
    fclose(f);
    return 0;
}

int project_save(Project *p) {
    char path[MAX_PATH];
    FILE *f;
    int i;
    mkdir_p(p->root);
    snprintf(path, sizeof(path), "%s/project.pdl", p->root);
    f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | name               | %s\n", p->name);
    fprintf(f, "META         | start_map          | %s\n", p->start_map);
    fprintf(f, "META         | start_x            | %d\n", p->start_x);
    fprintf(f, "META         | start_y            | %d\n", p->start_y);
    fclose(f);
    project_save_map(p);
    project_save_switches(p);
    for (i = 0; i < MAX_EVENTS; i++)
        if (p->events[i].used)
            project_save_event(p, i);
    p->dirty = 0;
    return 0;
}

int project_load(Project *p, const char *root) {
    memset(p, 0, sizeof(*p));
    snprintf(p->root, sizeof(p->root), "%s", root);
    load_project_pdl(p);
    if (!p->start_map[0]) snprintf(p->start_map, sizeof(p->start_map), "map_start");
    if (project_load_map(p, p->start_map) != 0) {
        int x, y;
        snprintf(p->map_id, sizeof(p->map_id), "map_start");
        p->map.w = MAP_W; p->map.h = MAP_H;
        for (y = 0; y < MAP_H; y++)
            for (x = 0; x < MAP_W; x++) {
                p->map.cells[y][x] = (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) ? '#' : '.';
                p->map.objects[y][x] = ' ';
            }
    }
    load_switches(p);
    load_events(p);
    project_scan_maps(p);
    p->dirty = 0;
    return 0;
}

void project_new_demo_defaults(Project *p, const char *root) {
    int x, y;
    Event *e;
    memset(p, 0, sizeof(*p));
    snprintf(p->root, sizeof(p->root), "%s", root);
    snprintf(p->name, sizeof(p->name), "planet aether");
    snprintf(p->start_map, sizeof(p->start_map), "map_start");
    snprintf(p->map_id, sizeof(p->map_id), "map_start");
    p->start_x = 3;
    p->start_y = 3;
    p->map.w = MAP_W;
    p->map.h = MAP_H;
    /* Factory floor (MZ-style industrial demo) */
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            p->map.objects[y][x] = ' ';
            if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1)
                p->map.cells[y][x] = '#';
            else if (y >= MAP_H - 5)
                p->map.cells[y][x] = '='; /* pipe corridor */
            else if ((x >= 8 && x <= 12 && y >= 6 && y <= 10) ||
                     (x >= 15 && x <= 19 && y >= 6 && y <= 10) ||
                     (x >= 22 && x <= 26 && y >= 6 && y <= 10))
                p->map.cells[y][x] = ':'; /* machine blocks */
            else if (y == 14 || y == 15)
                p->map.cells[y][x] = '!'; /* hazard strip */
            else
                p->map.cells[y][x] = '.';
        }
        p->map.cells[y][MAP_W] = 0;
        p->map.objects[y][MAP_W] = 0;
    }
    /* inner walls / rooms */
    for (x = 2; x < MAP_W - 2; x++) {
        p->map.cells[4][x] = '#';
        p->map.cells[MAP_H - 6][x] = '#';
    }
    for (y = 4; y < MAP_H - 6; y++) {
        p->map.cells[y][10] = '#';
        p->map.cells[y][20] = '#';
    }
    p->map.cells[8][10] = '+';
    p->map.cells[8][20] = '+';
    p->map.cells[4][5] = '+';
    /* upper layer props */
    p->map.objects[7][12] = 'C';
    p->map.objects[7][17] = 'C';
    p->map.objects[7][24] = 'C';
    p->map.objects[12][5] = 'L';

    switch_set(p, "door_open", 0);
    switch_set(p, "met_guard", 0);

    e = &p->events[0];
    e->used = 1;
    e->x = 5; e->y = 5;
    e->sprite = 'G';
    e->trigger = TR_ACTION;
    snprintf(e->name, sizeof(e->name), "door_guard");
    snprintf(e->dir, sizeof(e->dir), "ev_5_5");
    e->n_cmds = 0;
    e->cmds[e->n_cmds].type = CMD_SHOW_TEXT;
    snprintf(e->cmds[e->n_cmds].a, MAX_TEXT, "Halt! Factory access restricted.");
    e->n_cmds++;
    e->cmds[e->n_cmds].type = CMD_IF_SWITCH;
    snprintf(e->cmds[e->n_cmds].a, MAX_TEXT, "door_open");
    snprintf(e->cmds[e->n_cmds].b, sizeof(e->cmds[0].b), "1");
    e->n_cmds++;
    e->cmds[e->n_cmds].type = CMD_SHOW_TEXT;
    snprintf(e->cmds[e->n_cmds].a, MAX_TEXT, "Credentials accepted. Proceed.");
    e->n_cmds++;
    e->cmds[e->n_cmds].type = CMD_RET;
    e->n_cmds++;
    e->cmds[e->n_cmds].type = CMD_END;
    e->n_cmds++;
    e->cmds[e->n_cmds].type = CMD_SHOW_TEXT;
    snprintf(e->cmds[e->n_cmds].a, MAX_TEXT, "Find the crystal switch first.");
    e->n_cmds++;
    e->cmds[e->n_cmds].type = CMD_RET;
    e->n_cmds++;

    e = &p->events[1];
    e->used = 1;
    e->x = 24; e->y = 12;
    e->sprite = 'S';
    e->trigger = TR_ACTION;
    snprintf(e->name, sizeof(e->name), "switch_crystal");
    snprintf(e->dir, sizeof(e->dir), "ev_24_12");
    e->n_cmds = 0;
    e->cmds[e->n_cmds].type = CMD_SHOW_TEXT;
    snprintf(e->cmds[e->n_cmds].a, MAX_TEXT, "Crystal hums. Factory doors unlock!");
    e->n_cmds++;
    e->cmds[e->n_cmds].type = CMD_SET_SWITCH;
    snprintf(e->cmds[e->n_cmds].a, MAX_TEXT, "door_open");
    snprintf(e->cmds[e->n_cmds].b, sizeof(e->cmds[0].b), "1");
    e->n_cmds++;
    e->cmds[e->n_cmds].type = CMD_RET;
    e->n_cmds++;

    p->n_events = 2;
    p->dirty = 1;
    project_scan_maps(p);
}
