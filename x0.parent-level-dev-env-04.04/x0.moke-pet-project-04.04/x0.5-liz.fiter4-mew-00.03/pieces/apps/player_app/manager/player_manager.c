#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

// TPM Player Runner (v1.5 - PRISC BOOTSTRAP)
// Responsibility: Minimal C wrapper to launch PAL-driven boot script.

char project_root[1024] = ".";

void resolve_root() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[4096];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                v[strcspn(v, "\n\r")] = 0;
                strncpy(project_root, v, 1023); break;
            }
        }
        fclose(kvp);
    }
}

int main(int argc, char* argv[]) {
    resolve_root();
    
    // Default boot script
    char *boot_script = "pieces/apps/player_app/PAL/boot.asm";
    if (argc > 1) boot_script = argv[1];

    char *cmd = NULL;
    asprintf(&cmd, "'%s/prisc-xpanse+4]SHIP/prisc+x' '%s/%s' '' '' '%s/pieces/apps/player_app/manager/player_ops.txt'", 
             project_root, project_root, boot_script, project_root);

    if (cmd) {
        printf("PLAYR: Bootstrapping PAL Engine...\n");
        system(cmd);
        free(cmd);
    }

    return 0;
}
