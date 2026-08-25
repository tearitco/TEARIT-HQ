#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_LINE 8192
#define MAX_CELL_SIZE 1024

static int skip_whitespace(const char *json, int pos) {
    while (json[pos] && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\t' || json[pos] == '\r')) {
        pos++;
    }
    return pos;
}

static int parse_string(const char *json, int pos, char *result, int max_size) {
    int j = 0;
    pos++; // Skip opening quote
    while (json[pos] != '"' && json[pos] && j < max_size - 1) {
        if (json[pos] == '\\') {
            pos++;
            if (!json[pos]) break;
        }
        result[j++] = json[pos++];
    }
    result[j] = '\0';
    return json[pos] == '"' ? pos + 1 : -1;
}

static int parse_array_numbers(const char *json, int pos, double *values, int *count, int max_count, int is_timestamp) {
    pos = skip_whitespace(json, pos + 1); // Skip [
    *count = 0;
    while (json[pos] != ']' && json[pos]) {
        pos = skip_whitespace(json, pos);
        char *end;
        if (is_timestamp) {
            long long val = strtoll(json + pos, &end, 10);
            if (end == json + pos) break;
            if (*count < max_count) values[*count] = (double)val;
        } else {
            double val = strtod(json + pos, &end);
            if (end == json + pos) break;
            if (*count < max_count) values[*count] = val;
        }
        (*count)++;
        pos = end - json;
        pos = skip_whitespace(json, pos);
        if (json[pos] == ',') pos++;
    }
    return json[pos] == ']' ? pos + 1 : -1;
}

