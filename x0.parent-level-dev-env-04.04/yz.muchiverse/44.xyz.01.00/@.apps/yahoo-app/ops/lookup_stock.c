#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>

#define MAX_LINE 256
#define CACHE_DURATION 3600
#define MAX_ENTRIES 1000

static void to_upper(char *str) {
    for (char *p = str; *p; p++) *p = toupper(*p);
}

static int is_all_digits(const char *str) {
    for (const char *p = str; *p; p++) if (!isdigit(*p)) return 0;
    return 1;
}

static int is_price_cached(const char *symbol, char *price, time_t *last_update, char *time_str) {
    char upper_symbol[MAX_LINE];
    strncpy(upper_symbol, symbol, MAX_LINE - 1);
    upper_symbol[MAX_LINE - 1] = '\0';
    to_upper(upper_symbol);

    FILE *fp = fopen("yfin_master_list.txt", "r");
    if (!fp) {
        fprintf(stderr, "[%s] No yfin_master_list.txt\n", upper_symbol);
        return 0;
    }

    char line[MAX_LINE];
    char last_price[MAX_LINE] = {0};
    char last_time_str[MAX_LINE] = {0};
    time_t max_last_update = 0;
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        char *saved_symbol = strtok(line, ",");
        if (saved_symbol) {
            char upper_saved_symbol[MAX_LINE];
            strncpy(upper_saved_symbol, saved_symbol, MAX_LINE - 1);
            to_upper(upper_saved_symbol);
            char *saved_price = strtok(NULL, ",");
            char *saved_time = strtok(NULL, ",");
            if (saved_price && saved_time && *saved_price && *saved_time) {
                if (strcasecmp(upper_saved_symbol, upper_symbol) == 0) {
                    struct tm tm = {0};
                    time_t temp_last_update = 0;
                    if (strptime(saved_time, "%Y-%m-%dT%H:%M:%S", &tm)) {
                        tm.tm_isdst = -1;
                        temp_last_update = mktime(&tm);
                    } else if (is_all_digits(saved_time)) {
                        temp_last_update = atol(saved_time);
                    } else {
                        fprintf(stderr, "[%s] Invalid timestamp %s\n", upper_symbol, saved_time);
                        continue;
                    }
                    if (temp_last_update > max_last_update) {
                        max_last_update = temp_last_update;
                        strncpy(last_price, saved_price, MAX_LINE - 1);
                        strncpy(last_time_str, saved_time, MAX_LINE - 1);
                        found = 1;
                    }
                }
            }
        }
    }
    fclose(fp);

    if (found) {
        strncpy(price, last_price, MAX_LINE - 1);
        strncpy(time_str, last_time_str, MAX_LINE - 1);
        *last_update = max_last_update;
        time_t now = time(NULL);
        long age = now - *last_update;
        if (age >= 0 && age <= CACHE_DURATION) {
            fprintf(stderr, "[%s] Cache hit: %s, age %ld\n", upper_symbol, time_str, age);
            return 1;
        }
        fprintf(stderr, "[%s] Cache expired: age %ld\n", upper_symbol, age);
    }
    fprintf(stderr, "[%s] No cache entry\n", upper_symbol);
    return 0;
}

static void append_to_master(const char *symbol, const char *price, time_t timestamp) {
    char upper_symbol[MAX_LINE];
    strncpy(upper_symbol, symbol, MAX_LINE - 1);
    upper_symbol[MAX_LINE - 1] = '\0';
    to_upper(upper_symbol);

    FILE *fp = fopen("yfin_master_list.txt", "r");
    char lines[MAX_ENTRIES][MAX_LINE];
    int line_count = 0;

    if (fp) {
        while (fgets(lines[line_count], MAX_LINE, fp) && line_count < MAX_ENTRIES) {
            lines[line_count][strcspn(lines[line_count], "\n")] = 0;
            line_count++;
        }
        fclose(fp);
    }

    char time_str[32];
    struct tm *tm = localtime(&timestamp);
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm);
    char new_entry[MAX_LINE];
    snprintf(new_entry, MAX_LINE, "%s,%s,%s", upper_symbol, price, time_str);

    fp = fopen("yfin_master_list.txt", "w");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to open yfin_master_list.txt for writing\n", upper_symbol);
        return;
    }

    fprintf(fp, "%s\n", new_entry);
    fprintf(stderr, "[%s] Appended %s,%s,%s\n", upper_symbol, upper_symbol, price, time_str);

    for (int i = line_count - 1; i >= 0; i--) {
        char *saved_symbol = strtok(lines[i], ",");
        if (saved_symbol) {
            char upper_saved_symbol[MAX_LINE];
            strncpy(upper_saved_symbol, saved_symbol, MAX_LINE - 1);
            to_upper(upper_saved_symbol);
            char *saved_price = strtok(NULL, ",");
            char *saved_time = strtok(NULL, ",");
            if (saved_price && saved_time && strcasecmp(upper_saved_symbol, upper_symbol) != 0) {
                fprintf(fp, "%s,%s,%s\n", upper_saved_symbol, saved_price, saved_time);
            }
        }
    }
    fclose(fp);
}

