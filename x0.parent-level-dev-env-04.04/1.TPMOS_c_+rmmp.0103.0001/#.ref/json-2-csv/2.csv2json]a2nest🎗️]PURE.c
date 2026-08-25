#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROWS 1000
#define MAX_COLS 2
#define MAX_CELL_SIZE 1024
#define MAX_DEPTH 100

// Function to skip whitespace
int skip_whitespace(const char* str, int pos) {
    while (pos >= 0 && str[pos] != '\0' && 
           (str[pos] == ' ' || str[pos] == '\n' || str[pos] == '\t' || str[pos] == '\r')) {
        pos++;
    }
    return pos;
}

// Function to read CSV file contents
char* read_csv_file(const char* filename) {
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

// Function to parse CSV line into key-value pair
int parse_csv_line(const char* csv, int pos, char* key, char* value, int max_size) {
    int i = 0;
    
    pos++; // Skip opening quote
    while (csv[pos] != '"' && csv[pos] != '\0' && i < max_size - 1) {
        key[i++] = csv[pos++];
    }
    key[i] = '\0';
    if (csv[pos] != '"') return -1;
    pos += 2; // Skip closing quote and comma
    
    i = 0;
    pos++; // Skip opening quote
    while (csv[pos] != '"' && csv[pos] != '\0' && i < max_size - 1) {
        value[i++] = csv[pos++];
    }
    value[i] = '\0';
    if (csv[pos] != '"') return -1;
    pos++;
    
    return csv[pos] == '\n' || csv[pos] == '\0' ? pos + 1 : -1;
}

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

// Function to determine if a string is a number
int is_number(const char* str) {
    int has_decimal = 0;
    int i = 0;
    if (str[i] == '-') i++;
    while (str[i]) {
        if (str[i] == '.' && !has_decimal) {
            has_decimal = 1;
        } else if (str[i] < '0' || str[i] > '9') {
            return 0;
        }
        i++;
    }
    return 1;
}

// Structure to track nesting levels
typedef struct {
    char path[MAX_DEPTH][MAX_CELL_SIZE];
    int depth;
    int is_array[MAX_DEPTH];
    int array_indices[MAX_DEPTH];
} PathTracker;

// Function to write JSON with proper nesting
int csv_to_json(const char* csv, const char* output_file) {
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

    int pos = 0;
    while (csv[pos] != '\0') {
        if (resize_matrix(&matrix, &max_rows, row_count) < 0) return -1;

        char key[MAX_CELL_SIZE];
        char value[MAX_CELL_SIZE];
        
        pos = parse_csv_line(csv, pos, key, value, MAX_CELL_SIZE);
        if (pos < 0) return -1;

        strncpy(matrix[row_count], key, MAX_CELL_SIZE - 1);
        strncpy(matrix[row_count] + MAX_CELL_SIZE, value, MAX_CELL_SIZE - 1);
        row_count++;
        
        pos = skip_whitespace(csv, pos);
    }

    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        for (int i = 0; i < max_rows; i++) free(matrix[i]);
        free(matrix);
        return -1;
    }

    fprintf(fp, "{\n");
    PathTracker prev = {{0}, 0, {0}, {0}};
    
    for (int i = 0; i < row_count; i++) {
        char* key = matrix[i];
        char* value = matrix[i] + MAX_CELL_SIZE;
        
        PathTracker current = {{0}, 0, {0}, {0}};
        char* ptr = key;
        while (*ptr) {
            if (*ptr == '.') {
                current.depth++;
                ptr++;
            } else if (*ptr == '[') {
                current.is_array[current.depth] = 1;
                ptr++;
                int index = atoi(ptr);
                current.array_indices[current.depth] = index;
                while (*ptr != ']') ptr++;
                ptr++;
            } else {
                char* start = ptr;
                while (*ptr && *ptr != '.' && *ptr != '[') ptr++;
                strncpy(current.path[current.depth], start, ptr - start);
                current.depth++;
            }
        }
        current.depth--;

        // Calculate indent and closing brackets
        int common_depth = 0;
        while (common_depth <= prev.depth && common_depth <= current.depth &&
               strcmp(prev.path[common_depth], current.path[common_depth]) == 0 &&
               prev.is_array[common_depth] == current.is_array[common_depth]) {
            common_depth++;
        }

        // Close previous structures
        for (int j = prev.depth; j >= common_depth; j--) {
            fprintf(fp, "%*s%s\n", (j + 1) * 2, "", prev.is_array[j] ? "]" : "}");
        }

        // Open new structures
        for (int j = common_depth; j <= current.depth; j++) {
            if (j > 0 && !current.is_array[j-1]) {
                fprintf(fp, "%*s\"%s\": %s\n", j * 2, "", 
                       current.path[j], current.is_array[j] ? "[" : "{");
            } else if (current.is_array[j]) {
                fprintf(fp, "%*s[\n", j * 2, "");
            }
        }

        // Write value
        if (current.is_array[current.depth]) {
            if (is_number(value)) {
                fprintf(fp, "%*s%s%s\n", (current.depth + 1) * 2, "", value, 
                       current.array_indices[current.depth] < row_count - 1 ? "," : "");
            } else {
                fprintf(fp, "%*s\"%s\"%s\n", (current.depth + 1) * 2, "", value,
                       current.array_indices[current.depth] < row_count - 1 ? "," : "");
            }
        } else {
            if (is_number(value)) {
                fprintf(fp, "%*s\"%s\": %s%s\n", (current.depth + 1) * 2, "", 
                       current.path[current.depth], value, i < row_count - 1 ? "," : "");
            } else {
                fprintf(fp, "%*s\"%s\": \"%s\"%s\n", (current.depth + 1) * 2, "", 
                       current.path[current.depth], value, i < row_count - 1 ? "," : "");
            }
        }

        prev = current;
    }

    // Close remaining structures
    for (int j = prev.depth; j >= 0; j--) {
        fprintf(fp, "%*s%s\n", (j + 1) * 2, "", prev.is_array[j] ? "]" : "}");
    }

    fclose(fp);
    for (int i = 0; i < max_rows; i++) free(matrix[i]);
    free(matrix);
    return 0;
}

// Test program
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <csv_file>\n", argv[0]);
        return 1;
    }

    char* csv = read_csv_file(argv[1]);
    if (!csv) {
        printf("Error: Could not read file '%s'\n", argv[1]);
        return 1;
    }

    if (csv_to_json(csv, "output.json") == 0) {
        printf("JSON file has been generated as 'output.json'\n");
    } else {
        printf("Error converting CSV to JSON\n");
    }

    free(csv);
    return 0;
}
