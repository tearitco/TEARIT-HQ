/* sim.c — ships, combat, AI, Supremacy / Deathmatch / Freeplay */
#include "sw.h"

static const ShipDef SHIPS[SHIP_COUNT] = {
    { "Interceptor", 80,  40,  55, 40, 2.8f, 1.2f, 0.12f, 18, 0.85f,
      0.85f, 0.25f, 0.20f,  0.3f, 0.7f, 1.0f, 0 },
    { "Fighter",     120, 80,  42, 28, 2.2f, 1.8f, 0.18f, 22, 1.00f,
      0.75f, 0.75f, 0.80f,  0.4f, 0.8f, 1.0f, 0 },
    { "Bomber",      180, 60,  28, 18, 1.4f, 2.5f, 0.45f, 55, 1.25f,
      0.55f, 0.55f, 0.45f,  1.0f, 0.5f, 0.2f, 0 },
    { "Freighter",   220, 40,  32, 15, 1.2f, 3.0f, 0.35f, 28, 1.40f,
      0.50f, 0.52f, 0.58f,  0.6f, 0.6f, 0.9f, 0 },
    { "Speeder",     90,  30,  35, 30, 2.5f, 1.5f, 0.15f, 16, 0.90f,
      0.30f, 0.55f, 0.40f,  0.2f, 0.9f, 0.5f, 1 },
};

const ShipDef *sim_ship_def(int type) {
    if (type < 0 || type >= SHIP_COUNT) type = SHIP_FIGHTER;
    return &SHIPS[type];
}

const char *sim_mode_name(enum GameMode m) {
    switch (m) {
    case MODE_SUPREMACY: return "SUPREMACY";
    case MODE_DEATHMATCH: return "DEATHMATCH";
    case MODE_FREEPLAY: return "FREEPLAY";
    default: return "MENU";
    }
}

const char *sim_planet_name(enum Planet p) {
    switch (p) {
    case PLANET_ENDOR: return "Endor Prime";
    case PLANET_TATOOINE: return "Tatooine";
    case PLANET_HOTH: return "Hoth";
    case PLANET_MUSTAFAR: return "Mustafar";
    case PLANET_SPACE: return "Deep Space";
    default: return "?";
    }
}

const char *sim_ship_name(int type) {
    return sim_ship_def(type)->name;
}

static void set_status(Game *g, const char *s) {
    snprintf(g->status, sizeof(g->status), "%s", s);
    g->need_redraw = 1;
}

static int spawn_bullet(Game *g, int owner, int kind,
                        float x, float y, float z,
                        float vx, float vy, float vz,
                        float dmg, float r, float gb, float b) {
    int i;
    for (i = 0; i < MAX_BULLETS; i++) {
        Bullet *bl = &g->bullets[i];
        if (bl->alive) continue;
        bl->alive = 1;
        bl->owner = owner;
        bl->team = g->ents[owner].team;
        bl->kind = kind;
        bl->x = x; bl->y = y; bl->z = z;
        bl->vx = vx; bl->vy = vy; bl->vz = vz;
        bl->life = kind == 1 ? 2.5f : 1.4f;
        bl->dmg = dmg;
        bl->r = r; bl->g = gb; bl->b = b;
        return i;
    }
    return -1;
}

