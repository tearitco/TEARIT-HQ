/* ttg_compose.c — ASCII current_frame + simple RGB glyph raster + pulses */
#include "ttg.h"

static void put_row(char frame[][FRAME_W + 1], int row, const char *s) {
    int i;
    if (row < 0 || row >= FRAME_H) return;
    for (i = 0; i < FRAME_W; i++) frame[row][i] = ' ';
    frame[row][FRAME_W] = 0;
    if (!s) return;
    for (i = 0; s[i] && i < FRAME_W; i++) frame[row][i] = s[i];
}

int ttg_compose_frame(const Game *g) {
    char frame[FRAME_H][FRAME_W + 1];
    char line[FRAME_W + 8];
    char path[MAX_PATH];
    FILE *f;
    int r, x, y, i;

    for (r = 0; r < FRAME_H; r++) put_row(frame, r, "");

    if (g->phase == PH_TITLE || g->ui_mode == 2) {
        put_row(frame, 0, "TTG  Community Tabletop Tactics");
        put_row(frame, 1, "House dual-render  ·  master_ledger  ·  ASCII+GL");
        put_row(frame, 3, g->menu_sel == 0 ? " > [1] Play 1v1 vs AI (5 min, ante 50 TC)" : "   [1] Play 1v1 vs AI (5 min, ante 50 TC)");
        put_row(frame, 4, g->menu_sel == 1 ? " > [2] Quit" : "   [2] Quit");
        put_row(frame, 6, "Clocks: 2/5/10/30 via future menu  |  Board 12x12");
        put_row(frame, 7, "Pieces: King Soldier Wizard Farmer  |  Pot winner-takes-all");
        put_row(frame, 9, "Enter/1 start   arrows menu   q quit");
        put_row(frame, 11, g->msg);
        put_row(frame, 20, "TTG_FRAME 48x22  PR0-2 skeleton");
    } else {
        int c0m = g->clock_ms[0] / 60000, c0s = (g->clock_ms[0] / 1000) % 60;
        int c1m = g->clock_ms[1] / 60000, c1s = (g->clock_ms[1] / 1000) % 60;
        snprintf(line, sizeof(line), "TTG 1v1 pot:%d T%d seat%d/%s  %s",
                 g->pot_balance, g->turn_index, g->active_seat,
                 g->active_seat == 0 ? "You" : "AI",
                 g->phase == PH_END ? "MATCH END" : "");
        put_row(frame, 0, line);
        snprintf(line, sizeof(line), "Clk %02d:%02d %02d:%02d  ELO —  |  [?]help",
                 c0m, c0s, c1m, c1s);
        put_row(frame, 1, line);
        put_row(frame, 2, "  0123456789ab");
        for (y = 0; y < BOARD_H; y++) {
            char row[FRAME_W + 1];
            int p = 0;
            row[p++] = "0123456789ab"[y];
            row[p++] = ' ';
            for (x = 0; x < BOARD_W; x++) {
                Unit *u = ttg_unit_at((Game *)g, x, y);
                char ch = '.';
                if (u && u->alive) ch = ttg_role_glyph(u->role, u->seat);
                if (x == g->cursor_x && y == g->cursor_y) {
                    /* mark cursor with [c] style: uppercase already unit; use * for empty */
                    if (ch == '.') ch = '+';
                }
                row[p++] = ch;
            }
            row[p] = 0;
            put_row(frame, 3 + y, row);
        }
        {
            Unit *sel = ttg_unit_by_id((Game *)g, g->selected);
            if (sel)
                snprintf(line, sizeof(line), "Sel %s@%d,%d moved=%d acted=%d HP%d ATK%d",
                         ttg_role_name(sel->role), sel->x, sel->y, sel->moved, sel->acted,
                         sel->hp, sel->atk);
            else
                snprintf(line, sizeof(line), "Sel (none)  cursor %d,%d", g->cursor_x, g->cursor_y);
            put_row(frame, 16, line);
        }
        put_row(frame, 17, "[Arrows]cursor [Enter]sel/move [A]atk [E]nd [U]nit-at-cursor [Q]menu");
        snprintf(line, sizeof(line), "Msg: %s", g->msg);
        put_row(frame, 18, line);
        if (g->phase == PH_END) {
            snprintf(line, sizeof(line), "WINNER seat %s (%s)  pot %d  Enter title",
                     g->winner, g->end_reason, g->pot_balance);
            put_row(frame, 19, line);
        }
        /* legend */
        put_row(frame, 20, "K/S/W/F seat0  k/s/w/f seat1  + cursor empty");
        put_row(frame, 21, "ledger: data/master_ledger.txt  units: pieces/units/");
        (void)i;
    }

    ttg_path(g, path, sizeof(path), "pieces/display");
    ttg_mkdir_p(path);
    ttg_path(g, path, sizeof(path), "pieces/display/current_frame.txt");
    f = fopen(path, "w");
    if (!f) return -1;
    for (r = 0; r < FRAME_H; r++) {
        fputs(frame[r], f);
        fputc('\n', f);
    }
    fclose(f);
    return 0;
}

