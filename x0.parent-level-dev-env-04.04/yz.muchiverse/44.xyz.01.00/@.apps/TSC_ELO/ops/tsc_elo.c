/* tsc_elo - load/update ELO ratings for TSC_ELO players. Ratings live in
 * each player's xyzfs location, NOT in the game project (design doc
 * §6.5): <house>/<current_xyzfs>/home/games/tsc_ratings.txt, resolved via
 * the exact same house_root.txt -> current_login.txt chain as
 * ledger_append.c. When no user is logged in (current_xyzfs empty), we
 * fall back to the first real xyzfs user tree so the game still works
 * during development; if none exists we degrade to a fresh 1000 (no
 * persistence).
 *
 * File format (one line per player, pipe-delimited):
 *   player_name|rating|wins|losses|draws|games_played
 * New players start 1000 vs 1000, chess-style (K=32). Tier names
 * (Padawan -> GM) are cosmetic badges derived from the number - the
 * number does all the work.
 *
 * Subcommands:
 *   tsc_elo resolve                 print the resolved ratings file path
 *   tsc_elo get <name>              print the player's rating (default 1000)
 *   tsc_elo update <name> <opp> <r> compute + persist both ratings after a
 *                                    match; r = win | loss | draw (from
 *                                    <name>'s point of view)
 *
 * Self-contained, no shared headers. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_PLAYERS 256
#define K_FACTOR 32.0

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static int ratings_path(char *out, size_t out_sz) {
    char hr_path[PATH_BUF], house_root[MAX_PATH];
    snprintf(hr_path, sizeof(hr_path), "%s/pieces/system/house_root.txt", project_root);
    FILE *f = fopen(hr_path, "r");
    if (!f) return 0;
    if (!fgets(house_root, sizeof(house_root), f)) { fclose(f); return 0; }
    fclose(f);
    house_root[strcspn(house_root, "\r\n")] = '\0';
    if (!house_root[0]) return 0;

    char login_path[PATH_BUF], xyzfs[MAX_PATH] = "";
    snprintf(login_path, sizeof(login_path), "%s/0.user-pal👤️/00.login-signup/current_login.txt", house_root);
    read_kv_str(login_path, "current_xyzfs", xyzfs, sizeof(xyzfs));

    if (!xyzfs[0]) {
        /* No active login: fall back to the most recent real xyzfs user. */
        char users_dir[PATH_BUF];
        snprintf(users_dir, sizeof(users_dir),
                 "%s/xyzfs/users", house_root);
        DIR *d = opendir(users_dir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.') continue;
                char cand[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(cand, sizeof(cand), "%s/%s", users_dir, e->d_name);
                struct stat st;
                if (stat(cand, &st) == 0 && S_ISDIR(st.st_mode)) {
                    char home[PATH_BUF];
                    snprintf(home, sizeof(home), "%s/%s/home", users_dir, e->d_name);
#pragma GCC diagnostic pop
                    if (access(home, F_OK) == 0) {
                        snprintf(xyzfs, sizeof(xyzfs), "xyzfs/users/%s", e->d_name);
                        break;
                    }
                }
            }
            closedir(d);
        }
    }

    if (!xyzfs[0]) return 0;

    snprintf(out, out_sz, "%s/%s/home/games/tsc_ratings.txt", house_root, xyzfs);
    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/%s/home/games", house_root, xyzfs);
    char mkcmd[PATH_BUF + 32];
    snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s'", dir);
    { int _rc = system(mkcmd); (void)_rc; }
    return 1;
}

typedef struct {
    char name[MAX_LINE];
    int rating;
    int wins;
    int losses;
    int draws;
    int games;
} Player;

static int find_player(Player *players, int n, const char *name) {
    for (int i = 0; i < n; i++) {
        if (strcmp(players[i].name, name) == 0) return i;
    }
    return -1;
}

