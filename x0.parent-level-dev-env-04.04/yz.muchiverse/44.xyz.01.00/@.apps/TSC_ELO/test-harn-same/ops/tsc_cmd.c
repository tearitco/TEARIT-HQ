/* tsc_cmd - test-harness op. Appends ONE command line to a TSC_ELO
 * host session's widget_cmds/inbox.txt - the SAME cmd bus the setup
 * widget drives via setup_enqueue_cmd, written directly here so a
 * harness can drive scaffold/answer states without the GL window
 * (REAL_KEYS states use tk_inject_key into the widget instead). This
 * is the "host cmd-bus" side of the harness; tsc_setup drains it.
 *
 * Self-contained, no shared headers.
 * Usage: tsc_cmd.+x <host_session_dir> <cmd> [arg]
 *   cmd: MATCH|RATING|PLAYER|START|PING|CHALLENGE|ACCEPT|MOVE|RESIGN
 *   arg: e.g. MOVE strike | MATCH HvC | PLAYER Alice
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 4352

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tsc_cmd.+x <host_session_dir> <cmd> [arg]\n");
        return 1;
    }
    const char *session = argv[1];
    const char *cmd = argv[2];
    const char *arg = (argc >= 4) ? argv[3] : "";

    char inbox[PATH_BUF];
    snprintf(inbox, sizeof(inbox), "%s/pieces/system/widget_cmds/inbox.txt", session);

    char line[2048];
    if (strcmp(cmd, "START") == 0 || strcmp(cmd, "PING") == 0 ||
        strcmp(cmd, "RESIGN") == 0) {
        snprintf(line, sizeof(line), "%s\n", cmd);
    } else if (strcmp(cmd, "CHALLENGE") == 0) {
        snprintf(line, sizeof(line), "PVP:CHALLENGE\n");
    } else if (strcmp(cmd, "ACCEPT") == 0) {
        snprintf(line, sizeof(line), "PVP:ACCEPT\n");
    } else if (strcmp(cmd, "MATCH") == 0 || strcmp(cmd, "RATING") == 0 ||
               strcmp(cmd, "PLAYER") == 0 || strcmp(cmd, "MOVE") == 0) {
        if (!arg[0]) {
            fprintf(stderr, "%s needs an argument\n", cmd);
            return 1;
        }
        snprintf(line, sizeof(line), "%s:%s\n", cmd, arg);
    } else {
        fprintf(stderr, "unknown cmd %s\n", cmd);
        return 1;
    }

    FILE *f = fopen(inbox, "a");
    if (!f) {
        fprintf(stderr, "tsc_cmd: cannot open %s\n", inbox);
        return 1;
    }
    fputs(line, f);
    fclose(f);
    printf("CMD %s", line);
    return 0;
}
