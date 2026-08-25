/* player.c */
#include "player.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EYE_HEIGHT 1.62f
#define PLAYER_HALF_W 0.3f
#define PLAYER_HEIGHT 1.7f
#define GRAVITY -22.0f
#define JUMP_V 8.0f
#define WALK_SPEED 5.5f
#define FLY_SPEED 12.0f
#define MOUSE_SENS 0.12f

void player_init(Player *p, float x, float y, float z) {
    memset(p, 0, sizeof(*p));
    p->x = x;
    p->y = y;
    p->z = z;
    p->yaw = 0.0f;
    p->pitch = 0.0f;
    p->flying = 1; /* start in fly for easier first contact */
    p->mouse_captured = 0;
    p->win_w = 1280;
    p->win_h = 720;
}

void player_look(Player *p, float dx, float dy) {
    p->yaw += dx * MOUSE_SENS;
    p->pitch -= dy * MOUSE_SENS;
    if (p->pitch > 89.0f) p->pitch = 89.0f;
    if (p->pitch < -89.0f) p->pitch = -89.0f;
    while (p->yaw > 360.0f) p->yaw -= 360.0f;
    while (p->yaw < 0.0f) p->yaw += 360.0f;
}

static int solid_at(const World *w, int x, int y, int z) {
    uint8_t b = world_get(w, x, y, z);
    /* leaves are soft (no collision) for easier tree navigation */
    return b != BLK_AIR && b != BLK_LEAVES;
}

/* AABB vs solid voxels */
static int collides(const World *w, float x, float y, float z) {
    float minx = x - PLAYER_HALF_W;
    float maxx = x + PLAYER_HALF_W;
    float miny = y - EYE_HEIGHT;
    float maxy = y - EYE_HEIGHT + PLAYER_HEIGHT;
    float minz = z - PLAYER_HALF_W;
    float maxz = z + PLAYER_HALF_W;
    int x0 = (int)floorf(minx);
    int x1 = (int)floorf(maxx);
    int y0 = (int)floorf(miny);
    int y1 = (int)floorf(maxy);
    int z0 = (int)floorf(minz);
    int z1 = (int)floorf(maxz);
    int ix, iy, iz;
    for (ix = x0; ix <= x1; ix++)
        for (iy = y0; iy <= y1; iy++)
            for (iz = z0; iz <= z1; iz++)
                if (solid_at(w, ix, iy, iz))
                    return 1;
    return 0;
}

void player_update(Player *p, World *w, float dt) {
    float yaw_r = p->yaw * (float)M_PI / 180.0f;
    float forward_x = sinf(yaw_r);
    float forward_z = -cosf(yaw_r);
    float right_x = cosf(yaw_r);
    float right_z = sinf(yaw_r);
    float speed = p->flying ? FLY_SPEED : WALK_SPEED;
    float mx = 0.0f, mz = 0.0f, my = 0.0f;

    if (p->keys[(unsigned char)'w'] || p->keys[(unsigned char)'W']) {
        mx += forward_x; mz += forward_z;
    }
    if (p->keys[(unsigned char)'s'] || p->keys[(unsigned char)'S']) {
        mx -= forward_x; mz -= forward_z;
    }
    if (p->keys[(unsigned char)'a'] || p->keys[(unsigned char)'A']) {
        mx -= right_x; mz -= right_z;
    }
    if (p->keys[(unsigned char)'d'] || p->keys[(unsigned char)'D']) {
        mx += right_x; mz += right_z;
    }

    if (p->flying) {
        if (p->keys[(unsigned char)' ']) my += 1.0f;
        if (p->keys[(unsigned char)'c'] || p->keys[(unsigned char)'C'] ||
            p->special[/*GLUT_KEY shift via ascii*/0])
            my -= 1.0f;
        /* Shift often arrives as special; also accept lowercase f-mode down via 'q' no — use keys for shift stored separately */
        p->vx = mx * speed;
        p->vy = my * speed;
        p->vz = mz * speed;
        /* normalize horizontal if needed */
        {
            float len = sqrtf(mx * mx + mz * mz);
            if (len > 1e-4f) {
                p->vx = (mx / len) * speed;
                p->vz = (mz / len) * speed;
            }
        }
    } else {
        float len = sqrtf(mx * mx + mz * mz);
        if (len > 1e-4f) {
            p->vx = (mx / len) * speed;
            p->vz = (mz / len) * speed;
        } else {
            p->vx = 0.0f;
            p->vz = 0.0f;
        }
        p->vy += GRAVITY * dt;
        if (p->on_ground && p->keys[(unsigned char)' ']) {
            p->vy = JUMP_V;
            p->on_ground = 0;
        }
    }

    /* axis-separated collision */
    {
        float nx = p->x + p->vx * dt;
        if (!collides(w, nx, p->y, p->z))
            p->x = nx;
        else
            p->vx = 0.0f;

        float ny = p->y + p->vy * dt;
        if (!collides(w, p->x, ny, p->z)) {
            p->y = ny;
            p->on_ground = 0;
        } else {
            if (p->vy < 0.0f) p->on_ground = 1;
            p->vy = 0.0f;
        }

        float nz = p->z + p->vz * dt;
        if (!collides(w, p->x, p->y, nz))
            p->z = nz;
        else
            p->vz = 0.0f;
    }

    /* keep in world soft bounds */
    if (p->x < 1.0f) p->x = 1.0f;
    if (p->z < 1.0f) p->z = 1.0f;
    if (p->x > WORLD_W - 2.0f) p->x = WORLD_W - 2.0f;
    if (p->z > WORLD_D - 2.0f) p->z = WORLD_D - 2.0f;
    if (p->y < 2.0f) { p->y = 2.0f; p->vy = 0.0f; }
    if (p->y > WORLD_H + 20.0f) p->y = WORLD_H + 20.0f;

    player_raycast(p, w, 6.0f);
}

void player_raycast(Player *p, const World *w, float reach) {
    float yaw_r = p->yaw * (float)M_PI / 180.0f;
    float pitch_r = p->pitch * (float)M_PI / 180.0f;
    float dx = cosf(pitch_r) * sinf(yaw_r);
    float dy = sinf(pitch_r);
    float dz = cosf(pitch_r) * -cosf(yaw_r);

    float step = 0.05f;
    float t;
    int prev_x = (int)floorf(p->x);
    int prev_y = (int)floorf(p->y);
    int prev_z = (int)floorf(p->z);

    p->has_target = 0;
    for (t = 0.0f; t <= reach; t += step) {
        float px = p->x + dx * t;
        float py = p->y + dy * t;
        float pz = p->z + dz * t;
        int bx = (int)floorf(px);
        int by = (int)floorf(py);
        int bz = (int)floorf(pz);
        if (bx == prev_x && by == prev_y && bz == prev_z) continue;
        if (world_get(w, bx, by, bz) != BLK_AIR) {
            p->has_target = 1;
            p->tx = bx; p->ty = by; p->tz = bz;
            p->px = prev_x; p->py = prev_y; p->pz = prev_z;
            return;
        }
        prev_x = bx; prev_y = by; prev_z = bz;
    }
}
