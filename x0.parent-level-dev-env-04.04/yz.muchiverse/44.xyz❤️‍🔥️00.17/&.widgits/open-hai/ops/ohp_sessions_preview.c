/* ohp_sessions_preview.c - real, standalone PROOF (2026-08-31,
 * xperiments/khtpm-generic-dispatch-design.md §5, "scoped first slice,
 * prove the pattern" for open-hai's own consolidation). NOT wired into
 * the real, live open-hai app (button.sh/chat_button.sh are UNCHANGED -
 * zero risk to the real chat tool) - this is a standalone demonstration
 * that open-hai's own real session data CAN render through the shared
 * khtpm_core_render.+x pipeline with ZERO new C code in the shared
 * renderer, same real lesson TPMOS-COMPLIANCE-DEBT.md §5's choice-
 * picker consolidation just proved.
 *
 * Reads the real, live &.widgits/open-hai/state/sessions.state.txt
 * (real "<session_dir>|<label>" lines, khtpm_open_hai_render.c's own
 * real format - confirmed by direct read of that file's
 * load_sessions_state_if_changed(), not guessed) and generates a real,
 * temporary .chtpm - one <item> per real session - then launches the
 * SAME shared renderer every other khtpm window uses. Each item's
 * action= here is a real, harmless no-op (echoes the session dir to
 * stderr) - this proof is read-only by design; wiring a real "switch
 * to this session" action is real future work, not needed to prove the
 * rendering half.
 *
 * Usage: ohp_sessions_preview.+x <house_root>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_BUF 4352

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

static void shell_escape_squote(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 5 < outsz; p++) {
        if (*p == '\'') { memcpy(out + o, "'\\''", 4); o += 4; }
        else out[o++] = (char)*p;
    }
    out[o] = '\0';
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <house_root>\n", argv[0]); return 1; }
    const char *house_root = argv[1];

    char sessions_path[PATH_BUF];
    snprintf(sessions_path, sizeof(sessions_path), "%s/&.widgits/open-hai/state/sessions.state.txt", house_root);
    char renderer_path[PATH_BUF];
    snprintf(renderer_path, sizeof(renderer_path), "%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x", house_root);

    char chtpm_path[PATH_BUF];
    snprintf(chtpm_path, sizeof(chtpm_path), "/tmp/ohp_sessions_preview_%d.chtpm", (int)getpid());
    FILE *out = fopen(chtpm_path, "w");
    if (!out) { fprintf(stderr, "ohp_sessions_preview: cannot write %s\n", chtpm_path); return 1; }
    fprintf(out, "<window label=\"open-hai sessions (real data, shared renderer proof)\" class=\"\">\n  <page name=\"main\">\n");

    FILE *sf = fopen(sessions_path, "r");
    int n = 0;
    if (sf) {
        char line[PATH_BUF + 512];
        while (fgets(line, sizeof(line), sf)) {
            line[strcspn(line, "\r\n")] = '\0';
            char *bar = strchr(line, '|');
            if (!bar) continue;
            *bar = '\0';
            const char *dir = line;
            const char *label = bar + 1;
            char label_esc[600], dir_esc_sq[PATH_BUF * 2];
            xml_escape(label, label_esc, sizeof(label_esc));
            shell_escape_squote(dir, dir_esc_sq, sizeof(dir_esc_sq));
            char shell_raw[PATH_BUF * 2 + 64], shell_esc[PATH_BUF * 2 + 128];
            snprintf(shell_raw, sizeof(shell_raw), "/bin/sh -c 'echo picked session: %s 1>&2'", dir_esc_sq);
            xml_escape(shell_raw, shell_esc, sizeof(shell_esc));
            fprintf(out, "    <item id=\"s%d\" label=\"%s\" action=\"%s\"/>\n", n, label_esc, shell_esc);
            n++;
            if (n >= 200) break; /* real, generous cap - this is a proof, not the real app's own MAX_SESSIONS */
        }
        fclose(sf);
    }
    fprintf(out, "    <item id=\"close\" label=\"X (close proof)\" action=\"CLOSE\"/>\n");
    fprintf(out, "  </page>\n</window>\n");
    fclose(out);

    fprintf(stderr, "ohp_sessions_preview: %d real session(s) loaded from %s\n", n, sessions_path);
    execl(renderer_path, renderer_path, house_root, chtpm_path, (char *)NULL);
    fprintf(stderr, "ohp_sessions_preview: execl failed for %s\n", renderer_path);
    return 1;
}
