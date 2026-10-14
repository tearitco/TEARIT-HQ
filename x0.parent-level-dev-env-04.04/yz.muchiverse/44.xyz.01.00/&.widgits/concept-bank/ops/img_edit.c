/* img_edit - validate and promote candidate edits to the image word bank (BOOK 16.game/WORD-BANK-TO-IMAGE.md, &.hq-apps/img-hai/docs/EXTENDING.md).
 * Usage: img_edit validate <words.pdl> <parts.pdl> <file.cand>
 *        img_edit promote  <words.pdl> <parts.pdl> <file.cand> <edit-id> <promoter> <ledger.pdl>
 * validate: prints OK/REJECT per edit, exit 0 only if every edit is valid (applied in file order on an in-memory copy; the bank files are not touched).
 * promote: applies edits from the file in order up to and including <edit-id> (earlier ones are prerequisites), writes words.pdl/parts.pdl, appends one PROMOTED line to the ledger. A model never promotes:
 *   the promoter name must differ from the proposer of every applied edit, and is recorded. Promotion is the only path that writes the bank.
 * Exit 0 ok, 1 invalid/refused, 2 usage or cannot open. */
#include "img_bank.c"
#include <time.h>
int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: img_edit validate W P C | promote W P C id promoter ledger\n"); return 2; }
    int promote = !strcmp(argv[1], "promote"); if (!promote && strcmp(argv[1], "validate")) return 2; if (promote && argc < 8) return 2;
    int bad = 0, r = ib_load_words(argv[2]); if (r < 0) return 2; bad += r; r = ib_load_parts(argv[3]); if (r < 0) return 2; bad += r; bad += ib_check_parts();
    if (bad) { printf("INVALID BANK: %d errors (fix the bank before editing it)\n", bad); return 1; }
    r = ib_load_edits(argv[4]); if (r < 0) return 2; if (r) { printf("INVALID: %d malformed lines\n", r); return 1; }
    if (!promote) { int b = ib_validate_edits(); printf("%s: %d edits, %d invalid\n", b ? "INVALID" : "VALID", ib_nE, b); return b ? 1 : 0; }
    const char *id = argv[5], *who = argv[6]; int upto = -1; for (int i = 0; i < ib_nE; i++) if (!strcmp(IB_E[i].id, id)) upto = i; if (upto < 0) { printf("REFUSED: no edit %s in %s\n", id, argv[4]); return 1; }
    for (int i = 0; i <= upto; i++) { if (!strcmp(IB_E[i].proposer, who)) { printf("REFUSED: promoter %s proposed edit %s; a different person must promote it\n", who, IB_E[i].id); return 1; } }
    for (int i = 0; i <= upto; i++) { char err[200] = ""; if (ib_apply_edit(&IB_E[i], err, sizeof err)) { printf("REJECT %s: %s (nothing written)\n", IB_E[i].id, err); return 1; } }
    if (ib_rewrite(argv[2], 1) || ib_rewrite(argv[3], 0)) { printf("ERROR: could not write the bank\n"); return 2; }
    FILE *lf = fopen(argv[7], "a"); if (lf) { time_t t = time(NULL); char day[16]; strftime(day, sizeof day, "%Y-%m-%d", localtime(&t)); for (int i = 0; i <= upto; i++) fprintf(lf, "PROMOTED | %s | %s | %s | %s | %s | %s\n", IB_E[i].id, IB_E[i].type, IB_E[i].target, IB_E[i].proposer, who, day); fclose(lf); }
    printf("PROMOTED %d edit(s) up to %s by %s\n", upto + 1, id, who); return 0;
}
