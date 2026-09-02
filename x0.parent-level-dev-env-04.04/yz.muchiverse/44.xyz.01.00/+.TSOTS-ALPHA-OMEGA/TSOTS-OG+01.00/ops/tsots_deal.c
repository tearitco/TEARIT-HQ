/* tsots_deal - pick a fresh round of scrambled verses for TSOTS.
 * Reuses the exact bible-scanning logic from the original
 * 1.tsots-game-loc🤺️]Z77.c (which now reads the path from location.txt),
 * adapted to the house op shape (resolve_root via PRISC_PROJECT_ROOT,
 * kv state files under pieces/system/).
 *
 * Inputs:
 *   pieces/system/game_state.txt  (elo -> difficulty: verse count)
 *   location.txt                  (bible path, symlinked into the session)
 *   PRISC_PROJECT_ROOT env        (session dir)
 *
 * Outputs:
 *   pieces/system/round.txt       one line per display position:
 *                                 rank|chap:verse|stripped_text
 *   pieces/system/solution.txt    digits of the correct answer (rank+1
 *                                 per display position), no spaces
 *   game_state.txt updated: round++, status=playing, answer cleared
 *   bumps pieces/display/game_screen_changed.txt
 *
 * Usage: tsots_deal.+x
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define BIBLE_BEGIN 3000
#define BIBLE_END 100109
#define LINE_SIZE 512
#define MAX_VERSES 6
#define PATH_SIZE 1024
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define LOCATION_FILE "location.txt"

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[LINE_SIZE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[48][LINE_SIZE];
    int nlines = 0;
    if (f) {
        while (nlines < 48 && fgets(lines[nlines], LINE_SIZE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void bump_screen(void) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path),
             "%s/pieces/display/game_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static int is_blank(const char *str) {
    if (!str) return 1;
    while (*str) {
        if (!isspace((unsigned char)*str)) return 0;
        str++;
    }
    return 1;
}

/* A verse line looks like: NNNNN chap:verse text...
 * (5-digit line number, then chapter:verse, then the text). */
static int has_verse_reference(const char *line) {
    if (!line || !*line) return 0;
    const char *ptr = line;
    while (*ptr && isdigit((unsigned char)*ptr)) ptr++;
    if (*ptr != ' ' || !*(ptr + 1)) return 0;
    ptr++;
    if (!isdigit((unsigned char)*ptr)) return 0;
    while (*ptr && isdigit((unsigned char)*ptr)) ptr++;
    if (*ptr != ':' || !*(ptr + 1)) return 0;
    ptr++;
    if (!isdigit((unsigned char)*ptr)) return 0;
    while (*ptr && isdigit((unsigned char)*ptr)) ptr++;
    if (*ptr != ' ' || !*(ptr + 1)) return 0;
    return 1;
}

/* Split "NNNNN chap:verse text..." into ref ("chap:verse") and text. */
static void split_verse(const char *line, char *ref, size_t ref_sz, char *text, size_t text_sz) {
    const char *p = line;
    while (*p && isdigit((unsigned char)*p)) p++;
    if (*p == ' ') p++;
    const char *ref_start = p;
    while (*p && *p != ' ') p++;
    size_t ref_len = (size_t)(p - ref_start);
    if (ref_len >= ref_sz) ref_len = ref_sz - 1;
    memcpy(ref, ref_start, ref_len);
    ref[ref_len] = '\0';
    while (*p == ' ') p++;
    snprintf(text, text_sz, "%s", p);
}

/* Read the N lines starting at <line> that are verse references; write
 * their display rows (rank|ref|text) to out. Returns count found. */
