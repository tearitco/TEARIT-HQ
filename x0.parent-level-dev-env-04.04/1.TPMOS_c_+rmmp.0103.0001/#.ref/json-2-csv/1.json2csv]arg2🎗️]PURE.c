#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROWS 1000
#define MAX_COLS 2
#define MAX_CELL_SIZE 1024

// Function to skip whitespace
int skip_whitespace(const char* json, int pos) {
    while (pos >= 0 && json[pos] != '\0' && 
           (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\t' || json[pos] == '\r')) {
        pos++;
    }
    return pos;
}

// Function to parse a JSON string value
int parse_string(const char* json, int pos, char* result, int max_size) {
    int j = 0;
    pos++; // Skip opening quote
    while (json[pos] != '"' && json[pos] != '\0' && j < max_size - 1) {
        if (json[pos] == '\\') {
            pos++;
            if (json[pos] == '\0') break;
        }
        result[j++] = json[pos++];
    }
    result[j] = '\0';
    return json[pos] == '"' ? pos + 1 : -1;
}

// Forward declaration
int parse_value(const char* json, int pos, char*** matrix, 
                int* row_count, int* max_rows, char* prefix);

// Function to resize matrix if needed
int resize_matrix(char*** matrix, int* max_rows, int current_rows) {
    if (current_rows >= *max_rows) {
        *max_rows *= 2;
        char** temp = realloc(*matrix, *max_rows * sizeof(char*));
        if (!temp) return -1;
        *matrix = temp;
        for (int i = current_rows; i < *max_rows; i++) {
            (*matrix)[i] = calloc(MAX_COLS * MAX_CELL_SIZE, sizeof(char));
            if (!(*matrix)[i]) return -1;
        }
    }
    return 0;
}

// Function to parse JSON object
int parse_object(const char* json, int pos, char*** matrix, 
                 int* row_count, int* max_rows, char* prefix) {
    pos = skip_whitespace(json, pos + 1);
    while (json[pos] != '}' && json[pos] != '\0') {
        char key[MAX_CELL_SIZE];
        pos = parse_string(json, pos, key, MAX_CELL_SIZE);
        if (pos < 0) return -1;
        
        pos = skip_whitespace(json, pos);
        if (json[pos] != ':') return -1;
        pos = skip_whitespace(json, pos + 1);

        char new_prefix[MAX_CELL_SIZE];
        snprintf(new_prefix, MAX_CELL_SIZE, "%s%s%s", 
                prefix, *prefix ? "." : "", key);

        pos = parse_value(json, pos, matrix, row_count, max_rows, new_prefix);
        if (pos < 0) return -1;
        
        pos = skip_whitespace(json, pos);
        if (json[pos] == ',') pos = skip_whitespace(json, pos + 1);
    }
    return json[pos] == '}' ? pos + 1 : -1;
}

// Function to parse JSON array
int parse_array(const char* json, int pos, char*** matrix, 
                int* row_count, int* max_rows, char* prefix) {
    pos = skip_whitespace(json, pos + 1);
    int index = 0;
    
    while (json[pos] != ']' && json[pos] != '\0') {
        char new_prefix[MAX_CELL_SIZE];
        snprintf(new_prefix, MAX_CELL_SIZE, "%s[%d]", prefix, index++);

        pos = parse_value(json, pos, matrix, row_count, max_rows, new_prefix);
        if (pos < 0) return -1;
        
        pos = skip_whitespace(json, pos);
        if (json[pos] == ',') pos = skip_whitespace(json, pos + 1);
    }
    return json[pos] == ']' ? pos + 1 : -1;
}

// Function to parse any JSON value
int parse_value(const char* json, int pos, char*** matrix, 
                int* row_count, int* max_rows, char* prefix) {
    pos = skip_whitespace(json, pos);
    if (pos < 0 || json[pos] == '\0') return -1;

    if (json[pos] == '{') {
        return parse_object(json, pos, matrix, row_count, max_rows, prefix);
    }
    else if (json[pos] == '[') {
        return parse_array(json, pos, matrix, row_count, max_rows, prefix);
    }
    else {
        if (resize_matrix(matrix, max_rows, *row_count) < 0) return -1;

        strncpy((*matrix)[*row_count], prefix, MAX_CELL_SIZE - 1);
        
        char* value_ptr = (*matrix)[*row_count] + MAX_CELL_SIZE;
        if (json[pos] == '"') {
            pos = parse_string(json, pos, value_ptr, MAX_CELL_SIZE);
        } else {
            int j = 0;
            while (json[pos] != ',' && json[pos] != '}' && json[pos] != ']' && 
                   json[pos] != '\0' && json[pos] != ' ' && j < MAX_CELL_SIZE - 1) {
                value_ptr[j++] = json[pos++];
            }
            value_ptr[j] = '\0';
        }
        
        if (pos >= 0) (*row_count)++;
        return pos;
    }
    return -1;
}

// Function to read file contents
char* read_json_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, fp);
    buffer[read] = '\0';
    
    fclose(fp);
    return buffer;
}

// Main parsing function
int json_to_csv(const char* json, const char* output_file) {
    int max_rows = MAX_ROWS;
    int row_count = 0;

    char** matrix = malloc(max_rows * sizeof(char*));
    if (!matrix) return -1;
    
    for (int i = 0; i < max_rows; i++) {
        matrix[i] = calloc(MAX_COLS * MAX_CELL_SIZE, sizeof(char));
        if (!matrix[i]) {
            for (int j = 0; j < i; j++) free(matrix[j]);
            free(matrix);
            return -1;
        }
    }

    int pos = skip_whitespace(json, 0);
    if (json[pos] == '{') {
        pos = parse_object(json, pos, &matrix, &row_count, &max_rows, "");
    }
    else if (json[pos] == '[') {
        pos = parse_array(json, pos, &matrix, &row_count, &max_rows, "");
    }

    if (pos < 0) {
        for (int i = 0; i < max_rows; i++) free(matrix[i]);
        free(matrix);
        return -1;
    }

    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        for (int i = 0; i < max_rows; i++) free(matrix[i]);
        free(matrix);
        return -1;
    }

    for (int i = 0; i < row_count; i++) {
        fprintf(fp, "\"%s\",\"%s\"\n", matrix[i], matrix[i] + MAX_CELL_SIZE);
    }

    fclose(fp);
    for (int i = 0; i < max_rows; i++) free(matrix[i]);
    free(matrix);
    return 0;
}

// Test program with file input
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <json_file>\n", argv[0]);
        return 1;
    }

    char* json = read_json_file(argv[1]);
    if (!json) {
        printf("Error: Could not read file '%s'\n", argv[1]);
        return 1;
    }

    if (json_to_csv(json, "output.csv") == 0) {
        printf("CSV file has been generated as 'output.csv'\n");
    } else {
        printf("Error parsing JSON\n");
    }

    free(json);
    return 0;
}
