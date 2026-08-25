/* unit.h */
#ifndef DF_UNIT_H
#define DF_UNIT_H

#include "fort.h"

void unit_spawn_embark(Fort *f);
int  unit_find_path(Fort *f, Dwarf *d, int tx, int ty);
void unit_step_all(Fort *f);
const char *dwarf_state_name(int st);
int  unit_at(const Fort *f, int x, int y);

#endif
