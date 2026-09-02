/* job.c — designations → jobs, completion effects */
#include "job.h"
#include "map.h"
#include <stdio.h>
#include <string.h>

const char *job_kind_name(int k) {
    static const char *n[] = {
        "none", "mine", "cut tree", "haul", "build wall",
        "build workshop", "craft bed", "craft chair"
    };
    if (k < 0 || k > JOB_CRAFT_CHAIR) return "?";
    return n[k];
}

void job_free(Fort *f, int jidx) {
    if (jidx < 0 || jidx >= MAX_JOBS) return;
    f->jobs[jidx].used = 0;
    f->jobs[jidx].claimed_by = -1;
}

int job_add(Fort *f, int kind, int x, int y, int target_item, int work_need) {
    int i;
    /* avoid duplicate same kind at same tile (except haul) */
    if (kind != JOB_HAUL) {
        for (i = 0; i < MAX_JOBS; i++) {
            if (f->jobs[i].used && f->jobs[i].kind == kind &&
                f->jobs[i].x == x && f->jobs[i].y == y)
                return i;
        }
    }
    for (i = 0; i < MAX_JOBS; i++) {
        if (!f->jobs[i].used) {
            f->jobs[i].used = 1;
            f->jobs[i].kind = kind;
            f->jobs[i].x = x;
            f->jobs[i].y = y;
            f->jobs[i].target_item = target_item;
            f->jobs[i].claimed_by = -1;
            f->jobs[i].progress = 0;
            f->jobs[i].work_need = work_need > 0 ? work_need : 20;
            return i;
        }
    }
    return -1;
}

static int job_exists_for_desig(const Fort *f, int x, int y, int kind) {
    int i;
    for (i = 0; i < MAX_JOBS; i++) {
        if (f->jobs[i].used && f->jobs[i].x == x && f->jobs[i].y == y &&
            f->jobs[i].kind == kind)
            return 1;
    }
    return 0;
}

void job_sync_from_designations(Fort *f) {
    int x, y, i;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            uint8_t d = f->tiles[y][x].desig;
            int kind = JOB_NONE;
            int need = 24;
            if (d == DG_DIG) { kind = JOB_DIG; need = 30; }
            else if (d == DG_CUT) { kind = JOB_CUT; need = 20; }
            else if (d == DG_BUILD_WALL) { kind = JOB_BUILD_WALL; need = 40; }
            else if (d == DG_BUILD_WORKSHOP) { kind = JOB_BUILD_WS; need = 50; }
            if (kind != JOB_NONE && !job_exists_for_desig(f, x, y, kind))
                job_add(f, kind, x, y, -1, need);
        }
    }

    /* haul jobs: loose wood/stone not on matching stockpile → nearest stock */
    for (i = 0; i < MAX_ITEMS; i++) {
        int sx, sy, found, best, bd, dx, dy, d2;
        int want_stock;
        if (!f->items[i].used) continue;
        if (f->items[i].kind != IT_WOOD && f->items[i].kind != IT_STONE) continue;
        x = f->items[i].x;
        y = f->items[i].y;
        if (f->items[i].kind == IT_WOOD && f->tiles[y][x].stock_wood) continue;
        if (f->items[i].kind == IT_STONE && f->tiles[y][x].stock_stone) continue;
        /* already have haul for this item? */
        {
            int j, has = 0;
            for (j = 0; j < MAX_JOBS; j++) {
                if (f->jobs[j].used && f->jobs[j].kind == JOB_HAUL &&
                    f->jobs[j].target_item == i) {
                    has = 1;
                    break;
                }
            }
            if (has) continue;
        }
        want_stock = (f->items[i].kind == IT_WOOD) ? 1 : 0;
        found = 0;
        best = -1;
        bd = 999999;
        for (sy = 0; sy < MAP_H; sy++) {
            for (sx = 0; sx < MAP_W; sx++) {
                int ok = want_stock ? f->tiles[sy][sx].stock_wood
                                    : f->tiles[sy][sx].stock_stone;
                if (!ok) continue;
                if (!map_walkable(f, sx, sy)) continue;
                dx = sx - x;
                dy = sy - y;
                d2 = dx * dx + dy * dy;
                if (d2 < bd) {
                    bd = d2;
                    best = sy * MAP_W + sx;
                    found = 1;
                }
            }
        }
        if (found) {
            int tx = best % MAP_W, ty = best / MAP_W;
            /* store dest in job x,y; item in target_item */
            job_add(f, JOB_HAUL, tx, ty, i, 8);
        }
    }

    /* craft orders: find a workshop and create craft job if wood available */
    if (f->craft_order == 1 || f->craft_order == 2) {
        int wx = -1, wy = -1, has = 0, j;
        for (y = 0; y < MAP_H && wx < 0; y++)
            for (x = 0; x < MAP_W; x++)
                if (f->tiles[y][x].terrain == TR_WORKSHOP) {
                    wx = x;
                    wy = y;
                    break;
                }
        if (wx >= 0) {
            int kind = (f->craft_order == 1) ? JOB_CRAFT_BED : JOB_CRAFT_CHAIR;
            for (j = 0; j < MAX_JOBS; j++) {
                if (f->jobs[j].used && f->jobs[j].kind == kind) {
                    has = 1;
                    break;
                }
            }
            if (!has && f->wood_stock >= 1)
                job_add(f, kind, wx, wy, -1, 45);
        }
    }
}

