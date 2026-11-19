/* rule_food.c - RULE "food": where the k-th food of a snake game appears.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93). The game calls fd_spawn IN-PROCESS when a game starts and after each eat;
 * ops/rule_food_op.c calls the same function for a harness, an event page / Blueprintz or an AI tool call. No numbers (the seed and k are the game's state).
 * fd_spawn: the FREE cells are the board cells not on the snake body (body[0..len-1]), in ROW-MAJOR order (y outer, x inner), counted nfree. None free -> returns 0 and leaves *fx, *fy alone (the board is full: the game is won).
 * Otherwise pick = sm_mix(seed * 1000003 + k * 16 + 7) % nfree and the food goes to free[pick]; returns 1. The caller counts the spawn (k + 1). Harness: case rule_food. */
#ifndef RULE_FOOD_C
#define RULE_FOOD_C
#include "seed_mix.c"
static __attribute__((unused)) int fd_on(const int *bx, const int *by, int len, int x, int y) { for (int i = 0; i < len; i++) if (bx[i] == x && by[i] == y) return 1; return 0; }
static __attribute__((unused)) int fd_spawn(const int *bx, const int *by, int len, int cols, int rows, long seed, long k, int *fx, int *fy) {
    int nfree = 0; for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) if (!fd_on(bx, by, len, x, y)) nfree++;
    if (!nfree) return 0;
    int pick = (int)(sm_mix((uint64_t)seed * 1000003ULL + (uint64_t)k * 16 + 7) % (uint64_t)nfree), n = 0;
    for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) if (!fd_on(bx, by, len, x, y)) { if (n++ == pick) { *fx = x; *fy = y; return 1; } }
    return 0;
}
#endif