void sim_fire(Game *g, int ei) {
    Entity *e = &g->ents[ei];
    float fx, fy, fz, sp;
    const ShipDef *sd;
    if (!e->alive || e->fire_cd > 0) return;
    if (e->heat > 1.f) return;
    fx = sinf(e->yaw) * cosf(e->pitch);
    fy = -sinf(e->pitch);
    fz = -cosf(e->yaw) * cosf(e->pitch);
    norm3(&fx, &fy, &fz);

    if (!e->in_ship) {
        if (e->weapon == WPN_SABER) {
            /* short-range slash hit-scan */
            int j;
            e->fire_cd = 0.35f;
            e->energy = fmaxf(0.f, e->energy - 8.f);
            for (j = 0; j < g->n_ents; j++) {
                Entity *t = &g->ents[j];
                float dx, dy, dz, dist, dot;
                if (!t->alive || j == ei || t->team == e->team) continue;
                dx = t->x - e->x; dy = t->y - e->y; dz = t->z - e->z;
                dist = len3(dx, dy, dz);
                if (dist > 3.5f) continue;
                norm3(&dx, &dy, &dz);
                dot = dx * fx + dy * fy + dz * fz;
                if (dot > 0.55f) {
                    t->hp -= 35.f * e->buff_mult;
                    gfx_spawn_explosion(g, t->x, t->y + 1.f, t->z, 1.2f);
                    if (t->hp <= 0) {
                        t->alive = 0; t->deaths++;
                        e->kills++; e->score += 100;
                        set_status(g, "Saber kill!");
                    }
                }
            }
            /* slash FX */
            {
                int k;
                for (k = 0; k < MAX_FX; k++) {
                    if (g->fx[k].alive) continue;
                    g->fx[k].alive = 1;
                    g->fx[k].kind = 3;
                    g->fx[k].x = e->x + fx * 1.5f;
                    g->fx[k].y = e->y + 1.2f + fy * 1.5f;
                    g->fx[k].z = e->z + fz * 1.5f;
                    g->fx[k].life = g->fx[k].max_life = 0.15f;
                    g->fx[k].size = 1.5f;
                    g->fx[k].r = 0.3f; g->fx[k].g = 1.f; g->fx[k].b = 0.4f;
                    break;
                }
            }
            return;
        }
        sp = 90.f;
        e->fire_cd = (e->weapon == WPN_REPEATER) ? 0.08f : 0.18f;
        e->heat += (e->weapon == WPN_REPEATER) ? 0.08f : 0.12f;
        spawn_bullet(g, ei, 0,
                     e->x + fx * 1.2f, e->y + 1.4f + fy * 1.2f, e->z + fz * 1.2f,
                     fx * sp + e->vx, fy * sp + e->vy, fz * sp + e->vz,
                     e->weapon == WPN_REPEATER ? 8.f : 14.f,
                     e->team == TEAM_REBEL ? 0.3f : 1.f,
                     e->team == TEAM_REBEL ? 0.9f : 0.2f,
                     e->team == TEAM_REBEL ? 1.f : 0.2f);
        return;
    }

    sd = sim_ship_def(e->ship);
    e->fire_cd = sd->fire_rate;
    e->heat += 0.1f;
    sp = 120.f + sd->max_speed;
    if (e->ship == SHIP_BOMBER) {
        spawn_bullet(g, ei, 1,
                     e->x + fx * 2.f, e->y + fy * 2.f, e->z + fz * 2.f,
                     fx * 50.f + e->vx, fy * 50.f + e->vy - 2.f, fz * 50.f + e->vz,
                     sd->dmg * e->buff_mult, 1.f, 0.6f, 0.1f);
    } else {
        float ox = cosf(e->yaw) * 0.6f;
        float oz = sinf(e->yaw) * 0.6f;
        spawn_bullet(g, ei, 0,
                     e->x + fx * 2.f + ox, e->y + fy * 2.f, e->z + fz * 2.f + oz,
                     fx * sp + e->vx * 0.3f, fy * sp + e->vy * 0.3f, fz * sp + e->vz * 0.3f,
                     sd->dmg * e->buff_mult,
                     e->team == TEAM_REBEL ? 0.2f : 1.f,
                     e->team == TEAM_REBEL ? 1.f : 0.15f,
                     e->team == TEAM_REBEL ? 0.4f : 0.15f);
        spawn_bullet(g, ei, 0,
                     e->x + fx * 2.f - ox, e->y + fy * 2.f, e->z + fz * 2.f - oz,
                     fx * sp + e->vx * 0.3f, fy * sp + e->vy * 0.3f, fz * sp + e->vz * 0.3f,
                     sd->dmg * e->buff_mult,
                     e->team == TEAM_REBEL ? 0.2f : 1.f,
                     e->team == TEAM_REBEL ? 1.f : 0.15f,
                     e->team == TEAM_REBEL ? 0.4f : 0.15f);
    }
}

