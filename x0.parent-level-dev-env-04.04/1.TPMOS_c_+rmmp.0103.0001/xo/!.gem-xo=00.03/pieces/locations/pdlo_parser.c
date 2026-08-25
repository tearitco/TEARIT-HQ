#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 512
#define MAX_METHODS 20
#define MAX_EXECUTABLES_PER_METHOD 5

// Method executable entry
typedef struct {
    char method_name[50];
    char python_path[MAX_PATH];
    char c_path[MAX_PATH];
    char js_path[MAX_PATH];
} MethodExecutables;

// PDLO piece data
typedef struct {
    char piece_id[50];
    char version[20];
    MethodExecutables methods[MAX_METHODS];
    int method_count;
} PDLOData;

// Trim whitespace
char* trim(char *str) {
    while(*str == ' ' || *str == '\t') str++;
    char *end = str + strlen(str) - 1;
    while(end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    return str;
}

// Parse a METHOD line with executable routing
// Format: METHOD | name | return_type | python:path | c:path | js:path
int parse_method_line(const char* line, MethodExecutables* method) {
    if (strncmp(line, "METHOD", 6) != 0) return 0;
    
    char temp[1000];
    strncpy(temp, line, sizeof(temp)-1);
    temp[sizeof(temp)-1] = '\0';
    
    // Split by |
    char* parts[10];
    int part_count = 0;
    char* token = strtok(temp, "|");
    while(token && part_count < 10) {
        parts[part_count++] = trim(token);
        token = strtok(NULL, "|");
    }
    
    if (part_count < 3) return 0;
    
    // parts[0] = "METHOD", parts[1] = name, parts[2] = return_type
    strncpy(method->method_name, parts[1], sizeof(method->method_name)-1);
    
    // Initialize paths to empty
    method->python_path[0] = '\0';
    method->c_path[0] = '\0';
    method->js_path[0] = '\0';
    
    // Parse language:path pairs (parts[3] onwards)
    for (int i = 3; i < part_count; i++) {
        char* colon = strchr(parts[i], ':');
        if (!colon) continue;
        
        *colon = '\0';
        char* lang = trim(parts[i]);
        char* path = trim(colon + 1);
        
        if (strcmp(lang, "python") == 0) {
            strncpy(method->python_path, path, sizeof(method->python_path)-1);
        } else if (strcmp(lang, "c") == 0) {
            strncpy(method->c_path, path, sizeof(method->c_path)-1);
        } else if (strcmp(lang, "js") == 0) {
            strncpy(method->js_path, path, sizeof(method->js_path)-1);
        }
    }
    
    return 1;
}

// Parse PDLO file
int parse_pdl_file(const char* filepath, PDLOData* data) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open PDLO file: %s\n", filepath);
        return 0;
    }
    
    // Initialize
    data->piece_id[0] = '\0';
    data->version[0] = '\0';
    data->method_count = 0;
    
    char line[1000];
    while (fgets(line, sizeof(line), f)) {
        // Skip comments, empty lines, headers
        if (line[0] == '#' || line[0] == '-' || line[0] == '\n') continue;
        if (strncmp(line, "SECTION", 7) == 0) continue;
        
        // Parse SECTION | KEY | VALUE
        if (strncmp(line, "META", 4) == 0) {
            char temp[1000];
            strncpy(temp, line, sizeof(temp)-1);
            char* parts[3];
            int count = 0;
            char* token = strtok(temp, "|");
            while(token && count < 3) {
                char* trimmed = trim(token);
                if (strlen(trimmed) > 0) {
                    parts[count++] = trimmed;
                }
                token = strtok(NULL, "|");
            }
            if (count >= 3) {
                if (strcmp(parts[1], "piece_id") == 0) {
                    strncpy(data->piece_id, parts[2], sizeof(data->piece_id)-1);
                } else if (strcmp(parts[1], "version") == 0) {
                    strncpy(data->version, parts[2], sizeof(data->version)-1);
                }
            }
        }
        else if (strncmp(line, "METHOD", 6) == 0) {
            if (data->method_count < MAX_METHODS) {
                if (parse_method_line(line, &data->methods[data->method_count])) {
                    data->method_count++;
                }
            }
        }
    }
    
    fclose(f);
    return 1;
}

// Get executable path for a method
// Returns: 1 if found, 0 if not found
// Tries in order: c, python, js
int get_method_executable(PDLOData* pdlo, const char* method_name, char* output_path, size_t output_size) {
    for (int i = 0; i < pdlo->method_count; i++) {
        if (strcmp(pdlo->methods[i].method_name, method_name) == 0) {
            // Try C first, then python, then js
            if (pdlo->methods[i].c_path[0] != '\0') {
                strncpy(output_path, pdlo->methods[i].c_path, output_size-1);
                return 1;
            } else if (pdlo->methods[i].python_path[0] != '\0') {
                strncpy(output_path, pdlo->methods[i].python_path, output_size-1);
                return 1;
            } else if (pdlo->methods[i].js_path[0] != '\0') {
                strncpy(output_path, pdlo->methods[i].js_path, output_size-1);
                return 1;
            }
        }
    }
    return 0;
}

// Test
#ifdef TEST_PDLO_PARSER
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <pdl_file>\n", argv[0]);
        return 1;
    }
    
    PDLOData data;
    if (!parse_pdl_file(argv[1], &data)) {
        printf("Failed to parse %s\n", argv[1]);
        return 1;
    }
    
    printf("Piece: %s (version %s)\n", data.piece_id, data.version);
    printf("Methods (%d):\n", data.method_count);
    for (int i = 0; i < data.method_count; i++) {
        printf("  %s:\n", data.methods[i].method_name);
        if (data.methods[i].python_path[0]) printf("    python: %s\n", data.methods[i].python_path);
        if (data.methods[i].c_path[0]) printf("    c: %s\n", data.methods[i].c_path);
        if (data.methods[i].js_path[0]) printf("    js: %s\n", data.methods[i].js_path);
    }
    
    // Test executable lookup
    char exe_path[512];
    if (get_method_executable(&data, "increment_time", exe_path, sizeof(exe_path))) {
        printf("\nExecutable for increment_time: %s\n", exe_path);
    } else {
        printf("\nNo executable found for increment_time\n");
    }
    
    return 0;
}
#endif
