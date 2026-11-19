/* rule_fall.c - RULE "fall": a body whose top edge is BELOW the bottom of the world (plus a margin of fall_margin whole cells) has fallen out of the level.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93). The game calls fa_fell for each hero and fa_limit for the walkers' own fall-out (rule_patrol.c) IN-PROCESS;
 * ops/rule_fall_op.c calls the same functions for a harness, an event page / Blueprintz or an AI tool call. The number is DATA (RULE fall_margin), never a constant here. Q8 = 1/256 cell.
 * fa_limit = (level_h + margin) * 256. fa_fell = body y STRICTLY greater than the limit (a body whose top is exactly on the limit has not fallen). What a fall DOES (respawn, the falls counter) is the game's loop (respawn is rule_spawn). Harness: case rule_fall. */
#ifndef RULE_FALL_C
#define RULE_FALL_C
#include "phys_field.c"
static PF_UNUSED long fa_limit(long level_h, long margin) { return (level_h + margin) * PF_Q; }
static PF_UNUSED int fa_fell(const PF_Body *b, long level_h, long margin) { return b->y > fa_limit(level_h, margin); }
#endif