static void reset_entity(Game *g, int i, int team, int bot, float x, float z) {
    Entity *e = &g->ents[i];
    const ShipDef *sd;
    memset(e, 0, sizeof(*e));
    e->alive = 1;
    e->is_bot = bot;
    e->team = team;
    e->ship = g->selected_ship;
    if (bot) e->ship = rand() % 3; /* interceptor/fighter/bomber */
    e->weapon = WPN_BLASTER;
    e->in_ship = (g->mode == MODE_FREEPLAY && g->planet == PLANET_SPACE) ? 1 :
                 (g->mode != MODE_FREEPLAY ? 1 : 0);
    if (g->mode == MODE_FREEPLAY && g->planet != PLANET_SPACE)
        e->in_ship = 0;
    sd = sim_ship_def(e->ship);
    e->x = x;
    e->z = z;
    e->y = (g->planet == PLANET_SPACE) ? 40.f + frand() * 20.f
         : gen_height(g->planet, x, z) + (e->in_ship ? 12.f : 1.8f);
    e->yaw = frand() * 6.28f;
    e->hp = sd->max_hp;
    e->shield = sd->max_shield;
    e->energy = 100.f;
    e->oxygen = 100.f;
    e->buff_mult = 1.f;
    e->ai_target = -1;
    snprintf(e->name, sizeof(e->name), bot ? (team == TEAM_REBEL ? "Rebel-AI%d" : "Empire-AI%d") : "Player", i);
    if (!bot) snprintf(e->name, sizeof(e->name), "Pilot");
}

static void place_posts(Game *g) {
    int i;
    const char *names[] = { "Alpha", "Bravo", "Charlie", "Delta", "Echo" };
    g->n_posts = (g->mode == MODE_SUPREMACY) ? 5 : 0;
    for (i = 0; i < g->n_posts; i++) {
        float ang = (float)i / (float)g->n_posts * 6.28f;
        CommandPost *p = &g->posts[i];
        p->active = 1;
        p->team = TEAM_NONE;
        p->x = cosf(ang) * 55.f;
        p->z = sinf(ang) * 55.f;
        p->y = gen_height(g->planet, p->x, p->z);
        p->radius = 12.f;
        p->cap = 0.f;
        snprintf(p->name, sizeof(p->name), "%s", names[i]);
    }
}

void sim_init_menu(Game *g) {
    memset(g, 0, sizeof(*g));
    g->mode = MODE_MENU;
    g->running = 1;
    g->menu_sel = 0;
    g->selected_ship = SHIP_FIGHTER;
    g->difficulty = 1;
    g->planet = PLANET_ENDOR;
    g->seed = (unsigned)time(NULL);
    gen_init((unsigned)g->seed);
    g->local = 0;
    g->n_ents = 1;
    g->ents[0].alive = 1;
    g->ents[0].y = 5.f;
    set_status(g, "Choose mode — Enter to launch");
}

void sim_start_mode(Game *g, enum GameMode mode) {
    int i, n_bots;
    float spacing;
    g->mode = mode;
    g->time = 0;
    g->match_time = 0;
    g->ticket_rebel = 200.f;
    g->ticket_empire = 200.f;
    g->dm_limit = 25.f;
    g->res_ore = 20.f;
    g->res_wood = 20.f;
    g->res_scrap = 10.f;
    g->n_builds = 0;
    memset(g->builds, 0, sizeof(g->builds));
    memset(g->bullets, 0, sizeof(g->bullets));
    memset(g->fx, 0, sizeof(g->fx));

    if (mode == MODE_FREEPLAY) {
        /* cycle planet by menu_sub leftover: use difficulty as planet index hack */
        g->planet = (enum Planet)(g->menu_sub % PLANET_COUNT);
        if (g->planet > PLANET_MUSTAFAR && g->planet != PLANET_SPACE)
            g->planet = PLANET_ENDOR;
        n_bots = 4 + g->difficulty * 2;
    } else if (mode == MODE_SUPREMACY) {
        g->planet = PLANET_ENDOR;
        n_bots = 10 + g->difficulty * 4;
    } else {
        g->planet = PLANET_SPACE;
        n_bots = 8 + g->difficulty * 3;
    }

    gen_init((unsigned)(g->seed + mode * 17));
    place_posts(g);

    g->n_ents = 0;
    g->local = 0;
    reset_entity(g, 0, TEAM_REBEL, 0, 0.f, 8.f);
    g->ents[0].ship = g->selected_ship;
    {
        const ShipDef *sd = sim_ship_def(g->selected_ship);
        g->ents[0].hp = sd->max_hp;
        g->ents[0].shield = sd->max_shield;
    }
    g->n_ents = 1;

    spacing = 18.f;
    for (i = 0; i < n_bots && g->n_ents < MAX_ENTS; i++) {
        int team = (i & 1) ? TEAM_EMPIRE : TEAM_REBEL;
        float a = (float)i * 0.7f;
        float x = cosf(a) * (30.f + spacing * (i % 5));
        float z = sinf(a) * (30.f + spacing * (i % 5));
        if (mode == MODE_DEATHMATCH) team = TEAM_NONE; /* FFA-ish: still paint empire/rebel for color */
        if (mode == MODE_DEATHMATCH) team = (i & 1) ? TEAM_EMPIRE : TEAM_REBEL;
        reset_entity(g, g->n_ents, team, 1, x, z);
        g->n_ents++;
    }

    if (mode == MODE_SUPREMACY)
        set_status(g, "SUPREMACY — capture posts. Tickets drain when enemy holds more.");
    else if (mode == MODE_DEATHMATCH)
        set_status(g, "DEATHMATCH — first to 25 kills. Space dogfight.");
    else
        set_status(g, "FREEPLAY — explore, mine [F], build [1-5], E enter ship, planets P");
    g->need_redraw = 1;
}

