/* wsr_news_op - real port of Mar$.$treetRace.wsr]Q]k32/news_loop.c's
 * genuinely-working mechanic: scan every corp's latest price move,
 * sort by biggest |change%| first, write a real "FINANCIAL NEWS
 * HEADLINES" report - this is the exact thing the real game.c's main
 * menu banner scrolls through (confirmed by reading game.c directly),
 * and the one real system in the original source that hadn't been
 * ported yet. Reads corp_update_price.+x's own price_history.txt
 * output (last line per corp) rather than recomputing the change -
 * corp_update_price already did that math once, no need to redo it.
 *
 * Dynamic discovery over every corp_-prefixed piece directory (same
 * shape tick_all.sh already uses) - no hardcoded ticker list.
 *
 * Self-contained, no shared headers. Usage: wsr_news_op.+x (no args -
 * operates over the whole project, like the real news_loop.c's main()) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#define access _access
#define F_OK 0
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_CORPS 128

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
#ifdef _WIN32
    if (access("pieces", F_OK) == 0 && access("projects", F_OK) == 0) {
        snprintf(project_root, sizeof(project_root), ".");
        return;
    }
#endif
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_state_field(const char *state_path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(state_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

typedef struct {
    char ticker[64];
    float price;
    float change_percent;
} CorpNews;

/* Reads the LAST line of a corp's price_history.txt - format is
 * turn_number,old_price,new_price,change_percent% (written by
 * corp_update_price.+x). Returns 1 if a line was found. */
static int get_last_change(const char *hist_path, float *out_change) {
    FILE *f = fopen(hist_path, "r");
    if (!f) return 0;
    char line[MAX_LINE], last[MAX_LINE] = "";
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        strncpy(last, line, sizeof(last) - 1);
        last[sizeof(last) - 1] = '\0';
        found = 1;
    }
    fclose(f);
    if (!found) return 0;
    char *third_comma = strrchr(last, ',');
    if (!third_comma) return 0;
    *out_change = atof(third_comma + 1);
    return 1;
}

int main(void) {
    resolve_root();

    char pieces_dir[PATH_BUF];
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/projects/wsr-pal/pieces", project_root);

    CorpNews news[MAX_CORPS];
    int count = 0;

#ifdef _WIN32
    {
        char pattern[PATH_BUF];
        snprintf(pattern, sizeof(pattern), "%s/*", pieces_dir);
        for (char *p = pattern; *p; p++) if (*p == '/') *p = '\\';
        struct _finddata_t fi;
        intptr_t h = _findfirst(pattern, &fi);
        if (h == -1) return 1;
        do {
            if (strncmp(fi.name, "corp_", 5) != 0) continue;
            if (count >= MAX_CORPS) break;

            char state_path[PATH_BUF], hist_path[PATH_BUF];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", pieces_dir, fi.name);
            snprintf(hist_path, sizeof(hist_path), "%s/%s/price_history.txt", pieces_dir, fi.name);

            char price_str[MAX_LINE];
            read_state_field(state_path, "stock_price", price_str, sizeof(price_str));
            if (!price_str[0]) continue;

            float change = 0.0f;
            if (!get_last_change(hist_path, &change)) continue;

            snprintf(news[count].ticker, sizeof(news[count].ticker), "%s", fi.name + 5);
            news[count].price = atof(price_str);
            news[count].change_percent = change;
            count++;
        } while (_findnext(h, &fi) == 0);
        _findclose(h);
    }
#else
    DIR *d = opendir(pieces_dir);
    if (!d) return 1;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < MAX_CORPS) {
        if (strncmp(entry->d_name, "corp_", 5) != 0) continue;

        char state_path[PATH_BUF], hist_path[PATH_BUF];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", pieces_dir, entry->d_name);
        snprintf(hist_path, sizeof(hist_path), "%s/%s/price_history.txt", pieces_dir, entry->d_name);

        char price_str[MAX_LINE];
        read_state_field(state_path, "stock_price", price_str, sizeof(price_str));
        if (!price_str[0]) continue;

        float change = 0.0f;
        if (!get_last_change(hist_path, &change)) continue; /* no history yet - nothing to report */

        snprintf(news[count].ticker, sizeof(news[count].ticker), "%s", entry->d_name + 5); /* strip "corp_" */
        news[count].price = atof(price_str);
        news[count].change_percent = change;
        count++;
    }
    closedir(d);
#endif

    /* Sort by |change%| descending - same bubble sort shape as the
     * real news_loop.c's write_news_to_file(), small N so it's fine. */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - 1 - i; j++) {
            if (fabsf(news[j].change_percent) < fabsf(news[j + 1].change_percent)) {
                CorpNews tmp = news[j];
                news[j] = news[j + 1];
                news[j + 1] = tmp;
            }
        }
    }

    char news_path[PATH_BUF];
    snprintf(news_path, sizeof(news_path), "%s/projects/wsr-pal/pieces/wsr_menu/news.txt", project_root);
    FILE *out = fopen(news_path, "w");
    if (!out) return 1;

    fprintf(out, "FINANCIAL NEWS HEADLINES\n");
    fprintf(out, "========================\n");
    for (int i = 0; i < count && i < 20; i++) {
        if (news[i].change_percent == 0.0f) continue;
        char sign = (news[i].change_percent >= 0) ? '+' : '-';
        fprintf(out, "%s: $%.2f (%c%.2f%%)\n", news[i].ticker, news[i].price,
                sign, (sign == '+') ? news[i].change_percent : -news[i].change_percent);
    }
    fclose(out);

    return 0;
}
