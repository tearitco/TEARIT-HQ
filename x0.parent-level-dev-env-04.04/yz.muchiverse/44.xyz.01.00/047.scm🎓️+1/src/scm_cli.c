/* scm_cli.c — SCM v1 CLI: list | route | select | demo
 * Modes:
 *   scm_cli list                       - list intents from intent.conf
 *   scm_cli route <message> [--conf f] - print routed bucket|curriculum
 *   scm_cli select <curriculum_dir> <message> [--meta s] [--seed n] [--scores] [--rp]
 *   scm_cli demo [--conf f] [--seed n] [--meta s]   - REPL (routing + memory)
 */
#include "scm_core.h"

typedef struct {
    char *pos[8];
    int npos;
    const char *conf;
    unsigned seed;
    const char *meta;
    int scores;
    int all;
    int rp;
} Args;

static void parse_args(int argc, char **argv, int start, Args *a) {
    a->conf = "intent.conf";
    a->seed = (unsigned)time(NULL);
    a->meta = NULL;
    a->scores = 0;
    a->all = 0;
    a->rp = 0;
    a->npos = 0;
    for (int i = start; i < argc; i++) {
        if (!strcmp(argv[i], "--conf")) { if (i + 1 < argc) a->conf = argv[++i]; }
        else if (!strcmp(argv[i], "--seed")) { if (i + 1 < argc) a->seed = (unsigned)atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--meta")) { if (i + 1 < argc) a->meta = argv[++i]; }
        else if (!strcmp(argv[i], "--scores")) a->scores = 1;
        else if (!strcmp(argv[i], "--all")) a->all = 1;
        else if (!strcmp(argv[i], "--rp")) a->rp = 1;
        else if (a->npos < 8) a->pos[a->npos++] = argv[i];
    }
}

static void print_scores(ScmCurriculum *c, const double *fv) {
    int n = c->n_phrases;
    int *idx = malloc(n * sizeof(int));
    double *sc = malloc(n * sizeof(double));
    for (int i = 0; i < n; i++) { idx[i] = i; sc[i] = scm_score_phrase(c, &c->phrases[i], fv); }
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - 1 - i; j++)
            if (sc[idx[j]] < sc[idx[j+1]]) { int t = idx[j]; idx[j] = idx[j+1]; idx[j+1] = t; }
    int top = n < 8 ? n : 8;
    for (int k = 0; k < top; k++)
        printf("  %6.3f  %s\n", sc[idx[k]], c->phrases[idx[k]].text);
    free(idx);
    free(sc);
}

static int find_casual(ScmIntent *in, int n) {
    for (int i = 0; i < n; i++) if (!strcmp(in[i].bucket, "casual")) return i;
    return -1;
}

static void load_switch(ScmCurriculum *cur, const char *dir, const char *active_meta) {
    scm_curriculum_free(cur);
    if (scm_curriculum_load(cur, dir) > 0) {
        if (active_meta && *active_meta) scm_meta_load(cur, active_meta);
        printf("curriculum: %s (%d phrases)\n", cur->name, cur->n_phrases);
    } else {
        printf("could not load: %s\n", dir);
    }
}

