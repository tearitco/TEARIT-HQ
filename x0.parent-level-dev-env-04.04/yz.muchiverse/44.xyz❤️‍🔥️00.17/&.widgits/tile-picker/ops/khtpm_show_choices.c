/* khtpm_show_choices - real, generic "Show Choices" event command,
 * 2026-08-05. Real RPG Maker command (confirmed,
 * #.ref/menu/event.commands.1.txt), first real proof-of-concept target:
 * MUCHI_RANCHER's own Change Gold. This is the SECOND, harder half -
 * Show Choices, real branching - built for a real book-stack reading
 * app, reusable by ANY entity's own event.pal.
 *
 * REAL FIX 2026-08-16: forked khtpm_choice_picker.+x directly (a
 * standalone binary hand-building its own Elem tree - see that file's
 * own real, condemned history in TPMOS-COMPLIANCE-DEBT.md §5 and
 * CENTROID_GOLD_STD.md §3 rule 1's correction).
 *
 * REAL FIX 2026-08-31 (direct instruction: "can we do the first 2
 * first? (open-hai) and choice parser?" - the real khtpm-generic-
 * dispatch-design.md §5 consolidation, choice-picker first): retired
 * khtpm_choice_picker.c's hand-built Elem tree entirely. This op now
 * generates a REAL, temporary `.chtpm` file (real `<item action="...">`
 * markup, one per real choice) and launches the SAME shared
 * khtpm_core_render.+x every other khtpm window uses - zero new C code
 * needed in the shared renderer, because its own generic default page/
 * item path (the SAME one taskbar-settings/entity-right-click-menus
 * already use) already treats any unrecognized `action=` as a real
 * shell command and unconditionally quits after running it
 * (khtpm_core_render.c's own dispatch(), confirmed by direct read:
 * "real menus close after a real action fires"). Each generated
 * `<item>`'s action is a tiny real shell one-liner that writes the
 * picked token to result_path - the SAME real result-file contract
 * this op's own callers (dispatch.sh etc) already depend on, unchanged.
 * A synthesized Cancel row (`action="CLOSE"`) is added if the caller's
 * own choices_file didn't already include one - same real UX
 * khtpm_choice_picker.c's own synthesized-Cancel fix already
 * established, ported here rather than lost.
 *
 * Usage: khtpm_show_choices.+x <entity_package_dir> <choices_objects_file>
 *   entity_package_dir: kept for real positional compatibility with
 *   every existing caller (dispatch.sh etc) - read for real now (its
 *   own desktop_pos.txt still positions the window near the entity).
 *   choices_objects_file: a real, flat "OBJECT | label=.. | action=.."
 *   list (no PAGE header needed - always exactly one page). The caller
 *   is responsible for generating this file (real content, not
 *   hardcoded here) before calling this op. `action=` here is the
 *   real, caller-defined TOKEN to report back (e.g. "feed"), not a
 *   shell command - this op wraps it into a real one before handing it
 *   to the shared renderer.
 *
 * Real result file: a fresh temp file under /tmp, cleaned up after.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h> /* strcasecmp() */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include "self_exe.h" /* macOS leg: portable /proc/self/exe replacement */

#define PATH_BUF 4352
#define POLL_TIMEOUT_SEC 120

/* Real, minimal XML-attribute escaping - choice labels are free real
 * text (book titles, dialogue lines), '&'/'"'/'<'/'>' are the only
 * real risk inside a double-quoted attribute value. */
static void xml_escape(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 6 < outsz; p++) {
        switch (*p) {
            case '&': memcpy(out + o, "&amp;", 5); o += 5; break;
            case '"': memcpy(out + o, "&quot;", 6); o += 6; break;
            case '<': memcpy(out + o, "&lt;", 4); o += 4; break;
            case '>': memcpy(out + o, "&gt;", 4); o += 4; break;
            default: out[o++] = (char)*p; break;
        }
    }
    out[o] = '\0';
}

/* Real, minimal single-quote shell escaping for embedding the picked
 * token inside a single-quoted shell string ('\'' is the only real
 * escape needed there). */