static void ground_clamp(Game *g, Entity *e) {
    float h;
    if (g->planet == PLANET_SPACE) return;
    h = gen_height(g->planet, e->x, e->z);
    if (e->in_ship) {
        const ShipDef *sd = sim_ship_def(e->ship);
        float min_y = h + (sd->is_ground ? 1.5f : 3.f);
        if (e->y < min_y) {
            e->y = min_y;
            if (e->vy < 0) e->vy = 0;
            if (sd->is_ground) e->vy = 0;
        }
    } else {
        e->y = h + 1.7f;
        e->vy = 0;
    }
}

static void damage_ent(Game *g, int ti, float dmg, int attacker) {
    Entity *t = &g->ents[ti];
    if (!t->alive) return;
    if (t->shield > 0) {
        float s = fminf(t->shield, dmg);
        t->shield -= s;
        dmg -= s;
    }
    t->hp -= dmg;
    if (t->hp <= 0) {
        t->alive = 0;
        t->deaths++;
        gfx_spawn_explosion(g, t->x, t->y, t->z, t->in_ship ? 4.f : 1.5f);
        if (attacker >= 0 && attacker < g->n_ents) {
            g->ents[attacker].kills++;
            g->ents[attacker].score += t->in_ship ? 150.f : 100.f;
            /* between-kill buff */
            g->ents[attacker].buff_timer = 5.f;
            g->ents[attacker].buff_mult = 1.25f;
            g->ents[attacker].shield = fminf(
                sim_ship_def(g->ents[attacker].ship)->max_shield,
                g->ents[attacker].shield + 20.f);
        }
        /* respawn bots / player after delay via hp flag: use energy as timer */
        t->energy = -3.f; /* respawn countdown */
        if (g->mode == MODE_SUPREMACY) {
            if (t->team == TEAM_REBEL) g->ticket_rebel -= 1.f;
            else if (t->team == TEAM_EMPIRE) g->ticket_empire -= 1.f;
        }
    }
}

static void update_bullets(Game *g, float dt) {
    int i, j;
    for (i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &g->bullets[i];
        if (!b->alive) continue;
        b->x += b->vx * dt;
        b->y += b->vy * dt;
        b->z += b->vz * dt;
        if (b->kind == 1) b->vy -= 12.f * dt; /* rocket arc */
        b->life -= dt;
        if (b->life <= 0) { b->alive = 0; continue; }
        /* terrain hit */
        if (g->planet != PLANET_SPACE) {
            float h = gen_height(g->planet, b->x, b->z);
            if (b->y < h) {
                gfx_spawn_explosion(g, b->x, h + 0.5f, b->z, b->kind == 1 ? 3.f : 0.8f);
                b->alive = 0;
                continue;
            }
        }
        for (j = 0; j < g->n_ents; j++) {
            Entity *t = &g->ents[j];
            float dx, dy, dz, rad;
            if (!t->alive || j == b->owner) continue;
            if (g->mode != MODE_DEATHMATCH && t->team == b->team) continue;
            dx = t->x - b->x; dy = t->y - b->y; dz = t->z - b->z;
            rad = t->in_ship ? 2.2f : 0.9f;
            if (dx * dx + dy * dy + dz * dz < rad * rad) {
                damage_ent(g, j, b->dmg, b->owner);
                gfx_spawn_explosion(g, b->x, b->y, b->z, b->kind == 1 ? 2.5f : 0.6f);
                b->alive = 0;
                break;
            }
        }
    }
}

