/* rta_place.h - text-include shared by rta_life_op and rta_board_op (D120, pure, two consumers): where a person stands on the RTA grid.
 * Facts come from DATA: the road grid, and the STAND / BESIDE rows of the board data file (@.apps/rta-board/board.pdl). No ledger is read here.
 *   pl_load(p, grid, board)   0 ok, -1 grid unreadable, -2 board data unreadable
 *   pl_spawn(p,&x,&y)         the first road cell of the south-most road row (the same rule as rta_momentum_op)
 *   pl_standable(p,x,y)       inside the grid and a STAND glyph
 *   pl_beside(p,cx,cy,&x,&y)  the first free BESIDE cell of (cx,cy), else the cell itself
 */
#ifndef RTA_PLACE_H
#define RTA_PLACE_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define PL_W 220
#define PL_H 300
typedef struct { char g[PL_H][PL_W]; int w, h; char stand[64]; int nb, dx[16], dy[16]; } PL;

static int pl_load(PL *p, const char *grid, const char *board) {
    FILE *f = fopen(grid, "r"); char row[PL_W + 8];
    memset(p, 0, sizeof *p); snprintf(p->stand, sizeof p->stand, ".K");
    if (!f) return -1;
    while (fgets(row, sizeof row, f)) {
        if (row[0] == '#' || row[0] == '\n') continue;
        int n = (int)strcspn(row, "\r\n");
        if (n < 1 || n > PL_W || p->h >= PL_H) { fclose(f); return -1; }
        memcpy(p->g[p->h], row, (size_t)n); if (!p->w) p->w = n; if (n != p->w) { fclose(f); return -1; } p->h++;
    }
    fclose(f); if (!p->w || !p->h) return -1;
    f = fopen(board, "r"); if (!f) return -2;
    char l[512];
    while (fgets(l, sizeof l, f)) {
        int dx, dy;
        if (!strncmp(l, "STAND |", 7)) { const char *s = l + 7; while (*s == ' ') s++; size_t n = strcspn(s, " \r\n"); if (n > 0 && n < sizeof p->stand) { memcpy(p->stand, s, n); p->stand[n] = 0; } }
        else if (!strncmp(l, "BESIDE |", 8) && p->nb < 16 && sscanf(l, "BESIDE | %d | %d", &dx, &dy) == 2) { p->dx[p->nb] = dx; p->dy[p->nb] = dy; p->nb++; }
    }
    fclose(f); return 0;
}
static int pl_spawn(const PL *p, int *x, int *y) {
    for (int yy = p->h - 1; yy >= 0; yy--) for (int xx = 0; xx < p->w; xx++) if (p->g[yy][xx] == 'R') { *x = xx; *y = yy; return 0; }
    return -1;
}
static int pl_standable(const PL *p, int x, int y) { return x >= 0 && y >= 0 && x < p->w && y < p->h && strchr(p->stand, p->g[y][x]); }
static void pl_beside(const PL *p, int cx, int cy, int *ex, int *ey) {
    *ex = cx; *ey = cy;
    for (int i = 0; i < p->nb; i++) if (pl_standable(p, cx + p->dx[i], cy + p->dy[i])) { *ex = cx + p->dx[i]; *ey = cy + p->dy[i]; return; }
}
#endif
