/* grade_chapter - W1 op. Deterministically grades the generated chapter
 * against the gold Living Testament chapter. All checks are mechanical
 * (no model involved). Writes one row to proof/model-grades.csv.
 *
 * Metrics (0-100 weighted):
 *   verse_ratio   (0-25)  gen verses / gold verses, capped at 1
 *   word_ratio    (0-25)  1 - |gold-gen|/gold words, floor 0
 *   entity_cov    (0-20)  lexicon entities found in gold also found in gen
 *   style_markers (0-20)  biblical-opener rate per verse, gen vs gold ratio
 *   clean_register(0-10)  0 if any game-mechanics term appears in gen
 *
 * Usage: grade_chapter.+x <book> <chapter>
 * Env:   PRISC_PROJECT_ROOT or CWD. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 512)
#define MAX_LINE 65536
#define MAX_TEXT 2000000

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static char *read_full_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > MAX_TEXT) { fclose(f); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)size, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static const char *OPENERS[] = {
    "And it came to pass", "And it came about", "Now ", "Then ", "Therefore ",
    "For ", "Yet ", "Thus ", "After these things", "Behold ", "Thereupon ",
    "At length", NULL
};

static const char *GAME_TERMS[] = {
    "xp", " hit point", "hit points", "skill tree", "skill trees", "quest",
    "inventory", "tutorial", "level up", "boss fight", "player stats", NULL
};

/* count verses (lines starting with **N**) */
static int count_verses(const char *txt) {
    int n = 0;
    const char *p = txt;
    while (p && *p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len > 2 && p[0] == '*' && p[1] == '*' && p[2] >= '0' && p[2] <= '9') n++;
        p = nl ? nl + 1 : NULL;
    }
    return n;
}

static long count_words(const char *txt) {
    long w = 0;
    int inspace = 1;
    for (const char *p = txt; *p; p++) {
        if (isspace((unsigned char)*p)) inspace = 1;
        else if (inspace) { w++; inspace = 0; }
    }
    return w;
}

static int has_substr_ci(const char *txt, const char *needle) {
    size_t nlen = strlen(needle);
    size_t tlen = strlen(txt);
    for (size_t i = 0; i + nlen <= tlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            char a = txt[i + j], b = needle[j];
            if (tolower((unsigned char)a) != tolower((unsigned char)b)) break;
        }
        if (j == nlen) return 1;
    }
    return 0;
}

static double marker_rate(const char *txt, int verses) {
    if (verses <= 0) return 0.0;
    double hits = 0;
    const char *p = txt;
    while (p && *p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len >= 3 && p[0] == '*' && p[1] == '*' && p[2] >= '0' && p[2] <= '9') {
            const char *body = p + 2;
            while (*body >= '0' && *body <= '9') body++;
            if (*body == '*' && body[1] == '*') body += 2;
            while (*body == ' ' || *body == '\t') body++;
            for (int i = 0; OPENERS[i]; i++) {
                size_t olen = strlen(OPENERS[i]);
                if (len > (size_t)(body - p) + olen &&
                    strncasecmp(body, OPENERS[i], olen) == 0) { hits++; break; }
            }
        }
        p = nl ? nl + 1 : NULL;
    }
    return hits / (double)verses;
}

