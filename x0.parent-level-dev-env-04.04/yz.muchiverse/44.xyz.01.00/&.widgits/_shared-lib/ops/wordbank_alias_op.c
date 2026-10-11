/* wordbank_alias_op - resolve a phrase against an entity's wordbank, or record a use-score.
 *
 * Text-included from &.widgits/_shared-lib/khtpm_wordbank.c.
 * Design: ENTITY-WORD-BANK-DESIGN.md sec 7 (Parser/AI wiring).
 *
 * Usage:
 *   wordbank_alias_op.+x --lookup <entity_dir> "<input phrase>"
 *     Prints: CANON=<canon>|ALIAS=<alias>|WEIGHT=<w>  on match (exit 0)
 *             no match (exit 1)
 *   wordbank_alias_op.+x --use <entity_dir> "<canon>" "<alias>" <valence> [pal_hash]
 *     Appends a SCORE|...|source=use row to <entity_dir>/inventory/zz.wordbank/scores.txt
 *   wordbank_alias_op.+x --selftest
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <glob.h>
#include "../khtpm_wordbank.c"

static int selftest(void) {
    char tmpdir[] = "/tmp/wb_alias_selftest_XXXXXX";
    int bad = 0;
    if (!mkdtemp(tmpdir)) { fprintf(stderr, "FAIL mkdtemp\n"); return 1; }
    char invdir[512], wbdir[512], words[512], scores[512];
    snprintf(invdir, sizeof(invdir), "%s/inventory", tmpdir);
    mkdir(invdir, 0755);
    snprintf(wbdir, sizeof(wbdir), "%s/zz.wordbank", invdir);
    mkdir(wbdir, 0755);
    snprintf(words, sizeof(words), "%s/words.txt", wbdir);
    snprintf(scores, sizeof(scores), "%s/scores.txt", wbdir);
    /* seed words.txt with a known alias */
    FILE *f = fopen(words, "w");
    fprintf(f, "CANON=action:follow|ALIAS=follow|WEIGHT=0.8|SOURCE=user\n");
    fprintf(f, "CANON=action:stay|ALIAS=wait|WEIGHT=0.5|SOURCE=seed\n");
    fclose(f);
    f = fopen(scores, "w"); fclose(f);

    /* test lookup: "follow" should match action:follow */
    char canon[512], alias[256]; double weight;
    if (!wb_alias_lookup(tmpdir, "follow the red robot", canon, sizeof(canon), alias, sizeof(alias), &weight)) {
        fprintf(stderr, "FAIL lookup: no match for 'follow'\n"); bad++;
    } else if (strcmp(canon, "action:follow") != 0 || strcmp(alias, "follow") != 0 || weight != 0.8) {
        fprintf(stderr, "FAIL lookup: got %s|%s|%.4f\n", canon, alias, weight); bad++;
    }

    /* test lookup: "wait" should match action:stay (whole-word) */
    if (!wb_alias_lookup(tmpdir, "wait here", canon, sizeof(canon), alias, sizeof(alias), &weight)) {
        fprintf(stderr, "FAIL lookup: no match for 'wait'\n"); bad++;
    } else if (strcmp(canon, "action:stay") != 0) {
        fprintf(stderr, "FAIL lookup: got %s|%s\n", canon, alias); bad++;
    }

    /* test lookup: "following" should NOT match "follow" (not a whole word) */
    if (wb_alias_lookup(tmpdir, "following", canon, sizeof(canon), alias, sizeof(alias), &weight)) {
        fprintf(stderr, "FAIL lookup: 'following' should not match 'follow'\n"); bad++;
    }

    /* test use-score */
    wb_use_score(tmpdir, "action:follow", "follow", 1, "");
    char *buf = NULL;
    f = fopen(scores, "r");
    if (f) {
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        buf = malloc(sz + 1);
        size_t n = fread(buf, 1, sz, f); buf[n] = '\0';
        fclose(f);
    }
    if (!buf || strstr(buf, "SCORE|action:follow|follow|valence=+1|source=use") == NULL) {
        fprintf(stderr, "FAIL use_score: %s\n", buf ? buf : "(null)"); bad++;
    }
    free(buf);

    char cmd[512]; snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmpdir); system(cmd);
    printf(bad ? "selftest FAILED (%d)\n" : "selftest ok (lookup, whole-word, use-score)\n", bad);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest();

    if (argc > 1 && !strcmp(argv[1], "--lookup") && argc >= 4) {
        char canon[512], alias[256]; double weight;
        if (wb_alias_lookup(argv[2], argv[3], canon, sizeof(canon), alias, sizeof(alias), &weight)) {
            printf("CANON=%s|ALIAS=%s|WEIGHT=%.4f\n", canon, alias, weight);
            return 0;
        }
        return 1;
    }

    if (argc > 1 && !strcmp(argv[1], "--use") && argc >= 6) {
        int valence = atoi(argv[5]);
        const char *pal_hash = (argc >= 7) ? argv[6] : "";
        wb_use_score(argv[2], argv[3], argv[4], valence, pal_hash);
        return 0;
    }

    fprintf(stderr, "usage: wordbank_alias_op.+x --lookup <entity_dir> \"<input>\" | --use <entity_dir> <canon> <alias> <valence> [pal_hash] | --selftest\n");
    return 2;
}
