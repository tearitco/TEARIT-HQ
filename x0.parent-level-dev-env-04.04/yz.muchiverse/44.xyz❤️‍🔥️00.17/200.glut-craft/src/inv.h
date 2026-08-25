/* inv.h — hotbar inventory */
#ifndef GLUT_CRAFT_INV_H
#define GLUT_CRAFT_INV_H

#include <stdint.h>

#define HOTBAR_SLOTS 9

typedef struct {
    uint8_t slots[HOTBAR_SLOTS]; /* block ids */
    int selected;                /* 0..8 */
} Inventory;

void inv_init(Inventory *inv);
void inv_select(Inventory *inv, int slot); /* 0..8 */
uint8_t inv_selected_block(const Inventory *inv);

#endif
