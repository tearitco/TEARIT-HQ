#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#define MAX_PATH 2048
#define MAX_LINE 1024

int main(int argc, char *argv[]) {
    // sync_entity_positions
    // Reads all entity desktop_pos.txt files and syncs to entities_live.txt
    // Called by page_manager_tick to keep master ledger in sync with external position changes

    // Derive world_root from binary location
    // script_path: ...&.hq-apps/world-manager/ops/sync_entity_positions
    // We need: ...&.hq-apps/world-manager/
    char script_path[MAX_PATH];
    realpath(argv[0], script_path);

    char *last_slash = strrchr(script_path, '/');
    if (last_slash) {
        *last_slash = '\0';  // Remove /sync_entity_positions
        char *second_last = strrchr(script_path, '/');
        if (second_last) {
            *second_last = '\0';  // Remove /ops
        }
    }

    char page_root[MAX_PATH];
    strcpy(page_root, script_path);  // page_root is world-manager dir

    // Derive house_root
    char *hqapps_ptr = strstr(page_root, "/&.hq-apps");
    char house_root[MAX_PATH];
    if (hqapps_ptr) {
        strncpy(house_root, page_root, hqapps_ptr - page_root);
        house_root[hqapps_ptr - page_root] = '\0';
    } else {
        strcpy(house_root, page_root);
    }

    char state_dir[MAX_PATH];
    snprintf(state_dir, sizeof(state_dir), "%s/state", page_root);

    char entities_file[MAX_PATH];
    snprintf(entities_file, sizeof(entities_file), "%s/entities_live.txt", state_dir);

    // Read current entities_live.txt into memory
    typedef struct {
        char entity_id[256];
        int x, y;
    } Entity;

    Entity entities[256];
    int entity_count = 0;

    FILE *fp = fopen(entities_file, "r");
    if (fp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), fp) && entity_count < 256) {
            char *stripped = line;
            while (*stripped == ' ' || *stripped == '\t') stripped++;
            if (*stripped == '\0' || *stripped == '\n') continue;

            char entity_id[256] = {0};
            int x = 0, y = 0;

            char *pos = stripped;
            char *pipe = strchr(pos, '|');
            if (pipe) {
                strncpy(entity_id, pos, pipe - pos);
                entity_id[pipe - pos] = '\0';
                for (int i = strlen(entity_id) - 1; i >= 0 &&
                     (entity_id[i] == ' ' || entity_id[i] == '\t'); i--)
                    entity_id[i] = '\0';
            }

            pos = stripped;
            while ((pos = strstr(pos, "x=")) != NULL) {
                if (sscanf(pos, "x=%d", &x) == 1) break;
                pos++;
            }
            pos = stripped;
            while ((pos = strstr(pos, "y=")) != NULL) {
                if (sscanf(pos, "y=%d", &y) == 1) break;
                pos++;
            }

            strncpy(entities[entity_count].entity_id, entity_id, sizeof(entities[entity_count].entity_id) - 1);
            entities[entity_count].x = x;
            entities[entity_count].y = y;
            entity_count++;
        }
        fclose(fp);
    }

    // Find all entity directories and read their desktop_pos.txt
    char find_cmd[MAX_PATH * 2];
    snprintf(find_cmd, sizeof(find_cmd),
             "find '%s/xyzfs/users' -maxdepth 3 -name 'desktop_pos.txt' 2>/dev/null",
             house_root);

    FILE *find_fp = popen(find_cmd, "r");
    if (find_fp) {
        char pos_file[MAX_PATH];
        while (fgets(pos_file, sizeof(pos_file), find_fp) != NULL) {
            // Remove newline
            pos_file[strcspn(pos_file, "\n")] = '\0';

            // Extract entity_id from path: .../pals/<entity_id>/desktop_pos.txt
            char *pals_ptr = strstr(pos_file, "/pals/");
            if (!pals_ptr) continue;

            pals_ptr += 6;  // Skip "/pals/"
            char *slash_ptr = strchr(pals_ptr, '/');
            if (!slash_ptr) continue;

            char entity_id[256];
            strncpy(entity_id, pals_ptr, slash_ptr - pals_ptr);
            entity_id[slash_ptr - pals_ptr] = '\0';

            // Read desktop_pos.txt
            int desktop_x = 0, desktop_y = 0;
            FILE *pos_fp = fopen(pos_file, "r");
            if (pos_fp) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), pos_fp)) {
                    if (sscanf(line, "x=%d", &desktop_x) == 1) continue;
                    if (sscanf(line, "y=%d", &desktop_y) == 1) continue;
                }
                fclose(pos_fp);
            }

            // Update or add entity in array
            int found = 0;
            for (int i = 0; i < entity_count; i++) {
                if (strcmp(entities[i].entity_id, entity_id) == 0) {
                    entities[i].x = desktop_x;
                    entities[i].y = desktop_y;
                    found = 1;
                    break;
                }
            }

            if (!found && entity_count < 256) {
                strncpy(entities[entity_count].entity_id, entity_id, sizeof(entities[entity_count].entity_id) - 1);
                entities[entity_count].x = desktop_x;
                entities[entity_count].y = desktop_y;
                entity_count++;
            }
        }
        pclose(find_fp);
    }

    // Write updated entities_live.txt
    FILE *out_fp = fopen(entities_file, "w");
    if (out_fp) {
        for (int i = 0; i < entity_count; i++) {
            fprintf(out_fp, "%s | x=%d | y=%d\n",
                    entities[i].entity_id, entities[i].x, entities[i].y);
        }
        fclose(out_fp);
    }

    return 0;
}
