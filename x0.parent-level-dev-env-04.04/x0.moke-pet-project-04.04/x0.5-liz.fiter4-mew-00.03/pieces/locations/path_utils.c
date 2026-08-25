#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For access
#include <libgen.h> // For basename
#include <errno.h> // For errno

#define MAX_LINE_LENGTH 1024
#define MAX_PATH_LENGTH 1024
#define MAX_ATTR_VALUE_LENGTH 256 // Define this constant

// Global variables for resolved paths
static char g_project_root[MAX_PATH_LENGTH] = "";
static char g_pieces_dir[MAX_PATH_LENGTH] = "";
static int g_paths_initialized = 0;

// Helper to read a key-value pair from location_kvp
static int read_kvp_file(const char* file_path, const char* key_to_find, char* value_buffer) {
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        return -1; // File not found or couldn't be opened
    }

    char line[MAX_LINE_LENGTH];
    size_t key_len = strlen(key_to_find);

    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        if (strncmp(line, key_to_find, key_len) == 0 && line[key_len] == '=') {
            strncpy(value_buffer, line + key_len + 1, MAX_PATH_LENGTH - 1);
            value_buffer[MAX_PATH_LENGTH - 1] = '\0';
            fclose(file);
            return 0; // Success
        }
    }
    fclose(file);
    return -1; // Key not found
}

// Initializes g_project_root and g_pieces_dir from location_kvp
static int init_tpm_paths_internal() {
    if (g_paths_initialized) {
        return 0;
    }

    // Try to find location_kvp relative to current executable
    char location_kvp_path[MAX_PATH_LENGTH];
    // This is relative to where the current executable is run from.
    // For path_utils itself, it will be in pieces/locations/+x/
    // So it needs to look up two directories to find "pieces"
    // For now, let's hardcode relative to project root.
    // The run_chtpm.sh script will ensure location_kvp is set up relative to project root.
    snprintf(location_kvp_path, sizeof(location_kvp_path), "pieces/locations/location_kvp");

    if (access(location_kvp_path, F_OK) == -1) {
        fprintf(stderr, "[path_utils] Error: location_kvp not found at %s. errno: %d\n", location_kvp_path, errno);
        return -1;
    }

    if (read_kvp_file(location_kvp_path, "project_root", g_project_root) == -1) {
        fprintf(stderr, "[path_utils] Error: 'project_root' not found in %s. errno: %d\n", location_kvp_path, errno);
        return -1;
    }
    if (read_kvp_file(location_kvp_path, "pieces_dir", g_pieces_dir) == -1) {
        fprintf(stderr, "[path_utils] Error: 'pieces_dir' not found in %s. errno: %d\n", location_kvp_path, errno);
        memset(g_project_root, 0, sizeof(g_project_root)); // Clear project_root if pieces_dir fails
        return -1;
    }
    g_paths_initialized = 1;
    return 0;
}

// --- Main function ---
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  project_root\n");
        fprintf(stderr, "  pieces_dir\n");
        fprintf(stderr, "  piece_state <piece_id>\n");
        fprintf(stderr, "  piece_pdl <piece_id>\n");
        fprintf(stderr, "  plugin_path <piece_id> <plugin_name>\n");
        fprintf(stderr, "  chtpm_renderer <renderer_name>\n");
        fprintf(stderr, "  raw_input_ledger\n");
        fprintf(stderr, "  current_frame\n");
        fprintf(stderr, "  frame_changed\n");
        return 1;
    }

    if (init_tpm_paths_internal() == -1) {
        return 1; // Paths could not be initialized
    }

    const char* command = argv[1];
    char resolved_path[MAX_PATH_LENGTH * 2]; // Increased size to prevent truncation warnings

    if (strcmp(command, "project_root") == 0) {
        printf("%s\n", g_project_root);
    } else if (strcmp(command, "pieces_dir") == 0) {
        printf("%s\n", g_pieces_dir);
    } else if (strcmp(command, "piece_state") == 0) {
        if (argc != 3) { fprintf(stderr, "Usage: %s piece_state <piece_id>\n", argv[0]); return 1; }
        snprintf(resolved_path, sizeof(resolved_path), "%s/%s/state.txt", g_pieces_dir, argv[2]);
        printf("%s\n", resolved_path);
    } else if (strcmp(command, "piece_pdl") == 0) {
        if (argc != 3) { fprintf(stderr, "Usage: %s piece_pdl <piece_id>\n", argv[0]); return 1; }
        // basename(argv[2]) to get just the piece name from a potential path like ui_components/button
        char piece_name_buf[MAX_ATTR_VALUE_LENGTH]; // Max size for piece name
        strncpy(piece_name_buf, argv[2], sizeof(piece_name_buf) - 1);
        piece_name_buf[sizeof(piece_name_buf) - 1] = '\0';
        char *piece_name = basename(piece_name_buf);
        snprintf(resolved_path, sizeof(resolved_path), "%s/%s/%s.pdl", g_pieces_dir, argv[2], piece_name);
        printf("%s\n", resolved_path);
    } else if (strcmp(command, "plugin_path") == 0) {
        if (argc != 4) { fprintf(stderr, "Usage: %s plugin_path <piece_id> <plugin_name>\n", argv[0]); return 1; }
        snprintf(resolved_path, sizeof(resolved_path), "%s/%s/plugins/+x/%s", g_pieces_dir, argv[2], argv[3]);
        printf("%s\n", resolved_path);
    } else if (strcmp(command, "chtpm_renderer") == 0) {
        if (argc != 3) { fprintf(stderr, "Usage: %s chtpm_renderer <renderer_name>\n", argv[0]); return 1; }
        snprintf(resolved_path, sizeof(resolved_path), "%s/chtpm/renderers/+x/%s", g_pieces_dir, argv[2]);
        printf("%s\n", resolved_path);
    } else if (strcmp(command, "raw_input_ledger") == 0) {
        snprintf(resolved_path, sizeof(resolved_path), "%s/system/input_dispatcher/raw_input_ledger.txt", g_pieces_dir);
        printf("%s\n", resolved_path);
    } else if (strcmp(command, "current_frame") == 0) {
        snprintf(resolved_path, sizeof(resolved_path), "%s/chtpm/frame_buffer/current_frame.txt", g_pieces_dir);
        printf("%s\n", resolved_path);
    } else if (strcmp(command, "frame_changed") == 0) {
        snprintf(resolved_path, sizeof(resolved_path), "%s/chtpm/frame_buffer/frame_changed.txt", g_pieces_dir);
        printf("%s\n", resolved_path);
    } else {
        fprintf(stderr, "[path_utils] Unknown command: %s\n", command);
        return 1;
    }

    return 0;
}