void job_complete(Fort *f, int jidx) {
    Job *j;
    int x, y;
    if (jidx < 0 || jidx >= MAX_JOBS || !f->jobs[jidx].used) return;
    j = &f->jobs[jidx];
    x = j->x;
    y = j->y;

    switch (j->kind) {
    case JOB_DIG:
        if (map_in_bounds(x, y)) {
            uint8_t was = f->tiles[y][x].terrain;
            f->tiles[y][x].terrain = TR_FLOOR;
            f->tiles[y][x].desig = DG_NONE;
            if (was == TR_ROCK)
                item_add(f, IT_STONE, x, y, 1 + (x + y) % 2);
            else
                item_add(f, IT_STONE, x, y, 1); /* soil also yields rubble */
        }
        snprintf(f->msg, MAX_MSG, "Mined (%d,%d).", x, y);
        break;
    case JOB_CUT:
        if (map_in_bounds(x, y) && f->tiles[y][x].terrain == TR_TREE) {
            f->tiles[y][x].terrain = TR_FLOOR;
            f->tiles[y][x].desig = DG_NONE;
            item_add(f, IT_WOOD, x, y, 2);
        }
        snprintf(f->msg, MAX_MSG, "Felled tree at (%d,%d).", x, y);
        break;
    case JOB_BUILD_WALL:
        if (map_in_bounds(x, y) && f->tiles[y][x].terrain == TR_FLOOR) {
            /* consume 1 stone if any */
            int i, took = 0;
            for (i = 0; i < MAX_ITEMS; i++) {
                if (f->items[i].used && f->items[i].kind == IT_STONE) {
                    item_take(f, i, 1);
                    took = 1;
                    break;
                }
            }
            if (took) {
                f->tiles[y][x].terrain = TR_WALL;
                f->tiles[y][x].desig = DG_NONE;
                snprintf(f->msg, MAX_MSG, "Built wall at (%d,%d).", x, y);
            } else {
                snprintf(f->msg, MAX_MSG, "Need stone to build wall.");
                f->tiles[y][x].desig = DG_NONE;
            }
        }
        break;
    case JOB_BUILD_WS:
        if (map_in_bounds(x, y) && f->tiles[y][x].terrain == TR_FLOOR) {
            int i, took = 0;
            for (i = 0; i < MAX_ITEMS; i++) {
                if (f->items[i].used && f->items[i].kind == IT_WOOD &&
                    f->items[i].count >= 3) {
                    item_take(f, i, 3);
                    took = 1;
                    break;
                }
            }
            if (!took) {
                /* try accumulate from multiple stacks */
                int need = 3;
                for (i = 0; i < MAX_ITEMS && need > 0; i++) {
                    if (f->items[i].used && f->items[i].kind == IT_WOOD) {
                        int n = f->items[i].count < need ? f->items[i].count : need;
                        item_take(f, i, n);
                        need -= n;
                    }
                }
                took = (need == 0);
            }
            if (took) {
                f->tiles[y][x].terrain = TR_WORKSHOP;
                f->tiles[y][x].desig = DG_NONE;
                snprintf(f->msg, MAX_MSG, "Carpenter workshop ready.");
            } else {
                f->tiles[y][x].desig = DG_NONE;
                snprintf(f->msg, MAX_MSG, "Need 3 wood for workshop.");
            }
        }
        break;
    case JOB_HAUL: {
        int ii = j->target_item;
        if (ii >= 0 && ii < MAX_ITEMS && f->items[ii].used) {
            int kind = f->items[ii].kind;
            int cnt = f->items[ii].count;
            item_take(f, ii, cnt);
            item_add(f, kind, x, y, cnt);
            snprintf(f->msg, MAX_MSG, "Hauled %s to stockpile.", item_name(kind));
        }
        break;
    }
    case JOB_CRAFT_BED:
    case JOB_CRAFT_CHAIR: {
        int need = (j->kind == JOB_CRAFT_BED) ? 2 : 1;
        int left = need, i;
        for (i = 0; i < MAX_ITEMS && left > 0; i++) {
            if (f->items[i].used && f->items[i].kind == IT_WOOD) {
                int n = f->items[i].count < left ? f->items[i].count : left;
                item_take(f, i, n);
                left -= n;
            }
        }
        if (left == 0) {
            if (j->kind == JOB_CRAFT_BED) {
                item_add(f, IT_BED, x, y, 1);
                f->beds_made++;
                snprintf(f->msg, MAX_MSG, "Crafted a bed.");
            } else {
                item_add(f, IT_CHAIR, x, y, 1);
                f->chairs_made++;
                snprintf(f->msg, MAX_MSG, "Crafted a chair.");
            }
            f->craft_order = 0;
        } else {
            snprintf(f->msg, MAX_MSG, "Not enough wood to craft.");
            f->craft_order = 0;
        }
        break;
    }
    default:
        break;
    }
    item_count_stocks(f);
    job_free(f, jidx);
    f->dirty = 1;
}