static void update_fx(Game *g, float dt) {
    int i;
    for (i = 0; i < MAX_FX; i++) {
        Fx *f = &g->fx[i];
        if (!f->alive) continue;
        f->x += f->vx * dt; f->y += f->vy * dt; f->z += f->vz * dt;
        f->vy -= 4.f * dt;
        f->life -= dt;
        if (f->life <= 0) f->alive = 0;
    }
    g->shake = fmaxf(0.f, g->shake - dt * 4.f);
}

static void ai_step(Game *g, int ei, float dt) {
    Entity *e = &g->ents[ei];
    Entity *tgt = NULL;
    float best = 1e9f;
    int j;
    float fx, fy, fz, want_yaw, dyaw;
    float speed, turn;
    const ShipDef *sd;
    float diff_aim = 0.02f + g->difficulty * 0.03f;
    if (!e->alive || !e->is_bot) return;
    e->ai_think -= dt;
    sd = sim_ship_def(e->ship);
    /* pick target */
    if (e->ai_think <= 0.f || e->ai_target < 0 ||
        e->ai_target >= g->n_ents || !g->ents[e->ai_target].alive) {
        e->ai_think = 0.4f + frand() * 0.6f;
        e->ai_target = -1;
        for (j = 0; j < g->n_ents; j++) {
            float d;
            if (!g->ents[j].alive || j == ei) continue;
            if (g->mode != MODE_DEATHMATCH && g->ents[j].team == e->team) continue;
            d = len3(g->ents[j].x - e->x, g->ents[j].y - e->y, g->ents[j].z - e->z);
            if (d < best) { best = d; e->ai_target = j; }
        }
        e->ai_strafe = (frand() - 0.5f) * 2.f;
    }
    if (e->ai_target >= 0) tgt = &g->ents[e->ai_target];

    /* supremacy: sometimes go to neutral post */
    if (g->mode == MODE_SUPREMACY && (e->ai_target < 0 || frand() < 0.002f)) {
        for (j = 0; j < g->n_posts; j++) {
            if (g->posts[j].team != e->team) {
                float dx = g->posts[j].x - e->x;
                float dz = g->posts[j].z - e->z;
                want_yaw = atan2f(dx, -dz);
                dyaw = want_yaw - e->yaw;
                while (dyaw > M_PI) dyaw -= 2.f * (float)M_PI;
                while (dyaw < -M_PI) dyaw += 2.f * (float)M_PI;
                e->yaw += clampf(dyaw, -sd->turn * dt, sd->turn * dt);
                e->vx += sinf(e->yaw) * sd->accel * dt;
                e->vz += -cosf(e->yaw) * sd->accel * dt;
                break;
            }
        }
    }

    if (tgt) {
        fx = tgt->x - e->x;
        fy = tgt->y - e->y;
        fz = tgt->z - e->z;
        want_yaw = atan2f(fx, -fz);
        dyaw = want_yaw - e->yaw;
        while (dyaw > (float)M_PI) dyaw -= 2.f * (float)M_PI;
        while (dyaw < -(float)M_PI) dyaw += 2.f * (float)M_PI;
        e->yaw += clampf(dyaw, -sd->turn * dt, sd->turn * dt);
        {
            float dist = len3(fx, fy, fz);
            float want_pitch = -atan2f(fy, fmaxf(dist, 0.1f));
            e->pitch = lerpf(e->pitch, clampf(want_pitch, -0.6f, 0.6f), dt * 2.f);
        }
        /* throttle */
        e->vx += sinf(e->yaw) * cosf(e->pitch) * sd->accel * dt;
        e->vy += -sinf(e->pitch) * sd->accel * dt * 0.6f;
        e->vz += -cosf(e->yaw) * cosf(e->pitch) * sd->accel * dt;
        e->vx += cosf(e->yaw) * e->ai_strafe * sd->accel * 0.4f * dt;
        e->vz += sinf(e->yaw) * e->ai_strafe * sd->accel * 0.4f * dt;
        /* fire if aligned */
        {
            float ddx = fx, ddy = fy, ddz = fz;
            float pfx = sinf(e->yaw) * cosf(e->pitch);
            float pfy = -sinf(e->pitch);
            float pfz = -cosf(e->yaw) * cosf(e->pitch);
            norm3(&ddx, &ddy, &ddz);
            if (ddx * pfx + ddy * pfy + ddz * pfz > 0.92f - diff_aim)
                sim_fire(g, ei);
        }
    }

    /* drag + clamp speed */
    e->vx *= (1.f - 1.2f * dt);
    e->vy *= (1.f - 1.2f * dt);
    e->vz *= (1.f - 1.2f * dt);
    speed = len3(e->vx, e->vy, e->vz);
    if (speed > sd->max_speed) {
        float s = sd->max_speed / speed;
        e->vx *= s; e->vy *= s; e->vz *= s;
    }
    e->x += e->vx * dt;
    e->y += e->vy * dt;
    e->z += e->vz * dt;
    ground_clamp(g, e);
    if (e->fire_cd > 0) e->fire_cd -= dt;
    e->heat = fmaxf(0.f, e->heat - dt * 0.35f);
    if (e->buff_timer > 0) {
        e->buff_timer -= dt;
        if (e->buff_timer <= 0) e->buff_mult = 1.f;
    }
    (void)turn;
}

