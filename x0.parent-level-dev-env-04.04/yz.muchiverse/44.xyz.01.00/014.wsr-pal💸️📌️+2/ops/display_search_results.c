/* display_search_results - database search filter using criteria from
 * wsr_search_menu/state.txt. Scans corporations, applies filters. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
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
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    resolve_root();
    (void)argc; (void)argv;

    char search_state[PATH_BUF];
    snprintf(search_state, sizeof(search_state), "%s/projects/wsr-pal/pieces/wsr_search_menu/state.txt", project_root);

    char buf[64];
    read_state_field(search_state, "min_market_cap", buf, sizeof(buf));
    int min_market_cap = atoi(buf);

    read_state_field(search_state, "max_market_cap", buf, sizeof(buf));
    int max_market_cap = atoi(buf);

    read_state_field(search_state, "price_pct_book", buf, sizeof(buf));
    int price_pct_book = atoi(buf);

    read_state_field(search_state, "publicly_traded", buf, sizeof(buf));
    int publicly_traded = atoi(buf);

    read_state_field(search_state, "has_bonds", buf, sizeof(buf));
    int has_bonds = atoi(buf);

    printf("DATABASE SEARCH RESULTS (Criteria: Market Cap %d-%dM, Price<%d%% of Book)\n",
           min_market_cap, max_market_cap, price_pct_book);
    printf("%-8s  Price    BookVal  P/E    ROE    MktCap\n", "Ticker");
    printf("--------  -------  -------  ----   ----   ------\n");

    char corps_path[PATH_BUF];
    snprintf(corps_path, sizeof(corps_path), "%s/projects/wsr-pal/pieces", project_root);

    DIR *d = opendir(corps_path);
    if (!d) {
        printf("No corporations found.\n");
        return 0;
    }

    int results = 0;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (strncmp(entry->d_name, "corp_", 5) != 0) continue;

        const char *ticker = entry->d_name + 5;
        char corp_state[PATH_BUF];
        snprintf(corp_state, sizeof(corp_state), "%s/%s/state.txt", corps_path, entry->d_name);

        read_state_field(corp_state, "stock_price", buf, sizeof(buf));
        float price = atof(buf);
        if (price <= 0) continue;

        read_state_field(corp_state, "book_value", buf, sizeof(buf));
        float book_value = atof(buf);

        read_state_field(corp_state, "market_cap", buf, sizeof(buf));
        float market_cap = atof(buf);

        if (market_cap < min_market_cap || market_cap > max_market_cap) continue;

        float price_pct = (book_value > 0) ? (price * 100.0f / book_value) : 100.0f;
        if (price_pct > price_pct_book) continue;

        printf("%-8s  $%-6.2f  $%-6.2f  %3d    %3d    %.0fM\n",
               ticker, price, book_value, (int)(price/book_value * 100), 0, market_cap);
        results++;

        if (results >= 20) break;
    }
    closedir(d);

    printf("Results: %d stocks match criteria\n", results);
    return 0;
}
