/* swatch_picker_manager.c — 2-phase pick. argv[1]=house_root. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATH_BUF 4096
#define MAX_SWATCHES 64

/* REAL, NEW 2026-09-04, direct live request ("can we add grey and
 * brown to swatch colors... that shouldn't be hardcoded, should be
 * from layout/module") - the color list itself is no longer a
 * compiled-in array. Read once at startup from the house's own
 * swatches.pdl (next to this manager's own launcher, same real
 * "static template + real manager, no hardcoding" rule this whole
 * house is built on, applied to color DATA this time instead of
 * window layout). taskbar_settings_projector.c reads the SAME file
 * independently, for the same reason - one shared data source, two
 * real consumers, matching the house's own PICK:<n> convention
 * (a swatch's line number IS its index, unchanged from the old
 * hardcoded array's own ordering). */
static char g_hex_buf[MAX_SWATCHES][8];
static const char *g_hex[MAX_SWATCHES];
static int g_n_swatches = 0;

static void load_swatches(const char *house) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/&.widgits/taskbar-settings/swatches.pdl", house);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (g_n_swatches < MAX_SWATCHES && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char *bar = strchr(line, '|');
        if (!bar) continue;
        char *hex = bar + 1;
        hex[strcspn(hex, "\r\n")] = '\0';
        if (hex[0] != '#' || strlen(hex) != 7) continue; /* honest skip - malformed row */
        snprintf(g_hex_buf[g_n_swatches], sizeof(g_hex_buf[0]), "%s", hex);
        g_hex[g_n_swatches] = g_hex_buf[g_n_swatches];
        g_n_swatches++;
    }
    fclose(f);
}

static void write_state(const char *state_path, int phase, int bg, int fg, int apply) {
    FILE *out = fopen(state_path, "w");
    if (!out) return;
    fprintf(out, "phase=%d\nbg=%d\nfg=%d\napply=%d\n", phase, bg, fg, apply);
    fclose(out);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *house = argv[1];
    load_swatches(house);
    char action_path[PATH_BUF], state_path[PATH_BUF];
    snprintf(action_path, sizeof(action_path), "%s/#.desktop/taskbar_settings_action.txt", house);
    snprintf(state_path, sizeof(state_path), "%s/#.desktop/taskbar_settings_state.txt", house);

    /* Wipe leftover PICK/CLOSE from a prior instance so launch is always phase 0. */
    FILE *wipe = fopen(action_path, "w");
    if (wipe) { fputs("seq=0\n", wipe); fclose(wipe); }
    write_state(state_path, 0, -1, -1, 0);

    int phase = 0, bg = -1, fg = -1;
    unsigned last_seq = 0;
    unsigned long last_ck = 0;
    {
        FILE *af = fopen(action_path, "r");
        if (af) {
            unsigned long ck = 5381; int ch;
            while ((ch = fgetc(af)) != EOF) ck = ((ck << 5) + ck) + (unsigned char)ch;
            fclose(af);
            last_ck = ck;
        }
    }

    for (;;) {
        FILE *af = fopen(action_path, "r");
        if (af) {
            unsigned long ck = 5381;
            char buf[256]; size_t n = fread(buf, 1, sizeof(buf)-1, af);
            buf[n] = 0; fclose(af);
            for (size_t i = 0; i < n; i++) ck = ((ck << 5) + ck) + (unsigned char)buf[i];
            if (ck != last_ck) {
                last_ck = ck;
                unsigned seq = 0;
                char *seqp = strstr(buf, "seq=");
                if (seqp) seq = (unsigned)atoi(seqp + 4);
                if (seq != 0 && seq <= last_seq) continue;
                if (seq != 0) last_seq = seq;

                char *pick = strstr(buf, "PICK:");
                char *cls = strstr(buf, "CLOSE");
                if (cls && (!pick || cls < pick)) return 0;
                if (pick) {
                    int idx = atoi(pick + 5);
                    if (idx >= 0 && idx < g_n_swatches) {
                        if (phase == 0) { bg = idx; phase = 1; }
                        else if (phase == 1) { fg = idx; phase = 2; }
                    }
                }
                int apply = (phase >= 2 && bg >= 0 && fg >= 0) ? 1 : 0;
                write_state(state_path, phase, bg, fg, apply);
                if (apply) {
                    char cmd[PATH_BUF * 3];
                    snprintf(cmd, sizeof(cmd),
                             "'%s/*.monads/*.livedesk-taskbar/ops/+x/apply_theme_op.+x' '%s' '%s' '%s'",
                             house, house, g_hex[bg], g_hex[fg]);
                    (void)system(cmd);
                    return 0;
                }
            }
        }
        usleep(50000);
    }
}
