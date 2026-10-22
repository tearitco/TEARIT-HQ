/* dsr_rpg.c - dsr-rpg on the shared pets-like core (&.widgits/_shared-lib/petslike_core.c). Two roles in one file: the top builds the program, and the core
 * re-includes this file once per SECTION to pull in what is specific to this game. Design: @.apps/../#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/PETSLIKE-FRAMEWORK-PLAN.md */
#ifndef PET_SECTION
#define PET_APP "dsr-rpg"
#define PET_OP "dsr_rpg"
#define PET_SKIN "dsr_rpg.c"
#include "../../../&.widgits/_shared-lib/petslike_core.c"
#else
#if PET_SECTION == 1      /* core line 209 */
    nparty = 0; char names[NP][24]; int nn = 0, inparty = 0, inhero = 0; memset(&hero, 0, sizeof hero); snprintf(hero.name, 24, "Harold"); snprintf(hero.sheet, 24, "Actor1");
#elif PET_SECTION == 2      /* core line 211 */
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { if (split_bar(line, f, 3) < 3 || strcmp(f[0], "SYSTEM")) continue; if (!strcmp(f[1], "name")) { inparty = !strcmp(f[2], getenv("PET_PARTY") && getenv("PET_PARTY")[0] ? getenv("PET_PARTY") : "DsrRpgParty"); inhero = !strcmp(f[2], "Party"); } else if (inhero && !strcmp(f[1], "member1")) snprintf(hero.name, 24, "%s", f[2]); else if (inparty && !strncmp(f[1], "member", 6) && nn < NP) snprintf(names[nn++], 24, "%s", f[2]); } fclose(fp); }
    for (int i = 0; i < nn; i++) { memset(&party[nparty], 0, sizeof party[nparty]); snprintf(party[nparty].name, 24, "%s", names[i]); snprintf(party[nparty].id, 16, "%s", names[i]); for (char *c = party[nparty].id; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; party[nparty].idx = i; snprintf(party[nparty].sheet, 24, "Monster"); nparty++; }
#elif PET_SECTION == 3      /* core line 369 */
#elif PET_SECTION == 4      /* core line 445 */
#elif PET_SECTION == 5      /* core line 449 */
    for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) { int dx = OX + x * T, dy = OY + y * T;
        if (!over_sea(rm, x, y)) { draw_auto("World_A2", 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, dx, dy); continue; }
        draw_auto("World_A1", 0, 0, over_sea(rm, x, y - 1), over_sea(rm, x + 1, y), over_sea(rm, x, y + 1), over_sea(rm, x - 1, y), over_sea(rm, x + 1, y - 1), over_sea(rm, x + 1, y + 1), over_sea(rm, x - 1, y + 1), over_sea(rm, x - 1, y - 1), dx, dy); }
#elif PET_SECTION == 6      /* core line 459 */
    if (rm->over) draw_over(rm); else if (rm->outdoor) {      /* sky = an RPG Maker parallax picked by the game clock; ground, path and trees are Outside tiles */
        Sheet *sk = sheet("parallaxes", !strcmp(phase, "night") ? "StarlitSky" : (!strcmp(phase, "dusk") || !strcmp(phase, "dawn")) ? "Sunset" : "CloudySky1"); if (sk) blit(sk, 0, !strcmp(phase, "day") ? 330 : 120, COLS * T, 2 * T, OX, OY);
        for (int x = 0; x < COLS; x++) { tile("Outside_A5", 2, 0, 1, 1, x, WALL_ROW); for (int y = TRIG_ROW; y < ROWS; y++) tile("Outside_A5", 0, 2, 1, 1, x, y); }
        for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom) for (int y = TRIG_ROW; y < ROWS - 1; y++) tile("Outside_A5", 1, 2, 1, 1, doors[i].tx, y);
    } else
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) { if (y == 0) tile("Inside_A5", 0, 0, 1, 1, x, y); else if (y == WALL_ROW) tile("Inside_A5", 1, 3, 1, 1, x, y); else tile("Inside_A5", rm->fc, rm->fr, 1, 1, x, y); }
    Sheet *dr = sheet("characters", "!Door1");                                  /* the wood panel door: closed frame of the 4th door object */
    for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom && !rm->over) { blit(dr, 0, 192, S48, S48, OX + doors[i].tx * T, OY + doors[i].ty * T); if (!rm->outdoor) tile("Inside_A5", 0, 6, 1, 1, doors[i].tx, doors[i].ty + 1); }
    for (int i = 0; i < nscen; i++) if (scen[i].room == vroom) tile(scen[i].sheet, scen[i].c, scen[i].r, scen[i].w, scen[i].h, scen[i].x, scen[i].y);
#elif PET_SECTION == 7      /* core line 472 */
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; tile(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
#elif PET_SECTION == 8      /* core line 479 */
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int col = e->steps ? cyc[e->steps % 4] : 1, ci = i < 0 ? 0 : party[i].idx;
        blit(sheet("characters", i < 0 ? hero.sheet : party[i].sheet), (ci % 4) * 3 * S48 + col * S48, (ci / 4) * 4 * S48 + e->dir * S48, S48, S48, OX + e->x * T, OY + e->y * T - 8 * T / S48); }
#elif PET_SECTION == 9      /* core line 513 */
    { static const char *dflt[4] = {"hmm, tell me more.", "I'm listening.", "interesting!", "okay, Harold."}; unsigned h = 0; for (const char *c = s; *c; c++) h = h * 31 + (unsigned char)*c;
#elif PET_SECTION == 10      /* core line 763 */
#elif PET_SECTION == 11      /* core line 782 */
#elif PET_SECTION == 12      /* core line 833 */
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3; write_frame(party[i].sheet, party[i].idx, others[i].dir, pat); unsigned char px[4] = {128, 128, 128, 255}; frame_rgb(sheet("characters", party[i].sheet), party[i].idx, others[i].dir & 3, px);
#elif PET_SECTION == 13      /* core line 835 */
    if (tr.room == r) { int pat = tr.steps % 3; write_frame(hero.sheet, 0, tr.dir, pat); unsigned char px[4] = {200, 60, 60, 255}; frame_rgb(sheet("characters", hero.sheet), 0, tr.dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s 0 %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, dc[tr.dir & 3], pat); }
#elif PET_SECTION == 14      /* core line 928 */
        Room *rm = &rooms[r]; Sheet *a5i = sheet("tilesets", "Inside_A5"), *a5o = sheet("tilesets", "Outside_A5"), *dr = sheet("characters", "!Door1");
#elif PET_SECTION == 15      /* core line 930 */
        if (rm->outdoor) { tile24(a5o, rm->fc * S48, rm->fr * S48, floorpx); tile24(a5o, 2 * S48, 0, wallpx); memcpy(ceilpx, wallpx, sizeof ceilpx); tile24(a5o, 1 * S48, 2 * S48, pathpx); }
        else { tile24(a5i, rm->fc * S48, rm->fr * S48, floorpx); tile24(a5i, 1 * S48, 3 * S48, wallpx); tile24(a5i, 0, 0, ceilpx); memcpy(pathpx, floorpx, sizeof pathpx); }
        tile24(dr, 0, 192, doorpx); tile24(a5i, 0, 6 * S48, matpx);
#elif PET_SECTION == 16      /* core line 939 */
        for (int i = 0; i < npl; i++) if (pl[i].room == r) { Item *it = &cat[pl[i].item]; Sheet *s = sheet("tilesets", it->sheet);
#elif PET_SECTION == 17      /* core line 941 */
                tile24(s, (it->c + ci) * S48, (it->r + cj) * S48, a);
#endif
#endif