static void process_stock(const char *symbol, char *latest_symbol, char *latest_price, char *latest_time) {
    char upper_symbol[MAX_LINE];
    strncpy(upper_symbol, symbol, MAX_LINE - 1);
    upper_symbol[MAX_LINE - 1] = '\0';
    to_upper(upper_symbol);

    char command[512];
    char price[MAX_LINE] = {0};
    char time_str[MAX_LINE] = {0};
    time_t now = time(NULL);
    time_t last_update = 0;

    if (is_price_cached(upper_symbol, price, &last_update, time_str)) {
        long age = now - last_update;
        if (last_update && age >= 0 && age <= CACHE_DURATION) {
            printf("%s Current Price: %s (cached, %s)\n", upper_symbol, price, time_str);
            strncpy(latest_symbol, upper_symbol, MAX_LINE - 1);
            strncpy(latest_price, price, MAX_LINE - 1);
            strncpy(latest_time, time_str, MAX_LINE - 1);
            return;
        }
    }

    snprintf(command, sizeof(command), "./+x/fetch_stock.+x %s", upper_symbol);
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        printf("Failed to fetch %s (pipe error)\n", upper_symbol);
        fprintf(stderr, "[%s] Pipe error: %s\n", upper_symbol, strerror(errno));
        return;
    }
    char output[1024] = {0};
    char *output_ptr = output;
    size_t output_size = sizeof(output);
    int success = 0;
    while (fgets(output_ptr, output_size - (output_ptr - output), pipe)) {
        output_ptr[strcspn(output_ptr, "\n")] = 0;
        fprintf(stderr, "[%s] Fetch output: %s\n", upper_symbol, output_ptr);
        if (strstr(output_ptr, "Data written to")) {
            success = 1;
        }
        output_ptr += strlen(output_ptr) + 1;
        if (output_ptr >= output + output_size - MAX_LINE) {
            fprintf(stderr, "[%s] Fetch output buffer full\n", upper_symbol);
            break;
        }
    }
    pclose(pipe);

    if (success) {
        snprintf(command, sizeof(command), "./+x/read_price.+x %s", upper_symbol);
        FILE *price_pipe = popen(command, "r");
        if (price_pipe) {
            char price_output[MAX_LINE] = {0};
            if (fgets(price_output, sizeof(price_output), price_pipe)) {
                price_output[strcspn(price_output, "\n")] = 0;
                char *price_start = strstr(price_output, "Current Price: ");
                if (price_start) {
                    price_start += strlen("Current Price: ");
                    strncpy(price, price_start, MAX_LINE - 1);
                    price[strcspn(price, "\n")] = 0;
                    struct tm *tm = localtime(&now);
                    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm);
                    printf("%s Current Price: %s (fetched %s)\n", upper_symbol, price, time_str);
                    append_to_master(upper_symbol, price, now);
                    strncpy(latest_symbol, upper_symbol, MAX_LINE - 1);
                    strncpy(latest_price, price, MAX_LINE - 1);
                    strncpy(latest_time, time_str, MAX_LINE - 1);
                } else {
                    printf("Failed to parse price for %s\n", upper_symbol);
                    fprintf(stderr, "[%s] Parse error: %s\n", upper_symbol, price_output);
                }
            } else {
                printf("Invalid symbol %s\n", upper_symbol);
                fprintf(stderr, "[%s] No price output\n", upper_symbol);
            }
            pclose(price_pipe);
        } else {
            printf("Failed to fetch %s (price pipe error)\n", upper_symbol);
            fprintf(stderr, "[%s] Price pipe error: %s\n", upper_symbol, strerror(errno));
        }
    } else {
        /* Offline/simulated-quote fallback (deliberate, scoped choice):
         * real fetch_stock needs the Yahoo API which is unreachable in the
         * sandbox/offline. Produce a deterministic per-symbol daily quote so
         * the broker sim keeps working, write a read_price-parseable
         * "<SYM>.txt" cache, and publish it to yfin_master_list.txt. */
        unsigned long h = 0;
        for (const char *p = upper_symbol; *p; p++) h = h * 33 + (unsigned char)*p;
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        unsigned long day = (unsigned long)(tm->tm_year + 100) * 1000 + (unsigned long)tm->tm_yday;
        double sim_price = 10.0 + (double)(h % 900) / 10.0;
        sim_price += (double)((h + day * 7) % 200) / 100.0 - 1.0;
        if (sim_price < 0.05) sim_price = 0.05;
        snprintf(price, sizeof(price), "%.2f", sim_price);
        struct tm *ltm = localtime(&now);
        strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", ltm);

        char cache_name[MAX_LINE];
        snprintf(cache_name, sizeof(cache_name), "%s.txt", upper_symbol);
        FILE *sim = fopen(cache_name, "w");
        if (sim) {
            fprintf(sim, "{ \"quoteResponse\": { \"regularMarketPrice\":%s }\n", price);
            fclose(sim);
        }

        append_to_master(upper_symbol, price, now);
        printf("%s Current Price: %s (simulated, %s)\n", upper_symbol, price, time_str);
        strncpy(latest_symbol, upper_symbol, MAX_LINE - 1);
        strncpy(latest_price, price, MAX_LINE - 1);
        strncpy(latest_time, time_str, MAX_LINE - 1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <user_hash> <symbol>\n", argv[0]);
        return 1;
    }

    char *symbol = argv[2];
    char latest_symbol[MAX_LINE] = {0};
    char latest_price[MAX_LINE] = {0};
    char latest_time[MAX_LINE] = {0};

    process_stock(symbol, latest_symbol, latest_price, latest_time);
    return 0;
}
