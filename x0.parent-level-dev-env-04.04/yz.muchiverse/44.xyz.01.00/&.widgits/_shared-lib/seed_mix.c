/* seed_mix.c - the house's seeded 64-bit mixer (splitmix64 finaliser): the ONE implementation every seeded rule uses, so a seed gives the same numbers on every build.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment). game_core's `mix` and the seeded die are thin wrappers over sm_mix; rule_food.c uses it directly. Changing one constant changes every seeded game and every recorded run: the harness case rule_food pins the mixer's output. */
#ifndef SEED_MIX_C
#define SEED_MIX_C
#include <stdint.h>
static __attribute__((unused)) uint64_t sm_mix(uint64_t x) { x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; return x ^ (x >> 31); }
#endif
