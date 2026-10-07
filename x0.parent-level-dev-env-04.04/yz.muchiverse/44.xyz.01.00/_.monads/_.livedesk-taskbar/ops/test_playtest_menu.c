/* test_playtest_menu.c - the Player menu's play-test row and the play-mode file transitions, on a SCRATCH house. Not a click test (the handler needs a live taskbar state).
 * build+run (from this ops dir): gcc -std=c11 -O2 -w -I "../../../&.widgits/_shared-lib" -I . -o /tmp/tpm test_playtest_menu.c && /tmp/tpm */
#include "khtpm_taskbar_manager.c"
#include <unistd.h>
#include <sys/stat.h>
static int fails;
#define CK(name, cond) do { if (cond) printf("PASS|%s\n", name); else { printf("FAIL|%s\n", name); fails = 1; } } while (0)
static const char *label_of(HQMenuItem *m, int n, const char *cmd) { for (int i = 0; i < n; i++) if (!strcmp(m[i].command, cmd)) return m[i].label; return ""; }
static void rd(const char *h, char *out, size_t n) { char p[1200]; snprintf(p, sizeof p, "%s/#.desktop/khtpm_play_mode.state.txt", h); FILE *f = fopen(p, "r"); size_t k = 0; out[0] = 0; if (f) { k = fread(out, 1, n - 1, f); out[k] = 0; fclose(f); } }
int main(void) {
    char h[] = "/tmp/pt_house_XXXXXX"; HQMenuItem m[40]; int n; char buf[200], d[1200];
    if (!mkdtemp(h)) return 2;
    snprintf(d, sizeof d, "%s/#.desktop", h); mkdir(d, 0755);
    n = livedesk_build_player_menu(h, m, 40);
    CK("no file: play OFF, play-test OFF", !strcmp(label_of(m, n, "livedesk:play-toggle"), "1.play: OFF") && !strcmp(label_of(m, n, "livedesk:playtest-toggle"), "play-test: OFF"));
    CK("play-test row sits right after the play row", n > 2 && !strcmp(m[0].command, "livedesk:play-toggle") && !strcmp(m[1].command, "livedesk:playtest-toggle"));
    CK("other rows still present (stop, reset, save-game)", *label_of(m, n, "livedesk:play-stop") && *label_of(m, n, "livedesk:reset-entities"));
    khtpm_save_playtest(h, 1); rd(h, buf, sizeof buf);
    CK("play-test on writes mode=on + playtest=1", !strcmp(buf, "mode=on\nplaytest=1\n"));
    CK("older reader (first line, strstr mode=on) sees ON", khtpm_load_play_mode(h) == 1);
    n = livedesk_build_player_menu(h, m, 40);
    CK("menu: play ON, play-test ON", !strcmp(label_of(m, n, "livedesk:play-toggle"), "1.play: ON") && !strcmp(label_of(m, n, "livedesk:playtest-toggle"), "play-test: ON"));
    khtpm_save_playtest(h, 0); rd(h, buf, sizeof buf);
    CK("play-test off = plain play (mode=on only)", !strcmp(buf, "mode=on\n") && khtpm_load_play_mode(h) == 1 && !khtpm_load_playtest(h));
    khtpm_save_playtest(h, 1); khtpm_save_play_mode(h, 0); rd(h, buf, sizeof buf);
    CK("play toggle/stop (mode=off) clears play-test", !strcmp(buf, "mode=off\n") && !khtpm_load_playtest(h) && !khtpm_load_play_mode(h));
    khtpm_save_play_mode(h, 1); CK("plain play does not set play-test", !khtpm_load_playtest(h));
    snprintf(d, sizeof d, "rm -rf %s", h); if (system(d)) {}
    printf("VERDICT|%s\n", fails ? "FAIL" : "PASS");
    return fails;
}