static void shell_escape_squote(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 5 < outsz; p++) {
        if (*p == '\'') { memcpy(out + o, "'\\''", 4); o += 4; }
        else out[o++] = (char)*p;
    }
    out[o] = '\0';
}

static void trim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = '\0';
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: khtpm_show_choices.+x <entity_package_dir> <choices_objects_file>\n");
        return 1;
    }
    const char *package_dir = argv[1];
    const char *choices_file = argv[2];

    char pos_path[PATH_BUF];
    snprintf(pos_path, sizeof(pos_path), "%s/desktop_pos.txt", package_dir);
    int pos_x = -1, pos_y = -1;
    FILE *pf = fopen(pos_path, "r");
    if (pf) {
        char pline[128];
        while (fgets(pline, sizeof(pline), pf)) {
            if (strncmp(pline, "x=", 2) == 0) pos_x = atoi(pline + 2);
            else if (strncmp(pline, "y=", 2) == 0) pos_y = atoi(pline + 2);
        }
        fclose(pf);
    }
    char pos_x_str[16], pos_y_str[16];
    if (pos_x >= 0) snprintf(pos_x_str, sizeof(pos_x_str), "%d", pos_x);
    if (pos_y >= 0) snprintf(pos_y_str, sizeof(pos_y_str), "%d", pos_y);

    char result_path[PATH_BUF];
    snprintf(result_path, sizeof(result_path), "/tmp/khtpm_choice_result_%d.txt", (int)getpid());
    unlink(result_path);

    /* Real self-relative resolution to find both this house's root
     * (this binary's own real, fixed location is
     * <house_root>/&.widgits/tile-picker/ops/+x/khtpm_show_choices.+x
     * - 4 real path components up from here) and the shared renderer
     * binary next to house_root's own real taskbar ops dir. */
    char self_path[PATH_BUF];
    ssize_t slen = self_exe_readlink(self_path, sizeof(self_path));
    if (slen <= 0) { fprintf(stderr, "khtpm_show_choices: cannot resolve own path\n"); return 1; }
    self_path[slen] = '\0';
    /* REAL BUG FIX 2026-08-31, found live (execl() below was silently
     * failing - a wrong path, one directory level too shallow, landing
     * on &.widgits/ instead of house_root, since "%s/../../../.."
     * applied 4 real ".." to the FULL FILE path, and the filename
     * itself consumes one of those 4 levels - self_path must be
     * reduced to its own DIRECTORY first, same real idiom this file's
     * own picker_path resolution below already uses). */
    char self_dir[PATH_BUF];
    snprintf(self_dir, sizeof(self_dir), "%s", self_path);
    { char *last_slash = strrchr(self_dir, '/'); if (last_slash) *last_slash = '\0'; }
    char house_root[PATH_BUF];
    snprintf(house_root, sizeof(house_root), "%s/../../../..", self_dir);
    { char resolved[PATH_BUF]; if (realpath(house_root, resolved)) snprintf(house_root, sizeof(house_root), "%s", resolved); }
    char renderer_path[PATH_BUF];
    snprintf(renderer_path, sizeof(renderer_path),
             "%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x", house_root);

    /* Real, generated .chtpm - one <item> per real choice row, each
     * action= a real shell one-liner writing the caller's own real
     * token to result_path. Written to a fresh temp file, cleaned up
     * after the picker exits (its own real content is per-invocation,
     * never worth keeping). */
    char chtpm_path[PATH_BUF];
    snprintf(chtpm_path, sizeof(chtpm_path), "/tmp/khtpm_show_choices_%d.chtpm", (int)getpid());
    FILE *out = fopen(chtpm_path, "w");
    if (!out) { fprintf(stderr, "khtpm_show_choices: cannot write %s\n", chtpm_path); return 1; }
    fprintf(out, "<window label=\"Choose\" class=\"\">\n  <page name=\"main\">\n");

    FILE *cf = fopen(choices_file, "r");
    int n_items = 0, has_cancel = 0;
    if (cf) {
        char line[1024];
        while (fgets(line, sizeof(line), cf)) {
            trim(line);
            if (strncmp(line, "OBJECT", 6) != 0) continue;
            char *bar = strchr(line, '|');
            if (!bar) continue;
            char *p = bar + 1;
            char *label_kv = strstr(p, "label=");
            char *action_kv = strstr(p, "action=");
            if (!label_kv || !action_kv) continue;
            char *label_start = label_kv + 6;
            char *label_end = strchr(label_start, '|');
            char label_raw[300];
            size_t llen = label_end ? (size_t)(label_end - label_start) : strlen(label_start);
            while (llen && label_start[llen - 1] == ' ') llen--;
            if (llen >= sizeof(label_raw)) llen = sizeof(label_raw) - 1;
            memcpy(label_raw, label_start, llen);
            label_raw[llen] = '\0';

            char *action_start = action_kv + 7;
            while (*action_start == ' ') action_start++;
            char token_raw[1600];
            snprintf(token_raw, sizeof(token_raw), "%s", action_start);

            if (strcasecmp(label_raw, "Cancel") == 0) has_cancel = 1;

            char label_esc[400], token_esc_sq[3300];
            xml_escape(label_raw, label_esc, sizeof(label_esc));
            shell_escape_squote(token_raw, token_esc_sq, sizeof(token_esc_sq));

            char shell_action_raw[3600], shell_action_esc[3700];
            snprintf(shell_action_raw, sizeof(shell_action_raw),
                     "/bin/sh -c 'printf %%s '\\''%s'\\'' > '\\''%s'\\'''", token_esc_sq, result_path);
            xml_escape(shell_action_raw, shell_action_esc, sizeof(shell_action_esc));

            fprintf(out, "    <item id=\"c%d\" label=\"%s\" action=\"%s\"/>\n", n_items, label_esc, shell_action_esc);
            n_items++;
        }
        fclose(cf);
    }
    if (!has_cancel) fprintf(out, "    <item id=\"cancel\" label=\"Cancel\" action=\"CLOSE\"/>\n");
    fprintf(out, "  </page>\n</window>\n");
    fclose(out);

    if (n_items == 0) {
        fprintf(stderr, "khtpm_show_choices: no choices loaded from %s\n", choices_file);
        unlink(chtpm_path);
        return 1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); close(devnull); }
        if (pos_x >= 0 && pos_y >= 0)
            execl(renderer_path, renderer_path, house_root, chtpm_path, pos_x_str, pos_y_str, (char *)NULL);
        else
            execl(renderer_path, renderer_path, house_root, chtpm_path, (char *)NULL);
        /* REAL FIX 2026-08-31, found live: execl() failing here used to
         * be silent (a bare _exit(1)) - the parent's own poll loop just
         * reported "no pick made (cancelled or timed out)", identical
         * to a real cancel, making a real path bug (house_root
         * resolved wrong) indistinguishable from a human pressing
         * Escape. stderr is real here (not redirected, only stdout is
         * above) - safe to report. */
        fprintf(stderr, "khtpm_show_choices: execl failed for %s (errno-based reason: %s)\n", renderer_path, strerror(errno));
        _exit(1);
    } else if (pid < 0) {
        fprintf(stderr, "khtpm_show_choices: fork failed\n");
        unlink(chtpm_path);
        return 1;
    }

    int waited = 0;
    char picked[256] = "";
    while (waited < POLL_TIMEOUT_SEC * 10) {
        FILE *f = fopen(result_path, "r");
        if (f) {
            if (fgets(picked, sizeof(picked), f)) {
                picked[strcspn(picked, "\r\n")] = '\0';
            }
            fclose(f);
            if (picked[0]) break;
        }
        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) break;
        usleep(100000);
        waited++;
    }
    waitpid(pid, NULL, WNOHANG);
    unlink(result_path);
    unlink(chtpm_path);

    if (!picked[0]) {
        fprintf(stderr, "khtpm_show_choices: no pick made (cancelled or timed out)\n");
        return 2;
    }
    printf("%s\n", picked);
    return 0;
}
