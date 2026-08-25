#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#define MAX_PATH 1024
#define MAX_LINE 256
#define MAX_ENTITIES 64

char project_root[MAX_PATH] = ".";
const char* world_path = "pieces/world_tank_01/map_enclosure";
const char* state_path_world = "pieces/world_tank_01/state.txt";
const char* log_dir = "pieces/world_tank_01/logs";
char last_response[256] = "System Initialized.";

static void strip_trailing_slash(char* path) {
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
        len--;
    }
}

void resolve_root(const char* argv0) {
    char path[MAX_PATH];
    strncpy(path, argv0, MAX_PATH);
    path[MAX_PATH - 1] = '\0';

    char* pieces = strstr(path, "/pieces/");
    if (!pieces) {
        pieces = strstr(path, "pieces/");
    }

    if (pieces) {
        *pieces = '\0';
        strip_trailing_slash(path);
        if (strlen(path) == 0) {
            strcpy(project_root, ".");
        } else {
            char resolved[MAX_PATH];
            if (realpath(path, resolved)) {
                strncpy(project_root, resolved, MAX_PATH);
                project_root[MAX_PATH - 1] = '\0';
            } else {
                strncpy(project_root, path, MAX_PATH);
                project_root[MAX_PATH - 1] = '\0';
            }
        }
    } else {
        strcpy(project_root, ".");
    }
    printf("[Manager] Resolved Root: %s\n", project_root);
}

typedef struct {
    char id[64];
    char type[64];
} Entity;

typedef struct {
    char hp[32];
    char hunger[32];
    char type[64];
    char life[32];
} EntitySnapshot;

int get_epoch() {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s", project_root, state_path_world);
    FILE* f = fopen(full_path, "r");
    if (!f) return 1;
    char line[MAX_LINE];
    int epoch = 1;
    while (fgets(line, MAX_LINE, f)) {
        if (strncmp(line, "epoch=", 6) == 0) {
            epoch = atoi(line + 6);
            break;
        }
    }
    fclose(f);
    return epoch;
}

void increment_epoch(int current) {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s", project_root, state_path_world);
    FILE* f = fopen(full_path, "w");
    if (f) {
        fprintf(f, "epoch=%d\nstatus=active\n", current + 1);
        fclose(f);
    }
}

void log_event(int epoch, const char* entity, const char* action, const char* target) {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s/epoch_%d.txt", project_root, log_dir, epoch);
    FILE* f = fopen(full_path, "a");
    if (f) {
        time_t now;
        time(&now);
        char* ts = ctime(&now);
        ts[strlen(ts)-1] = '\0'; // remove newline
        fprintf(f, "[%s] %s | %s | %s\n", ts, entity, action, target ? target : "NONE");
        fclose(f);
    }
}

void log_state(int epoch, const char* entity, const char* phase, const EntitySnapshot* snapshot) {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s/epoch_%d.txt", project_root, log_dir, epoch);
    FILE* f = fopen(full_path, "a");
    if (f) {
        time_t now;
        time(&now);
        char* ts = ctime(&now);
        ts[strlen(ts)-1] = '\0';
        fprintf(f,
                "[%s] STATE | %s | phase=%s | hp=%s | hunger=%s | life=%s | type=%s | control=auto\n",
                ts,
                entity,
                phase,
                snapshot->hp,
                snapshot->hunger,
                snapshot->life,
                snapshot->type);
        fclose(f);
    }
}

static void pulse_frame_marker(void) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fputs("M\n", f);
        fclose(f);
    }

    snprintf(path, MAX_PATH, "%s/pieces/chtpm/frame_buffer/frame_changed.txt", project_root);
    f = fopen(path, "a");
    if (f) {
        fputs("M\n", f);
        fclose(f);
    }
}

