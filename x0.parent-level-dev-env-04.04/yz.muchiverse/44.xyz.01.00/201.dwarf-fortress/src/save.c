/* save.c — text fort save under saves/<name>/ */
#include "save.h"
#include "map.h"
#include "unit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static int mkpath(const char *path) {
    char tmp[512];
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    {
        char *p;
        for (p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = 0;
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
                *p = '/';
            }
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

int save_fort(const Fort *f, const char *root, const char *name) {
    char dir[512], path[576];
    FILE *fp;
    int x, y, i;

    snprintf(dir, sizeof(dir), "%s/%s", root, name);
    if (mkpath(dir) != 0) return -1;

    /* meta */
    snprintf(path, sizeof(path), "%s/meta.txt", dir);
    fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "format=df-clone-1\n");
    fprintf(fp, "fort=%s\n", f->fort_name);
    fprintf(fp, "seed=%d\n", f->seed);
    fprintf(fp, "year=%d\n", f->year);
    fprintf(fp, "season=%d\n", f->season);
    fprintf(fp, "day=%d\n", f->day);
    fprintf(fp, "tick=%d\n", f->tick);
    fprintf(fp, "beds=%d\n", f->beds_made);
    fprintf(fp, "chairs=%d\n", f->chairs_made);
    fprintf(fp, "craft_order=%d\n", f->craft_order);
    fprintf(fp, "map=%dx%d\n", MAP_W, MAP_H);
    fclose(fp);

    /* map: terrain,desig,stock flags as CSV lines y= */
    snprintf(path, sizeof(path), "%s/map.txt", dir);
    fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "W %d H %d\n", MAP_W, MAP_H);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            const Tile *t = &f->tiles[y][x];
            fprintf(fp, "%d,%d,%d,%d", t->terrain, t->desig, t->stock_wood,
                    t->stock_stone);
            if (x + 1 < MAP_W) fputc(' ', fp);
        }
        fputc('\n', fp);
    }
    fclose(fp);

    snprintf(path, sizeof(path), "%s/items.txt", dir);
    fp = fopen(path, "w");
    if (!fp) return -1;
    for (i = 0; i < MAX_ITEMS; i++) {
        if (!f->items[i].used) continue;
        fprintf(fp, "%d %d %d %d\n", f->items[i].kind, f->items[i].x,
                f->items[i].y, f->items[i].count);
    }
    fclose(fp);

    snprintf(path, sizeof(path), "%s/dwarves.txt", dir);
    fp = fopen(path, "w");
    if (!fp) return -1;
    for (i = 0; i < MAX_DWARVES; i++) {
        const Dwarf *d = &f->dwarves[i];
        if (!d->used) continue;
        fprintf(fp, "%s %d %d %d %d %d\n", d->name, d->x, d->y, d->hunger,
                d->thirst, d->state);
    }
    fclose(fp);
    return 0;
}

int load_fort(Fort *f, const char *root, const char *name) {
    char dir[512], path[576], line[4096];
    FILE *fp;
    int x, y, i, w = 0, h = 0;

    snprintf(dir, sizeof(dir), "%s/%s", root, name);

    snprintf(path, sizeof(path), "%s/meta.txt", dir);
    fp = fopen(path, "r");
    if (!fp) return -1;
    /* reset dynamic */
    memset(f->items, 0, sizeof(f->items));
    memset(f->jobs, 0, sizeof(f->jobs));
    memset(f->dwarves, 0, sizeof(f->dwarves));
    f->n_dwarves = 0;
    f->craft_order = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "fort=", 5) == 0)
            sscanf(line + 5, "%31s", f->fort_name);
        else if (strncmp(line, "seed=", 5) == 0)
            f->seed = atoi(line + 5);
        else if (strncmp(line, "year=", 5) == 0)
            f->year = atoi(line + 5);
        else if (strncmp(line, "season=", 7) == 0)
            f->season = atoi(line + 7);
        else if (strncmp(line, "day=", 4) == 0)
            f->day = atoi(line + 4);
        else if (strncmp(line, "tick=", 5) == 0)
            f->tick = atoi(line + 5);
        else if (strncmp(line, "beds=", 5) == 0)
            f->beds_made = atoi(line + 5);
        else if (strncmp(line, "chairs=", 7) == 0)
            f->chairs_made = atoi(line + 7);
        else if (strncmp(line, "craft_order=", 12) == 0)
            f->craft_order = atoi(line + 12);
    }
    fclose(fp);

    snprintf(path, sizeof(path), "%s/map.txt", dir);
    fp = fopen(path, "r");
    if (!fp) return -1;
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    sscanf(line, "W %d H %d", &w, &h);
    if (w != MAP_W || h != MAP_H) {
        fclose(fp);
        return -1;
    }
    for (y = 0; y < MAP_H; y++) {
        char *p;
        if (!fgets(line, sizeof(line), fp)) break;
        p = line;
        for (x = 0; x < MAP_W; x++) {
            int ter = 0, des = 0, sw = 0, ss = 0;
            int n = 0;
            if (sscanf(p, "%d,%d,%d,%d%n", &ter, &des, &sw, &ss, &n) < 4) break;
            f->tiles[y][x].terrain = (uint8_t)ter;
            f->tiles[y][x].desig = (uint8_t)des;
            f->tiles[y][x].stock_wood = (uint8_t)sw;
            f->tiles[y][x].stock_stone = (uint8_t)ss;
            p += n;
            while (*p == ' ') p++;
        }
    }
    fclose(fp);

    snprintf(path, sizeof(path), "%s/items.txt", dir);
    fp = fopen(path, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            int kind, ix, iy, cnt;
            if (sscanf(line, "%d %d %d %d", &kind, &ix, &iy, &cnt) == 4)
                item_add(f, kind, ix, iy, cnt);
        }
        fclose(fp);
    }

    snprintf(path, sizeof(path), "%s/dwarves.txt", dir);
    fp = fopen(path, "r");
    if (fp) {
        i = 0;
        while (fgets(line, sizeof(line), fp) && i < MAX_DWARVES) {
            char nm[16];
            int dx, dy, hun, thr, st;
            if (sscanf(line, "%15s %d %d %d %d %d", nm, &dx, &dy, &hun, &thr,
                       &st) >= 5) {
                Dwarf *d = &f->dwarves[i];
                d->used = 1;
                snprintf(d->name, sizeof(d->name), "%s", nm);
                d->x = dx;
                d->y = dy;
                d->hunger = hun;
                d->thirst = thr;
                d->state = DW_IDLE;
                d->job = -1;
                d->path_len = 0;
                i++;
            }
        }
        f->n_dwarves = i;
        fclose(fp);
    } else {
        unit_spawn_embark(f);
    }

    item_count_stocks(f);
    f->sel_dwarf = -1;
    f->mode = MODE_LOOK;
    f->drag = 0;
    f->paused = 1;
    f->dirty = 1;
    f->cam_x = MAP_W / 2 - 12;
    f->cam_y = MAP_H / 2 - 10;
    f->cur_x = MAP_W / 2;
    f->cur_y = MAP_H / 2;
    snprintf(f->msg, MAX_MSG, "Loaded fort '%s'.", f->fort_name);
    return 0;
}
