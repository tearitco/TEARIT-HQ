/* save.h */
#ifndef DF_SAVE_H
#define DF_SAVE_H

#include "fort.h"

int save_fort(const Fort *f, const char *root, const char *name);
int load_fort(Fort *f, const char *root, const char *name);

#endif
