#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LINE 256
#define MAX_POINTS 4000

/* chart_data.c - range-sliced daily-close history for the yahoo-chart widget.
 *
 * Reads data/research/<SYMBOL>.csv (rows "YYYY-MM-DD,close[,marker]") and
 * prints the most recent `range` worth of trading days, oldest first, one
 * "YYYY-MM-DD,close" row per line. When the series is empty or too short
 * (offline first run), backfills a deterministic simulated 10-year daily
 * series (same hash-seeded style as lookup_stock.c's offline quote) so the
 * chart always renders. The simulator never touches the network.
 *
 * Usage: chart_data.+x <symbol> <range>
 *   range: 10y 5y 2.5y 1y 1mo 1wk
 */

static int range_days(const char *range) {
    if (strcmp(range, "10y") == 0) return 3650;
    if (strcmp(range, "5y") == 0) return 1825;
    if (strcmp(range, "2.5y") == 0) return 912;
    if (strcmp(range, "1y") == 0) return 365;
    if (strcmp(range, "1mo") == 0) return 30;
    if (strcmp(range, "1wk") == 0) return 7;
    return 365;
}

static unsigned long sym_hash(const char *symbol) {
    unsigned long h = 0;
    for (const char *p = symbol; *p; p++) h = h * 33 + (unsigned char)toupper(*p);
    return h ? h : 1;
}

/* Skip weekends so the synthetic series looks like a trading calendar. */
static void prev_trading_day(struct tm *tm) {
    do {
        time_t t = mktime(tm);
        t -= 86400;
        localtime_r(&t, tm);
    } while (tm->tm_wday == 0 || tm->tm_wday == 6);
}

static int load_series(const char *path, char dates[MAX_POINTS][16], double closes[MAX_POINTS], int *count) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[MAX_LINE];
    *count = 0;
    while (fgets(line, sizeof(line), fp) && *count < MAX_POINTS) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0]) continue;
        char date[16];
        double close;
        if (sscanf(line, "%15[^,],%lf", date, &close) == 2) {
            if (date[0] != 'd' && strlen(date) >= 10) {
                strncpy(dates[*count], date, 15);
                closes[*count] = close;
                (*count)++;
            }
        }
    }
    fclose(fp);
    return *count;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: chart_data.+x <symbol> <range>\n");
        return 1;
    }
    char symbol[MAX_LINE];
    snprintf(symbol, sizeof(symbol), "%s", argv[1]);
    for (char *p = symbol; *p; p++) *p = toupper(*p);
    const char *range = argv[2];
    int days = range_days(range);

    char dates[MAX_POINTS][16] = {0};
    double closes[MAX_POINTS] = {0};
    int count = 0;
    char path[256];
    snprintf(path, sizeof(path), "data/research/%s.csv", symbol);
    int have_real = load_series(path, dates, closes, &count) > 0;

    if (!have_real) {
        /* Deterministic offline backfill: multiplicative walk seeded by the
         * symbol hash, one close per trading day back `days` from today. */
        unsigned long h = sym_hash(symbol);
        double base = 20.0 + (double)(h % 500) / 10.0;
        double price = base;
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        int walk[4000];
        for (int i = 0; i < days; i++) walk[i] = (int)((h >> (i % 24)) + i * 2654435761u) % 201;
        int idx = 0;
        while (idx < days) {
            prev_trading_day(&tm);
            double delta = (double)(walk[idx] - 100) / 100.0;
            price *= 1.0 + delta / 40.0;
            if (price < 0.05) price = 0.05;
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
            snprintf(dates[idx], 16, "%s", buf);
            closes[idx] = price;
            idx++;
        }
        /* Reverse oldest-first. */
        for (int i = 0, j = idx - 1; i < j; i++, j--) {
            char td[16];
            double tc;
            strncpy(td, dates[i], 15); tc = closes[i];
            strncpy(dates[i], dates[j], 15); closes[i] = closes[j];
            strncpy(dates[j], td, 15); closes[j] = tc;
        }
        count = idx;
    } else {
        /* Slice the real series to the requested range (most recent `days`
         * rows, oldest first). */
        int start = count - days;
        if (start < 0) start = 0;
        int keep = count - start;
        if (keep != count) {
            for (int i = 0; i < keep; i++) {
                strncpy(dates[i], dates[start + i], 15);
                closes[i] = closes[start + i];
            }
            count = keep;
        }
    }

    for (int i = 0; i < count; i++) {
        printf("%s,%.2f\n", dates[i], closes[i]);
    }
    return 0;
}
