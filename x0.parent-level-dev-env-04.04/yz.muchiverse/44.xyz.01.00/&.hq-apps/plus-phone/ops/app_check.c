/* app_check.c - validates a plus-phone APP PACKAGE (design: BOOK 16.game/PLUS-PHONE-DESIGN-2026-11-15.md sections 12, 22, 24). ONE checker: app-edit runs it before export, phone_op runs it before install.
 *   app_check <package_dir> [<house>]
 * A package is a folder named after its app id:  manifest.pdl, icon/ (a sprite folder: sprite.csv), screens/*.xhtpm (optional; none = a BLANK app, which is valid).
 * manifest.pdl rows (KEY | value): APP | id | <id>   TITLE   ICON | <dir>   ORDER | <int>   PIN | first   ORIENT | portrait|landscape|both   PLATFORM | <int, supported: 1>   SCREEN | <name> | <path under the package>
 * Rules (each failure prints `CHECK-FAIL <what> | fix: <how>`; the op prints CHECK-OK <id> and exits 0 only when there are none): the folder name equals the id (letters, digits, - _); a TITLE of 1..24 chars; the ICON dir has a sprite.csv;
 * ORDER an integer; PIN only `first`; ORIENT one of the three words; PLATFORM an integer not above 1 (a newer package than the shell is refused, never half-run); every SCREEN file exists, stays inside the package (no `..`, not absolute) and,
 * when a house is given, passes layout_check; an UNKNOWN key is an error (a typo must not pass silently). Reads only; writes nothing.
 * Build: gcc -std=gnu11 -Wall -O2 -o +x/app_check.+x app_check.c   (sh build_phone.sh builds it) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/wait.h>
#define P 4096
static int nerr = 0;
static void bad(const char *what, const char *fix) { printf("CHECK-FAIL %s | fix: %s\n", what, fix); nerr++; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static int word_ok(const char *s) { if (!*s || strlen(s) > 40) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '-' || *s == '_')) return 0; return 1; }
static int int_ok(const char *s) { if (!*s) return 0; if (*s == '-') s++; if (!*s) return 0; for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0; return 1; }
static int run_wait(char *const av[]) { pid_t c = fork(); if (c < 0) return -1; if (c == 0) { int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 1); dup2(nul, 2); dup2(nul, 0); } execv(av[0], av); _exit(127); } int st; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: app_check <package_dir> [<house>]\n"); return 2; }
    char pkg[P]; snprintf(pkg, sizeof pkg, "%s", argv[1]); size_t pl = strlen(pkg); while (pl > 1 && pkg[pl - 1] == '/') pkg[--pl] = 0;
    const char *house = argc >= 3 ? argv[2] : NULL; char tmp[P]; snprintf(tmp, sizeof tmp, "%s", pkg); const char *folder = basename(tmp);
    char mf[P]; snprintf(mf, sizeof mf, "%s/manifest.pdl", pkg); FILE *f = fopen(mf, "r"); if (!f) { bad("no manifest.pdl in the package", "add manifest.pdl with APP, TITLE, ICON, PLATFORM rows"); printf("CHECK-FAILED %d problem(s)\n", nerr); return 1; }
    char id[64] = "", title[200] = "", icon[200] = "", order[24] = "", pin[24] = "", orient[24] = "", plat[24] = ""; int n_screen = 0; char ln[1024], screens[16][400]; int lno = 0;
    while (fgets(ln, sizeof ln, f)) { lno++; char *p = trim(ln); if (!*p || *p == '#') continue; char *fl[4] = { 0 }; int nf = 0; while (nf < 4) { char *bar = (nf == 3) ? NULL : strchr(p, '|'); if (bar) *bar = 0; fl[nf++] = trim(p); if (!bar) break; p = bar + 1; }
        char where[120]; snprintf(where, sizeof where, "line %d", lno);
        if (!strcmp(fl[0], "APP") && nf >= 3 && !strcmp(fl[1], "id")) snprintf(id, sizeof id, "%s", fl[2]);
        else if (!strcmp(fl[0], "TITLE") && nf >= 2) snprintf(title, sizeof title, "%s", fl[1]);
        else if (!strcmp(fl[0], "ICON") && nf >= 2) snprintf(icon, sizeof icon, "%s", fl[1]);
        else if (!strcmp(fl[0], "ORDER") && nf >= 2) snprintf(order, sizeof order, "%s", fl[1]);
        else if (!strcmp(fl[0], "PIN") && nf >= 2) snprintf(pin, sizeof pin, "%s", fl[1]);
        else if (!strcmp(fl[0], "ORIENT") && nf >= 2) snprintf(orient, sizeof orient, "%s", fl[1]);
        else if (!strcmp(fl[0], "PLATFORM") && nf >= 2) snprintf(plat, sizeof plat, "%s", fl[1]);
        else if (!strcmp(fl[0], "DEP") && nf >= 3) { if (!word_ok(fl[1]) || !int_ok(fl[2])) { char m[200]; snprintf(m, sizeof m, "%s: a DEP row needs a dependency id (letters, digits, - or _) and a whole minimum version", where); bad(m, "write DEP | <dep id> | <minimum version>"); } }
        else if (!strcmp(fl[0], "SCREEN") && nf >= 3) { if (n_screen < 16) snprintf(screens[n_screen++], 400, "%s|%s", fl[1], fl[2]); }
        else { char m[200]; snprintf(m, sizeof m, "%s: unknown or incomplete row \"%s\"", where, fl[0]); bad(m, "known keys: APP | id | x, TITLE, ICON, ORDER, PIN, ORIENT, PLATFORM, SCREEN | name | path, DEP | id | min version"); } }
    fclose(f);
    if (!id[0]) bad("no APP | id | <id> row", "add APP | id | <id>"); else if (!word_ok(id)) bad("the app id has a character that is not a letter, digit, - or _", "rename the id"); else if (strcmp(id, folder)) { char m[200]; snprintf(m, sizeof m, "the folder is named %s but the app id is %s", folder, id); bad(m, "name the package folder after its id"); }
    if (!title[0] || strlen(title) > 24) bad("TITLE must be 1 to 24 characters", "shorten or add TITLE | <name>");
    if (!icon[0]) bad("no ICON row", "add ICON | icon and an icon/sprite.csv"); else { char sp[P]; snprintf(sp, sizeof sp, "%s/%s/sprite.csv", pkg, icon); if (strstr(icon, "..") || icon[0] == '/' || access(sp, R_OK)) bad("the ICON folder has no readable sprite.csv (or points outside the package)", "put the icon art in <package>/<ICON>/sprite.csv"); }
    if (order[0] && !int_ok(order)) bad("ORDER is not an integer", "use a whole number, 1 is first");
    if (pin[0] && strcmp(pin, "first")) bad("PIN may only be `first`", "write PIN | first or remove the row");
    if (orient[0] && strcmp(orient, "portrait") && strcmp(orient, "landscape") && strcmp(orient, "both")) bad("ORIENT must be portrait, landscape or both", "fix the word");
    if (!plat[0] || !int_ok(plat)) bad("PLATFORM must be an integer", "write PLATFORM | 1"); else if (atoi(plat) > 1) { char m[120]; snprintf(m, sizeof m, "PLATFORM %s is newer than this phone supports (1)", plat); bad(m, "export for platform 1, or update the phone"); }
    char chk[P] = ""; if (house) snprintf(chk, sizeof chk, "%s/@.apps/layout-studio/ops/+x/layout_check.+x", house);
    for (int i = 0; i < n_screen; i++) { char *bar = strchr(screens[i], '|'); *bar = 0; const char *nm = screens[i], *path = bar + 1;
        if (!word_ok(nm)) { char m[200]; snprintf(m, sizeof m, "screen name \"%s\" is not a plain word", nm); bad(m, "use letters, digits, - or _"); continue; }
        if (strstr(path, "..") || path[0] == '/') { char m[260]; snprintf(m, sizeof m, "screen %s: the path must stay inside the package", nm); bad(m, "use a relative path such as screens/home.xhtpm"); continue; }
        char sf[P]; snprintf(sf, sizeof sf, "%s/%s", pkg, path); if (access(sf, R_OK)) { char m[300]; snprintf(m, sizeof m, "screen %s: %s is not readable", nm, path); bad(m, "create the file or remove the SCREEN row"); continue; }
        if (chk[0] && !access(chk, X_OK)) { char *av[] = { chk, sf, NULL }; if (run_wait(av) != 0) { char m[300]; snprintf(m, sizeof m, "screen %s (%s) fails layout_check", nm, path); bad(m, "run layout_check on it and fix the ERROR lines"); } } }
    if (nerr) { printf("CHECK-FAILED %d problem(s)\n", nerr); return 1; }
    printf("CHECK-OK %s (%d screen%s%s)\n", id, n_screen, n_screen == 1 ? "" : "s", n_screen ? "" : ", a blank app"); return 0;
}
