/* petslike_autotile.c - pure text-include, no dependencies: the RPG Maker autotile shape picker shared by the pets-like skins (rpg-pet, df-hq draw it onto the frame, petslike_skin_lib.c's sk_auto_tile makes a tile picture of it). Owner 2026-10-23: "i thought they would all share tiling".
 * The rule (verified against the tsots maps: tile id = kind * 48 + shape, the shape picked from the 8 neighbours of the same kind; all 8 present = shape 0; the 48 shapes are 4 quadrant pieces; same as _.monads/_.livedesk-taskbar/ops/tile_autotile.c's FLOOR_AUTOTILE_TABLE, computed per quadrant: for each corner look at its two adjacent sides and its diagonal).
 * sk_auto_quads(n, e, s, w, ne, se, sw, nw, q): neighbours (1 = the same kind) -> the four 24 px pieces (column, row in 24 px units) inside the kind's 2 x 3 tile block, order TL TR BL BR. Needs <string.h>. */
static void sk_auto_quads(int n, int e, int s, int w, int ne, int se, int sw, int nw, int q[4][2]) {
    if (!n && !e && !s && !w) { static const int iso[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}; memcpy(q, iso, sizeof(int) * 8); return; }
    #define SKPICK(i, a, b, d, bothA, bothB, onlyA_, onlyB_, none_) { const int *p_ = ((a) && (b)) ? ((d) ? bothA : bothB) : (a) ? onlyA_ : (b) ? onlyB_ : none_; q[i][0] = p_[0]; q[i][1] = p_[1]; }
    static const int TLf[2] = {2, 4}, TLn[2] = {2, 0}, TLw[2] = {2, 2}, TLm[2] = {0, 4}, TLx[2] = {0, 2};      /* flat, notch (diagonal missing), N missing, W missing, both missing */
    static const int TRf[2] = {1, 4}, TRn[2] = {3, 0}, TRw[2] = {1, 2}, TRm[2] = {3, 4}, TRx[2] = {3, 2};
    static const int BLf[2] = {2, 3}, BLn[2] = {2, 1}, BLw[2] = {2, 5}, BLm[2] = {0, 3}, BLx[2] = {0, 5};
    static const int BRf[2] = {1, 3}, BRn[2] = {3, 1}, BRw[2] = {1, 5}, BRm[2] = {3, 3}, BRx[2] = {3, 5};
    SKPICK(0, n, w, nw, TLf, TLn, TLw, TLm, TLx)      /* "only A" = the first side present, i.e. the other side (W) is missing */
    SKPICK(1, n, e, ne, TRf, TRn, TRw, TRm, TRx)
    SKPICK(2, s, w, sw, BLf, BLn, BLw, BLm, BLx)
    SKPICK(3, s, e, se, BRf, BRn, BRw, BRm, BRx)
    #undef SKPICK
    /* SKPICK(a present only) means the OTHER side is missing: swap to the table's meaning (TLm = W missing, TLw = N missing) */
    if (n && !w) { q[0][0] = TLm[0]; q[0][1] = TLm[1]; } else if (!n && w) { q[0][0] = TLw[0]; q[0][1] = TLw[1]; }
    if (n && !e) { q[1][0] = TRm[0]; q[1][1] = TRm[1]; } else if (!n && e) { q[1][0] = TRw[0]; q[1][1] = TRw[1]; }
    if (s && !w) { q[2][0] = BLm[0]; q[2][1] = BLm[1]; } else if (!s && w) { q[2][0] = BLw[0]; q[2][1] = BLw[1]; }
    if (s && !e) { q[3][0] = BRm[0]; q[3][1] = BRm[1]; } else if (!s && e) { q[3][0] = BRw[0]; q[3][1] = BRw[1]; }
}
