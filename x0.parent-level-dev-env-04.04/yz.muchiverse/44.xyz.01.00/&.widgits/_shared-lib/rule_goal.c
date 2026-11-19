/* rule_goal.c - RULE "goal": a body whose CENTRE is in the goal cell has reached the goal (the level is cleared).
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93). The game tick loop calls gl_reached IN-PROCESS for each body; ops/rule_goal_op.c
 * calls the same function for a harness, an event page / Blueprintz or an AI tool call. No numbers. Same centre-cell expression as rule_pickup.c: ((x + w/2) / 256, (y + h/2) / 256) with C integer division (truncates toward zero).
 * What reaching the goal DOES (which seat wins, the game phase) is the game's loop, not this rule. Harness: case rule_goal. */
#ifndef RULE_GOAL_C
#define RULE_GOAL_C
#include "phys_field.c"
static PF_UNUSED int gl_reached(const PF_Body *b, int gx, int gy) { return (b->x + b->w / 2) / PF_Q == gx && (b->y + b->h / 2) / PF_Q == gy; }
#endif
