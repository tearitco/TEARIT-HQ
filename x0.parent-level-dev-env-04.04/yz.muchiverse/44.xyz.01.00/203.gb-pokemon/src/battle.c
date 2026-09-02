/* battle.c — wild encounter state machine: Fight / Run */
#include "gb.h"

static PartyMon *player_active(Game *g) {
    int i;
    for (i = 0; i < g->player.party_n; i++) {
        if (g->player.party[i].hp > 0)
            return &g->player.party[i];
    }
    return g->player.party_n > 0 ? &g->player.party[0] : NULL;
}

static void set_line(Battle *b, const char *s) {
    snprintf(b->line, sizeof(b->line), "%s", s);
}

static void pick_wild_moves(Game *g, PartyMon *w) {
    const Species *sp = mon_species(g, w->species);
    w->move_id[0] = 1; /* TACKLE */
    w->pp[0] = 35;
    if (sp) {
        switch (sp->type) {
        case TYPE_GRASS: w->move_id[1] = 8; w->pp[1] = 25; break;
        case TYPE_FIRE:  w->move_id[1] = 4; w->pp[1] = 25; break;
        case TYPE_WATER: w->move_id[1] = 9; w->pp[1] = 30; break;
        default:         w->move_id[1] = 6; w->pp[1] = 30; break;
        }
    } else {
        w->move_id[1] = 1;
        w->pp[1] = 35;
    }
}

void battle_start_wild(Game *g, int species_id, int level) {
    const Species *sp;
    Battle *b = &g->battle;
    char buf[MSG_LEN];

    memset(b, 0, sizeof(*b));
    b->active = 1;
    b->phase = BPHASE_INTRO;
    b->menu_sel = 0;
    b->move_sel = 0;
    b->wait_frames = 45;

    sp = mon_species(g, species_id);
    if (!sp) sp = mon_species(g, 4); /* PIDGEY fallback */
    if (!sp && g->species_n > 0) sp = &g->species[0];
    if (!sp) return;

    mon_init_from_species(&b->wild, sp, level);
    pick_wild_moves(g, &b->wild);

    snprintf(buf, sizeof(buf), "Wild %s appeared!", sp->name);
    set_line(b, buf);
    g->mode = MODE_BATTLE;
    g->need_redraw = 1;
}

static void apply_exp(Game *g, PartyMon *p, const PartyMon *wild) {
    int gain = wild->level * 4 + 8;
    const Species *sp;
    p->exp += gain;
    /* level up every 20 exp * level-ish */
    while (p->exp >= p->level * 20) {
        p->exp -= p->level * 20;
        p->level++;
        sp = mon_species(g, p->species);
        if (sp) {
            int old_max = p->max_hp;
            mon_init_from_species(p, sp, p->level);
            mon_give_starter_moves(g, p);
            /* keep current hp ratio-ish + heal on level */
            p->hp = p->max_hp - (old_max > p->hp ? 0 : 0);
            p->hp = p->max_hp; /* full heal on level for MVP friendliness */
        }
    }
}

static void do_player_attack(Game *g) {
    Battle *b = &g->battle;
    PartyMon *p = player_active(g);
    const MoveDef *mv;
    const Species *ws, *ps;
    int dmg, mult2;
    char buf[MSG_LEN];
    const char *eff = "";

    if (!p || p->hp <= 0) {
        b->phase = BPHASE_LOSE;
        set_line(b, "You blacked out!");
        b->wait_frames = 60;
        return;
    }

    mv = mon_move(g, p->move_id[b->move_sel]);
    if (!mv) mv = mon_move(g, 1);

    if (mv->power <= 0) {
        const Species *ps0 = mon_species(g, p->species);
        snprintf(buf, sizeof(buf), "%s used %s! (no effect)",
                 ps0 ? ps0->name : "???", mv->name);
        set_line(b, buf);
        b->phase = BPHASE_ENEMY;
        b->wait_frames = 40;
        return;
    }

    if (p->pp[b->move_sel] > 0)
        p->pp[b->move_sel]--;

    ws = mon_species(g, b->wild.species);
    ps = mon_species(g, p->species);
    mult2 = mon_type_mult(mv->type, ws ? ws->type : TYPE_NORMAL);
    dmg = mon_calc_damage_typed(g, p, &b->wild, mv);
    b->wild.hp -= dmg;
    if (b->wild.hp < 0) b->wild.hp = 0;
    b->flash = 8;

    if (mult2 >= 4) eff = " Super effective!";
    else if (mult2 <= 1) eff = " Not very effective...";

    snprintf(buf, sizeof(buf), "%s used %s!%s -%d",
             ps ? ps->name : "???", mv->name, eff, dmg);
    set_line(b, buf);

    if (b->wild.hp <= 0) {
        b->phase = BPHASE_WIN;
        apply_exp(g, p, &b->wild);
        snprintf(buf, sizeof(buf), "Enemy %s fainted! +EXP",
                 ws ? ws->name : "???");
        set_line(b, buf);
        b->wait_frames = 70;
    } else {
        b->phase = BPHASE_ENEMY;
        b->wait_frames = 40;
    }
}