void job_try_assign(Fort *f) {
    int d, j;
    for (d = 0; d < MAX_DWARVES; d++) {
        Dwarf *dw;
        if (!f->dwarves[d].used) continue;
        dw = &f->dwarves[d];
        if (dw->state != DW_IDLE || dw->job >= 0) continue;
        /* pick nearest unclaimed job */
        {
            int best = -1, bd = 999999;
            for (j = 0; j < MAX_JOBS; j++) {
                int dx, dy, d2;
                if (!f->jobs[j].used) continue;
                if (f->jobs[j].claimed_by >= 0) continue;
                /* craft needs wood */
                if (f->jobs[j].kind == JOB_CRAFT_BED ||
                    f->jobs[j].kind == JOB_CRAFT_CHAIR) {
                    item_count_stocks(f);
                    if (f->wood_stock < 1) continue;
                }
                if (f->jobs[j].kind == JOB_BUILD_WALL) {
                    item_count_stocks(f);
                    if (f->stone_stock < 1) continue;
                }
                if (f->jobs[j].kind == JOB_BUILD_WS) {
                    item_count_stocks(f);
                    if (f->wood_stock < 3) continue;
                }
                if (f->jobs[j].kind == JOB_HAUL) {
                    int ii = f->jobs[j].target_item;
                    if (ii < 0 || !f->items[ii].used) {
                        job_free(f, j);
                        continue;
                    }
                }
                dx = f->jobs[j].x - dw->x;
                dy = f->jobs[j].y - dw->y;
                /* for haul, path to item first — use item pos for distance */
                if (f->jobs[j].kind == JOB_HAUL) {
                    int ii = f->jobs[j].target_item;
                    dx = f->items[ii].x - dw->x;
                    dy = f->items[ii].y - dw->y;
                }
                d2 = dx * dx + dy * dy;
                if (d2 < bd) {
                    bd = d2;
                    best = j;
                }
            }
            if (best >= 0) {
                f->jobs[best].claimed_by = d;
                dw->job = best;
                dw->state = DW_PATH;
                dw->path_len = 0;
                dw->path_i = 0;
            }
        }
    }
}
