#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_LINE 8192
#define MAX_CELL_SIZE 1024
#define MAX_POINTS 1000

// ... (existing helper functions: skip_whitespace, parse_string, parse_array_numbers) ...
// (I will keep the logic but wrap it in a proper TPMOS Op structure)

static int skip_whitespace(const char *json, int pos) {
    while (json[pos] && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\t' || json[pos] == '\r')) {
        pos++;
    }
    return pos;
}

static int parse_string(const char *json, int pos, char *result, int max_size) {
    int j = 0;
    pos++; 
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
    pos = skip_whitespace(json, pos + 1);
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <symbol>\n", argv[0]);
        return 1;
    }

    char *symbol = argv[1];
    char in_file[256], out_file[256];
    snprintf(in_file, sizeof(in_file), "%s.txt", symbol);
    snprintf(out_file, sizeof(out_file), "%s.kvp", symbol);

    FILE *fp = fopen(in_file, "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s\n", in_file);
        return 1;
    }

    // Read full file into memory
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, fp);
    buffer[size] = '\0';
    fclose(fp);

    // Extraction logic (Simplified search for TPMOS speed)
    char *price_ptr = strstr(buffer, "\"regularMarketPrice\":");
    char *prev_close_ptr = strstr(buffer, "\"previousClose\":");
    char *symbol_ptr = strstr(buffer, "\"symbol\":\"");
    
    FILE *out = fopen(out_file, "w");
    if (!out) {
        free(buffer);
        return 1;
    }

    if (symbol_ptr) {
        symbol_ptr += 10;
        char s[32]; int i=0;
        while(symbol_ptr[i] != '"') { s[i] = symbol_ptr[i]; i++; }
        s[i] = '\0';
        fprintf(out, "symbol=%s\n", s);
    }
    
    if (price_ptr) {
        price_ptr += 21;
        fprintf(out, "price=%.2f\n", atof(price_ptr));
    }

    if (prev_close_ptr) {
        prev_close_ptr += 16;
        fprintf(out, "prev_close=%.2f\n", atof(prev_close_ptr));
    }

    fclose(out);
    free(buffer);
    printf("Converted %s to %s\n", in_file, out_file);
    return 0;
}
