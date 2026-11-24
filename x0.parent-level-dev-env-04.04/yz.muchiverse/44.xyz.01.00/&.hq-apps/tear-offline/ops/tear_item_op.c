/* tear_item_op - writes ONE command into tear-offline's action file (the layout's <item>/<cli_io> actions call this; no shell logic).
 *   item  : tear_item_op.+x '<VERB ARG>' <pkg> <house>
 *   cli_io: tear_item_op.+x <verb> <pkg> <house> '<typed text>'      (verb in send|post|import|wallet; the renderer appends pkg house text)
 * The manager (tear_offline_manager) polls <pkg>/tear_action.txt for a higher seq. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tear_item_op.+x tear_item_op.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    char cmd[1024]; const char *v = argv[1], *pkg = argv[2];
    if (argc >= 5 && argv[4][0]) {
        const char *t = argv[4];
        if (!strcmp(v, "send") || !strcmp(v, "post") || !strcmp(v, "import") || !strcmp(v, "wallet")) {
            char verb[16]; snprintf(verb, sizeof verb, "%s", v); for (char *p = verb; *p; p++) *p = (char)(*p - 32 * (*p >= 'a' && *p <= 'z'));
            snprintf(cmd, sizeof cmd, "%s:%s", verb, t); }
        else return 0;
    } else if (!strncmp(v, "TAB ", 4)) snprintf(cmd, sizeof cmd, "TAB:%s", v + 4);
    else if (!strcmp(v, "MINE") || !strcmp(v, "FAUCET") || !strcmp(v, "EXPORT") || !strcmp(v, "FETCH") || !strcmp(v, "REFRESH")) snprintf(cmd, sizeof cmd, "%s", v);
    else return 0;
    for (char *p = cmd; *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';
    char sp[1100], ap[1100]; snprintf(sp, sizeof sp, "%s/tear.seq", pkg); snprintf(ap, sizeof ap, "%s/tear_action.txt", pkg);
    long n = 0; FILE *f = fopen(sp, "r"); if (f) { if (fscanf(f, "%ld", &n) != 1) n = 0; fclose(f); }
    n++; f = fopen(sp, "w"); if (f) { fprintf(f, "%ld\n", n); fclose(f); }
    f = fopen(ap, "w"); if (!f) return 1; fprintf(f, "seq=%ld\ncmd=%s\n", n, cmd); fclose(f); return 0; }
