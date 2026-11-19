/* rule_input.c - RULE "input": turns a seat's LATCHED input (the last direction pressed, a jump press, a fire press) into this tick's controls for the body step.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93). The game calls in_controls and in_consume for each hero each tick IN-PROCESS;
 * ops/rule_input_op.c calls the same functions for a harness, an event page / Blueprintz or an AI tool call. No numbers.
 * in_controls: mask = 1 if dir < 0, 2 if dir > 0 (else 0), plus 4 if jp (the mask pf_step in phys_field.c takes); a non-zero dir also sets *facing = dir (facing is KEPT while dir is 0); *shoot = fire && armed (an unarmed press shoots nothing).
 * in_consume: a jump press and a fire press are ONE-SHOT: both latches are cleared after the tick (the direction stays latched until a new one is pressed). Harness: case rule_input. */
#ifndef RULE_INPUT_C
#define RULE_INPUT_C
static __attribute__((unused)) int in_controls(int dir, int jp, int fire, int armed, int *facing, int *shoot) {
    if (dir) *facing = dir;
    *shoot = fire && armed;
    return (dir < 0 ? 1 : dir > 0 ? 2 : 0) | (jp ? 4 : 0);
}
static __attribute__((unused)) void in_consume(int *jp, int *fire) { *jp = 0; *fire = 0; }
#endif
