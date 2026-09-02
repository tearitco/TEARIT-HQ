#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#define MAX_LINE 256
#define MAX_SYMS 10000

/* research_refresh.c - batch-cycle daily refresh of the research universe.
 *
 * Reads data/research/universe.txt (one uppercase symbol per line), advances
 * the batch-cycle pointer in data/research/cycle.txt, and appends today's
 * daily close to data/research/<SYMBOL>.csv for each symbol in the batch.
 *
 * Policy (see data/research/research.pdl):
 *   - daily cadence  : a run updates cycle.txt's position; the same date is
 *                      not re-fetched unless --force.
 *   - rate limit     : live Yahoo requests are spaced `min_delay_between`
 *                      seconds apart; simulated rows use no delay.
 *   - backoff        : a failed live fetch is retried with doubling delay
 *                      (base 2s, x2 per attempt, max 64s, 3 attempts) before
 *                      falling back to a deterministic simulated quote.
 *   - history        : daily closes only; series rows are deduped by date.
 *
 * Usage: research_refresh.+x [--batch N] [--sleep N] [--force]
 */

static void to_upper(char *str) {
    for (char *p = str; *p; p++) *p = toupper(*p);
}

static int load_universe(const char *path, char syms[MAX_SYMS][MAX_LINE], int *count) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[research_refresh] No universe file: %s (%s)\n", path, strerror(errno));
        return -1;
    }
    char line[MAX_LINE];
    *count = 0;
    while (fgets(line, sizeof(line), fp) && *count < MAX_SYMS) {
        line[strcspn(line, "\r\n")] = 0;
        char *tok = strtok(line, " \t");
        if (tok && *tok) {
            strncpy(syms[*count], tok, MAX_LINE - 1);
            syms[*count][MAX_LINE - 1] = 0;
            to_upper(syms[*count]);
            (*count)++;
        }
    }
    fclose(fp);
    return *count;
}

static int today_str(char *buf, size_t n) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    return strftime(buf, n, "%Y-%m-%d", tm) > 0 ? 0 : -1;
}

static int load_cycle(const char *path, int *position, char *last_date, size_t n, int *batch_size) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        char key[MAX_LINE], val[MAX_LINE];
        if (sscanf(line, "%[^=]=%s", key, val) == 2) {
            if (strcmp(key, "position") == 0) *position = atoi(val);
            else if (strcmp(key, "last_run_date") == 0) strncpy(last_date, val, n - 1);
            else if (strcmp(key, "batch_size") == 0) *batch_size = atoi(val);
        }
    }
    fclose(fp);
    return 0;
}

static void save_cycle(const char *path, int universe_size, int position, int batch_size, const char *date) {
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    fprintf(fp, "universe=%d\n", universe_size);
    fprintf(fp, "batch_size=%d\n", batch_size);
    fprintf(fp, "position=%d\n", position);
    fprintf(fp, "last_run_date=%s\n", date);
    fclose(fp);
}

/* Deterministic per-symbol daily quote, same formula as lookup_stock.c's
 * offline fallback so prices are consistent across the app. */
static double simulated_close(const char *symbol) {
    unsigned long h = 0;
    for (const char *p = symbol; *p; p++) h = h * 33 + (unsigned char)*p;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    unsigned long day = (unsigned long)(tm->tm_year + 100) * 1000 + (unsigned long)tm->tm_yday;
    double sim_price = 10.0 + (double)(h % 900) / 10.0;
    sim_price += (double)((h + day * 7) % 200) / 100.0 - 1.0;
    if (sim_price < 0.05) sim_price = 0.05;
    return sim_price;
}

/* Try a live quote: cache (yfin_master_list) -> fetch_stock -> read_price.
 * Returns 1 on a real price, 0 on offline fallback. On success the close is
 * in `price`; the "(simulated)" marker is set by the caller only when 0. */
