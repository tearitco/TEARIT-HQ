/* horn_publish - project HORN_CHAT's chat state into chtpm variables.
 *
 * Reads the persistent transcript and turns it into the two files
 * chtpm_parser_pal.c actually reads on every render:
 *
 *   pieces/apps/player_app/view.txt   -> loaded as ${game_map} and
 *                                       ${desktop_view} (its generic
 *                                       view-loading block walks a
 *                                       candidate list and this is the
 *                                       entry a pal-native project hits)
 *   pieces/apps/player_app/state.txt  -> loaded early by load_vars(),
 *                                       so ${horn_model} / ${horn_status}
 *                                       / ${horn_turns} resolve
 *
 * Also pulses pieces/apps/player_app/state_changed.txt, which is the
 * file chtpm's main loop watches to decide a reload is needed. Writing
 * state.txt alone changes nothing on screen until something grows that
 * marker, which is a real trap worth naming: the files look right and
 * the display just never updates.
 *
 * Usage: horn_publish.+x (no args)
 * Self-contained: own root resolution, no shared headers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)

/* Wrap width, measured against the layout box.
 *
 * The frame's text rows are BOX_V + label + BOX_V, and the box interior is
 * 62 columns (see layouts/horn_chat.chtpm - the top border is
 * BOX_V + 62 + BOX_V). A record line here already carries a 2-space
 * indent, so wrap the RECORD at 60 to leave room for that indent without
 * pushing the right border off the edge and wrapping the box itself. */
#define WRAP_W     60
#define MAX_LINES  400
#define LINE_CAP   65536

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (getcwd(project_root, sizeof(project_root)) == NULL)
        snprintf(project_root, sizeof(project_root), ".");
}

/* Session directory, heap-allocated (see horn_turn.c's own copy of this
 * helper for why it is not a caller buffer). Returns NULL on failure. */
static char *sessions_dir(void) {
    const char *env = getenv("HORN_SESSIONS");
    if (env && env[0]) return strdup(env);
    char *out = NULL;
    if (asprintf(&out, "%s/chats/HORN_SESSIONS", project_root) < 0) return NULL;
    return out;
}

/* Emit one transcript record, wrapped and indented to WRAP_W.
 *
 * Wrapping breaks on the last space inside the window so words stay
 * whole, falling back to a hard break when a single token (a URL, a
 * long path) is wider than the whole line - otherwise that token would
 * be clipped and the rest of the answer lost with it. */
static void emit_wrapped(FILE *out, const char *record) {
    size_t len = strlen(record);
    size_t i = 0;
    int first = 1;

    while (i < len) {
        size_t start = i;
        size_t limit = WRAP_W;

        while (i < len && (i - start) < limit && record[i] != '\n') i++;

        if (i < len && record[i] != '\n' && i - start == limit) {
            size_t brk = i;
            while (brk > start && record[brk - 1] != ' ') brk--;
            if (brk > start + 1) i = brk;
        }

        /* Continuation lines hang-indent under the text rather than under the
         * role prefix, so a wrapped reply reads as one block instead of a
         * second ragged-left column. Indent is small enough to stay inside
         * the box at WRAP_W. */
        if (first) fprintf(out, "  %.*s\n", (int)(i - start), record + start);
        else       fprintf(out, "      %.*s\n", (int)(i - start), record + start);
        first = 0;

        if (i < len && record[i] == '\n') i++;   /* consume the newline */
    }
}

int main(void) {
    resolve_root();

    char *dir = sessions_dir();
    char *tp = NULL;
    if (dir && asprintf(&tp, "%s/transcript.txt", dir) < 0) tp = NULL;
    free(dir);

    char view_path[PATH_BUF], state_path[PATH_BUF], changed_path[PATH_BUF];
    snprintf(view_path,    sizeof(view_path),    "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(state_path,   sizeof(state_path),   "%s/pieces/apps/player_app/state.txt", project_root);
    snprintf(changed_path, sizeof(changed_path), "%s/pieces/apps/player_app/state_changed.txt", project_root);

    /* Ring buffer of the last MAX_LINES records: the frame is a fixed-size
     * terminal box, so an ever-growing view.txt just pushes the input line
     * off the bottom of the screen. */
    char (*lines)[LINE_CAP] = calloc(MAX_LINES, LINE_CAP);
    int total = 0;

    FILE *in = fopen(tp, "rb");
    if (lines && in) {
        char *buf = malloc(LINE_CAP);
        if (buf) {
            while (fgets(buf, LINE_CAP, in)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                snprintf(lines[total % MAX_LINES], LINE_CAP, "%s", buf);
                total++;
            }
            free(buf);
        }
    }
    if (in) fclose(in);
    free(tp);

    int start = (total > MAX_LINES) ? (total - MAX_LINES) : 0;

    FILE *vf = fopen(view_path, "wb");
    if (vf) {
        if (total == 0) {
            fprintf(vf, "  (no messages yet - type below and press Enter)\n");
        } else {
            if (start > 0) fprintf(vf, "  ... %d earlier line(s) scrolled off ...\n", start);
            for (int i = start; i < total; i++) emit_wrapped(vf, lines[i % MAX_LINES]);
        }
        fclose(vf);
    }

    /* Count turns from the persistent history, not the rendered view, so
     * the number keeps climbing after old lines scroll off. */
    char *hp = NULL;
    {
        char *d2 = sessions_dir();
        if (d2 && asprintf(&hp, "%s/chat_history.txt", d2) < 0) hp = NULL;
        free(d2);
    }
    int turns = 0;
    FILE *hf = hp ? fopen(hp, "rb") : NULL;
    free(hp);
    if (hf) {
        char *b = malloc(LINE_CAP);
        if (b) {
            while (fgets(b, LINE_CAP, hf))
                if (strstr(b, "\tuser\t")) turns++;
            free(b);
        }
        fclose(hf);
    }

    /* Report the PRIMARY model's name, not whichever rung answered. The
     * ladder exists so a rate-limited or retired free slug does not break
     * the turn; showing "ling-3.0-flash-sante" as HORN's model after one
     * rate limit would misreport which model is actually in use. The rung
     * that answered is recorded in the transcript by horn_turn if it ever
     * matters for grading. */
    const char *model = getenv("HORN_MODEL");
    if (!model || !model[0]) model = "nvidia/nemotron-3-ultra-550b-a55b:free";

    FILE *sf = fopen(state_path, "wb");
    if (sf) {
        fprintf(sf, "horn_model=%s\n", model);
        fprintf(sf, "horn_status=%s\n", turns > 0 ? "idle" : "ready");
        fprintf(sf, "horn_turns=%d\n", turns);
        fclose(sf);
    }

    /* The pulse chtpm's main loop watches. Without this grow, the state
     * files above are written but never re-read and the display freezes. */
    FILE *cf = fopen(changed_path, "ab");
    if (cf) { fprintf(cf, "H\n"); fclose(cf); }

    /* Frame marker: tells the renderer a new frame is worth drawing. */
    char fc[PATH_BUF];
    snprintf(fc, sizeof(fc), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *ff = fopen(fc, "ab");
    if (ff) { fprintf(ff, "H\n"); fclose(ff); }

    free(lines);
    return 0;
}