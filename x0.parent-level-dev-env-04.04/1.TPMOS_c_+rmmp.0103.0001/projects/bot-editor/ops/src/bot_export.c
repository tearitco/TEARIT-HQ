#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";

char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

int main(int argc, char *argv[]) {
    resolve_paths();

    if (argc < 2) {
        fprintf(stderr, "Usage: bot_export <bot_id> [export_path]\n");
        return 1;
    }

    char *bot_id = argv[1];
    char export_dir[MAX_PATH];
    if (argc > 2) {
        strncpy(export_dir, argv[2], sizeof(export_dir)-1);
    } else {
        snprintf(export_dir, sizeof(export_dir), "%s/exports/%s_standalone", project_root, bot_id);
    }

    char *bot_piece_path = NULL;
    asprintf(&bot_piece_path, "%s/pieces/ai_bots/%s", project_root, bot_id);

    struct stat st;
    if (stat(bot_piece_path, &st) != 0) {
        fprintf(stderr, "Error: Bot piece %s not found\n", bot_id);
        return 1;
    }

    char cmd[MAX_PATH * 3];
    
    // 1. Create export structure
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/bin' '%s/src' '%s/layouts' '%s/piece' '%s/pieces/display' '%s/pieces/keyboard' '%s/pieces/apps/player_app/manager'", 
             export_dir, export_dir, export_dir, export_dir, export_dir, export_dir, export_dir);
    system(cmd);

    // 2. Copy source code for local compilation
    snprintf(cmd, sizeof(cmd), "cp '%s/projects/bot-editor/manager/exo-bot_manager.c' '%s/src/'", project_root, export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "cp '%s'/projects/bot-editor/ops/src/*.c '%s/src/'", project_root, export_dir);
    system(cmd);

    // 3. Create local compile script
    char compile_path[MAX_PATH];
    snprintf(compile_path, sizeof(compile_path), "%s/compile_exo.sh", export_dir);
    FILE *cf = fopen(compile_path, "w");
    if (cf) {
        fprintf(cf, "#!/bin/bash\n");
        fprintf(cf, "echo \"Compiling EXO-BOT locally...\"\n");
        fprintf(cf, "mkdir -p bin pieces/apps/exo-bot/plugins/+x\n");
        fprintf(cf, "gcc -o bin/exo-bot_manager.+x src/exo-bot_manager.c -pthread\n");
        fprintf(cf, "cp bin/exo-bot_manager.+x pieces/apps/exo-bot/plugins/+x/exo-bot_manager.+x\n");
        fprintf(cf, "for f in src/*.c; do\n");
        fprintf(cf, "  name=$(basename \"$f\" .c)\n");
        fprintf(cf, "  if [ \"$name\" != \"exo-bot_manager\" ]; then\n");
        fprintf(cf, "    gcc -o bin/\"$name\".+x \"$f\"\n");
        fprintf(cf, "  fi\n");
        fprintf(cf, "done\n");
        fprintf(cf, "echo \"Done.\"\n");
        fclose(cf);
        chmod(compile_path, 0755);
    }

    // 2. Copy bot piece
    snprintf(cmd, sizeof(cmd), "cp -r '%s'/* '%s/piece/'", bot_piece_path, export_dir);
    system(cmd);

    // 3. Copy GUI Stack (Parser, Orchestrator, Renderer, Keyboard)
    snprintf(cmd, sizeof(cmd), "cp '%s/pieces/chtpm/plugins/+x/orchestrator.+x' '%s/bin/'", project_root, export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "cp '%s/pieces/chtpm/plugins/+x/chtpm_parser.+x' '%s/bin/'", project_root, export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "cp '%s/pieces/display/plugins/+x/renderer.+x' '%s/bin/'", project_root, export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "cp '%s/pieces/keyboard/plugins/+x/keyboard_input.+x' '%s/bin/'", project_root, export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "cp '%s/pieces/system/prisc/prisc+x' '%s/bin/'", project_root, export_dir);
    system(cmd);
    
    // Copy Agent Layer Ops
    snprintf(cmd, sizeof(cmd), "cp '%s'/projects/bot-editor/ops/+x/fs_* '%s/bin/'", project_root, export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "cp '%s'/projects/bot-editor/ops/+x/ai_* '%s/bin/'", project_root, export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "cp '%s/projects/bot-editor/ops/+x/bot_tune.+x' '%s/bin/'", project_root, export_dir);
    system(cmd);

    // 4. Copy Layouts
    snprintf(cmd, sizeof(cmd), "cp '%s/projects/bot-editor/layouts/editor.chtpm' '%s/layouts/main.chtpm'", project_root, export_dir);
    system(cmd);

    // 5. Initialize Minimal Pieces
    snprintf(cmd, sizeof(cmd), "touch '%s/pieces/display/current_layout.txt'", export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "echo 'main.chtpm' > '%s/pieces/display/current_layout.txt'", export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "touch '%s/pieces/keyboard/history.txt'", export_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "echo 'project_id=exo-bot' > '%s/pieces/apps/player_app/manager/state.txt'", export_dir);
    system(cmd);

    // 6. Create master launcher
    char runner_path[MAX_PATH];
    snprintf(runner_path, sizeof(runner_path), "%s/run_exo.sh", export_dir);
    FILE *rf = fopen(runner_path, "w");
    if (rf) {
        fprintf(rf, "#!/bin/bash\n");
        fprintf(rf, "EXO_ROOT=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n");
        fprintf(rf, "export PATH=\"$EXO_ROOT/bin:$PATH\"\n");
        fprintf(rf, "echo \"Initializing EXO-BOT Shell...\"\n");
        fprintf(rf, "cd \"$EXO_ROOT\"\n");
        fprintf(rf, "./bin/orchestrator.+x ./layouts/main.chtpm\n");
        fclose(rf);
        chmod(runner_path, 0755);
    }

    printf("Bot %s exported successfully to %s\n", bot_id, export_dir);

    free(bot_piece_path);
    return 0;
}