static void write_autonomous_projection(int epoch, const char* entity, const char* phase, const EntitySnapshot* snapshot) {
    static int projection_dirs_ready = 0;
    if (!projection_dirs_ready) {
        char path[MAX_PATH];
        snprintf(path, MAX_PATH, "%s/pieces", project_root);
        mkdir(path, 0777);
        snprintf(path, MAX_PATH, "%s/pieces/display", project_root);
        mkdir(path, 0777);
        snprintf(path, MAX_PATH, "%s/pieces/chtpm", project_root);
        mkdir(path, 0777);
        snprintf(path, MAX_PATH, "%s/pieces/chtpm/frame_buffer", project_root);
        mkdir(path, 0777);
        projection_dirs_ready = 1;
    }

    const char* frame_targets[] = {
        "pieces/display/current_frame.txt",
        "pieces/chtpm/frame_buffer/current_frame.txt"
    };

    for (size_t i = 0; i < sizeof(frame_targets) / sizeof(frame_targets[0]); i++) {
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", project_root, frame_targets[i]);
        FILE *f = fopen(full_path, "w");
        if (!f) continue;

        fprintf(f, "============================================================\n");
        fprintf(f, " MOKE-PET AUTONOMOUS\n");
        fprintf(f, "============================================================\n");
        fprintf(f, " STATUS: RUNNING\n");
        fprintf(f, " ACTIVE: %s\n", entity);
        fprintf(f, " EPOCH:  %d\n", epoch);
        fprintf(f, " PHASE:  %s\n", phase);
        fprintf(f, " HP:     %s\n", snapshot->hp);
        fprintf(f, " HUNGER: %s\n", snapshot->hunger);
        fprintf(f, " LIFE:   %s\n", snapshot->life);
        fprintf(f, " TYPE:   %s\n", snapshot->type);
        fprintf(f, " LOG:    %s\n", last_response);
        fprintf(f, "============================================================\n");
        fclose(f);
    }

    pulse_frame_marker();
}

int read_value_from_file(const char* path, const char* prefix, char* out, size_t out_size) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, MAX_LINE, f)) {
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            strncpy(out, line + strlen(prefix), out_size - 1);
            out[out_size - 1] = '\0';
            out[strcspn(out, "\n\r")] = 0;
            found = 1;
            break;
        }
    }

    fclose(f);
    return found;
}

void load_snapshot(const char* id, EntitySnapshot* snapshot) {
    strcpy(snapshot->hp, "N/A");
    strcpy(snapshot->hunger, "N/A");
    strcpy(snapshot->type, "unknown");
    strcpy(snapshot->life, "unknown");

    char stats_path[MAX_PATH];
    snprintf(stats_path, MAX_PATH, "%s/%s/%s/memory/stats.txt", project_root, world_path, id);
    read_value_from_file(stats_path, "hp=", snapshot->hp, sizeof(snapshot->hp));
    read_value_from_file(stats_path, "hunger=", snapshot->hunger, sizeof(snapshot->hunger));

    char state_path[MAX_PATH];
    snprintf(state_path, MAX_PATH, "%s/%s/%s/state.txt", project_root, world_path, id);
    if (read_value_from_file(state_path, "type | ", snapshot->type, sizeof(snapshot->type))) {
        if (strcmp(snapshot->type, "food") == 0) {
            strcpy(snapshot->life, "dead");
        } else {
            strcpy(snapshot->life, "alive");
        }
    } else {
        strcpy(snapshot->life, "unknown");
    }
}

void print_snapshot(int epoch, const char* entity, const char* phase) {
    EntitySnapshot snapshot;
    load_snapshot(entity, &snapshot);
    printf("[Manager] STATE %s | phase=%s | hp=%s | hunger=%s | life=%s | type=%s | control=auto\n",
           entity, phase, snapshot.hp, snapshot.hunger, snapshot.life, snapshot.type);
    log_state(epoch, entity, phase, &snapshot);
    write_autonomous_projection(epoch, entity, phase, &snapshot);
}

int get_type(const char* id, char* type) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s/%s/%s/state.txt", project_root, world_path, id);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, f)) {
        if (strncmp(line, "type | ", 7) == 0) {
            strncpy(type, line + 7, 63);
            type[strcspn(type, "\n\r")] = 0;
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

void resolve_op(const char* entity, const char* method, char* out_path) {
    char pdl_path[MAX_PATH];
    snprintf(pdl_path, MAX_PATH, "%s/%s/%s/piece.pdl", project_root, world_path, entity);
    FILE* f = fopen(pdl_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, f)) {
        if (strstr(line, method)) {
            char* pipe2 = strrchr(line, '|');
            if (pipe2) {
                strncpy(out_path, pipe2 + 2, MAX_PATH);
                out_path[strcspn(out_path, "\n\r")] = 0;
                out_path[strcspn(out_path, " \t")] = 0;
                if (strstr(out_path, "/ops/+x/") == NULL) {
                    char* ops_dir = strstr(out_path, "/ops/");
                    if (ops_dir) {
                        char rebuilt[MAX_PATH];
                        size_t prefix_len = (size_t)(ops_dir - out_path);
                        snprintf(rebuilt, MAX_PATH, "%.*s/ops/+x/%s", (int)prefix_len, out_path, ops_dir + 5);
                        strncpy(out_path, rebuilt, MAX_PATH - 1);
                        out_path[MAX_PATH - 1] = '\0';
                    }
                }
                fclose(f); return;
            }
        }
    }
    fclose(f);
}

