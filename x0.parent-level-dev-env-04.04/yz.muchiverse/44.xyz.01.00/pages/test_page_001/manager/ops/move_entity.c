#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH 2048
#define MAX_LINE 1024

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: move_entity <entity_id> <target_x> <target_y>\n");
        return 1;
    }

    char entity_id[256];
    int target_x, target_y;

    strncpy(entity_id, argv[1], sizeof(entity_id) - 1);
    entity_id[sizeof(entity_id) - 1] = '\0';

    if (sscanf(argv[2], "%d", &target_x) != 1 ||
        sscanf(argv[3], "%d", &target_y) != 1) {
        fprintf(stderr, "move_entity: target_x and target_y must be integers\n");
        return 1;
    }

    // Derive page_root from binary location
    char script_path[MAX_PATH];
    realpath(argv[0], script_path);

    // Navigate up: ops/ -> manager/
    char *last_slash = strrchr(script_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        char *second_last = strrchr(script_path, '/');
        if (second_last) {
            *second_last = '\0';
        }
    }

    char page_root[MAX_PATH];
    strcpy(page_root, script_path);

    char state_dir[MAX_PATH];
    snprintf(state_dir, sizeof(state_dir), "%s/state", page_root);

    char entities_file[MAX_PATH];
    snprintf(entities_file, sizeof(entities_file), "%s/entities_live.txt", state_dir);

    char anim_queue_file[MAX_PATH];
    snprintf(anim_queue_file, sizeof(anim_queue_file), "%s/animation_queue.txt", state_dir);

    // Read current position of entity
    int current_x = 0, current_y = 0;
    FILE *fp = fopen(entities_file, "r");
    if (fp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), fp)) {
            char line_entity_id[256] = {0};
            int x_val = 0, y_val = 0;

            char *pos = line;
            char *pipe = strchr(pos, '|');
            if (pipe) {
                strncpy(line_entity_id, pos, pipe - pos);
                line_entity_id[pipe - pos] = '\0';
                for (int i = strlen(line_entity_id) - 1; i >= 0 &&
                     (line_entity_id[i] == ' ' || line_entity_id[i] == '\t'); i--)
                    line_entity_id[i] = '\0';
            }

            if (strcmp(line_entity_id, entity_id) == 0) {
                pos = line;
                while ((pos = strstr(pos, "x=")) != NULL) {
                    if (sscanf(pos, "x=%d", &x_val) == 1) {
                        current_x = x_val;
                        break;
                    }
                    pos++;
                }
                pos = line;
                while ((pos = strstr(pos, "y=")) != NULL) {
                    if (sscanf(pos, "y=%d", &y_val) == 1) {
                        current_y = y_val;
                        break;
                    }
                    pos++;
                }
                break;
            }
        }
        fclose(fp);
    }

    // Queue animation entry
    fp = fopen(anim_queue_file, "a");
    if (fp) {
        fprintf(fp, "%s | x=%d | y=%d | target_x=%d | target_y=%d\n",
                entity_id, current_x, current_y, target_x, target_y);
        fclose(fp);
    }

    // Update entities_live.txt with new position
    char temp_file[MAX_PATH];
    snprintf(temp_file, sizeof(temp_file), "%s.tmp", entities_file);

    FILE *temp_fp = fopen(entities_file, "r");
    FILE *out_fp = fopen(temp_file, "w");
    if (temp_fp && out_fp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), temp_fp)) {
            char line_entity_id[256] = {0};
            char *pos = line;
            char *pipe = strchr(pos, '|');
            if (pipe) {
                strncpy(line_entity_id, pos, pipe - pos);
                line_entity_id[pipe - pos] = '\0';
                for (int i = strlen(line_entity_id) - 1; i >= 0 &&
                     (line_entity_id[i] == ' ' || line_entity_id[i] == '\t'); i--)
                    line_entity_id[i] = '\0';
            }

            if (strcmp(line_entity_id, entity_id) == 0) {
                fprintf(out_fp, "%s | x=%d | y=%d\n", entity_id, target_x, target_y);
            } else {
                fputs(line, out_fp);
            }
        }
        fclose(temp_fp);
        fclose(out_fp);
        rename(temp_file, entities_file);
    }

    return 0;
}