static void update_posts(Game *g, float dt) {
    int i, j;
    int hold_r = 0, hold_e = 0;
    if (g->mode != MODE_SUPREMACY) return;
    for (i = 0; i < g->n_posts; i++) {
        CommandPost *p = &g->posts[i];
        int near_r = 0, near_e = 0;
        for (j = 0; j < g->n_ents; j++) {
            Entity *e = &g->ents[j];
            float d;
            if (!e->alive) continue;
            d = len3(e->x - p->x, 0, e->z - p->z);
            if (d < p->radius) {
                if (e->team == TEAM_REBEL) near_r++;
                else if (e->team == TEAM_EMPIRE) near_e++;
            }
        }
        if (near_r > near_e) p->cap = clampf(p->cap + dt * 0.25f * (near_r - near_e), -1.f, 1.f);
        else if (near_e > near_r) p->cap = clampf(p->cap - dt * 0.25f * (near_e - near_r), -1.f, 1.f);
        if (p->cap > 0.95f) p->team = TEAM_REBEL;
        else if (p->cap < -0.95f) p->team = TEAM_EMPIRE;
        else if (fabsf(p->cap) < 0.05f) p->team = TEAM_NONE;
        if (p->team == TEAM_REBEL) hold_r++;
        if (p->team == TEAM_EMPIRE) hold_e++;
    }
    /* ticket bleed */
    if (hold_e > hold_r) g->ticket_rebel -= dt * (hold_e - hold_r) * 0.8f;
    if (hold_r > hold_e) g->ticket_empire -= dt * (hold_r - hold_e) * 0.8f;
    g->ticket_rebel = fmaxf(0.f, g->ticket_rebel);
    g->ticket_empire = fmaxf(0.f, g->ticket_empire);
}

static void respawn_check(Game *g, float dt) {
    int i;
    for (i = 0; i < g->n_ents; i++) {
        Entity *e = &g->ents[i];
        if (e->alive) continue;
        if (e->energy > -0.1f) e->energy = -3.f;
        e->energy += dt; /* from -3 toward 0 */
        if (e->energy >= 0.f) {
            float a = frand() * 6.28f;
            float r = 20.f + frand() * 40.f;
            reset_entity(g, i, e->team, e->is_bot, cosf(a) * r, sinf(a) * r);
            if (i == g->local)
                e->ship = g->selected_ship;
        }
    }
}

