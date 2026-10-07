/* curricula_engine.c - generate/refine curricula from Concept Bank weights
 *
 * Reads Concept Bank spokes/weights, produces curriculum items ranked by
 * weight and dependency order.
 *
 * Usage: curricula_engine.+x <concept_bank_dir> [grade_level] [format]
 * Grade levels: -1=all, 0=preschool, 1=elementary, 2=middle, 3=high, 4=college, 5=grad
 * Format: json|text
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>

#define PATH_BUF 4352
#define MAX_ENTRIES 128
#define MAX_DEPS 16

typedef struct {
    char spoke[64];
    char master[64];
    double weight;
    int grade_level;
    char deps[MAX_DEPS][64];
    int n_deps;
} SpokeEntry;

static int parse_spoke_file(const char *spoke_path, const char *spoke_name, SpokeEntry *entry) {
    FILE *f = fopen(spoke_path, "r");
    if (!f) return 0;
    char line[256];
    entry->spoke[0] = '\0';
    entry->master[0] = '\0';
    entry->weight = 0.0;
    entry->grade_level = 0;
    entry->n_deps = 0;
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
        } else if (strncmp(p, "GRADE", 5) == 0) {
            char *sep = strchr(p, '=');
            if (!sep) sep = strchr(p, '|');
            if (sep) entry->grade_level = atoi(sep + 1);
        } else if (strncmp(p, "DEPENDS_ON", 10) == 0) {
            char *sep = strchr(p, '=');
            if (!sep) sep = strchr(p, '|');
            if (sep && entry->n_deps < MAX_DEPS) {
                sep++;
                while (*sep == ' ') sep++;
                int i = 0;
                while (*sep && *sep != ',' && *sep != '\n' && i < 63) {
                    entry->deps[entry->n_deps][i++] = *sep++;
                }
                entry->deps[entry->n_deps][i] = '\0';
                entry->n_deps++;
            }
        }
    }
    fclose(f);
    return entry->master[0] != '\0';
}

static int cmp_weight_desc(const void *a, const void *b) {
    double wa = ((SpokeEntry*)a)->weight;
    double wb = ((SpokeEntry*)b)->weight;
    if (wa < wb) return 1;
    if (wa > wb) return -1;
    return 0;
}

static const char *grade_name(int g) {
    switch (g) {
        case 0: return "Preschool";
        case 1: return "Elementary";
        case 2: return "Middle School";
        case 3: return "High School";
        case 4: return "College";
        case 5: return "Graduate";
        default: return "Unknown";
    }
}

static void print_json(const SpokeEntry *entries, int n, int grade_filter) {
    printf("[\n");
    for (int i = 0; i < n; i++) {
        if (grade_filter >= 0 && entries[i].grade_level != grade_filter) continue;
        printf("  {\n");
        printf("    \"spoke\": \"%s\",\n", entries[i].spoke);
        printf("    \"master\": \"%s\",\n", entries[i].master);
        printf("    \"weight\": %.4f,\n", entries[i].weight);
        printf("    \"grade\": %d,\n", entries[i].grade_level);
        printf("    \"depends_on\": [");
        for (int d = 0; d < entries[i].n_deps; d++) {
            printf("%s\"%s\"", d ? ", " : "", entries[i].deps[d]);
        }
        printf("]\n");
        printf("  }%s\n", i < n - 1 ? "," : "");
    }
    printf("]\n");
}

static void print_text(const SpokeEntry *entries, int n, int grade_filter) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (grade_filter >= 0 && entries[i].grade_level != grade_filter) continue;
        count++;
    }
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           CONCEPT BANK CURRICULUM                          ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    if (grade_filter >= 0) {
        printf("║  Grade filter: %-12s (%d items)                          ║\n", grade_name(grade_filter), count);
    } else {
        printf("║  All grades (%d items)                                     ║\n", count);
    }
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    for (int i = 0; i < n; i++) {
        if (grade_filter >= 0 && entries[i].grade_level != grade_filter) continue;
        printf("  ▸ %-30s → %-12s (weight: %.2f) [%s]\n", 
               entries[i].spoke, entries[i].master, entries[i].weight, grade_name(entries[i].grade_level));
        if (entries[i].n_deps > 0) {
            printf("      Depends on: ");
            for (int d = 0; d < entries[i].n_deps; d++) {
                printf("%s%s", d ? ", " : "", entries[i].deps[d]);
            }
            printf("\n");
        }
    }
    printf("\nTotal items: %d\n", count);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: curricula_engine.+x <concept_bank_dir> [grade_level] [format]\n");
        return 1;
    }
    const char *bank_dir = argv[1];
    int grade_filter = argc > 2 ? atoi(argv[2]) : -1;
    const char *format = argc > 3 ? argv[3] : "text";

    char spokes_dir[PATH_BUF];
    snprintf(spokes_dir, sizeof(spokes_dir), "%s/data/spokes", bank_dir);
    DIR *d = opendir(spokes_dir);
    if (!d) {
        if (strcmp(format, "json") == 0) printf("[]\n");
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
    
    if (strcmp(format, "json") == 0) {
        print_json(entries, n, grade_filter);
    } else {
        print_text(entries, n, grade_filter);
    }
    return 0;
}