/* player.h — camera, movement, collision, raycast */
#ifndef GLUT_CRAFT_PLAYER_H
#define GLUT_CRAFT_PLAYER_H

#include "world.h"

typedef struct {
    float x, y, z;       /* eye position */
    float yaw, pitch;    /* degrees: yaw around Y, pitch up/down */
    float vx, vy, vz;
    int flying;
    int on_ground;
    int keys[256];       /* ASCII key down */
    int special[256];
    int mouse_captured;
    int win_w, win_h;
    /* reach target */
    int has_target;
    int tx, ty, tz;      /* block to break */
    int px, py, pz;      /* place adjacent */
} Player;

void player_init(Player *p, float x, float y, float z);
void player_update(Player *p, World *w, float dt);
void player_look(Player *p, float dx, float dy);
void player_raycast(Player *p, const World *w, float reach);

#endif
