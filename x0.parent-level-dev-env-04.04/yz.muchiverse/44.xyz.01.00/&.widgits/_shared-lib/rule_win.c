/* rule_win.c - RULE "win": which seat has won the level: the LOWEST-numbered body whose centre is in the goal cell, or none.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93). The game calls wn_winner once per tick over all seats IN-PROCESS;
 * ops/rule_win_op.c calls the same function for a harness, an event page / Blueprintz or an AI tool call. No numbers. Built on rule_goal.c (gl_reached).
 * wn_winner returns the seat index (0-based) of the first body, in seat order, that has reached the goal; -1 when nobody has. A tie in one tick therefore goes to the lowest seat (existing behaviour, pinned).
 * What a win DOES (the game phase becomes OVER, the ledger rows) is the game's loop. Harness: case rule_win. */
#ifndef RULE_WIN_C
#define RULE_WIN_C
#include "rule_goal.c"
static PF_UNUSED int wn_winner(const PF_Body *b, int n, int gx, int gy) {
    for (int i = 0; i < n; i++) if (gl_reached(&b[i], gx, gy)) return i;
    return -1;
}
#endif
