/* apply_theme_op.c — real, standalone TPMOS-shaped op (2026-08-16,
 * Stage 5 §5d.3 step 1 real starter-app proof, khtpm-merge-how2.md).
 * Was `apply_theme()`, real business logic baked directly inside
 * khtpm_taskbar_settings_render.c's own render loop — the exact class
 * of thing Stage 5 needs OUT of every app's renderer before one shared
 * binary is possible (a shared binary can't call an app-specific
 * function by name). A real, discrete, one-shot action (read state,
 * rewrite state, spawn a restart script, exit) — matches
 * `1.TPMOS_c_+rmmp.0103.0001/projects/fuzz-op/ops/toggle_clock.c`'s
 * own real shape exactly, so this is a standalone op binary (invoked
 * via `system()`), NOT a persistent `<module>` (which is for ongoing,
 * long-running logic — this has none).
 *
 * Usage: apply_theme_op <house_root> <bg_hex> <fg_hex>
 *
 * Real behavior, unchanged from the original in-process version:
 * writes ONLY the bg/fg COLOR keys into livedesk_theme.pdl, preserving
 * any other COLOR rows already there, then spawns
 * `run_khtpm_strip.sh new` to restart the taskbar with the new theme. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* macOS leg (2026-08-22): no `setsid` binary on macOS — drop the prefix
 * there (nohup+& already detaches for this pattern); Linux byte-identical. */
#ifdef __APPLE__
#define KTB_SETSID ""
#else
#define KTB_SETSID "setsid "
#endif


#define PATH_BUF 4096

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: apply_theme_op <house_root> <bg_hex> <fg_hex>\n");
        return 1;
    }
    const char *house_root = argv[1];
    const char *bg_hex = argv[2];
    const char *fg_hex = argv[3];

    char path[PATH_BUF], tmp[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", house_root);
    snprintf(tmp, sizeof(tmp), "%s/#.desktop/livedesk_theme.pdl.tmp", house_root);

    char kept[8][256];
    int n_kept = 0;
    FILE *rf = fopen(path, "r");
    if (rf) {
        char line[256];
        while (fgets(line, sizeof(line), rf) && n_kept < 8) {
            if (strncmp(line, "COLOR", 5) != 0) continue;
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = strchr(p, '|');
            if (!end) continue;
            char key[16];
            size_t klen = (size_t)(end - p);
            while (klen && p[klen - 1] == ' ') klen--;
            if (klen >= sizeof(key)) continue;
            memcpy(key, p, klen); key[klen] = 0;
            if (strcmp(key, "bg") == 0 || strcmp(key, "fg") == 0) continue;
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(kept[n_kept], sizeof(kept[n_kept]), "%s", line);
            n_kept++;
        }
        fclose(rf);
    }

    FILE *wf = fopen(tmp, "w");
    if (!wf) { fprintf(stderr, "apply_theme_op: cannot write %s\n", tmp); return 1; }
    fputs("SECTION      | KEY                | VALUE\n----------------------------------------\n", wf);
    fprintf(wf, "COLOR        | bg                   | %s\n", bg_hex);
    fprintf(wf, "COLOR        | fg                   | %s\n", fg_hex);
    for (int i = 0; i < n_kept; i++) fprintf(wf, "%s\n", kept[i]);
    fclose(wf);
    remove(path);
    rename(tmp, path);

    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), KTB_SETSID "nohup sh '%s/*.monads/*.livedesk-taskbar/ops/run_khtpm_strip.sh' new >/dev/null 2>&1 &",
             house_root);
    int rc = system(cmd);
    (void)rc;

    fprintf(stderr, "apply_theme_op: wrote %s (bg=%s fg=%s)\n", path, bg_hex, fg_hex);
    return 0;
}