static int check_clean_register(const char *txt) {
    for (int i = 0; GAME_TERMS[i]; i++) {
        if (has_substr_ci(txt, GAME_TERMS[i])) return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    resolve_root();
    if (argc < 3) { fprintf(stderr, "usage: grade_chapter.+x <book> <chapter>\n"); return 1; }
    const char *book = argv[1], *chapter = argv[2];

    char gold_path[PATH_BUF], gen_path[PATH_BUF];
    snprintf(gold_path, sizeof(gold_path), "%s/canon/lt/%s/%s.txt", project_root, book, chapter);
    snprintf(gen_path, sizeof(gen_path), "%s/canon/work/%s/%s/chapter.generated.txt", project_root, book, chapter);

    char *gold = read_full_file(gold_path);
    char *gen = read_full_file(gen_path);
    if (!gold) { fprintf(stderr, "gold not found: %s\n", gold_path); return 1; }
    if (!gen) { fprintf(stderr, "generated not found: %s\n", gen_path); return 1; }

    int gv = count_verses(gold), nv = count_verses(gen);
    long gw = count_words(gold), nw = count_words(gen);

    double verse_ratio = (double)nv / (double)gv; if (verse_ratio > 1.0) verse_ratio = 1.0;
    double wdiff = gw > 0 ? (double)labs(gw - nw) / (double)gw : 0.0;
    double word_ratio = 1.0 - wdiff; if (word_ratio < 0.0) word_ratio = 0.0;

    /* entity coverage: lexicon entities whose primary display name appears
     * in gold, and check each also appears in gen */
    char lex_path[PATH_BUF];
    snprintf(lex_path, sizeof(lex_path), "%s/canon/lexicon/entities.pdl", project_root);
    FILE *lf = fopen(lex_path, "r");
    int total = 0, covered = 0;
    if (lf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), lf)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char *p1 = strchr(line, '|'); if (!p1) continue;
            char *p2 = strchr(p1 + 1, '|'); if (!p2) continue;
            char *p3 = strchr(p2 + 1, '|'); if (!p3) continue;
            *p1 = '\0'; *p2 = '\0'; *p3 = '\0';
            const char *name = p2 + 1;
            if (!has_substr_ci(gold, name)) continue; /* only entities the gold chapter actually uses */
            total++;
            if (has_substr_ci(gen, name)) covered++;
        }
        fclose(lf);
    }
    double entity_cov = total > 0 ? (double)covered / (double)total : 0.0;

    double g_rate = marker_rate(gold, gv);
    double n_rate = marker_rate(gen, nv);
    double style_ratio = g_rate > 0 ? (n_rate / g_rate) : (n_rate > 0 ? 1.0 : 0.0);
    if (style_ratio > 1.0) style_ratio = 1.0;

    int clean = check_clean_register(gen);

    double grade = verse_ratio * 25 + word_ratio * 25 + entity_cov * 20 + style_ratio * 20 + (clean ? 10 : 0);

    printf("grade_chapter: %s/%s grade=%.1f/100\n", book, chapter, grade);
    printf("  verses   gen=%d gold=%d (ratio %.2f)\n", nv, gv, verse_ratio);
    printf("  words    gen=%ld gold=%ld (ratio %.2f)\n", nw, gw, word_ratio);
    printf("  entities covered %d/%d (%.2f)\n", covered, total, entity_cov);
    printf("  style    gen marker/verse=%.2f gold=%.2f (ratio %.2f)\n", n_rate, g_rate, style_ratio);
    printf("  register clean=%s\n", clean ? "yes" : "no");

    /* proof row */
    char proof_dir[PATH_BUF], proof_path[PATH_BUF];
    snprintf(proof_dir, sizeof(proof_dir), "%s/proof", project_root);
    char mk[PATH_BUF * 2];
    snprintf(mk, sizeof(mk), "mkdir -p '%s'", proof_dir);
    int rc = system(mk); (void)rc;
    snprintf(proof_path, sizeof(proof_path), "%s/model-grades.csv", proof_dir);
    int fresh = 0;
    FILE *pf = fopen(proof_path, "r");
    if (!pf) fresh = 1; else fclose(pf);
    pf = fopen(proof_path, "a");
    if (pf) {
        if (fresh) fprintf(pf, "book,chapter,grade,verses_gen,verses_gold,words_gen,words_gold,entity_cov,style_ratio,register\n");
        fprintf(pf, "%s,%s,%.1f,%d,%d,%ld,%ld,%.2f,%.2f,%d\n",
                book, chapter, grade, nv, gv, nw, gw, entity_cov, style_ratio, clean);
        fclose(pf);
    }

    free(gold); free(gen);
    return 0;
}
