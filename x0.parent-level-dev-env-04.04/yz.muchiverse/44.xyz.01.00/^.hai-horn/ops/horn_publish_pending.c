/* horn_publish_pending - republish only when a prompt is waiting.
 *
 * Called from the pal loop's idle tick. Publishing unconditionally would
 * touch state.txt and pulse frame_changed.txt several times a second for a
 * session that is simply sitting there, so this checks for the pending
 * file first and stays silent unless something actually needs answering.
 *
 * Also tracks WHICH pending prompt has already been published, so a prompt
 * that arrives while a frame is in flight gets picked up rather than
 * assumed visible. Without that the box can sit there looking idle while
 * horn_turn is blocked waiting for a y/n that was never rendered - the
 * worst possible failure for an approval prompt.
 *
 * Usage: horn_publish_pending.+x   (no args)
 * Self-contained.
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

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (getcwd(project_root, sizeof(project_root)) == NULL)
        snprintf(project_root, sizeof(project_root), ".");
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    b[got] = '\0';
    fclose(f);
    return b;
}

static void touch(const char *rel) {
    char *p = NULL;
    if (asprintf(&p, "%s/%s", project_root, rel) < 0 || !p) return;
    FILE *f = fopen(p, "ab");
    if (f) { fprintf(f, "H\n"); fclose(f); }
    free(p);
}

int main(void) {
    resolve_root();

    char *pend = NULL;
    if (asprintf(&pend, "%s/pieces/horn/pending.json", project_root) < 0 || !pend) return 0;
    char *body = read_all(pend);
    free(pend);
    if (!body || !body[0]) { free(body); return 0; }

    /* Signature of the pending prompt. The stamp file records the last one
     * published, so an unchanged prompt does not repaint and a CHANGED one
     * always does. */
    char sig[512];
    snprintf(sig, sizeof(sig), "%.400s", body);

    char *stamp = NULL;
    if (asprintf(&stamp, "%s/pieces/horn/pending_stamp.txt", project_root) < 0 || !stamp) { free(body); return 0; }
    char *prev = read_all(stamp);
    int same = prev && strcmp(prev, sig) == 0;
    free(prev);
    if (!same) {
        FILE *f = fopen(stamp, "wb");
        if (f) { fprintf(f, "%s", sig); fclose(f); }
        /* Both markers: state_changed makes chtpm re-read the vars, and
         * frame_changed makes the renderer actually draw the result. */
        touch("pieces/apps/player_app/state_changed.txt");
        touch("pieces/display/frame_changed.txt");
    }
    free(stamp);
    free(body);
    return 0;
}
