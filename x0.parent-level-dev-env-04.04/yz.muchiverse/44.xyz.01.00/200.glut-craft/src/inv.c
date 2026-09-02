/* inv.c */
#include "inv.h"
#include "world.h"

void inv_init(Inventory *inv) {
    inv->slots[0] = BLK_GRASS;
    inv->slots[1] = BLK_DIRT;
    inv->slots[2] = BLK_STONE;
    inv->slots[3] = BLK_WOOD;
    inv->slots[4] = BLK_SAND;
    inv->slots[5] = BLK_COBBLE;
    inv->slots[6] = BLK_LEAVES;
    inv->slots[7] = BLK_PLANKS;
    inv->slots[8] = BLK_STONE;
    inv->selected = 0;
}

void inv_select(Inventory *inv, int slot) {
    if (slot < 0 || slot >= HOTBAR_SLOTS) return;
    inv->selected = slot;
}

uint8_t inv_selected_block(const Inventory *inv) {
    return inv->slots[inv->selected];
}
