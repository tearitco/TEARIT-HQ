/* auto_ctl - the two switches and the kill file.   auto_ctl --user DIR status | on | off | studio-on | studio-off | stop | clear-stop  [--human]
 * on, studio-on, clear-stop need --human (a person's arm action; the conductor never passes it. This is a convention that keeps the loop from enabling itself, NOT a security boundary: a real enforcement is the auto-hq button, slice A7).
 * studio-on needs state=on. off also turns studio off. stop creates <user>/auto/STOP (every loop checks it before each action). Every change is one ledger row. Exit 0 ok, 2 usage, 4 refused. */
#include "auto_common.c"
static void set_key(const char *user, const char *key, const char *val) { char p[PL], t[PL], ln[PL], cp[PL]; snprintf(p, sizeof p, "%s/auto.pdl", user); snprintf(t, sizeof t, "%s/auto.pdl.tmp", user); FILE *o = fopen(t, "w"); if (!o) return; FILE *f = fopen(p, "r"); int done = 0, any = 0;
    if (f) { while (fgets(ln, sizeof ln, f)) { any = 1; snprintf(cp, sizeof cp, "%s", ln); char *fd[4]; int n = ac_split(cp, fd, 4); if (n >= 3 && !strcmp(fd[0], "AUTO") && !strcmp(fd[1], key)) { if (!done) fprintf(o, "AUTO | %s | %s\n", key, val); done = 1; } else fputs(ln, o); } fclose(f); }
    if (!any) fputs("SECTION | KEY | VALUE\n------------------------------\n", o);
    if (!done) fprintf(o, "AUTO | %s | %s\n", key, val); fclose(o); rename(t, p); }
int main(int argc, char **argv) {
    const char *user = NULL, *cmd = NULL; int human = 0;
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--user") && i + 1 < argc) user = argv[++i]; else if (!strcmp(argv[i], "--human")) human = 1; else if (!cmd) cmd = argv[i]; else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; } }
    if (!user || !cmd) { fprintf(stderr, "usage: auto_ctl --user DIR status|on|off|studio-on|studio-off|stop|clear-stop [--human]\n"); return 2; } mkdir(user, 0700);
    if (!strcmp(cmd, "status")) { printf("AUTO | state | %s\nAUTO | studio | %s\nAUTO | stop_file | %s\n", ac_is_on(user, "state") ? "on" : "off", ac_is_on(user, "studio") ? "on" : "off", ac_stop_present(user) ? "present" : "absent"); return 0; }
    if ((!strcmp(cmd, "on") || !strcmp(cmd, "studio-on") || !strcmp(cmd, "clear-stop")) && !human) { ac_ledger(user, "CTL | %ld | %s | REFUSED | needs --human", (long)time(NULL), cmd); fprintf(stderr, "refused: %s needs --human (a person's action)\n", cmd); return 4; }
    if (!strcmp(cmd, "on")) { if (ac_stop_present(user)) { fprintf(stderr, "refused: STOP file present (clear-stop first)\n"); return 4; } set_key(user, "state", "on"); }
    else if (!strcmp(cmd, "off")) { set_key(user, "state", "off"); set_key(user, "studio", "off"); }
    else if (!strcmp(cmd, "studio-on")) { if (!ac_is_on(user, "state")) { ac_ledger(user, "CTL | %ld | studio-on | REFUSED | auto is off", (long)time(NULL)); fprintf(stderr, "refused: studio needs auto=on\n"); return 4; } set_key(user, "studio", "on"); }
    else if (!strcmp(cmd, "studio-off")) set_key(user, "studio", "off");
    else if (!strcmp(cmd, "stop")) { char d[PL], p[PL]; snprintf(d, sizeof d, "%s/auto", user); mkdir(d, 0700); snprintf(p, sizeof p, "%s/STOP", d); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%ld\n", (long)time(NULL)); fclose(f); } set_key(user, "studio", "off"); }
    else if (!strcmp(cmd, "clear-stop")) { char p[PL]; snprintf(p, sizeof p, "%s/auto/STOP", user); unlink(p); }
    else { fprintf(stderr, "unknown command %s\n", cmd); return 2; }
    ac_ledger(user, "CTL | %ld | %s | ok | %s", (long)time(NULL), cmd, human ? "human" : "-"); printf("ok %s\n", cmd); return 0;
}
