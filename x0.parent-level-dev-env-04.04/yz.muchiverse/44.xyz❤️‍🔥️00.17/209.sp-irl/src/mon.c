/* mon.c — species/move tables, type chart, damage */
#include "gb.h"

const Species *mon_species(const Game *g, int id) {
    int i;
    if (!g) return NULL;
    for (i = 0; i < g->species_n; i++)
        if (g->species[i].id == id) return &g->species[i];
    return g->species_n > 0 ? &g->species[0] : NULL;
}

const MoveDef *mon_move(const Game *g, int id) {
    int i;
    if (!g) return NULL;
    for (i = 0; i < g->moves_n; i++)
        if (g->moves[i].id == id) return &g->moves[i];
    return g->moves_n > 0 ? &g->moves[0] : NULL;
}

int data_load_mons(Game *g, const char *path) {
    FILE *f;
    char line[128];
    int n = 0;

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "data_load_mons: %s\n", path);
        return -1;
    }
    g->species_n = 0;
    while (fgets(line, sizeof(line), f) && n < MON_MAX) {
        Species *s;
        int id, type, hp, atk, def, spd;
        char name[NAME_LEN];
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%d %15s %d %d %d %d %d",
                   &id, name, &type, &hp, &atk, &def, &spd) != 7)
            continue;
        s = &g->species[n++];
        s->id = id;
        snprintf(s->name, sizeof(s->name), "%s", name);
        s->type = type;
        s->base_hp = hp;
        s->base_atk = atk;
        s->base_def = def;
        s->base_spd = spd;
    }
    fclose(f);
    g->species_n = n;
    return n > 0 ? 0 : -1;
}

int data_load_moves(Game *g, const char *path) {
    FILE *f;
    char line[128];
    int n = 0;

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "data_load_moves: %s\n", path);
        return -1;
    }
    g->moves_n = 0;
    while (fgets(line, sizeof(line), f) && n < MOVE_MAX) {
        MoveDef *m;
        int id, type, power, pp, range;
        char name[NAME_LEN];
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%d %15s %d %d %d %d",
                   &id, name, &type, &power, &pp, &range) < 5)
            continue;
        if (range < 1) range = 1;
        m = &g->moves[n++];
        m->id = id;
        snprintf(m->name, sizeof(m->name), "%s", name);
        m->type = type;
        m->power = power;
        m->max_pp = pp;
        m->range = range;
    }
    fclose(f);
    g->moves_n = n;
    return n > 0 ? 0 : -1;
}

/* classic-ish level curve: HP = floor(2*base*L/100)+L+10
 * atk/def/spd = floor(2*base*L/100)+5
 */
void mon_init_from_species(PartyMon *pm, const Species *sp, int level) {
    if (!pm || !sp) return;
    memset(pm, 0, sizeof(*pm));
    pm->species = sp->id;
    pm->level = level < 1 ? 1 : level;
    pm->max_hp = (2 * sp->base_hp * pm->level) / 100 + pm->level + 10;
    pm->hp = pm->max_hp;
    pm->atk = (2 * sp->base_atk * pm->level) / 100 + 5;
    pm->def = (2 * sp->base_def * pm->level) / 100 + 5;
    pm->spd = (2 * sp->base_spd * pm->level) / 100 + 5;
    pm->exp = 0;
    pm->move_id[0] = 1; /* TACKLE */
    pm->move_id[1] = 1;
    pm->pp[0] = 35;
    pm->pp[1] = 35;
}

void mon_give_starter_moves(Game *g, PartyMon *pm) {
    const Species *sp;
    if (!g || !pm) return;
    sp = mon_species(g, pm->species);
    if (!sp) return;
    pm->move_id[0] = 1; /* TACKLE always */
    pm->pp[0] = 35;
    switch (sp->type) {
    case TYPE_GRASS: pm->move_id[1] = 3; pm->pp[1] = 25; break; /* VINE_WHIP */
    case TYPE_FIRE:  pm->move_id[1] = 4; pm->pp[1] = 25; break; /* EMBER */
    case TYPE_WATER: pm->move_id[1] = 5; pm->pp[1] = 25; break; /* WATER_GUN */
    default:         pm->move_id[1] = 6; pm->pp[1] = 30; break; /* QUICK_ATK */
    }
}

/* return multiplier * 2 so: 0=immune, 1=half, 2=normal, 4=super */
int mon_type_mult(int atk_type, int def_type) {
    if (atk_type == TYPE_NORMAL) return 2;
    if (atk_type == def_type) return 2; /* neutral same */
    /* Grass > Water > Fire > Grass */
    if (atk_type == TYPE_GRASS && def_type == TYPE_WATER) return 4;
    if (atk_type == TYPE_GRASS && def_type == TYPE_FIRE)  return 1;
    if (atk_type == TYPE_FIRE  && def_type == TYPE_GRASS) return 4;
    if (atk_type == TYPE_FIRE  && def_type == TYPE_WATER) return 1;
    if (atk_type == TYPE_WATER && def_type == TYPE_FIRE)  return 4;
    if (atk_type == TYPE_WATER && def_type == TYPE_GRASS) return 1;
    return 2;
}

int mon_calc_damage(const PartyMon *atk, const PartyMon *def, const MoveDef *mv) {
    int base, mult2, dmg, roll;
    const Species *ds;
    if (!atk || !def || !mv) return 1;
    if (mv->power <= 0) return 0;

    /* simplified R/B-ish:
     * dmg = ((2L/5+2) * power * A/D)/50 + 2  then * type * random
     */
    base = ((2 * atk->level / 5 + 2) * mv->power * atk->atk) / (def->def > 0 ? def->def : 1);
    base = base / 50 + 2;

    /* type effectiveness vs defender species type — caller passes wild/player mon;
     * we need species type; stored only as stats. Use move type vs... we need def type.
     * PartyMon doesn't store type; look up via global g — but mon.c has no g access for def species.
     * Pass through mon_type_mult from battle using species. For now use move-only STAB-ish:
     * We'll look up defender type if available via a weak approach: re-calc in battle.
     * Here: accept damage without type, battle applies mult.
     */
    (void)ds;
    mult2 = 2;
    dmg = (base * mult2) / 2;

    /* 85–100% random */
    roll = 85 + (rand() % 16);
    dmg = (dmg * roll) / 100;
    if (dmg < 1) dmg = 1;
    return dmg;
}

/* damage with type chart — preferred entry */
int mon_calc_damage_typed(const Game *g, const PartyMon *atk, const PartyMon *def,
                          const MoveDef *mv) {
    int base, mult2, dmg, roll;
    const Species *as, *ds;
    if (!atk || !def || !mv || !g) return 1;
    if (mv->power <= 0) return 0;

    base = ((2 * atk->level / 5 + 2) * mv->power * atk->atk) / (def->def > 0 ? def->def : 1);
    base = base / 50 + 2;

    as = mon_species(g, atk->species);
    ds = mon_species(g, def->species);
    mult2 = mon_type_mult(mv->type, ds ? ds->type : TYPE_NORMAL);

    /* STAB: same type as attacker species */
    if (as && mv->type == as->type && mv->type != TYPE_NORMAL)
        base = (base * 3) / 2;

    dmg = (base * mult2) / 2;
    roll = 85 + (rand() % 16);
    dmg = (dmg * roll) / 100;
    if (dmg < 1) dmg = 1;
    return dmg;
}