void sim_update(Game *g, float dt) {
    int i;
    if (g->mode == MODE_MENU || g->paused) return;
    g->time += dt;
    g->match_time += dt;

    for (i = 0; i < g->n_ents; i++) {
        Entity *e = &g->ents[i];
        if (!e->alive) continue;
        if (e->is_bot) {
            ai_step(g, i, dt);
            continue;
        }
        /* player physics applied in input */
        if (e->fire_cd > 0) e->fire_cd -= dt;
        e->heat = fmaxf(0.f, e->heat - dt * 0.4f);
        if (e->buff_timer > 0) {
            e->buff_timer -= dt;
            if (e->buff_timer <= 0) e->buff_mult = 1.f;
        }
        /* shield regen */
        {
            const ShipDef *sd = sim_ship_def(e->ship);
            e->shield = fminf(sd->max_shield, e->shield + dt * 4.f);
        }
        if (g->mode == MODE_FREEPLAY) {
            e->energy = fminf(100.f, e->energy + dt * 3.f);
            if (g->planet == PLANET_SPACE || g->planet == PLANET_MUSTAFAR)
                e->oxygen = fmaxf(0.f, e->oxygen - dt * 1.5f);
            else
                e->oxygen = fminf(100.f, e->oxygen + dt * 5.f);
            if (e->oxygen <= 0.f) e->hp -= dt * 8.f;
            /* passive resource near terrain noise */
            if (!e->in_ship && gen_noise2(e->x * 0.1f, e->z * 0.1f) > 0.85f)
                g->res_ore += dt * 0.5f;
        }
        e->x += e->vx * dt;
        e->y += e->vy * dt;
        e->z += e->vz * dt;
        /* drag */
        {
            float drag = e->in_ship ? 0.8f : 6.f;
            e->vx *= (1.f - drag * dt);
            e->vz *= (1.f - drag * dt);
            if (e->in_ship) e->vy *= (1.f - drag * dt);
        }
        if (e->in_ship) {
            const ShipDef *sd = sim_ship_def(e->ship);
            float sp = len3(e->vx, e->vy, e->vz);
            if (sp > sd->max_speed) {
                float s = sd->max_speed / sp;
                e->vx *= s; e->vy *= s; e->vz *= s;
            }
        }
        ground_clamp(g, e);
        if (e->hp <= 0 && e->alive) damage_ent(g, i, 999.f, -1);
    }

    update_bullets(g, dt);
    update_fx(g, dt);
    update_posts(g, dt);
    respawn_check(g, dt);

    /* win checks */
    if (g->mode == MODE_SUPREMACY) {
        if (g->ticket_empire <= 0.f) set_status(g, "REBEL VICTORY — Empire tickets depleted! Esc menu");
        if (g->ticket_rebel <= 0.f) set_status(g, "EMPIRE VICTORY — Rebel tickets depleted! Esc menu");
    } else if (g->mode == MODE_DEATHMATCH) {
        for (i = 0; i < g->n_ents; i++) {
            if (g->ents[i].kills >= g->dm_limit) {
                char buf[80];
                snprintf(buf, sizeof(buf), "WINNER: %s (%d kills) — Esc menu",
                         g->ents[i].name, (int)g->ents[i].kills);
                set_status(g, buf);
            }
        }
    }
    g->need_redraw = 1;
}

void sim_player_input(Game *g, float dt,
                      int key_w, int key_a, int key_s, int key_d,
                      int key_space, int key_shift, int key_ctrl,
                      int mouse_l, int mouse_r,
                      float mdx, float mdy) {
    Entity *e;
    const ShipDef *sd;
    float sens = 0.0025f;
    if (g->mode == MODE_MENU || g->paused) return;
    e = &g->ents[g->local];
    if (!e->alive) return;
    sd = sim_ship_def(e->ship);

    e->yaw += mdx * sens;
    e->pitch += mdy * sens;
    e->pitch = clampf(e->pitch, -1.2f, 1.2f);

    if (e->in_ship) {
        float thr = 0.f;
        if (key_w) thr += 1.f;
        if (key_s) thr -= 0.5f;
        if (key_shift) thr *= 1.45f; /* boost */
        e->vx += sinf(e->yaw) * cosf(e->pitch) * sd->accel * thr * dt;
        e->vy += -sinf(e->pitch) * sd->accel * thr * dt;
        e->vz += -cosf(e->yaw) * cosf(e->pitch) * sd->accel * thr * dt;
        if (key_a) {
            e->vx -= cosf(e->yaw) * sd->accel * 0.7f * dt;
            e->vz -= sinf(e->yaw) * sd->accel * 0.7f * dt;
        }
        if (key_d) {
            e->vx += cosf(e->yaw) * sd->accel * 0.7f * dt;
            e->vz += sinf(e->yaw) * sd->accel * 0.7f * dt;
        }
        if (key_space) e->vy += sd->accel * 0.6f * dt;
        if (key_ctrl) e->vy -= sd->accel * 0.6f * dt;
        if (key_shift) e->energy = fmaxf(0.f, e->energy - dt * 15.f);
    } else {
        float sp = key_shift ? 14.f : 8.f;
        float fx = sinf(e->yaw), fz = -cosf(e->yaw);
        float rx = cosf(e->yaw), rz = sinf(e->yaw);
        if (key_w) { e->vx += fx * sp * dt * 8.f; e->vz += fz * sp * dt * 8.f; }
        if (key_s) { e->vx -= fx * sp * dt * 8.f; e->vz -= fz * sp * dt * 8.f; }
        if (key_a) { e->vx -= rx * sp * dt * 8.f; e->vz -= rz * sp * dt * 8.f; }
        if (key_d) { e->vx += rx * sp * dt * 8.f; e->vz += rz * sp * dt * 8.f; }
        if (key_space) { /* jump-ish already grounded */ }
    }
    if (mouse_l) sim_fire(g, g->local);
    if (mouse_r && e->in_ship && e->ship == SHIP_BOMBER) {
        e->weapon = WPN_ROCKET;
        sim_fire(g, g->local);
    }
}

