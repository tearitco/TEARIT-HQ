/* pet_nudge.c - wake an idle board engine (text-included by petslike_core.c and by rpg_edit.c; pure: stdio/stdlib/string/sys-stat/unistd only).
 *
 * An idle pc-hq engine session only renders on input, so after a game moved its sprites the game appends the no-op key 126 (tilde: bound to nothing, so the view and camera are never touched; the old "1" is the POV
 * key) to that session's relay <session>/pieces/apps/player_app/interact_relay.txt. Which session: every board window has its own ui.txt carrying a bv_session=<dir> line. A candidate counts only when its ui.txt
 * exists, names a session and that session's relay exists (a stale file points at a dead session); among several candidates the one whose ui.txt is NEWEST wins. */
#ifndef PET_NUDGE_C
#define PET_NUDGE_C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* the live relay named by ui_path's bv_session= line; returns 1 and fills rp (and the ui.txt mtime) when it exists */
static int pet_session_relay(const char *ui_path, char *rp, size_t cap, time_t *mtime) {
    struct stat us; if (stat(ui_path, &us)) return 0;
    FILE *f = fopen(ui_path, "r"); if (!f) return 0;
    char ub[4096], sess[4096] = ""; size_t n = fread(ub, 1, sizeof ub - 1, f); ub[n] = 0; fclose(f);
    for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11);
    if (!sess[0]) return 0;
    snprintf(rp, cap, "%s/pieces/apps/player_app/interact_relay.txt", sess);
    if (access(rp, F_OK)) return 0;
    if (mtime) { *mtime = us.st_mtime; }
    return 1;
}
/* append the no-op key to the relay of the newest live candidate; returns 1 when a key was written */
static int pet_nudge(const char *const *ui_paths, int n) {
    char best[4200] = ""; time_t bt = 0;
    for (int k = 0; k < n; k++) { char rp[4200]; time_t mt = 0; if (pet_session_relay(ui_paths[k], rp, sizeof rp, &mt) && (!best[0] || mt > bt)) { snprintf(best, sizeof best, "%s", rp); bt = mt; } }
    if (!best[0]) return 0;
    FILE *f = fopen(best, "a"); if (!f) return 0; fputs("126\n", f); fclose(f); return 1;
}
#endif
