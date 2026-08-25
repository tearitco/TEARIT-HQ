/* apply_cell - W1 op. Assembles all generated cell_NN.txt files for a
 * chapter into one normalized chapter: verse lines are renumbered 1..N in
 * order, non-verse noise (headers/commentary) is dropped, continuation lines
 * of a wrapped verse are joined to it. Header block comes from chapter.pdl.
 *
 * Output: canon/work/<book>/<chapter>/chapter.generated.txt
 * Usage: apply_cell.+x <book> <chapter>
 * Env:   PRISC_PROJECT_ROOT or CWD. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 512)
#define MAX_LINE 65536
#define MAX_CELLS 64
#define MAX_TEXT 2000000

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* returns 1 if line is a verse-start (**N** ...), sets num */
static int is_verse_start(const char *line, int *num) {
    const char *t = line;
    while (*t == ' ' || *t == '\t') t++;
    if (t[0] != '*' || t[1] != '*') return 0;
    int n = 0;
    const char *p = t + 2;
    while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
    if (*p != '*' || p[1] != '*') return 0;
    *num = n;
    return 1;
}

int main(int argc, char **argv) {
    resolve_root();
    if (argc < 3) { fprintf(stderr, "usage: apply_cell.+x <book> <chapter>\n"); return 1; }
    const char *book = argv[1], *chapter = argv[2];

    char work[PATH_BUF], cells_dir[PATH_BUF];
    snprintf(work, sizeof(work), "%s/canon/work/%s/%s", project_root, book, chapter);
    snprintf(cells_dir, sizeof(cells_dir), "%s/cells", work);

    char hdr_path[PATH_BUF];
    snprintf(hdr_path, sizeof(hdr_path), "%s/chapter.pdl", work);
    FILE *hf = fopen(hdr_path, "r");
    char ctitle[MAX_LINE] = "Untitled";
    if (hf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), hf)) {
            if (strncmp(line, "chapter|", 8) == 0) {
                char *p1 = strchr(line + 8, '|');
                if (p1) {
                    *p1 = '\0';
                    char *p2 = strchr(p1 + 1, '|');
                    if (p2) { *p2 = '\0'; snprintf(ctitle, sizeof(ctitle), "%s", p1 + 1); }
                }
            }
        }
        fclose(hf);
    }

    char out_path[PATH_BUF];
    snprintf(out_path, sizeof(out_path), "%s/chapter.generated.txt", work);
    FILE *of = fopen(out_path, "w");
    if (!of) { fprintf(stderr, "cannot write %s\n", out_path); return 1; }

    fprintf(of, "# The Living Testament\n\n");
    fprintf(of, "## Book of the Soul Pen\n\n");
    fprintf(of, "### Chapter %d\n\n", chapter[0] == 'c' ? atoi(chapter + 2) : atoi(chapter));
    fprintf(of, "### %s\n\n", ctitle);

    int verse = 0, missing = 0, dropped = 0;
    for (int c = 1; c <= MAX_CELLS; c++) {
        char cp[PATH_BUF];
        snprintf(cp, sizeof(cp), "%s/cells/cell_%02d.txt", work, c);
        FILE *cf = fopen(cp, "r");
        if (!cf) { missing++; continue; }
        char line[MAX_LINE];
        char current[MAX_TEXT];
        current[0] = '\0';
        int have = 0;
        while (fgets(line, sizeof(line), cf)) {
            line[strcspn(line, "\n")] = '\0';
            int num = 0;
            if (is_verse_start(line, &num)) {
                if (have) { verse++; fprintf(of, "**%d** %s\n", verse, current); }
                const char *t = line;
                while (*t == ' ' || *t == '\t') t++;
                const char *body = strstr(t, "**");
                if (body) body = strstr(body + 2, "**") + 2; else body = t;
                while (*body == ' ' || *body == '\t') body++;
                snprintf(current, sizeof(current), "%s", body);
                have = 1;
            } else {
                const char *t = line;
                while (*t == ' ' || *t == '\t') t++;
                if (!have || t[0] == '\0') { if (t[0] != '\0') dropped++; continue; }
                /* continuation line of current verse */
                size_t clen = strlen(current), tlen = strlen(t);
                if (clen + tlen + 2 < sizeof(current)) {
                    current[clen++] = ' ';
                    memcpy(current + clen, t, tlen + 1);
                }
            }
        }
        if (have) { verse++; fprintf(of, "**%d** %s\n", verse, current); }
        fclose(cf);
    }
    fclose(of);

    printf("apply_cell: %s/%s -> %d verses assembled (missing cells=%d, dropped noise lines=%d)\n",
           book, chapter, verse, missing, dropped);
    if (verse == 0) { fprintf(stderr, "no verses assembled - cells missing?\n"); return 1; }
    return 0;
}
