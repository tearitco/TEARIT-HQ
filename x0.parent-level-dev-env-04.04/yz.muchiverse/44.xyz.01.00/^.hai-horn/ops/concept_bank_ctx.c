/* concept_bank_ctx.c - load Concept Bank spokes/weights for prompt injection
 *
 * Usage: concept_bank_ctx.+x <house_root> <max_items>
 * Output: JSON array of {spoke, master, weight} for top-N by abs(weight)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>

#define PATH_BUF 4352
#define MAX_ENTRIES 64

typedef struct {
    char spoke[64];
    char master[64];
    double weight;
} SpokeEntry;

static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int parse_spoke_file(const char *spoke_path, const char *spoke_name, SpokeEntry *entry) {
    FILE *f = fopen(spoke_path, "r");
    if (!f) return 0;
    char line[256];
    entry->spoke[0] = '\0';
    entry->master[0] = '\0';
    entry->weight = 0.0;
    snprintf(entry->spoke, sizeof(entry->spoke), "%s", spoke_name);
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ') p++;
        if (*p == '#') continue;
        if (strncmp(p, "SLOT", 4) == 0) {
            char *pt = strstr(p, "POINTS_TO=");
            if (pt) {
                pt += 10;
                int i = 0;
                while (pt[i] && pt[i] != ' ' && pt[i] != '|' && pt[i] != '\n' && i < 63) {
                    entry->master[i] = pt[i];
                    i++;
                }
                entry->master[i] = '\0';
            }
            char *wt = strstr(p, "WEIGHT=");
            if (wt) {
                wt += 7;
                entry->weight = strtod(wt, NULL);
            }
        }
    }
    fclose(f);
    return entry->master[0] != '\0';
}

int cmp_weight_desc(const void *a, const void *b) {
    double wa = fabs(((SpokeEntry*)a)->weight);
    double wb = fabs(((SpokeEntry*)b)->weight);
    if (wa < wb) return 1;
    if (wa > wb) return -1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: concept_bank_ctx.+x <house_root> [max_items]\n");
        return 1;
    }
    const char *house_root = argv[1];
    int max_items = argc > 2 ? atoi(argv[2]) : 8;

    char spokes_dir[PATH_BUF];
    snprintf(spokes_dir, sizeof(spokes_dir), "%s/&.widgits/concept-bank/data/spokes", house_root);
    DIR *d = opendir(spokes_dir);
    if (!d) {
        printf("[]");
        return 0;
    }

    SpokeEntry entries[MAX_ENTRIES];
    int n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && n < MAX_ENTRIES) {
        size_t len = strlen(de->d_name);
        if (len > 4 && strcmp(de->d_name + len - 4, ".pdl") == 0) {
            char spoke[64];
            size_t namelen = len - 4;
            if (namelen >= sizeof(spoke)) namelen = sizeof(spoke) - 1;
            memcpy(spoke, de->d_name, namelen);
            spoke[namelen] = '\0';

            char spoke_path[PATH_BUF];
            snprintf(spoke_path, sizeof(spoke_path), "%s/%s", spokes_dir, de->d_name);
            if (parse_spoke_file(spoke_path, spoke, &entries[n])) {
                n++;
            }
        }
    }
    closedir(d);

    qsort(entries, n, sizeof(SpokeEntry), cmp_weight_desc);
    if (n > max_items) n = max_items;

    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%s{\"spoke\":\"%s\",\"master\":\"%s\",\"weight\":%.4f}", i ? "," : "", entries[i].spoke, entries[i].master, entries[i].weight);
    }
    printf("]\n");
    return 0;
}