static void do_enemy_attack(Game *g) {
    Battle *b = &g->battle;
    PartyMon *p = player_active(g);
    const MoveDef *mv;
    const Species *ws, *ps;
    int slot, dmg;
    char buf[MSG_LEN];

    if (!p) {
        b->phase = BPHASE_LOSE;
        set_line(b, "You blacked out!");
        b->wait_frames = 60;
        return;
    }

    slot = (rand() % 2);
    mv = mon_move(g, b->wild.move_id[slot]);
    if (!mv || mv->power <= 0) mv = mon_move(g, 1);

    dmg = mon_calc_damage_typed(g, &b->wild, p, mv);
    p->hp -= dmg;
    if (p->hp < 0) p->hp = 0;
    b->flash = 8;

    ws = mon_species(g, b->wild.species);
    ps = mon_species(g, p->species);
    snprintf(buf, sizeof(buf), "Wild %s used %s! -%d",
             ws ? ws->name : "???", mv->name, dmg);
    set_line(b, buf);

    if (p->hp <= 0) {
        b->phase = BPHASE_LOSE;
        snprintf(buf, sizeof(buf), "%s fainted! You blacked out!",
                 ps ? ps->name : "Your mon");
        set_line(b, buf);
        b->wait_frames = 70;
    } else {
        b->phase = BPHASE_MENU;
        b->wait_frames = 30;
    }
}

static void try_run(Game *g) {
    Battle *b = &g->battle;
    PartyMon *p = player_active(g);
    int chance, roll;

    /* speed-based flee */
    if (p && p->spd >= b->wild.spd)
        chance = 75;
    else
        chance = 40;
    roll = rand() % 100;
    if (roll < chance) {
        b->phase = BPHASE_RUN_OK;
        set_line(b, "Got away safely!");
        b->wait_frames = 40;
    } else {
        b->phase = BPHASE_RUN_FAIL;
        set_line(b, "Can't escape!");
        b->wait_frames = 35;
    }
}

static void end_battle_to_overworld(Game *g) {
    Battle *b = &g->battle;
    int lose = (b->phase == BPHASE_LOSE);

    b->active = 0;
    g->mode = MODE_OVERWORLD;

    if (lose) {
        /* warp to start, heal party (blackout) */
        int i;
        g->player.x = g->map.start_x;
        g->player.y = g->map.start_y;
        for (i = 0; i < g->player.party_n; i++)
            g->player.party[i].hp = g->player.party[i].max_hp;
        snprintf(g->msg, sizeof(g->msg), "Returned to start. Party healed.");
        g->msg_return = MODE_OVERWORLD;
        g->mode = MODE_MSG;
    }
    g->need_redraw = 1;
}

void battle_tick(Game *g) {
    Battle *b = &g->battle;
    if (!b->active || g->mode != MODE_BATTLE) return;

    if (b->flash > 0) b->flash--;

    if (b->wait_frames > 0) {
        b->wait_frames--;
        g->need_redraw = 1;
        if (b->wait_frames > 0) return;
    }

    /* auto-advance phases that are timed */
    switch (b->phase) {
    case BPHASE_INTRO:
        b->phase = BPHASE_MENU;
        set_line(b, "What will you do?");
        g->need_redraw = 1;
        break;
    case BPHASE_PLAYER:
        /* damage already applied; wait done → enemy or win handled in do_player */
        break;
    case BPHASE_ENEMY:
        do_enemy_attack(g);
        g->need_redraw = 1;
        break;
    case BPHASE_RUN_FAIL:
        b->phase = BPHASE_ENEMY;
        b->wait_frames = 5;
        g->need_redraw = 1;
        break;
    case BPHASE_WIN:
    case BPHASE_RUN_OK:
    case BPHASE_LOSE:
        end_battle_to_overworld(g);
        break;
    default:
        break;
    }
}

void battle_input(Game *g, int key) {
    Battle *b = &g->battle;
    if (!b->active) return;

    /* during wait / intro, A skips wait */
    if (b->wait_frames > 0) {
        if (key == VK_A || key == 'z' || key == 'Z' || key == '\r' || key == ' ')
            b->wait_frames = 0;
        return;
    }

    switch (b->phase) {
    case BPHASE_MENU:
        if (key == VK_UP || key == VK_LEFT || key == 'w' || key == 'a') {
            b->menu_sel = 0;
            g->need_redraw = 1;
        } else if (key == VK_DOWN || key == VK_RIGHT || key == 's' || key == 'd') {
            b->menu_sel = 1;
            g->need_redraw = 1;
        } else if (key == VK_A || key == 'z' || key == 'Z' || key == '\r' || key == ' ') {
            if (b->menu_sel == 0) {
                b->phase = BPHASE_PLAYER;
                do_player_attack(g);
                g->need_redraw = 1;
            } else {
                try_run(g);
                g->need_redraw = 1;
            }
        } else if (key == '1') {
            b->move_sel = 0;
            b->menu_sel = 0;
            b->phase = BPHASE_PLAYER;
            do_player_attack(g);
            g->need_redraw = 1;
        } else if (key == '2') {
            b->move_sel = 1;
            b->menu_sel = 0;
            b->phase = BPHASE_PLAYER;
            do_player_attack(g);
            g->need_redraw = 1;
        }
        break;
    case BPHASE_WIN:
    case BPHASE_LOSE:
    case BPHASE_RUN_OK:
        if (key == VK_A || key == 'z' || key == 'Z' || key == '\r' || key == ' ')
            end_battle_to_overworld(g);
        break;
    case BPHASE_INTRO:
        if (key == VK_A || key == 'z' || key == 'Z' || key == '\r' || key == ' ') {
            b->wait_frames = 0;
            b->phase = BPHASE_MENU;
            set_line(b, "What will you do?");
            g->need_redraw = 1;
        }
        break;
    default:
        break;
    }
}