void sim_try_enter_exit(Game *g) {
    Entity *e = &g->ents[g->local];
    if (!e->alive) return;
    if (g->mode == MODE_DEATHMATCH) {
        set_status(g, "Ships only in Deathmatch arena");
        return;
    }
    e->in_ship = !e->in_ship;
    if (e->in_ship) {
        e->y += 4.f;
        set_status(g, "Entered ship — WASD fly, LMB fire, Shift boost");
    } else {
        set_status(g, "On foot — blaster/saber, F mine, 1-5 build");
        e->vx = e->vy = e->vz = 0;
    }
}

void sim_cycle_weapon(Game *g, int dir) {
    Entity *e = &g->ents[g->local];
    if (e->in_ship) return;
    e->weapon = (e->weapon + dir + WPN_COUNT) % WPN_COUNT;
    if (e->weapon == WPN_ROCKET) e->weapon = (dir > 0) ? WPN_SABER : WPN_REPEATER;
    {
        const char *n[] = { "Blaster", "Repeater", "Rocket", "Lightsaber" };
        char buf[64];
        snprintf(buf, sizeof(buf), "Weapon: %s", n[e->weapon]);
        set_status(g, buf);
    }
}

void sim_cycle_ship(Game *g, int dir) {
    g->selected_ship = (g->selected_ship + dir + SHIP_COUNT) % SHIP_COUNT;
    if (g->mode != MODE_MENU) {
        Entity *e = &g->ents[g->local];
        const ShipDef *sd = sim_ship_def(g->selected_ship);
        e->ship = g->selected_ship;
        e->hp = sd->max_hp;
        e->shield = sd->max_shield;
    }
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Ship: %s", sim_ship_name(g->selected_ship));
        set_status(g, buf);
    }
}

void sim_place_build(Game *g, int btype) {
    Entity *e;
    int i;
    float cost_ore = 5.f, cost_wood = 5.f, cost_scrap = 0.f;
    if (g->mode != MODE_FREEPLAY) return;
    e = &g->ents[g->local];
    if (!e->alive || e->in_ship) return;
    if (btype <= 0 || btype >= BLD_COUNT) return;
    switch (btype) {
    case BLD_TURRET: cost_ore = 15; cost_scrap = 10; cost_wood = 0; break;
    case BLD_SHIELD_GEN: cost_ore = 20; cost_scrap = 15; break;
    case BLD_OUTPOST: cost_ore = 25; cost_wood = 20; cost_scrap = 10; break;
    case BLD_FARM: cost_wood = 15; cost_ore = 5; break;
    case BLD_MINE: cost_ore = 10; cost_scrap = 5; break;
    default: break;
    }
    if (g->res_ore < cost_ore || g->res_wood < cost_wood || g->res_scrap < cost_scrap) {
        set_status(g, "Not enough resources");
        return;
    }
    for (i = 0; i < MAX_BUILD; i++) {
        if (g->builds[i].used) continue;
        g->builds[i].used = 1;
        g->builds[i].type = btype;
        g->builds[i].team = e->team;
        g->builds[i].x = e->x + sinf(e->yaw) * 4.f;
        g->builds[i].z = e->z - cosf(e->yaw) * 4.f;
        g->builds[i].y = gen_height(g->planet, g->builds[i].x, g->builds[i].z);
        g->builds[i].hp = 100.f;
        g->res_ore -= cost_ore;
        g->res_wood -= cost_wood;
        g->res_scrap -= cost_scrap;
        g->n_builds++;
        set_status(g, "Structure deployed");
        return;
    }
}
