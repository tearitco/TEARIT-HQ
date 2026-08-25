/* rag/build_index.c - RAG memory builder for 046.gemma-cli-agent.
 * Scans a source tree, tokenizes each file into words, and writes an
 * inverted index: word -> file:line. This is what lets gemma "see" a
 * codebase bigger than its context window - the agent queries this index
 * and retrieves only the relevant snippets.
 *
 * This is INFRASTRUCTURE (supervisor-built). The agent (gemma-authored)
 * calls it via `rag <query>`. Ground rule: index is generated, source is
 * truth. memory/ is git-ignored-style (generated, never hand-edited).
 *
 * Usage: build_index.+x <root> <index_path>
 *   root       - directory tree to scan (e.g. the agent's own root)
 *   index_path - output file, e.g. memory/index.pdl
 *
 * Output format (house .pdl-ish, sorted words):
 *   SECTION    | WORD            | LOC
 *   ------------------------------------
 *   IDX        | word            | file:line [count]
 *   ...
 *   IDX_SUM    | words=N files=M
 *
 * Self-contained, no shared headers - house style. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define MAX_WORDS 200000
#define MAX_ENTRY 4096

typedef struct {
    char word[64];
    char file[MAX_PATH];
    int line;
    int count;
} Entry;

static Entry entries[MAX_WORDS];
static int n_entries = 0;
static int n_files = 0;

static int is_source_ext(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    return strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 ||
           strcmp(ext, ".sh") == 0 || strcmp(ext, ".md") == 0 ||
           strcmp(ext, ".txt") == 0;
}

static int cmp_entry(const void *a, const void *b) {
    const Entry *ea = a, *eb = b;
    int c = strcmp(ea->word, eb->word);
    if (c != 0) return c;
    c = strcmp(ea->file, eb->file);
    if (c != 0) return c;
    return ea->line - eb->line;
}

static void index_file(const char *path, const char *name) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    n_files++;
    char *buf = malloc(1 << 20);
    size_t n = fread(buf, 1, (1 << 20) - 1, f);
    buf[n] = '\0';
    fclose(f);

    char *save = NULL;
    int line = 0;
    for (char *ln = strtok_r(buf, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
        line++;
        char *wsave = NULL;
        for (char *w = strtok_r(ln, " \t(){}[]<>;,=.:+*/\\\"'`~!@#$%^&|-\n", &wsave);
             w; w = strtok_r(NULL, " \t(){}[]<>;,=.:+*/\\\"'`~!@#$%^&|-\n", &wsave)) {
            if (strlen(w) < 2) continue;             /* skip single chars */
            if (n_entries >= MAX_WORDS) break;
            Entry *e = &entries[n_entries++];
            snprintf(e->word, sizeof(e->word), "%s", w);
            snprintf(e->file, sizeof(e->file), "%s", path);
            e->line = line;
            e->count = 1;
        }
    }
    free(buf);
}

static void walk(const char *dir, const char *base) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char p[MAX_PATH * 2];
        snprintf(p, sizeof(p), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            /* skip generated dirs */
            if (strcmp(de->d_name, "memory") == 0 || strcmp(de->d_name, "proof") == 0 ||
                strcmp(de->d_name, "node_modules") == 0 || strcmp(de->d_name, "+x") == 0)
                continue;
            walk(p, base);
        } else if (S_ISREG(st.st_mode) && is_source_ext(de->d_name)) {
            index_file(p, de->d_name);
        }
    }
    closedir(d);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: build_index.+x <root> <index_path>\n");
        return 1;
    }
    const char *root = argv[1];
    const char *index_path = argv[2];

    walk(root, root);

    qsort(entries, n_entries, sizeof(Entry), cmp_entry);

    FILE *out = fopen(index_path, "w");
    if (!out) { perror("fopen index"); return 1; }
    fprintf(out, "SECTION|WORD|LOC\n");
    fprintf(out, "------------------------------------\n");
    for (int i = 0; i < n_entries; i++) {
        /* merge consecutive same word+file lines into a range */
        int j = i;
        while (j + 1 < n_entries &&
               strcmp(entries[j+1].word, entries[i].word) == 0 &&
               strcmp(entries[j+1].file, entries[i].file) == 0)
            j++;
        fprintf(out, "IDX|%s|%s:%d-%d [%d]\n", entries[i].word, entries[i].file,
                entries[i].line, entries[j].line, j - i + 1);
        i = j;
    }
    fprintf(out, "IDX_SUM|words=%d\n", n_entries);
    fclose(out);

    printf("indexed %d words from %d files -> %s\n", n_entries, n_files, index_path);
    return 0;
}
