/* img_prompt - build one text-to-image prompt for a brixel part from the image word bank (BOOK 16.game/WORD-BANK-TO-IMAGE.md, &.hq-apps/img-hai/docs/).
 * Usage: img_prompt <words.pdl> <parts.pdl> <brixel> [min_weight] [overlay.cand]      (default cutoff 0.30)
 * An overlay is a candidate-edit file (see img_bank.c) applied IN MEMORY before the prompt is built: it lets a batch try a proposed tuning without promoting it. An invalid overlay is an error (exit 1).
 * Prints PROMPT / NEGATIVE / USED / DROPPED. Order inside the prompt: subject, material, colour, angle, style, size; within a kind by weight (high first), ties by file order. Negative words go to NEGATIVE.
 * Exit 0 ok, 1 invalid bank/overlay or unknown brixel, 2 cannot open a file. Pure: reads files, writes stdout, never a model call, never writes the bank. */
#include "img_bank.c"
static int cmp_pick(const void *a, const void *b) { const IbWord *x = *(IbWord *const *)a, *y = *(IbWord *const *)b; int kx = ib_kind_index(x->kind), ky = ib_kind_index(y->kind); if (kx != ky) return kx - ky; if (x->w != y->w) return x->w < y->w ? 1 : -1; return x->ord - y->ord; }
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: img_prompt words.pdl parts.pdl brixel [min_weight] [overlay.cand]\n"); return 2; }
    double cut = argc > 4 && argv[4][0] ? atof(argv[4]) : 0.30; int bad = 0;
    int r = ib_load_words(argv[1]); if (r < 0) return 2; bad += r; r = ib_load_parts(argv[2]); if (r < 0) return 2; bad += r; bad += ib_check_parts();
    if (argc > 5 && argv[5][0]) { ib_quiet = 1; r = ib_load_edits(argv[5]); if (r < 0) return 2; bad += r; if (!r) bad += ib_validate_edits(); }
    IbPart *part = ib_part(argv[3]); if (!part) { printf("ERR no PART row for brixel %s\n", argv[3]); bad++; }
    if (bad) { printf("INVALID: %d errors\n", bad); return 1; }
    IbWord *pick[IB_MAXPW]; int np = 0; char dropped[1024] = "";
    for (int k = 0; k < part->n; k++) { IbWord *w = ib_word(part->ids[k]); if (w->w < cut) { strncat(dropped, w->id, sizeof dropped - strlen(dropped) - 2); strcat(dropped, " "); continue; } pick[np++] = w; }
    qsort(pick, np, sizeof *pick, cmp_pick);
    char pos[1024] = "", neg[512] = "", used[1024] = ""; for (int i = 0; i < np; i++) { char *d = !strcmp(pick[i]->kind, "negative") ? neg : pos; if (d[0]) strcat(d, ", "); strcat(d, pick[i]->text); char u[80]; snprintf(u, sizeof u, "%s(%.2f) ", pick[i]->id, pick[i]->w); strcat(used, u); }
    printf("PROMPT: %s\nNEGATIVE: %s\nUSED: %s\nDROPPED(<%.2f): %s\n", pos, neg, used, cut, dropped[0] ? dropped : "-");
    return 0;
}