static void demo(ScmIntent *intents, int ni, unsigned *seed, const char *metaname) {
    int casual = find_casual(intents, ni);
    ScmCurriculum cur;
    ScmMemory mem;
    memset(&mem, 0, sizeof(mem));
    char active_meta[SCM_MAX_NAME] = {0};
    if (metaname) snprintf(active_meta, sizeof(active_meta), "%s", metaname);
    int turn = 0;

    load_switch(&cur, casual >= 0 ? intents[casual].curriculum : "corpuses/small-talk", active_meta);
    printf("SCM demo — /quit, /curriculum <dir>, /rp <pack>, /meta <situation>\n");

    char line[SCM_MAX_LINE];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        char *msg = line;
        msg[strcspn(msg, "\n")] = 0;
        while (*msg == ' ' || *msg == '\t') msg++;
        if (!*msg) continue;
        if (!strcmp(msg, "/quit")) break;
        if (!strncmp(msg, "/curriculum ", 12)) {
            const char *dir = scm_trim(msg + 12);
            load_switch(&cur, dir, active_meta);
            continue;
        }
        if (!strncmp(msg, "/rp ", 4)) {
            int r = scm_route(msg, intents, ni);
            if (r >= 0 && intents[r].curriculum[0]) {
                load_switch(&cur, intents[r].curriculum, active_meta);
            } else {
                printf("no rp pack: %s\n", scm_trim(msg + 4));
            }
            continue;
        }
        if (!strncmp(msg, "/meta ", 6)) {
            snprintf(active_meta, sizeof(active_meta), "%s", scm_trim(msg + 6));
            scm_meta_load(&cur, active_meta);
            printf("meta: %s\n", active_meta);
            continue;
        }

        int ri = scm_route(msg, intents, ni);
        int rp_on = 0;
        if (ri >= 0 && ri != casual && intents[ri].curriculum[0] && strcmp(intents[ri].curriculum, cur.path)) {
            load_switch(&cur, intents[ri].curriculum, active_meta);
            rp_on = 1;
        } else if (ri >= 0 && ri != casual) {
            rp_on = 1;
        }
        if (cur.n_phrases == 0) { printf("(no phrases in active curriculum)\n"); continue; }

        double fv[SCM_MAX_FEATURES] = {0};
        scm_feature_vec(msg, &cur, fv, turn, rp_on);
        int idxs[SCM_MAX_FEATURES];
        int k = scm_select(&cur, fv, cur.seq_len, idxs, &mem, seed);
        for (int j = 0; j < k; j++) {
            char filled[SCM_MAX_LINE];
            scm_slot_fill(&cur, cur.phrases[idxs[j]].text, filled, sizeof(filled), seed);
            if (j > 0) printf(" ");
            printf("%s", filled);
        }
        if (k == 0) printf("%s", cur.fallback);
        printf("\n");
        for (int j = 0; j < k; j++) scm_mem_push(&mem, cur.phrases[idxs[j]].text);
        turn++;
    }
    scm_curriculum_free(&cur);
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "demo";
    Args a;

    if (!strcmp(mode, "list")) {
        parse_args(argc, argv, 2, &a);
        ScmIntent intents[SCM_MAX_FEATURES];
        int n = scm_intents_load(a.conf, intents, SCM_MAX_FEATURES);
        printf("intents from %s:\n", a.conf);
        for (int i = 0; i < n; i++) printf("  %s -> %s\n", intents[i].bucket, intents[i].curriculum);
        return 0;
    }

    if (!strcmp(mode, "route")) {
        parse_args(argc, argv, 2, &a);
        if (a.npos < 1) { fprintf(stderr, "usage: scm_cli route <message> [--conf f]\n"); return 2; }
        ScmIntent intents[SCM_MAX_FEATURES];
        int n = scm_intents_load(a.conf, intents, SCM_MAX_FEATURES);
        int r = scm_route(a.pos[0], intents, n);
        if (r >= 0) printf("%s|%s\n", intents[r].bucket, intents[r].curriculum);
        else printf("casual|(none)\n");
        return 0;
    }

    if (!strcmp(mode, "select")) {
        parse_args(argc, argv, 2, &a);
        if (a.npos < 2) { fprintf(stderr, "usage: scm_cli select <curriculum_dir> <message> [flags]\n"); return 2; }
        ScmCurriculum c;
        if (scm_curriculum_load(&c, a.pos[0]) <= 0) { fprintf(stderr, "cannot load curriculum: %s\n", a.pos[0]); return 1; }
        if (a.meta) scm_meta_load(&c, a.meta);
        double fv[SCM_MAX_FEATURES] = {0};
        scm_feature_vec(a.pos[1], &c, fv, 0, a.rp);
        int idxs[SCM_MAX_FEATURES];
        ScmMemory mem;
        memset(&mem, 0, sizeof(mem));
        int k = scm_select(&c, fv, c.seq_len, idxs, &mem, &a.seed);
        if (a.all) {
            for (int i = 0; i < c.n_phrases; i++)
                printf("SCORE\t%.3f\t%s\n", scm_score_phrase(&c, &c.phrases[i], fv), c.phrases[i].text);
            for (int j = 0; j < k; j++) {
                char filled[SCM_MAX_LINE];
                scm_slot_fill(&c, c.phrases[idxs[j]].text, filled, sizeof(filled), &a.seed);
                if (j > 0) printf(" ");
                printf("%s", filled);
            }
            if (k == 0) printf("%s", c.fallback);
            printf("\n");
        } else {
            if (a.scores) print_scores(&c, fv);
            for (int j = 0; j < k; j++) {
                char filled[SCM_MAX_LINE];
                scm_slot_fill(&c, c.phrases[idxs[j]].text, filled, sizeof(filled), &a.seed);
                if (j > 0) printf(" ");
                printf("%s", filled);
            }
            if (k == 0) printf("%s", c.fallback);
            printf("\n");
        }
        scm_curriculum_free(&c);
        return 0;
    }

    parse_args(argc, argv, 2, &a);
    ScmIntent intents[SCM_MAX_FEATURES];
    int n = scm_intents_load(a.conf, intents, SCM_MAX_FEATURES);
    demo(intents, n, &a.seed, a.meta);
    return 0;
}