static int load_players(const char *path, Player *players, int max) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        *p1 = '\0';
        char *name = line;
        char *rest = p1 + 1;
        snprintf(players[n].name, sizeof(players[n].name), "%s", name);
        char *tok = strtok(rest, "|");
        players[n].rating = tok ? atoi(tok) : 1000;
        players[n].wins = 0; players[n].losses = 0;
        players[n].draws = 0; players[n].games = 0;
        int field = 1;
        tok = strtok(NULL, "|");
        while (tok && field < 5) {
            if (field == 1) players[n].wins = atoi(tok);
            else if (field == 2) players[n].losses = atoi(tok);
            else if (field == 3) players[n].draws = atoi(tok);
            else if (field == 4) players[n].games = atoi(tok);
            tok = strtok(NULL, "|");
            field++;
        }
        n++;
    }
    fclose(f);
    return n;
}

static void save_players(const char *path, Player *players, int n) {
    char tmp[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
#pragma GCC diagnostic pop
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) {
        fprintf(f, "%s|%d|%d|%d|%d|%d\n",
                players[i].name, players[i].rating,
                players[i].wins, players[i].losses,
                players[i].draws, players[i].games);
    }
    fclose(f);
    rename(tmp, path);
}

static double expected_score(int ra, int rb) {
    return 1.0 / (1.0 + pow(10.0, (double)(rb - ra) / 400.0));
}

int main(int argc, char **argv) {
    resolve_root();
    if (argc < 2) {
        fprintf(stderr, "Usage: tsc_elo resolve | get <name> | update <name> <opp> <win|loss|draw>\n");
        return 1;
    }

    char path[PATH_BUF];
    int has_path = ratings_path(path, sizeof(path));

    if (strcmp(argv[1], "resolve") == 0) {
        printf("%s\n", has_path ? path : "(no xyzfs user found - ratings not persisted)");
        return 0;
    }

    if (!has_path) {
        fprintf(stderr, "tsc_elo: no resolvable ratings file\n");
        return 1;
    }

    Player players[MAX_PLAYERS];
    int n = load_players(path, players, MAX_PLAYERS);

    if (strcmp(argv[1], "get") == 0 && argc >= 3) {
        int idx = find_player(players, n, argv[2]);
        printf("%d\n", idx >= 0 ? players[idx].rating : 1000);
        return 0;
    }

    if (strcmp(argv[1], "update") == 0 && argc >= 5) {
        const char *name = argv[2];
        const char *opp = argv[3];
        const char *result = argv[4];

        int ia = find_player(players, n, name);
        int ib = find_player(players, n, opp);
        if (ia < 0) {
            ia = n++;
            snprintf(players[ia].name, sizeof(players[ia].name), "%s", name);
            players[ia].rating = 1000;
            players[ia].wins = 0; players[ia].losses = 0;
            players[ia].draws = 0; players[ia].games = 0;
        }
        if (ib < 0) {
            ib = n++;
            snprintf(players[ib].name, sizeof(players[ib].name), "%s", opp);
            players[ib].rating = 1000;
            players[ib].wins = 0; players[ib].losses = 0;
            players[ib].draws = 0; players[ib].games = 0;
        }

        double sa, sb;
        if (strcmp(result, "win") == 0) { sa = 1.0; sb = 0.0; }
        else if (strcmp(result, "loss") == 0) { sa = 0.0; sb = 1.0; }
        else { sa = 0.5; sb = 0.5; }

        double ea = expected_score(players[ia].rating, players[ib].rating);
        double eb = expected_score(players[ib].rating, players[ia].rating);

        int old_a = players[ia].rating;
        int old_b = players[ib].rating;
        players[ia].rating = (int)(old_a + K_FACTOR * (sa - ea));
        players[ib].rating = (int)(old_b + K_FACTOR * (sb - eb));

        players[ia].games++;
        players[ib].games++;
        if (sa == 1.0) players[ia].wins++;
        else if (sa == 0.0) players[ia].losses++;
        else players[ia].draws++;
        if (sb == 1.0) players[ib].wins++;
        else if (sb == 0.0) players[ib].losses++;
        else players[ib].draws++;

        save_players(path, players, n);
        printf("%s:%d->%d %s:%d->%d\n",
               players[ia].name, old_a, players[ia].rating,
               players[ib].name, old_b, players[ib].rating);
        return 0;
    }

    fprintf(stderr, "Usage: tsc_elo resolve | get <name> | update <name> <opp> <win|loss|draw>\n");
    return 1;
}