static int fetch_live_price(const char *symbol, char *price, size_t price_sz,
                            int min_delay, int max_attempts) {
    char command[1024];
    char cache_price[MAX_LINE] = {0};
    char cache_time[MAX_LINE] = {0};
    int cache_hit = 0;

    snprintf(command, sizeof(command),
             "grep -i '^%s,' yfin_master_list.txt | head -1", symbol);
    FILE *cfp = popen(command, "r");
    if (cfp) {
        char line[MAX_LINE];
        if (fgets(line, sizeof(line), cfp)) {
            line[strcspn(line, "\r\n")] = 0;
            char s[MAX_LINE], p[MAX_LINE], t[MAX_LINE];
            if (sscanf(line, "%[^,],%[^,],%s", s, p, t) == 3) {
                struct tm tm = {0};
                time_t ts = 0;
                if (strptime(t, "%Y-%m-%dT%H:%M:%S", &tm)) {
                    tm.tm_isdst = -1;
                    ts = mktime(&tm);
                } else {
                    ts = atol(t);
                }
                time_t now = time(NULL);
                if (ts > 0 && now - ts < 86400) {
                    strncpy(cache_price, p, sizeof(cache_price) - 1);
                    strncpy(cache_time, t, sizeof(cache_time) - 1);
                    cache_hit = 1;
                }
            }
        }
        pclose(cfp);
    }
    if (cache_hit) {
        snprintf(price, price_sz, "%s", cache_price);
        fprintf(stderr, "[%s] research: cached close %s @ %s\n", symbol, cache_price, cache_time);
        return 1;
    }

    int attempt;
    for (attempt = 0; attempt < max_attempts; attempt++) {
        if (min_delay > 0 && attempt == 0) sleep(min_delay);
        snprintf(command, sizeof(command), "./+x/fetch_stock.+x %s 2>/dev/null | grep -q 'Data written'", symbol);
        if (system(command) == 0) {
            snprintf(command, sizeof(command), "./+x/read_price.+x %s", symbol);
            FILE *pipe = popen(command, "r");
            if (pipe) {
                char out[MAX_LINE];
                if (fgets(out, sizeof(out), pipe)) {
                    out[strcspn(out, "\r\n")] = 0;
                    char *st = strstr(out, "Current Price: ");
                    if (st) {
                        st += strlen("Current Price: ");
                        snprintf(price, price_sz, "%s", st);
                        pclose(pipe);
                        fprintf(stderr, "[%s] research: live close %s\n", symbol, price);
                        return 1;
                    }
                }
                pclose(pipe);
            }
        }
        if (attempt < max_attempts - 1) {
            int delay = 2;
            for (int i = 0; i < attempt; i++) delay *= 2;
            if (delay > 64) delay = 64;
            fprintf(stderr, "[%s] research: fetch failed, backing off %ds\n", symbol, delay);
            sleep(delay);
        }
    }
    return 0;
}

static int series_has_date(const char *path, const char *date) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strncmp(line, date, 10) == 0) { found = 1; break; }
    }
    fclose(fp);
    return found;
}

static void append_close(const char *symbol, const char *date, double close, int simulated) {
    char path[256];
    snprintf(path, sizeof(path), "data/research/%s.csv", symbol);
    if (series_has_date(path, date)) return;
    FILE *fp = fopen(path, "a");
    if (!fp) {
        fprintf(stderr, "[%s] research: cannot open series %s (%s)\n", symbol, path, strerror(errno));
        return;
    }
    fprintf(fp, "%s,%.2f%s\n", date, close, simulated ? ",(simulated)" : "");
    fclose(fp);
}

int main(int argc, char *argv[]) {
    int batch_size = 25;
    int min_delay = 2;
    int force = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--batch") == 0 && i + 1 < argc) batch_size = atoi(argv[++i]);
        else if (strcmp(argv[i], "--sleep") == 0 && i + 1 < argc) min_delay = atoi(argv[++i]);
        else if (strcmp(argv[i], "--force") == 0) force = 1;
    }

    char syms[MAX_SYMS][MAX_LINE] = {0};
    int universe_size = 0;
    if (load_universe("data/research/universe.txt", syms, &universe_size) < 0) return 1;
    if (universe_size == 0) {
        fprintf(stderr, "[research_refresh] universe is empty\n");
        return 1;
    }

    int position = 0;
    int state_batch = batch_size;
    char last_date[32] = "";
    load_cycle("data/research/cycle.txt", &position, last_date, sizeof(last_date), &state_batch);

    char today[32];
    today_str(today, sizeof(today));
    if (!force && strcmp(last_date, today) == 0) {
        fprintf(stderr, "[research_refresh] already ran today (%s); use --force to rerun\n", today);
        return 0;
    }

    if (position < 0 || position >= universe_size) position = 0;
    if (batch_size < 1) batch_size = 25;

    int done = 0;
    fprintf(stderr, "[research_refresh] batch: %d symbols from %d of %d\n", batch_size, position, universe_size);
    for (int i = 0; i < batch_size && done < batch_size; i++) {
        int idx = (position + i) % universe_size;
        char price[MAX_LINE] = {0};
        int simulated = 0;
        if (!fetch_live_price(syms[idx], price, sizeof(price), min_delay, 3)) {
            simulated = 1;
            snprintf(price, sizeof(price), "%.2f", simulated_close(syms[idx]));
            fprintf(stderr, "[%s] research: simulated close %s\n", syms[idx], price);
        }
        append_close(syms[idx], today, atof(price), simulated);
        done++;
    }

    save_cycle("data/research/cycle.txt", universe_size, (position + done) % universe_size, batch_size, today);
    fprintf(stderr, "[research_refresh] done: %d symbols refreshed, next position %d\n",
            done, (position + done) % universe_size);
    printf("research_refresh: %d symbols updated for %s\n", done, today);
    return 0;
}
