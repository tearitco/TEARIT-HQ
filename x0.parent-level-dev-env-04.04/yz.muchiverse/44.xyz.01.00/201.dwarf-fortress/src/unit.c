/* unit.c — dwarves, BFS pathfinding, work ticks */
#include "unit.h"
#include "job.h"
#include "map.h"
#include <stdio.h>
#include <string.h>

static const char *DWARF_NAMES[] = {
    "Urist", "Dodok", "Litast", "Meng", "Rakust", "Sibrek", "Cog"
};

const char *dwarf_state_name(int st) {
    if (st == DW_IDLE) return "idle";
    if (st == DW_PATH) return "pathing";
    if (st == DW_WORK) return "working";
    return "?";
}

int unit_at(const Fort *f, int x, int y) {
    int i;
    for (i = 0; i < MAX_DWARVES; i++) {
        if (f->dwarves[i].used && f->dwarves[i].x == x && f->dwarves[i].y == y)
            return i;
    }
    return -1;
}

void unit_spawn_embark(Fort *f) {
    int cx = MAP_W / 2, cy = MAP_H / 2;
    int i, n = 5;
    static const int ox[] = { 0, 1, -1, 0, 1, -1, 0 };
    static const int oy[] = { 0, 0, 0, 1, 1, 1, -1 };
    memset(f->dwarves, 0, sizeof(f->dwarves));
    f->n_dwarves = n;
    for (i = 0; i < n; i++) {
        Dwarf *d = &f->dwarves[i];
        d->used = 1;
        d->x = cx + ox[i];
        d->y = cy + oy[i];
        if (!map_walkable(f, d->x, d->y)) {
            d->x = cx;
            d->y = cy;
        }
        d->state = DW_IDLE;
        d->job = -1;
        d->hunger = 30 + i * 5;
        d->thirst = 20 + i * 3;
        snprintf(d->name, sizeof(d->name), "%s", DWARF_NAMES[i % 7]);
    }
}

/* BFS 4-dir path into dwarf path arrays. Target may be unwalkable (dig/cut):
 * path to nearest adjacent walkable, or onto target if walkable. */
int unit_find_path(Fort *f, Dwarf *d, int tx, int ty) {
    int qx[MAP_W * MAP_H], qy[MAP_W * MAP_H];
    int head = 0, tail = 0;
    int prev[MAP_H][MAP_W];
    int vis[MAP_H][MAP_W];
    int x, y, i, goal_x = -1, goal_y = -1;
    int adj_only = !map_walkable(f, tx, ty);

    memset(vis, 0, sizeof(vis));
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++)
            prev[y][x] = -1;

    if (!map_in_bounds(d->x, d->y)) return 0;
    qx[tail] = d->x;
    qy[tail] = d->y;
    tail++;
    vis[d->y][d->x] = 1;

    while (head < tail) {
        int cx = qx[head], cy = qy[head];
        static const int dx4[] = { 1, -1, 0, 0 };
        static const int dy4[] = { 0, 0, 1, -1 };
        head++;

        if (adj_only) {
            int k;
            for (k = 0; k < 4; k++) {
                if (cx + dx4[k] == tx && cy + dy4[k] == ty) {
                    goal_x = cx;
                    goal_y = cy;
                    head = tail; /* break outer */
                    break;
                }
            }
            if (goal_x >= 0) break;
        } else if (cx == tx && cy == ty) {
            goal_x = cx;
            goal_y = cy;
            break;
        }

        for (i = 0; i < 4; i++) {
            int nx = cx + dx4[i], ny = cy + dy4[i];
            if (!map_in_bounds(nx, ny)) continue;
            if (vis[ny][nx]) continue;
            if (!map_walkable(f, nx, ny)) continue;
            vis[ny][nx] = 1;
            prev[ny][nx] = cy * MAP_W + cx;
            qx[tail] = nx;
            qy[tail] = ny;
            tail++;
        }
    }

    if (goal_x < 0) {
        d->path_len = 0;
        d->path_i = 0;
        return 0;
    }

    /* reconstruct */
    {
        int pathx[MAX_PATH], pathy[MAX_PATH];
        int len = 0;
        x = goal_x;
        y = goal_y;
        while (!(x == d->x && y == d->y)) {
            if (len >= MAX_PATH - 1) break;
            pathx[len] = x;
            pathy[len] = y;
            len++;
            {
                int p = prev[y][x];
                if (p < 0) break;
                x = p % MAP_W;
                y = p / MAP_W;
            }
        }
        /* reverse into dwarf */
        d->path_len = len;
        d->path_i = 0;
        for (i = 0; i < len; i++) {
            d->path_x[i] = pathx[len - 1 - i];
            d->path_y[i] = pathy[len - 1 - i];
        }
    }
    return d->path_len > 0 || (d->x == goal_x && d->y == goal_y);
}

