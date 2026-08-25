/* render.h — voxels + HUD */
#ifndef GLUT_CRAFT_RENDER_H
#define GLUT_CRAFT_RENDER_H

#include "world.h"
#include "player.h"
#include "inv.h"

void render_init(void);
void render_world(const World *w, const Player *p);
void render_hud(const Player *p, const Inventory *inv, float fps, const char *status);

#endif