void dispatch(const char* entity, const char* method, const char* arg1, const char* arg2) {
    char op_path[MAX_PATH] = {0};
    resolve_op(entity, method, op_path);
    if (strlen(op_path) == 0) return;

    if (arg1 && arg2) {
        printf("[Manager] %s acts: %s(%s, %s)\n", entity, method, arg1, arg2);
    } else if (arg1) {
        printf("[Manager] %s acts: %s(%s)\n", entity, method, arg1);
    } else {
        printf("[Manager] %s acts: %s()\n", entity, method);
    }
    pid_t pid = fork();
    if (pid == 0) {
        if (chdir(project_root) != 0) {
            perror("chdir");
            exit(1);
        }
        if (arg1 && arg2) execl(op_path, op_path, arg1, arg2, NULL);
        else if (arg1) execl(op_path, op_path, arg1, NULL);
        else execl(op_path, op_path, NULL);
        perror(op_path);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

int main(int argc, char* argv[]) {
    resolve_root(argv[0]);
    int epoch = get_epoch();
    printf("=== Epoch %d ===\n", epoch);

    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s/%s", project_root, world_path);
    DIR* d = opendir(search_path);
    if (!d) return 1;

    char lizard_ids[MAX_ENTITIES][64];
    int lizard_count = 0;
    struct dirent* entry;
    while ((entry = readdir(d)) && lizard_count < MAX_ENTITIES) {
        if (entry->d_name[0] == '.') continue;
        char type[64];
        if (get_type(entry->d_name, type) && strcmp(type, "lizard") == 0) {
            strncpy(lizard_ids[lizard_count++], entry->d_name, 63);
        }
    }
    closedir(d);

    for (int i = 0; i < lizard_count; i++) {
        const char* liz = lizard_ids[i];
        printf("\n--- Turn: %s ---\n", liz);
        print_snapshot(epoch, liz, "turn-start");
        
        // 0. Metabolism Phase
        dispatch(liz, "breathe", NULL, NULL);
        print_snapshot(epoch, liz, "after-breathe");

        // 1. Perception Phase: Run 'scan' Op
        dispatch(liz, "scan", NULL, NULL);
        print_snapshot(epoch, liz, "after-scan");
        
        char obs_path[MAX_PATH];
        snprintf(obs_path, MAX_PATH, "%s/%s/%s/memory/observations.txt", project_root, world_path, liz);
        FILE* f = fopen(obs_path, "r");
        if (!f) {
            dispatch(liz, "rest", NULL, NULL);
            log_event(epoch, liz, "rest", NULL);
            print_snapshot(epoch, liz, "after-rest");
            dispatch(liz, "check_death", NULL, NULL);
            print_snapshot(epoch, liz, "after-check_death");
            continue;
        }

        char target_food[64] = "";
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, f)) {
            char* pipe = strchr(line, '|');
            if (pipe) {
                *pipe = '\0';
                char* id = line;
                char* type = pipe + 2;
                type[strcspn(type, "\n\r")] = 0;
                
                if (strstr(type, "food")) {
                    char* id_trim = id;
                    while(*id_trim == ' ') id_trim++;
                    char* space = strchr(id_trim, ' ');
                    if (space) *space = '\0';
                    strncpy(target_food, id_trim, 63);
                    break;
                }
            }
        }
        fclose(f);

        if (strlen(target_food) > 0) {
            dispatch(liz, "eat", target_food, NULL);
            log_event(epoch, liz, "eat", target_food);
            print_snapshot(epoch, liz, "after-eat");
        } else {
            dispatch(liz, "rest", NULL, NULL);
            log_event(epoch, liz, "rest", NULL);
            print_snapshot(epoch, liz, "after-rest");
        }

        // 3. Mortality Phase
        dispatch(liz, "check_death", NULL, NULL);
        print_snapshot(epoch, liz, "after-check_death");
    }

    // 4. Perception-to-Training Loop
    printf("\n--- Training Phase ---\n");
    char epoch_log[MAX_PATH];
    snprintf(epoch_log, MAX_PATH, "%s/epoch_%d.txt", log_dir, epoch);
    for (int i = 0; i < lizard_count; i++) {
        dispatch(lizard_ids[i], "train", epoch_log, lizard_ids[i]);
        print_snapshot(epoch, lizard_ids[i], "after-train");
    }

    increment_epoch(epoch);
    printf("\n=== Epoch %d Complete ===\n", epoch);
    return 0;
}
