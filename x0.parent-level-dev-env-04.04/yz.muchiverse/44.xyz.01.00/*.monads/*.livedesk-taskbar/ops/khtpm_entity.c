/* Pal / desktop-entity process. Unfactor piece 4 (2026-09-18):
 * compile unit for khtpm_entity.+x. Still includes khtpm_core_render.c
 * (tp_main lives there) until a later surgical split. Spawners exec
 * this binary with argv[1]=package_dir. */
#define KHTPM_ENTITY_BIN
#include "khtpm_core_render.c"
