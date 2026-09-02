#include "gb.h"

static int tdist(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

void tactics_calc_move_range(Game *g, int uidx, int range_map[TACT_ROWS][TACT_COLS]) {
    TacticsUnit *u = &g->tactics.units[uidx];
    int visited[TACT_ROWS][TACT_COLS], sx[64], sy[64], sd[64], head = 0, tail = 0;
    memset(visited, 0, sizeof(visited));
    memset(range_map, 0, sizeof(int) * TACT_ROWS * TACT_COLS);
    sx[tail] = u->x; sy[tail] = u->y; sd[tail] = 0; tail++;
    visited[u->y][u->x] = 1;
    while (head < tail) {
        int cx = sx[head], cy = sy[head], cd = sd[head]; head++;
        if (cd > u->move_range) continue;
        range_map[cy][cx] = 1;
        { int dx[] = {0,0,-1,1}, dy[] = {-1,1,0,0}, j;
        for (j = 0; j < 4; j++) {
            int nx = cx + dx[j], ny = cy + dy[j], blocked = 0, k;
            if (nx < 0 || nx >= TACT_COLS || ny < 0 || ny >= TACT_ROWS) continue;
            if (visited[ny][nx]) continue;
            for (k = 0; k < g->tactics.unit_n; k++) {
                TacticsUnit *ou = &g->tactics.units[k];
                if (k != uidx && ou->active && ou->hp > 0 && ou->x == nx && ou->y == ny)
                    { blocked = 1; break; }
            }
            if (blocked) continue;
            visited[ny][nx] = 1;
            sx[tail] = nx; sy[tail] = ny; sd[tail] = cd + 1; tail++;
        }}
    }
}

static int team_alive(TacticsBattle *tb, int player) {
    int i;
    for (i = 0; i < tb->unit_n; i++)
        if (tb->units[i].active && tb->units[i].player == player && tb->units[i].hp > 0) return 1;
    return 0;
}

static int all_team_done(TacticsBattle *tb, int player) {
    int i;
    for (i = 0; i < tb->unit_n; i++)
        if (tb->units[i].active && tb->units[i].player == player && tb->units[i].hp > 0)
            if (!(tb->units[i].moved && tb->units[i].acted)) return 0;
    return 1;
}

static void switch_phase(Game *g) {
    TacticsBattle *tb = &g->tactics;
    int i;
    if (!team_alive(tb, 1)) {
        tb->phase = 2; snprintf(tb->msg, sizeof(tb->msg), "Player 1 wins!"); tb->wait = 90;
        tb->ai_state = 0; return;
    }
    if (!team_alive(tb, 0)) {
        tb->phase = 3; snprintf(tb->msg, sizeof(tb->msg), "Player 2 wins!"); tb->wait = 90;
        tb->ai_state = 0; return;
    }
    tb->phase = tb->phase == 0 ? 1 : 0;
    if (tb->phase == 1) {
        snprintf(tb->msg, sizeof(tb->msg), "Player 2's turn (AI)");
        tb->ai_state = 1;
        tb->ai_delay = 30;
        tb->ai_unit = 0;
    } else {
        tb->turn_num++;
        snprintf(tb->msg, sizeof(tb->msg), "Player 1's turn (Turn %d)", tb->turn_num);
        tb->ai_state = 0;
    }
    for (i = 0; i < tb->unit_n; i++)
        if (tb->units[i].active && tb->units[i].hp > 0)
            { tb->units[i].moved = 0; tb->units[i].acted = 0; }
    tb->sel_unit = -1; tb->sub_phase = 0; tb->wait = 40;
}

static void deploy_mon(Game *g, int party_idx) {
    TacticsBattle *tb = &g->tactics;
    PartyMon *pm = &g->player.party[party_idx];
    TacticsUnit *u;
    if (pm->hp <= 0) return;
    if (tb->unit_n >= TACT_MAX_UNITS) return;
    /* check if already deployed */
    int j; for (j = 0; j < tb->unit_n; j++)
        if (tb->units[j].player == 0 && tb->units[j].active &&
            tb->units[j].species == pm->species && tb->units[j].hp == pm->hp) return;
    u = &tb->units[tb->unit_n++];
    memset(u, 0, sizeof(*u)); u->active = 1; u->player = 0;
    u->species = pm->species; u->level = pm->level;
    u->hp = pm->hp; u->max_hp = pm->max_hp;
    u->atk = pm->atk; u->def = pm->def; u->spd = pm->spd;
    u->move_range = 3 + u->spd / 8;
    if (u->move_range < 3) u->move_range = 3;
    if (u->move_range > 7) u->move_range = 7;
    u->move_id[0] = pm->move_id[0]; u->move_id[1] = pm->move_id[1];
    /* find next free slot on player's side */
    int taken[6] = {0};
    for (j = 0; j < tb->unit_n; j++)
        if (tb->units[j].active && tb->units[j].player == 0 && tb->units[j].hp > 0 &&
            tb->units[j].y >= 0 && tb->units[j].y < 6)
            taken[tb->units[j].y] = 1;
    int slot = 0; while (slot < 6 && taken[slot]) slot++;
    if (slot >= 6) slot = 0;
    u->x = 1; u->y = 1 + slot;
    if (u->y >= TACT_ROWS) u->y = TACT_ROWS - 1;
}

void tactics_pvp_start(Game *g) {
    TacticsBattle *tb = &g->tactics;
    int i, p1_species[] = {1,2,3,4,5,9}, p2_species[] = {4,5,6,7,8,9};
    memset(tb, 0, sizeof(*tb));
    tb->active = 1; tb->phase = 0; tb->turn_num = 1;
    tb->cursor_x = 1; tb->cursor_y = 3; tb->sel_unit = -1; tb->sub_phase = 0;
    tb->move_sel = 0; tb->ai_state = 0; tb->ai_stuck = 0;
    for (i = 0; i < 6; i++) {
        const Species *sp = mon_species(g, p1_species[i]);
        if (sp) {
            TacticsUnit *u = &tb->units[tb->unit_n++];
            memset(u, 0, sizeof(*u)); u->active = 1; u->player = 0;
            u->x = 1; u->y = 1 + i; if (u->y >= TACT_ROWS) u->y = TACT_ROWS - 1;
            u->species = sp->id; u->level = 10;
            u->max_hp = (2 * sp->base_hp * 10) / 100 + 10 + 10; u->hp = u->max_hp;
            u->atk = (2 * sp->base_atk * 10) / 100 + 5;
            u->def = (2 * sp->base_def * 10) / 100 + 5;
            u->spd = (2 * sp->base_spd * 10) / 100 + 5;
            u->move_range = 3 + u->spd / 8; if (u->move_range < 3) u->move_range = 3; if (u->move_range > 7) u->move_range = 7;
            u->move_id[0] = 1;
            u->move_id[1] = sp->type == TYPE_GRASS ? 3 : sp->type == TYPE_FIRE ? 4 : sp->type == TYPE_WATER ? 5 : 6;
        }
        sp = mon_species(g, p2_species[i]);
        if (sp) {
            TacticsUnit *u = &tb->units[tb->unit_n++];
            memset(u, 0, sizeof(*u)); u->active = 1; u->player = 1;
            u->x = TACT_COLS - 2; u->y = 1 + i; if (u->y >= TACT_ROWS) u->y = TACT_ROWS - 1;
            u->species = sp->id; u->level = 10;
            u->max_hp = (2 * sp->base_hp * 10) / 100 + 10 + 10; u->hp = u->max_hp;
            u->atk = (2 * sp->base_atk * 10) / 100 + 5;
            u->def = (2 * sp->base_def * 10) / 100 + 5;
            u->spd = (2 * sp->base_spd * 10) / 100 + 5;
            u->move_range = 3 + u->spd / 8; if (u->move_range < 3) u->move_range = 3; if (u->move_range > 7) u->move_range = 7;
            u->move_id[0] = 1;
            u->move_id[1] = sp->type == TYPE_GRASS ? 3 : sp->type == TYPE_FIRE ? 4 : sp->type == TYPE_WATER ? 5 : 6;
        }
    }
    g->mode = MODE_PVP_BATTLE;
    snprintf(tb->msg, sizeof(tb->msg), "Player 1's turn (Turn 1)");
    tb->wait = 30; g->need_redraw = 1;
}

static void do_attack(Game *g, int atk_idx, int def_idx) {
    TacticsBattle *tb = &g->tactics;
    TacticsUnit *u = &tb->units[atk_idx];
    TacticsUnit *enemy = &tb->units[def_idx];
    const MoveDef *mv = mon_move(g, u->move_id[tb->move_sel]);
    if (!mv) mv = mon_move(g, u->move_id[0]);
    if (!mv) return;
    int atk_range = mv->range;
    if (tdist(u->x, u->y, enemy->x, enemy->y) > atk_range) return;
    int base = ((2 * u->level / 5 + 2) * mv->power * u->atk) / (enemy->def > 0 ? enemy->def : 1);
    base = base / 50 + 2;
    int roll = 85 + (rand() % 16); int dmg = (base * roll) / 100;
    if (dmg < 1) dmg = 1;
    const Species *ua = mon_species(g, u->species);
    const Species *ud = mon_species(g, enemy->species);
    snprintf(tb->msg, sizeof(tb->msg), "%s's %s hits %s for %d!",
             ua ? ua->name : "?", mv->name, ud ? ud->name : "?", dmg);
    enemy->hp -= dmg; if (enemy->hp < 0) enemy->hp = 0;
    u->acted = 1; tb->sel_unit = -1; tb->sub_phase = 0; tb->wait = 40;
    g->need_redraw = 1;
    if (!team_alive(tb, 1)) { tb->phase = 2; snprintf(tb->msg, sizeof(tb->msg), "Player 1 wins!"); tb->wait = 90; tb->ai_state = 0; }
    else if (!team_alive(tb, 0)) { tb->phase = 3; snprintf(tb->msg, sizeof(tb->msg), "Player 2 wins!"); tb->wait = 90; tb->ai_state = 0; }
    else if (all_team_done(tb, tb->phase == 0 ? 0 : 1)) switch_phase(g);
}

void tactics_input(Game *g, int vk) {
    TacticsBattle *tb = &g->tactics;
    int cur_player, i;
    if (!tb->active) return;

    /* Deploy phase */
    if (tb->deploying) {
        if (vk == VK_UP) {
            tb->deploy_sel = tb->deploy_sel > 0 ? tb->deploy_sel - 1 : g->player.party_n - 1;
            if (tb->deploy_sel < 0) tb->deploy_sel = 0;
            g->need_redraw = 1;
        } else if (vk == VK_DOWN) {
            tb->deploy_sel = tb->deploy_sel + 1 < g->player.party_n ? tb->deploy_sel + 1 : 0;
            g->need_redraw = 1;
        } else if (vk == VK_A && g->player.party_n > 0) {
            deploy_mon(g, tb->deploy_sel);
            g->need_redraw = 1;
        } else if (vk == VK_B) {
            tb->deploying = 0;
            tb->phase = 0;
            tb->turn_num = 1;
            tb->sel_unit = -1; tb->sub_phase = 0;
            tb->wait = 20;
            snprintf(tb->msg, sizeof(tb->msg), "Turn 1 — deploy more with menu!");
            if (tb->tactics_wild) snprintf(tb->msg, sizeof(tb->msg), "Wild battle!");
            else if (tb->tactics_trainer) {
                /* find trainer name */
                if (tb->trainer_idx >= 0 && tb->trainer_idx < g->map.trainers_n)
                    snprintf(tb->msg, sizeof(tb->msg), "%s wants to battle!",
                             g->map.trainers[tb->trainer_idx].name);
            }
            g->need_redraw = 1;
        }
        return;
    }

    /* Emergency: Q to force-end active player's turn (always works) */
    if (vk == 'q' || vk == 'Q') {
        if (tb->phase < 2) {
            int cur = tb->phase;
            int j;
            for (j = 0; j < tb->unit_n; j++)
                if (tb->units[j].active && tb->units[j].player == cur && tb->units[j].hp > 0)
                    { tb->units[j].moved = 1; tb->units[j].acted = 1; }
            tb->sel_unit = -1; tb->sub_phase = 0; tb->wait = 0; tb->ai_state = 0; tb->ai_delay = 0;
            switch_phase(g);
            g->need_redraw = 1;
        }
        return;
    }

    if (tb->wait > 0) {
        if (vk == VK_A || vk == 'z' || vk == 'Z' || vk == '\r' || vk == ' ')
            tb->wait = 0;
        return;
    }
    /* Block input during AI turn */
    if (tb->phase == 1 && tb->ai_state > 0) return;
    cur_player = tb->phase;

    if (tb->sub_phase == 0) {
        if (vk == VK_UP || vk == 'w') { if (tb->cursor_y > 0) tb->cursor_y--; g->need_redraw = 1; }
        else if (vk == VK_DOWN || vk == 's') { if (tb->cursor_y < TACT_ROWS-1) tb->cursor_y++; g->need_redraw = 1; }
        else if (vk == VK_LEFT || vk == 'a') { if (tb->cursor_x > 0) tb->cursor_x--; g->need_redraw = 1; }
        else if (vk == VK_RIGHT || vk == 'd') { if (tb->cursor_x < TACT_COLS-1) tb->cursor_x++; g->need_redraw = 1; }
        else if (vk == VK_A) {
            for (i = 0; i < tb->unit_n; i++) {
                TacticsUnit *u = &tb->units[i];
                if (u->active && u->hp > 0 && u->player == cur_player &&
                    u->x == tb->cursor_x && u->y == tb->cursor_y && !(u->moved && u->acted)) {
                    tb->sel_unit = i; tb->sub_phase = 1;
                    tb->cursor_x = u->x; tb->cursor_y = u->y; g->need_redraw = 1; return;
                }
            }
        }
    } else if (tb->sub_phase == 1) {
        TacticsUnit *u = &tb->units[tb->sel_unit];
        int range_map[TACT_ROWS][TACT_COLS]; tactics_calc_move_range(g, tb->sel_unit, range_map);
        if (vk == VK_UP || vk == 'w') { if (tb->cursor_y > 0) tb->cursor_y--; g->need_redraw = 1; }
        else if (vk == VK_DOWN || vk == 's') { if (tb->cursor_y < TACT_ROWS-1) tb->cursor_y++; g->need_redraw = 1; }
        else if (vk == VK_LEFT || vk == 'a') { if (tb->cursor_x > 0) tb->cursor_x--; g->need_redraw = 1; }
        else if (vk == VK_RIGHT || vk == 'd') { if (tb->cursor_x < TACT_COLS-1) tb->cursor_x++; g->need_redraw = 1; }
        else if (vk == VK_A) {
            if (range_map[tb->cursor_y][tb->cursor_x] && !(tb->cursor_x == u->x && tb->cursor_y == u->y)) {
                u->x = tb->cursor_x; u->y = tb->cursor_y; u->moved = 1;
                tb->sub_phase = 2; g->need_redraw = 1;
            }
        } else if (vk == VK_B || vk == 'x' || vk == 'X') {
            u->moved = 1; tb->sub_phase = 2;
            tb->cursor_x = u->x; tb->cursor_y = u->y; g->need_redraw = 1;
        }
    } else if (tb->sub_phase == 2) {
        TacticsUnit *u = &tb->units[tb->sel_unit];
        /* Move selection with 1/2 keys or L/R */
        if (vk == '1' || vk == VK_LEFT) {
            tb->move_sel = 0; g->need_redraw = 1;
        } else if (vk == '2' || vk == VK_RIGHT) {
            tb->move_sel = 1; g->need_redraw = 1;
        } else if (vk == VK_UP || vk == 'w') { if (tb->cursor_y > 0) tb->cursor_y--; g->need_redraw = 1; }
        else if (vk == VK_DOWN || vk == 's') { if (tb->cursor_y < TACT_ROWS-1) tb->cursor_y++; g->need_redraw = 1; }
        else if (vk == VK_LEFT || vk == 'a') { if (tb->cursor_x > 0) tb->cursor_x--; g->need_redraw = 1; }
        else if (vk == VK_RIGHT || vk == 'd') { if (tb->cursor_x < TACT_COLS-1) tb->cursor_x++; g->need_redraw = 1; }
        else if (vk == VK_A) {
            for (i = 0; i < tb->unit_n; i++) {
                TacticsUnit *enemy = &tb->units[i];
                if (!(enemy->active && enemy->hp > 0 && enemy->player != cur_player &&
                      enemy->x == tb->cursor_x && enemy->y == tb->cursor_y)) continue;
                const MoveDef *mv = mon_move(g, u->move_id[tb->move_sel]);
                if (!mv) mv = mon_move(g, u->move_id[0]);
                int atk_range = mv ? mv->range : 1;
                if (tdist(u->x, u->y, enemy->x, enemy->y) > atk_range) continue;
                do_attack(g, tb->sel_unit, i);
                return;
            }
        } else if (vk == VK_B || vk == 'x' || vk == 'X') {
            u->acted = 1; tb->sel_unit = -1; tb->sub_phase = 0; g->need_redraw = 1;
            if (all_team_done(tb, cur_player)) switch_phase(g);
        }
    }
}

static int ai_find_target(TacticsBattle *tb, int uidx) {
    TacticsUnit *u = &tb->units[uidx];
    int i, nearest = -1, near_d = 999;
    for (i = 0; i < tb->unit_n; i++) {
        if (tb->units[i].active && tb->units[i].player != u->player && tb->units[i].hp > 0) {
            int d = tdist(u->x, u->y, tb->units[i].x, tb->units[i].y);
            if (d < near_d) { near_d = d; nearest = i; }
        }
    }
    return nearest;
}

void tactics_start_wild(Game *g) {
    TacticsBattle *tb = &g->tactics;
    int i, num_wild;
    static const int WPOOL[] = {4,5,6,7,8,9};
    memset(tb, 0, sizeof(*tb));
    tb->active = 1; tb->phase = 0; tb->turn_num = 0;
    tb->cursor_x = 1; tb->cursor_y = 3; tb->sel_unit = -1; tb->sub_phase = 0;
    tb->move_sel = 0; tb->ai_state = 0; tb->ai_stuck = 0; tb->tactics_wild = 1;
    tb->deploying = 1; tb->deploy_sel = 0; tb->deploy_timer = 0;

    /* enemy mons deployed automatically */
    num_wild = 1 + (rand() % 2);
    for (i = 0; i < num_wild && tb->unit_n < TACT_MAX_UNITS; i++) {
        int sid = WPOOL[rand() % 6], lv = 2 + (rand() % 4);
        const Species *sp = mon_species(g, sid);
        if (!sp) sp = mon_species(g, 4);
        if (!sp && g->species_n > 0) sp = &g->species[0];
        if (sp) {
            TacticsUnit *u = &tb->units[tb->unit_n++];
            memset(u, 0, sizeof(*u)); u->active = 1; u->player = 1;
            u->species = sp->id; u->level = lv;
            u->x = TACT_COLS - 2; u->y = 2 + i * 2; if (u->y >= TACT_ROWS) u->y = TACT_ROWS - 2;
            u->max_hp = (2 * sp->base_hp * lv) / 100 + lv + 10; u->hp = u->max_hp;
            u->atk = (2 * sp->base_atk * lv) / 100 + 5;
            u->def = (2 * sp->base_def * lv) / 100 + 5;
            u->spd = (2 * sp->base_spd * lv) / 100 + 5;
            u->move_range = 3 + u->spd / 8; if (u->move_range < 3) u->move_range = 3; if (u->move_range > 7) u->move_range = 7;
            u->move_id[0] = 1;
            u->move_id[1] = sp->type == TYPE_GRASS ? 3 : sp->type == TYPE_FIRE ? 4 : sp->type == TYPE_WATER ? 5 : 6;
        }
    }

    g->mode = MODE_PVP_BATTLE;
    snprintf(tb->msg, sizeof(tb->msg), "Wild battle!");
    tb->wait = 30; g->need_redraw = 1;
}

void tactics_start_trainer(Game *g, Trainer *t) {
    TacticsBattle *tb = &g->tactics;
    int i, t_idx;

    memset(tb, 0, sizeof(*tb));
    tb->active = 1; tb->phase = 0; tb->turn_num = 1;
    tb->cursor_x = 1; tb->cursor_y = 3; tb->sel_unit = -1; tb->sub_phase = 0;
    tb->move_sel = 0; tb->ai_state = 0; tb->ai_stuck = 0;
    tb->tactics_wild = 0; tb->tactics_trainer = 1;

    /* find trainer index in map */
    t_idx = -1;
    for (i = 0; i < g->map.trainers_n; i++)
        if (&g->map.trainers[i] == t) { t_idx = i; break; }
    tb->trainer_idx = t_idx;
    tb->deploying = 1; tb->deploy_sel = 0; tb->deploy_timer = 0;
    tb->turn_num = 0;

    for (i = 0; i < t->party_n && tb->unit_n < TACT_MAX_UNITS; i++) {
        PartyMon *tp = &t->party[i];
        TacticsUnit *u = &tb->units[tb->unit_n++];
        memset(u, 0, sizeof(*u)); u->active = 1; u->player = 1;
        u->species = tp->species; u->level = tp->level;
        u->hp = tp->hp; u->max_hp = tp->max_hp;
        u->atk = tp->atk; u->def = tp->def; u->spd = tp->spd;
        u->move_range = 3 + u->spd / 8; if (u->move_range < 3) u->move_range = 3; if (u->move_range > 7) u->move_range = 7;
        u->move_id[0] = tp->move_id[0]; u->move_id[1] = tp->move_id[1];
        u->x = TACT_COLS - 2; u->y = 2 + i * 2;
        if (u->y >= TACT_ROWS) u->y = TACT_ROWS - 2;
    }

    g->mode = MODE_PVP_BATTLE;
    snprintf(tb->msg, sizeof(tb->msg), "%s wants to battle!", t->name);
    tb->wait = 45; g->need_redraw = 1;
}

void tactics_tick(Game *g) {
    TacticsBattle *tb = &g->tactics;
    if (!tb->active) return;

    /* deploy phase — timer counts up, blackout at 300 frames */
    if (tb->deploying) {
        if (tb->wait > 0) { tb->wait--; g->need_redraw = 1; return; }
        tb->deploy_timer++;
        if (tb->deploy_timer > 300) {
            /* idled too long — force blackout */
            snprintf(tb->msg, sizeof(tb->msg), "Too slow! You blacked out!");
            tb->phase = 3; tb->wait = 90;
            tb->deploying = 0;
        }
        return;
    }

    if (tb->wait > 0) {
        tb->wait--; g->need_redraw = 1;
        if (tb->wait == 0 && tb->phase >= 2) {
            if (tb->tactics_wild) {
                if (tb->phase == 2) {
                    int j;
                    for (j = 0; j < g->player.party_n; j++) {
                        PartyMon *pm = &g->player.party[j];
                        if (pm->hp > 0) {
                            pm->exp += 20;
                            while (pm->exp >= pm->level * 20) {
                                pm->exp -= pm->level * 20;
                                pm->level++;
                                const Species *sp = mon_species(g, pm->species);
                                if (sp) {
                                    int old_max = pm->max_hp;
                                    mon_init_from_species(pm, sp, pm->level);
                                    mon_give_starter_moves(g, pm);
                                    pm->hp = pm->max_hp > old_max ? pm->max_hp : pm->hp;
                                }
                            }
                        }
                    }
                } else {
                    g->player.x = g->map.start_x; g->player.y = g->map.start_y;
                    int k; for (k = 0; k < g->player.party_n; k++) g->player.party[k].hp = g->player.party[k].max_hp;
                }
                tb->active = 0; g->mode = MODE_OVERWORLD; g->need_redraw = 1;
            } else if (tb->tactics_trainer) {
                if (tb->phase == 2) {
                    int j;
                    for (j = 0; j < g->player.party_n; j++) {
                        PartyMon *pm = &g->player.party[j];
                        if (pm->hp > 0) {
                            pm->exp += 30;
                            while (pm->exp >= pm->level * 20) {
                                pm->exp -= pm->level * 20;
                                pm->level++;
                                const Species *sp = mon_species(g, pm->species);
                                if (sp) {
                                    int old_max = pm->max_hp;
                                    mon_init_from_species(pm, sp, pm->level);
                                    mon_give_starter_moves(g, pm);
                                    pm->hp = pm->max_hp > old_max ? pm->max_hp : pm->hp;
                                }
                            }
                        }
                    }
                    /* gym badge award */
                    if (tb->tactics_gym && tb->gym_badge_id >= 0 && tb->gym_badge_id < MAX_BADGES) {
                        g->player.badges[tb->gym_badge_id] = 1;
                        g->player.badge_count++;
                        snprintf(tb->msg, sizeof(tb->msg), "Got Badge %d!", tb->gym_badge_id + 1);
                        tb->wait = 60;
                    }
                    if (tb->trainer_idx >= 0 && tb->trainer_idx < g->map.trainers_n)
                        g->map.trainers[tb->trainer_idx].defeated = 1;
                } else {
                    g->player.x = g->map.start_x; g->player.y = g->map.start_y;
                    int k; for (k = 0; k < g->player.party_n; k++) g->player.party[k].hp = g->player.party[k].max_hp;
                }
                tb->active = 0; g->mode = MODE_OVERWORLD; g->need_redraw = 1;
            } else {
                tb->active = 0; g->mode = MODE_TITLE; g->need_redraw = 1;
            }
        }
        return;
    }
    /* AI stuck safety: if AI hasn't progressed in 5 seconds, force next phase */
    if (tb->phase == 1 && tb->ai_state > 0) {
        tb->ai_stuck++;
        if (tb->ai_stuck > 300) {
            int j;
            for (j = 0; j < tb->unit_n; j++)
                if (tb->units[j].active && tb->units[j].player == 1 && tb->units[j].hp > 0)
                    { tb->units[j].moved = 1; tb->units[j].acted = 1; }
            tb->ai_state = 0;
            switch_phase(g);
            return;
        }
    }
    /* AI turn processing */
    if (tb->phase == 1 && tb->ai_state > 0 && tb->wait == 0) {
        tb->ai_stuck = 0;
        if (tb->ai_delay > 0) { tb->ai_delay--; return; }
        if (tb->ai_state == 1) {
            /* Find next unacted unit */
            while (tb->ai_unit < tb->unit_n) {
                TacticsUnit *u = &tb->units[tb->ai_unit];
                if (u->active && u->hp > 0 && u->player == 1 && !(u->moved && u->acted)) break;
                tb->ai_unit++;
            }
            if (tb->ai_unit >= tb->unit_n) {
                if (all_team_done(tb, 1)) switch_phase(g);
                else { tb->ai_state = 0; return; }
                return;
            }
            int uidx = tb->ai_unit;
            TacticsUnit *u = &tb->units[uidx];
            int target = ai_find_target(tb, uidx);
            if (target < 0) { tb->ai_unit++; tb->ai_delay = 10; return; }
            tb->ai_target = target;
            TacticsUnit *enemy = &tb->units[target];
            const MoveDef *mv = mon_move(g, u->move_id[1]);
            if (!mv) mv = mon_move(g, u->move_id[0]);
            int atk_range = mv ? mv->range : 1;
            int d = tdist(u->x, u->y, enemy->x, enemy->y);
            if (d <= atk_range) {
                tb->ai_state = 3; tb->ai_delay = 15;
            } else {
                int range_map[TACT_ROWS][TACT_COLS];
                tactics_calc_move_range(g, uidx, range_map);
                int best_x = u->x, best_y = u->y, best_d = d;
                int j, k;
                for (j = 0; j < TACT_ROWS; j++)
                    for (k = 0; k < TACT_COLS; k++)
                        if (range_map[j][k]) {
                            int nd = tdist(k, j, enemy->x, enemy->y);
                            if (nd < best_d || (nd == best_d && (abs(k - enemy->x) + abs(j - enemy->y) < best_d))) {
                                best_d = nd; best_x = k; best_y = j;
                            }
                        }
                tb->ai_tx = best_x; tb->ai_ty = best_y;
                if (best_x != u->x || best_y != u->y) {
                    tb->ai_state = 2; tb->ai_delay = 15;
                    const Species *sp = mon_species(g, u->species);
                    snprintf(tb->msg, sizeof(tb->msg), "P2 %s moves", sp ? sp->name : "?");
                } else {
                    tb->ai_state = 3; tb->ai_delay = 10;
                }
            }
        } else if (tb->ai_state == 2) {
            TacticsUnit *u = &tb->units[tb->ai_unit];
            u->x = tb->ai_tx; u->y = tb->ai_ty; u->moved = 1;
            tb->ai_state = 3; tb->ai_delay = 20;
        } else if (tb->ai_state == 3) {
            int uidx = tb->ai_unit;
            TacticsUnit *u = &tb->units[uidx];
            int target = tb->ai_target;
            TacticsUnit *enemy = &tb->units[target];
            if (target >= 0 && target < tb->unit_n && enemy->active && enemy->hp > 0) {
                const MoveDef *mv = mon_move(g, u->move_id[1]);
                if (!mv) mv = mon_move(g, u->move_id[0]);
                int atk_range = mv ? mv->range : 1;
                if (tdist(u->x, u->y, enemy->x, enemy->y) <= atk_range) {
                    do_attack(g, uidx, target);
                    tb->ai_unit++;
                    tb->ai_state = 1;
                    tb->ai_delay = 20;
                    return;
                }
            }
            u->acted = 1;
            tb->ai_unit++;
            tb->ai_state = 1;
            tb->ai_delay = 10;
            g->need_redraw = 1;
            if (all_team_done(tb, 1)) switch_phase(g);
        }
    }
}
