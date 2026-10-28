/* khtpm_pin_pick.c - WHICH open_book_page.txt a board obeys (S3b-1 of the games-as-books pivot, 2026-10-28; BOOK 16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md section 23).
 * Text-included by bv_render_3d.c and bv_render_2d.c (pure: one function, no globals, no I/O of its own; the caller says what "readable" means because the engines open files through host_fopen).
 *   pin_path_pick(house, board_root, readable, out, n):
 *     board_root = the folder of the board the engine is drawing (its pc-hq folder, e.g. @.apps/rpg-space/rpg-space-pchq); "" or NULL = unknown.
 *     The board's OWN pin  <board_root>/pieces/display/open_book_page.txt  wins when readable; otherwise the house-wide pin  <house>/@.apps/piececraft-hq/pieces/display/open_book_page.txt  (what every engine read before).
 * Why: the taskbar Synch writes the house-wide pin ("every pc-hq takes the desk's book and page"); a game board with its own pin (source=board) must not be flattened by it. piececraft-hq itself has no pin of its own
 * beside that file, so its behaviour does not change. The result is always NUL-terminated and never longer than n-1 characters (a longer path is cut: the caller's fopen then fails and the pin counts as absent). */
#include <stdio.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"      /* the cut is the documented behaviour */
static void pin_path_pick(const char *house, const char *board_root, int (*readable)(const char *), char *out, size_t n) {
    if (!out || n == 0) {
        return;
    }
    snprintf(out, n, "%s/@.apps/piececraft-hq/pieces/display/open_book_page.txt", house ? house : "");
    if (board_root && board_root[0] && readable) {
        char own[4400];
        int w = snprintf(own, sizeof own, "%s/pieces/display/open_book_page.txt", board_root);
        if (w > 0 && (size_t)w < sizeof own && readable(own)) {
            snprintf(out, n, "%s", own);
        }
    }
}
#pragma GCC diagnostic pop