int parse_stock_json(const char *filename, long long *timestamps, double *prices, int *count, int max_count) {
    fprintf(stderr, "[Processing] Parsing %s\n", filename);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to open: %s\n", filename, strerror(errno));
        return 1;
    }

    char *buffer = malloc(1);
    if (!buffer) {
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
            fclose(fp);
            return 1;
        }
        buffer = temp;
        strcat(buffer, line);
    }
    fclose(fp);

    fprintf(stderr, "[Processing] buffer_size=%zu, buffer_start=%.20s\n", buffer_size, buffer_size ? buffer : "(empty)");
    if (!buffer_size) {
        fprintf(stderr, "[%s] Empty file\n", filename);
        free(buffer);
        return 1;
    }

    int pos = skip_whitespace(buffer, 0);
    if (buffer[pos] != '{') {
        free(buffer);
        return 1;
    }

    pos++; // Skip {
    *count = 0;
    int found_timestamp = 0, found_close = 0;
    double *temp_timestamps = malloc(max_count * sizeof(double));
    double *temp_prices = malloc(max_count * sizeof(double));
    int temp_ts_count = 0, temp_prices_count = 0;

    while (buffer[pos] != '}' && buffer[pos]) {
        pos = skip_whitespace(buffer, pos);
        char key[MAX_CELL_SIZE];
        pos = parse_string(buffer, pos, key, MAX_CELL_SIZE);
        if (pos < 0) break;
        pos = skip_whitespace(buffer, pos);
        if (buffer[pos] != ':') break;
        pos = skip_whitespace(buffer, pos + 1);

        if (!strcmp(key, "chart")) {
            pos = skip_whitespace(buffer, pos);
            if (buffer[pos] != '{') break;
            pos++; // Skip {
            while (buffer[pos] != '}' && buffer[pos]) {
                pos = parse_string(buffer, pos, key, MAX_CELL_SIZE);
                if (pos < 0) break;
                pos = skip_whitespace(buffer, pos);
                if (buffer[pos] != ':') break;
                pos = skip_whitespace(buffer, pos + 1);

                if (!strcmp(key, "result")) {
                    pos = skip_whitespace(buffer, pos);
                    if (buffer[pos] != '[') break;
                    pos++; // Skip [
                    pos = skip_whitespace(buffer, pos);
                    if (buffer[pos] != '{') break;
                    pos++; // Skip {
                    while (buffer[pos] != '}' && buffer[pos]) {
                        pos = parse_string(buffer, pos, key, MAX_CELL_SIZE);
                        if (pos < 0) break;
                        pos = skip_whitespace(buffer, pos);
                        if (buffer[pos] != ':') break;
                        pos = skip_whitespace(buffer, pos + 1);

                        if (!strcmp(key, "timestamp")) {
                            pos = parse_array_numbers(buffer, pos, temp_timestamps, &temp_ts_count, max_count, 1);
                            found_timestamp = 1;
                        } else if (!strcmp(key, "indicators")) {
                            pos = skip_whitespace(buffer, pos);
                            if (buffer[pos] != '{') break;
                            pos++; // Skip {
                            while (buffer[pos] != '}' && buffer[pos]) {
                                pos = parse_string(buffer, pos, key, MAX_CELL_SIZE);
                                if (pos < 0) break;
                                pos = skip_whitespace(buffer, pos);
                                if (buffer[pos] != ':') break;
                                pos = skip_whitespace(buffer, pos + 1);

                                if (!strcmp(key, "quote")) {
                                    pos = skip_whitespace(buffer, pos);
                                    if (buffer[pos] != '[') break;
                                    pos++; // Skip [
                                    pos = skip_whitespace(buffer, pos);
                                    if (buffer[pos] != '{') break;
                                    pos++; // Skip {
                                    while (buffer[pos] != '}' && buffer[pos]) {
                                        pos = parse_string(buffer, pos, key, MAX_CELL_SIZE);
                                        if (pos < 0) break;
                                        pos = skip_whitespace(buffer, pos);
                                        if (buffer[pos] != ':') break;
                                        pos = skip_whitespace(buffer, pos + 1);

                                        if (!strcmp(key, "close")) {
                                            pos = parse_array_numbers(buffer, pos, temp_prices, &temp_prices_count, max_count, 0);
                                            found_close = 1;
                                        } else {
                                            while (buffer[pos] && buffer[pos] != ',' && buffer[pos] != '}') pos++;
                                            if (buffer[pos] == ',') pos++;
                                        }
                                    }
                                    pos = skip_whitespace(buffer, pos);
                                    if (buffer[pos] == '}') pos++;
                                    if (buffer[pos] == ']') pos++;
                                } else {
                                    while (buffer[pos] && buffer[pos] != ',' && buffer[pos] != '}') pos++;
                                    if (buffer[pos] == ',') pos++;
                                }
                            }
                            if (buffer[pos] == '}') pos++;
                        } else {
                            while (buffer[pos] && buffer[pos] != ',' && buffer[pos] != '}') pos++;
                            if (buffer[pos] == ',') pos++;
                        }
                    }
                    if (buffer[pos] == '}') pos++;
                    if (buffer[pos] == ']') pos++;
                } else {
                    while (buffer[pos] && buffer[pos] != ',' && buffer[pos] != '}') pos++;
                    if (buffer[pos] == ',') pos++;
                }
            }
            if (buffer[pos] == '}') pos++;
        } else {
            while (buffer[pos] && buffer[pos] != ',' && buffer[pos] != '}') pos++;
            if (buffer[pos] == ',') pos++;
        }
    }

    if (!found_timestamp || !found_close || temp_ts_count != temp_prices_count) {
        fprintf(stderr, "[%s] Failed to find matching timestamp and close arrays\n", filename);
        free(temp_timestamps);
        free(temp_prices);
        free(buffer);
        return 1;
    }

    *count = temp_ts_count < max_count ? temp_ts_count : max_count;
    for (int i = 0; i < *count; i++) {
        timestamps[i] = (long long)temp_timestamps[i];
        prices[i] = temp_prices[i];
    }

    free(temp_timestamps);
    free(temp_prices);
    free(buffer);
    return 0;
}