static int collect_verses(FILE *bible, unsigned int start_line, int n,
                          char rows[MAX_VERSES][LINE_SIZE]) {
    if (fseek(bible, 0, SEEK_SET) != 0) return 0;
    unsigned int i;
    for (i = 0; i < start_line - 1; ++i) {
        char buffer[LINE_SIZE];
        if (!fgets(buffer, LINE_SIZE, bible)) return 0;
    }
    int count = 0;
    unsigned int current_line = start_line;
    while (count < n) {
        char buffer[LINE_SIZE] = {0};
        if (!fgets(buffer, LINE_SIZE, bible)) break;
        buffer[strcspn(buffer, "\n")] = '\0';
        current_line++;
        if (has_verse_reference(buffer)) {
            char ref[64], text[LINE_SIZE];
            split_verse(buffer, ref, sizeof(ref), text, sizeof(text));
            if (*text != '\0' && !is_blank(text)) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(rows[count], LINE_SIZE, "%s|%s", ref, text);
#pragma GCC diagnostic pop
                count++;
            }
        }
        if (current_line - start_line > 1000) break;
    }
    return count;
}

static int rand_between(int lo, int hi) {
    return lo + (rand() % (hi + 1 - lo));
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/game_state.txt", project_root);

    char elo_raw[32] = "1000";
    read_kv_str(state_path, "elo", elo_raw, sizeof(elo_raw));
    int elo = atoi(elo_raw);
    if (elo < 100) elo = 100;

    int n = 4;
    if (elo >= 1100) n++;
    if (elo >= 1300) n++;
    if (n > MAX_VERSES) n = MAX_VERSES;

    char loc_path[PATH_BUF];
    snprintf(loc_path, sizeof(loc_path), "%s/%s", project_root, LOCATION_FILE);
    char bible_path[PATH_SIZE];
    {
        FILE *loc = fopen(loc_path, "r");
        if (!loc) return 1;
        if (!fgets(bible_path, sizeof(bible_path), loc)) { fclose(loc); return 1; }
        fclose(loc);
        bible_path[strcspn(bible_path, "\r\n")] = '\0';
    }
    if (*bible_path == '\0') return 1;

    FILE *bible = fopen(bible_path, "r");
    if (!bible) return 1;
    struct stat st;
    if (fstat(fileno(bible), &st) != 0 || st.st_size == 0) {
        fclose(bible);
        return 1;
    }

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    char rows[MAX_VERSES][LINE_SIZE];
    int verse_count = 0;
    int retries = 0;
    const int MAX_RETRIES = 10;
    while (verse_count < n && retries < MAX_RETRIES) {
        unsigned int start_line = (unsigned int)rand_between(BIBLE_BEGIN, BIBLE_END);
        int got = collect_verses(bible, start_line, n, rows);
        if (got == n) {
            verse_count = got;
        } else {
            retries++;
            verse_count = 0;
        }
    }
    fclose(bible);
    if (verse_count < n) return 1;

    /* scrambled[d] = pick rank (0-based bible order) shown at display d */
    int scrambled[MAX_VERSES];
    for (int i = 0; i < verse_count; i++) scrambled[i] = i;
    for (int i = verse_count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = scrambled[i];
        scrambled[i] = scrambled[j];
        scrambled[j] = t;
    }

    char round_path[PATH_BUF], solution_path[PATH_BUF];
    snprintf(round_path, sizeof(round_path), "%s/pieces/system/round.txt", project_root);
    snprintf(solution_path, sizeof(solution_path), "%s/pieces/system/solution.txt", project_root);

    FILE *rf = fopen(round_path, "w");
    if (!rf) return 1;
    char solution[MAX_VERSES + 1];
    for (int d = 0; d < verse_count; d++) {
        fprintf(rf, "%d|%s\n", scrambled[d], rows[scrambled[d]]);
        solution[d] = (char)('1' + scrambled[d]);
    }
    solution[verse_count] = '\0';
    fclose(rf);

    FILE *sf = fopen(solution_path, "w");
    if (!sf) return 1;
    fputs(solution, sf);
    fclose(sf);

    char round_raw[32] = "0";
    read_kv_str(state_path, "round", round_raw, sizeof(round_raw));
    int round = atoi(round_raw) + 1;
    char v[32];
    snprintf(v, sizeof(v), "%d", round);
    write_kv(state_path, "round", v);
    write_kv(state_path, "status", "playing");
    write_kv(state_path, "answer", "");
    write_kv(state_path, "last_result", "none");

    bump_screen();
    return 0;
}
