#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_LINE 8192
#define MAX_POINTS 10000

static void to_upper(char *str) {
    for (char *p = str; *p; p++) *p = toupper(*p);
}

// Simple JSON parser to extract timestamps and prices
static int parse_json(const char *filename, long long *timestamps, double *prices, int *count, int max_count) {
    fprintf(stderr, "Debug: Parsing %s\n", filename);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        // Try lowercase
        char lower_filename[64];
        strncpy(lower_filename, filename, sizeof(lower_filename));
        for (char *p = lower_filename; *p; p++) *p = tolower(*p);
        fp = fopen(lower_filename, "r");
        if (!fp) {
            fprintf(stderr, "[%s] Failed to open: %s\n", filename, strerror(errno));
            return 1;
        }
    }

    char *buffer = malloc(1);
    if (!buffer) {
        fprintf(stderr, "[%s] Memory allocation failed\n", filename);
        fclose(fp);
        return 1;
    }
    buffer[0] = '\0';
    size_t buffer_size = 0;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        size_t line_len = strlen(line);
        buffer_size += line_len;
        char *temp = realloc(buffer, buffer_size + 1);
        if (!temp) {
            free(buffer);
            fprintf(stderr, "[%s] Memory allocation failed\n", filename);
            fclose(fp);
            return 1;
        }
        buffer = temp;
        strcat(buffer, line);
    }
    fclose(fp);

    fprintf(stderr, "Debug: buffer_size=%zu, buffer_start=%.20s\n", buffer_size, buffer_size > 0 ? buffer : "(empty)");

    if (buffer_size == 0) {
        fprintf(stderr, "[%s] Empty file\n", filename);
        free(buffer);
        return 1;
    }

    char *ptr = strstr(buffer, "\"timestamp\":");
    if (!ptr) {
        fprintf(stderr, "[%s] No timestamp data found\n", filename);
        free(buffer);
        return 1;
    }
    ptr = strchr(ptr, '[');
    if (!ptr) {
        fprintf(stderr, "[%s] Invalid timestamp array\n", filename);
        free(buffer);
        return 1;
    }
    ptr++;

    char *close_ptr = strstr(buffer, "\"close\":");
    if (!close_ptr) {
        fprintf(stderr, "[%s] No close price data found\n", filename);
        free(buffer);
        return 1;
    }
    close_ptr = strchr(close_ptr, '[');
    if (!close_ptr) {
        fprintf(stderr, "[%s] Invalid close price array\n", filename);
        free(buffer);
        return 1;
    }
    close_ptr++;

    *count = 0;
    while (*ptr != ']' && *close_ptr != ']' && *count < max_count) {
        // Skip whitespace
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n') ptr++;
        while (*close_ptr == ' ' || *close_ptr == '\t' || *close_ptr == '\n') close_ptr++;

        char *end;
        long long timestamp = strtoll(ptr, &end, 10);
        if (end == ptr || timestamp < 1000000000) {
            fprintf(stderr, "[%s] Invalid timestamp at pos %ld\n", filename, (long)(end - buffer));
            free(buffer);
            return 1;
        }

        double price = strtod(close_ptr, &end);
        if (end == close_ptr || price <= 0.0) {
            fprintf(stderr, "[%s] Invalid price at pos %ld\n", filename, (long)(end - buffer));
            free(buffer);
            return 1;
        }

        timestamps[*count] = timestamp;
        prices[*count] = price;
        (*count)++;
        fprintf(stderr, "Debug: Parsed timestamp=%lld, price=%.2f\n", timestamp, price);

        // Advance pointers
        ptr = strchr(end, ',');
        if (!ptr && *end != ']') {
            fprintf(stderr, "[%s] Expected comma or end at pos %ld\n", filename, (long)(end - buffer));
            free(buffer);
            return 1;
        }
        ptr = ptr ? ptr + 1 : end;

        close_ptr = strchr(end, ',');
        if (!close_ptr && *end != ']') {
            fprintf(stderr, "[%s] Expected comma or end at pos %ld\n", filename, (long)(end - buffer));
            free(buffer);
            return 1;
        }
        close_ptr = close_ptr ? close_ptr + 1 : end;
    }

    free(buffer);
    return *count < 2 ? 1 : 0;
}

// Aggregate minute data to daily closes (last price per day)
static void aggregate_daily(long long *timestamps, double *prices, int count, long long *daily_timestamps, double *daily_prices, int *daily_count) {
    *daily_count = 0;
    if (count == 0) return;

    time_t t0 = (time_t)timestamps[0];
    struct tm *tm = localtime(&t0);
    int last_day = tm->tm_yday;
    int last_year = tm->tm_year;

    for (int i = 0; i < count; i++) {
        time_t ti = (time_t)timestamps[i];
        tm = localtime(&ti);
        int current_day = tm->tm_yday;
        int current_year = tm->tm_year;

        if (i == count - 1 || current_day != last_day || current_year != last_year) {
            daily_timestamps[*daily_count] = timestamps[i];
            daily_prices[*daily_count] = prices[i];
            (*daily_count)++;
            last_day = current_day;
            last_year = current_year;
        }
    }
}

