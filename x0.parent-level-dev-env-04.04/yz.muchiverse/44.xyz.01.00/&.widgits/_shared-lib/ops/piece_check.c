/* piece_check - read-only audit of PIECE identity across all games and the user's Master Pal List (BOOK 16.game/PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md section 6).
 * It NEVER writes. It reads <house>/@.apps/<game>/state/uid_<slug>.txt for every game (a folder with toy.pdl) and <house>/xyzfs/users/<user>/home/livedesk/pals/<pal>/entity_uid.txt.
 * A valid uid = 8 to 64 lowercase hex characters (owner 2026-10-28: any length is fine; the wallet id is derived by hash, so length does not matter).
 * Findings (one line each):
 *   ERROR empty_uid    <game> <slug>                   the uid file is empty or not a valid uid
 *   ERROR dup_uid      <uid8> <game:slug> <game:slug>  one valid uid used by two different pets (a piece in two games must be an explicit shared cast row, which does not exist yet)
 *   WARN  name_collision <slug> games=<n> uids=<m>     the same slug in 2+ games with 2+ different uids (different pieces with one name: rename on purpose)
 *   WARN  not_durable  <game> files=<n>                the uid files live under state/ and the game's .gitignore ignores state/ (identity lost with the folder)
 *   WARN  no_master    <game> <slug>                   no master-list pal is named <slug> or ends with _<slug>
 *   WARN  master_bad_uid <user>/<pal>                  a master-list pal whose entity_uid.txt is missing or invalid
 * Last line: `piece_check: games=<g> pets=<p> master=<m> errors=<e> warnings=<w>`.  Exit 0 = no ERROR, 1 = at least one ERROR, 2 = usage / no house.
 * Usage: piece_check <house>.   Build: sh build_piece_check.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAXP 1024
typedef struct { char game[64], slug[64], uid[80]; int valid; } Pet;
static Pet P[MAXP]; static int np;
static char M[512][80]; static int nm; 
static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
static int is_file(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISREG(s.st_mode); }
static int read_uid(const char *p, char *out, size_t cap) { out[0] = 0; FILE *f = fopen(p, "r"); if (!f) return 0; if (fgets(out, (int)cap, f)) out[strcspn(out, "\r\n \t")] = 0; fclose(f); size_t n = strlen(out); if (n < 8 || n > 64) return 0; for (size_t i = 0; i < n; i++) if (!((out[i] >= '0' && out[i] <= '9') || (out[i] >= 'a' && out[i] <= 'f'))) return 0; return 1; }
static int cmp_str(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
int main(int argc, char **argv) {
    if (argc != 2 || !is_dir(argv[1])) { fprintf(stderr, "Usage: piece_check <house>\n"); return 2; }
    const char *house = argv[1]; int errors = 0, warns = 0, ngames = 0, nmaster_bad = 0;
    char apps[PATH_MAX]; snprintf(apps, sizeof apps, "%.3000s/@.apps", house); DIR *d = opendir(apps); struct dirent *e; static char games[128][64]; int ng = 0;
    while (d && (e = readdir(d)) && ng < 128) { char t[PATH_MAX]; if (e->d_name[0] == '.') continue; snprintf(t, sizeof t, "%.3000s/%.60s/toy.pdl", apps, e->d_name); if (is_file(t)) snprintf(games[ng++], 64, "%.63s", e->d_name); }
    if (d) { closedir(d); }
    qsort(games, (size_t)ng, 64, cmp_str);
    for (int g = 0; g < ng; g++) {
        char sd[PATH_MAX]; snprintf(sd, sizeof sd, "%.3000s/%.63s/state", apps, games[g]); DIR *s = opendir(sd); if (!s) continue; ngames++; int files = 0; static char names[256][80]; int nn = 0;
        while ((e = readdir(s)) && nn < 256) { size_t n = strlen(e->d_name); if (n > 8 && !strncmp(e->d_name, "uid_", 4) && !strcmp(e->d_name + n - 4, ".txt")) snprintf(names[nn++], 80, "%.79s", e->d_name); }
        closedir(s); qsort(names, (size_t)nn, 80, cmp_str);
        for (int i = 0; i < nn && np < MAXP; i++) { char fp[PATH_MAX]; snprintf(fp, sizeof fp, "%.3000s/%.79s", sd, names[i]); Pet *q = &P[np++]; snprintf(q->game, 64, "%.63s", games[g]); snprintf(q->slug, 64, "%.*s", (int)strlen(names[i]) - 8 > 63 ? 63 : (int)strlen(names[i]) - 8, names[i] + 4); q->valid = read_uid(fp, q->uid, sizeof q->uid); files++; }
        char gi[PATH_MAX]; snprintf(gi, sizeof gi, "%.3000s/%.63s/.gitignore", apps, games[g]); FILE *f = fopen(gi, "r"); int ign = 0; char l[256];
        while (f && fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n ")] = 0; if (!strcmp(l, "state/") || !strcmp(l, "state") || !strcmp(l, "/state/")) ign = 1; } if (f) fclose(f);
        if (ign && files) { printf("WARN not_durable %s files=%d\n", games[g], files); warns++; }
    }
    char users[PATH_MAX]; snprintf(users, sizeof users, "%.3000s/xyzfs/users", house); d = opendir(users);
    while (d && (e = readdir(d))) { if (e->d_name[0] == '.') continue; char pd[PATH_MAX]; snprintf(pd, sizeof pd, "%.3000s/%.100s/home/livedesk/pals", users, e->d_name); DIR *pp = opendir(pd); struct dirent *x; if (!pp) continue;
        while ((x = readdir(pp))) { if (x->d_name[0] == '.') continue; char up[PATH_MAX], u[80]; snprintf(up, sizeof up, "%.3000s/%.100s/entity_uid.txt", pd, x->d_name); if (!is_dir(up) && nm < 512) { snprintf(M[nm++], 80, "%.79s", x->d_name); if (!read_uid(up, u, sizeof u)) { printf("WARN master_bad_uid %.60s/%.60s\n", e->d_name, x->d_name); warns++; nmaster_bad++; } } }
        closedir(pp); }
    if (d) { closedir(d); }
    for (int i = 0; i < np; i++) if (!P[i].valid) { printf("ERROR empty_uid %s %s\n", P[i].game, P[i].slug); errors++; }
    for (int i = 0; i < np; i++) for (int j = i + 1; j < np; j++) if (P[i].valid && P[j].valid && !strcmp(P[i].uid, P[j].uid) && !(!strcmp(P[i].game, P[j].game) && !strcmp(P[i].slug, P[j].slug))) { printf("ERROR dup_uid %.8s %s:%s %s:%s\n", P[i].uid, P[i].game, P[i].slug, P[j].game, P[j].slug); errors++; }
    for (int i = 0; i < np; i++) { int first = 1; for (int j = 0; j < i; j++) if (!strcmp(P[j].slug, P[i].slug)) first = 0; if (!first) continue;
        int games_n = 0, uids_n = 0; static char gs[64][64], us[64][80]; for (int j = 0; j < np; j++) if (!strcmp(P[j].slug, P[i].slug)) { int sg = 0, su = 0; for (int k = 0; k < games_n; k++) if (!strcmp(gs[k], P[j].game)) sg = 1; if (!sg && games_n < 64) snprintf(gs[games_n++], 64, "%.63s", P[j].game); for (int k = 0; k < uids_n; k++) if (!strcmp(us[k], P[j].valid ? P[j].uid : "")) su = 1; if (!su && uids_n < 64) snprintf(us[uids_n++], 80, "%.79s", P[j].valid ? P[j].uid : ""); }
        if (games_n >= 2 && uids_n >= 2) { printf("WARN name_collision %s games=%d uids=%d\n", P[i].slug, games_n, uids_n); warns++; } }
    for (int i = 0; i < np; i++) { int found = 0; size_t sl = strlen(P[i].slug); for (int k = 0; k < nm && !found; k++) { size_t ml = strlen(M[k]); if (!strcasecmp(M[k], P[i].slug) || (ml > sl + 1 && M[k][ml - sl - 1] == '_' && !strcasecmp(M[k] + ml - sl, P[i].slug))) found = 1; } if (!found) { printf("WARN no_master %s %s\n", P[i].game, P[i].slug); warns++; } }
    printf("piece_check: games=%d pets=%d master=%d errors=%d warnings=%d\n", ngames, np, nm, errors, warns); (void)nmaster_bad;
    return errors ? 1 : 0;
}
