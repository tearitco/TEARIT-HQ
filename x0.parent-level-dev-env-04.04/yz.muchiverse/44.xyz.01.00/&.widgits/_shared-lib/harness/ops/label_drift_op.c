/* label_drift_op - the DRIFT CHECK of the board label baker (2026-11-09, case `chutes` cs_page_drift). game_core.c carries a COPY of deskopoly.c's label baker (font tables, five font tiers, wrap, glyph, mask) because deskopoly.c is pinned by its own cases
 * and is not edited (decision D67). This op #includes deskopoly.c itself (its main renamed away) and prints, for the same inputs, the same lines the game_core verbs `labelmask` and `fontsum` print, so a case can compare them:
 *   label_drift_op labelmask <A> <text...>   -> LABELMASK A=.. ink=.. glyph=.. outline=.. fnv=..      (deskopoly.c's own label_mask)
 *   label_drift_op fontsum                   -> FONTSUM f57=<fnv> f35=<fnv> tiers=<fnv>             (deskopoly.c's own tables)
 * Build: gcc -std=gnu11 -O2 -o label_drift_op.+x label_drift_op.c   (warnings from deskopoly.c itself are not ours). */
#define main deskopoly_main_renamed
#include "../../../../@.apps/deskopoly/ops/deskopoly.c"
#undef main
static unsigned long long fnv_(unsigned long long h, const void *p, size_t n) { const unsigned char *s = p; for (size_t i = 0; i < n; i++) { h ^= s[i]; h *= 1099511628211ULL; } return h; }
int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "fontsum")) {
        unsigned long long a = 14695981039346656037ULL, b = a, c = a; a = fnv_(a, F57, sizeof F57); for (size_t k = 0; k < sizeof F35 / sizeof *F35; k++) { b = fnv_(b, &F35[k].c, 1); b = fnv_(b, F35[k].r, 5); } for (int k = 0; k < NTIERS; k++) { c = fnv_(c, &TIERS[k].f35, sizeof(int)); c = fnv_(c, &TIERS[k].sc, sizeof(int)); c = fnv_(c, &TIERS[k].gap, sizeof(int)); c = fnv_(c, &TIERS[k].lgap, sizeof(int)); }
        printf("FONTSUM f57=%016llx f35=%016llx tiers=%016llx\n", a, b, c); return 0; }
    if (argc >= 4 && !strcmp(argv[1], "labelmask")) {
        int A = atoi(argv[2]); char tx[200] = ""; for (int i = 3; i < argc; i++) { if (tx[0]) strncat(tx, " ", sizeof tx - strlen(tx) - 1); strncat(tx, argv[i], sizeof tx - strlen(tx) - 1); }
        static unsigned char m[96 * 96]; g_lab_trunc = 0; int ink = label_mask(tx, A, m); int ng = 0, no = 0; unsigned long long h = 14695981039346656037ULL; for (int i = 0; i < A * A; i++) { if (m[i] == 2) ng++; else if (m[i] == 1) no++; h = fnv_(h, &m[i], 1); }
        printf("LABELMASK A=%d ink=%d glyph=%d outline=%d fnv=%016llx\n", A, ink, ng, no, h); return 0; }
    fprintf(stderr, "usage: label_drift_op labelmask <A> <text...> | fontsum\n"); return 2;
}