static int adjacent_or_on(int x, int y, int tx, int ty) {
    int dx = x > tx ? x - tx : tx - x;
    int dy = y > ty ? y - ty : ty - y;
    if (x == tx && y == ty) return 1;
    return (dx + dy == 1);
}

static void unit_begin_work(Fort *f, Dwarf *d) {
    Job *j;
    if (d->job < 0) {
        d->state = DW_IDLE;
        return;
    }
    j = &f->jobs[d->job];
    d->state = DW_WORK;
    d->work_timer = 0;
    (void)j;
}

static void unit_tick_one(Fort *f, int di) {
    Dwarf *d = &f->dwarves[di];
    Job *j;
    int tx, ty, dest_x, dest_y;

    if (!d->used) return;

    /* mild need stub */
    if ((f->tick % 40) == di) {
        d->hunger = d->hunger < 100 ? d->hunger + 1 : 100;
        d->thirst = d->thirst < 100 ? d->thirst + 1 : 100;
    }

    if (d->job < 0 || !f->jobs[d->job].used) {
        d->job = -1;
        d->state = DW_IDLE;
        return;
    }
    j = &f->jobs[d->job];

    /* haul: go to item first, then to stockpile */
    if (j->kind == JOB_HAUL) {
        int ii = j->target_item;
        if (ii < 0 || !f->items[ii].used) {
            job_free(f, d->job);
            d->job = -1;
            d->state = DW_IDLE;
            return;
        }
        /* phase: if not holding (use progress flag 0=to item, 1=to stock) */
        if (j->progress == 0) {
            dest_x = f->items[ii].x;
            dest_y = f->items[ii].y;
        } else {
            dest_x = j->x;
            dest_y = j->y;
        }
    } else {
        dest_x = j->x;
        dest_y = j->y;
    }

    tx = dest_x;
    ty = dest_y;

    if (d->state == DW_PATH) {
        int need_adj = (j->kind == JOB_DIG || j->kind == JOB_CUT ||
                        j->kind == JOB_BUILD_WALL);
        int arrived = 0;

        if (d->path_len == 0 && d->path_i == 0) {
            /* compute path */
            if (need_adj && !map_walkable(f, tx, ty)) {
                if (!unit_find_path(f, d, tx, ty)) {
                    /* stuck — release job */
                    j->claimed_by = -1;
                    d->job = -1;
                    d->state = DW_IDLE;
                    return;
                }
            } else {
                if (d->x == tx && d->y == ty) {
                    arrived = 1;
                } else if (!unit_find_path(f, d, tx, ty)) {
                    j->claimed_by = -1;
                    d->job = -1;
                    d->state = DW_IDLE;
                    return;
                }
            }
        }

        if (!arrived) {
            if (need_adj && adjacent_or_on(d->x, d->y, j->x, j->y) &&
                j->kind != JOB_HAUL) {
                arrived = 1;
            } else if (!need_adj && d->x == tx && d->y == ty) {
                arrived = 1;
            } else if (j->kind == JOB_HAUL && d->x == tx && d->y == ty) {
                arrived = 1;
            } else if (d->path_i < d->path_len) {
                d->x = d->path_x[d->path_i];
                d->y = d->path_y[d->path_i];
                d->path_i++;
                f->dirty = 1;
                if (need_adj && adjacent_or_on(d->x, d->y, j->x, j->y) &&
                    j->kind != JOB_HAUL)
                    arrived = 1;
                else if (d->x == tx && d->y == ty)
                    arrived = 1;
            } else {
                /* path exhausted */
                if (need_adj && adjacent_or_on(d->x, d->y, j->x, j->y))
                    arrived = 1;
                else if (d->x == tx && d->y == ty)
                    arrived = 1;
                else {
                    /* repath next tick */
                    d->path_len = 0;
                    d->path_i = 0;
                }
            }
        }

        if (arrived) {
            if (j->kind == JOB_HAUL && j->progress == 0) {
                /* picked up — now go to stockpile */
                j->progress = 1;
                d->path_len = 0;
                d->path_i = 0;
                d->state = DW_PATH;
            } else {
                unit_begin_work(f, d);
            }
        }
        return;
    }

    if (d->state == DW_WORK) {
        d->work_timer++;
        j->progress = d->work_timer; /* for non-haul */
        if (d->work_timer >= j->work_need) {
            int jid = d->job;
            d->job = -1;
            d->state = DW_IDLE;
            d->path_len = 0;
            job_complete(f, jid);
        }
        f->dirty = 1;
    }
}

void unit_step_all(Fort *f) {
    int i;
    job_sync_from_designations(f);
    job_try_assign(f);
    for (i = 0; i < MAX_DWARVES; i++)
        unit_tick_one(f, i);
    item_count_stocks(f);
}