// Linear regression: returns result[0]=slope, result[1]=intercept, result[2]=r_squared
static void linear_regression(long long *timestamps, double *prices, int count, double *result) {
    result[0] = 0.0; // slope
    result[1] = 0.0; // intercept
    result[2] = 0.0; // r_squared
    if (count < 2) return;

    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0;
    for (int i = 0; i < count; i++) {
        double x = (double)timestamps[i];
        double y = prices[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    double n = (double)count;
    result[0] = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
    result[1] = (sum_y - result[0] * sum_x) / n;

    // Compute R-squared
    double ss_tot = 0.0, ss_res = 0.0;
    double y_mean = sum_y / n;
    for (int i = 0; i < count; i++) {
        double x = (double)timestamps[i];
        double y = prices[i];
        double y_pred = result[0] * x + result[1];
        ss_res += (y - y_pred) * (y - y_pred);
        ss_tot += (y - y_mean) * (y - y_mean);
    }
    result[2] = ss_tot > 0.0 ? 1.0 - (ss_res / ss_tot) : 0.0;
}

// Mock data for testing if lookup_stock.+x fails
static void generate_mock_data(const char *filename, const char *symbol) {
    fprintf(stderr, "Debug: Generating mock data for %s\n", filename);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to create mock data: %s\n", filename, strerror(errno));
        return;
    }
    // 2 points for simplicity
    fprintf(fp, "{\"chart\":{\"result\":[{\"meta\":{\"symbol\":\"%s\"},\"timestamp\":[1749562200,1749562260],\"indicators\":{\"quote\":[{\"close\":[120.50,120.55]}]}}]}}", symbol);
    fclose(fp);
    printf("Generated mock data in %s\n", filename);
}

int main(int argc, char *argv[]) {
    fprintf(stderr, "Debug: Starting predictions with argc=%d\n", argc);
    char *user_hash = "CA33D1"; // Try CA33D1 instead of TEST
    char symbol[32];

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s [<user_hash>] <symbol>\n", argv[0]);
        return 1;
    }

    if (argc == 3) {
        user_hash = argv[1];
        strncpy(symbol, argv[2], 31);
    } else {
        strncpy(symbol, argv[1], 31);
        printf("Warning: No user hash provided, using default 'CA33D1' for testing.\n");
    }
    symbol[31] = '\0';
    to_upper(symbol);

    fprintf(stderr, "Debug: user_hash=%s, symbol=%s\n", user_hash, symbol);

    // Validate inputs
    if (strlen(symbol) == 0 || strpbrk(symbol, " \t\n\"\'\\")) {
        fprintf(stderr, "Error: Invalid symbol '%s'\n", symbol);
        return 1;
    }
    if (strlen(user_hash) == 0 || strpbrk(user_hash, " \t\n\"\'\\")) {
        fprintf(stderr, "Error: Invalid user hash '%s', using 'CA33D1'\n", user_hash);
        user_hash = "CA33D1";
    }

    // Prediction horizons
    const char *ranges[] = {"1mo", "6mo", "2y"};
    long long horizons[] = {86400LL, 30LL * 86400LL, 365LL * 86400LL};
    const char *labels[] = {"1 Day", "1 Month", "1 Year"};
    int aggregates[] = {0, 1, 1};
    int num_predictions = 3;

    printf("Predictions for %s:\n", symbol);

    for (int p = 0; p < num_predictions; p++) {
        fprintf(stderr, "Debug: Processing horizon %s\n", labels[p]);
        // Fetch historical data
        char command[512];
        char log_file[64];
        snprintf(log_file, sizeof(log_file), "lookup_error_%s_%s.log", symbol, ranges[p]);
        snprintf(command, sizeof(command), "./+x/lookup_stock.+x %s %s > A.txt 2> %s",
                 user_hash, symbol, log_file); // Use A.txt explicitly
        fprintf(stderr, "Debug: Executing: %s\n", command);
        fprintf(stderr, "Debug: Args: user_hash=%s, symbol=%s\n", user_hash, symbol);
        int ret = system(command);
        char filename[64];
        snprintf(filename, sizeof(filename), "A.txt"); // Hardcode A.txt

        // Check A.txt contents
        FILE *fp = fopen(filename, "r");
        long size = 0;
        if (fp) {
            fseek(fp, 0, SEEK_END);
            size = ftell(fp);
            fclose(fp);
            fprintf(stderr, "Debug: %s size=%ld bytes\n", filename, size);
        } else {
            fprintf(stderr, "Debug: %s not readable\n", filename);
        }

        // Parse data
        long long timestamps[MAX_POINTS];
        double prices[MAX_POINTS];
        int count = 0;
        int parse_result = parse_json(filename, timestamps, prices, &count, MAX_POINTS);
        if (parse_result != 0 || count < 2) {
            fprintf(stderr, "Error: Failed to parse valid data from %s\n", filename);
            printf("%s: Failed to load data\n", labels[p]);
            generate_mock_data(filename, symbol); // Fallback to mock data
            parse_result = parse_json(filename, timestamps, prices, &count, MAX_POINTS);
            if (parse_result != 0 || count < 2) {
                printf("%s: Failed to load mock data\n", labels[p]);
                continue;
            }
        }

        // Aggregate to daily if needed
        long long daily_timestamps[MAX_POINTS];
        double daily_prices[MAX_POINTS];
        int daily_count = count;
        if (aggregates[p]) {
            aggregate_daily(timestamps, prices, count, daily_timestamps, daily_prices, &daily_count);
            if (daily_count < 2) {
                printf("%s: Insufficient daily data\n", labels[p]);
                continue;
            }
        } else {
            memcpy(daily_timestamps, timestamps, count * sizeof(long long));
            memcpy(daily_prices, prices, count * sizeof(double));
        }

        // Perform regression
        double result[3]; // slope, intercept, r_squared
        linear_regression(daily_timestamps, daily_prices, daily_count, result);

        // Predict future price
        time_t now = time(NULL);
        double future_time = (double)(now + horizons[p]);
        double predicted_price = result[0] * future_time + result[1];

        printf("%s: $%.2f (R²: %.2f)\n", labels[p], predicted_price, result[2]);
    }

    return 0;
}