/* 5x7 tiny font for digits/letters we need — fallback blocks for others */
static const unsigned char FONT5[96][7] = {
    /* space and minimal set: we'll draw solid cell per char color-coded */
};

int ttg_compose_rgb(const Game *g) {
    /* RGBA raw FRAME_W*8 x FRAME_H*8 (scale 8) — simple cell coloring from ASCII frame */
    char path[MAX_PATH], frame[FRAME_H][FRAME_W + 2];
    FILE *f;
    int scale = 8;
    int rw = FRAME_W * scale, rh = FRAME_H * scale;
    unsigned char *rgba;
    int y, x, r, c, i;
    size_t nbytes;

    ttg_path(g, path, sizeof(path), "pieces/display/current_frame.txt");
    f = fopen(path, "r");
    if (!f) return -1;
    for (r = 0; r < FRAME_H; r++) {
        if (!fgets(frame[r], sizeof(frame[r]), f)) frame[r][0] = 0;
        /* strip nl */
        {
            char *p = frame[r];
            while (*p && *p != '\n' && *p != '\r') p++;
            *p = 0;
        }
    }
    fclose(f);

    nbytes = (size_t)rw * rh * 4;
    rgba = (unsigned char *)calloc(1, nbytes);
    if (!rgba) return -1;

    for (r = 0; r < FRAME_H; r++) {
        for (c = 0; c < FRAME_W; c++) {
            char ch = (c < (int)strlen(frame[r])) ? frame[r][c] : ' ';
            unsigned char R = 20, G = 22, B = 30;
            if (ch == '.') { R = 40; G = 48; B = 40; }
            else if (ch == '+') { R = 220; G = 200; B = 40; }
            else if (ch == 'K' || ch == 'S' || ch == 'W' || ch == 'F') {
                R = 60; G = 120; B = 255;
            } else if (ch == 'k' || ch == 's' || ch == 'w' || ch == 'f') {
                R = 230; G = 70; B = 70;
            } else if (ch != ' ') {
                R = 180; G = 190; B = 210;
            }
            for (y = 0; y < scale; y++) {
                for (x = 0; x < scale; x++) {
                    int px = c * scale + x;
                    int py = r * scale + y;
                    size_t o = ((size_t)py * rw + px) * 4;
                    int border = (x == 0 || y == 0);
                    rgba[o + 0] = border ? (unsigned char)(R / 2) : R;
                    rgba[o + 1] = border ? (unsigned char)(G / 2) : G;
                    rgba[o + 2] = border ? (unsigned char)(B / 2) : B;
                    rgba[o + 3] = 255;
                }
            }
        }
    }

    ttg_path(g, path, sizeof(path), "pieces/display/rgb_frame.raw");
    f = fopen(path, "wb");
    if (!f) { free(rgba); return -1; }
    fwrite(rgba, 1, nbytes, f);
    fclose(f);
    free(rgba);

    /* receipt for gl_mirror / harness */
    ttg_path(g, path, sizeof(path), "pieces/display/rgb_frame.receipt.txt");
    {
        char rec[128];
        snprintf(rec, sizeof(rec), "width=%d\nheight=%d\nbytes=%zu\nformat=RGBA32\n", rw, rh, nbytes);
        ttg_write_file(path, rec);
    }
    (void)FONT5; (void)i;
    return 0;
}

void ttg_pulse(const Game *g) {
    char path[MAX_PATH];
    ttg_path(g, path, sizeof(path), "pieces/display/renderer_pulse.txt");
    ttg_append_file(path, "X\n");
    ttg_path(g, path, sizeof(path), "pieces/display/rgb_frame_changed.txt");
    ttg_append_file(path, "X\n");
    ttg_path(g, path, sizeof(path), "pieces/display/frame_changed.txt");
    ttg_append_file(path, "X\n");
}
