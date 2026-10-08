/* wordbank_ensure_op - seed every entity's word / synonym bank (zz.wordbank/) from on-disk sources.
 *
 * Usage: wordbank_ensure_op.+x <house_root> [--apply] [--report FILE] [--pals-root DIR] [--selftest]
 *   default is a DRY RUN: nothing is written, the report says what would happen.
 *   <house_root>      the 44.xyz.01.00 folder; pals are found at xyzfs/users/<uuid>/home/livedesk/pals/<entity>
 *   --pals-root DIR   scan this one pals folder instead (used to test --apply on a COPY of the pals tree)
 *   --report FILE     where the report is written (default stdout)
 *   --selftest        check row parsing and seed dedup, then exit
 * Exit: 0 ok, 1 errors (write failures), 2 usage.
 * The work is in the text-included &.widgits/_shared-lib/khtpm_wordbank.c (the manager's spawn hook will call the same wb_ensure()).
 */

#define _GNU_SOURCE
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../khtpm_wordbank.c"

static int selftest(void) {
    char tmpdir[] = "/tmp/wb_selftest_XXXXXX";
    int bad = 0;

    if (!mkdtemp(tmpdir)) { fprintf(stderr, "FAIL mkdtemp\n"); return 1; }

    /* test wb_row_exists */
    do {
        char wp[1024]; snprintf(wp, sizeof(wp), "%s/words.txt", tmpdir);
        FILE *f = fopen(wp, "w"); if (!f) { fprintf(stderr, "FAIL open words.txt\n"); bad++; break; }
        fprintf(f, "CANON=name:foo|ALIAS=foo|WEIGHT=0.5|SOURCE=seed\n");
        fprintf(f, "CANON=kind:bar|ALIAS=bar|WEIGHT=0.5|SOURCE=seed\n");
        fclose(f);
        if (!wb_row_exists(wp, "name:foo", "foo")) { fprintf(stderr, "FAIL row_exists true\n"); bad++; }
        if (!wb_row_exists(wp, "kind:bar", "bar")) { fprintf(stderr, "FAIL row_exists false\n"); bad++; }
    } while (0);

    /* test wb_meta_kind */
    do {
        char mp[1024]; snprintf(mp, sizeof(mp), "%s/meta.pdl", tmpdir);
        FILE *f = fopen(mp, "w"); if (!f) { fprintf(stderr, "FAIL open meta.pdl\n"); bad++; break; }
        fprintf(f, "STATE        | kind                 | robot\nMETHOD       | go                  | sh -c 'true'\n");
        fclose(f);
        char kind[128] = "";
        if (!wb_meta_kind(tmpdir, kind, sizeof(kind)) || strcmp(kind, "robot")) { fprintf(stderr, "FAIL wb_meta_kind got %s\n", kind); bad++; }
    } while (0);

    /* test wb_menu_labels */
    do {
        char mp[1024]; snprintf(mp, sizeof(mp), "%s/menu.chtpm", tmpdir);
        FILE *f = fopen(mp, "w"); if (!f) { fprintf(stderr, "FAIL open menu.chtpm\n"); bad++; break; }
        fprintf(f, "<item label=\"Go\"/><item label=\"Stop\"/><item label=\"Go\"/>\n");
        fclose(f);
        char labels[4][128]; size_t nl = 0;
        wb_menu_labels(tmpdir, labels, 4, &nl);
        if (nl != 3 || strcmp(labels[0], "Go") || strcmp(labels[1], "Stop") || strcmp(labels[2], "Go")) { fprintf(stderr, "FAIL wb_menu_labels nl=%zu %s/%s/%s\n", nl, labels[0], labels[1], labels[2]); bad++; }
    } while (0);

    /* test wb_method_names */
    do {
        char mp[1024]; snprintf(mp, sizeof(mp), "%s/meta.pdl", tmpdir);
        FILE *f = fopen(mp, "w"); if (!f) { fprintf(stderr, "FAIL open meta.pdl\n"); bad++; break; }
        fprintf(f, "METHOD       | Events (hq)          | sh -c 'true'\nMETHOD       | Dir                  | sh -c 'true'\n");
        fclose(f);
        char names[4][128]; size_t nm = 0;
        wb_method_names(tmpdir, names, 4, &nm);
        if (nm != 2 || strcmp(names[0], "Events (hq)") || strcmp(names[1], "Dir")) { fprintf(stderr, "FAIL wb_method_names nm=%zu %s/%s\n", nm, names[0], names[1]); bad++; }
    } while (0);

    /* test wb_add_seed dedup */
    do {
        char inv[1024], wbdir[1024]; snprintf(inv, sizeof(inv), "%s/inventory", tmpdir); snprintf(wbdir, sizeof(wbdir), "%s/zz.wordbank", inv);
        mkdir(inv, 0755); mkdir(wbdir, 0755);
        char wp[1024]; snprintf(wp, sizeof(wp), "%s/words.txt", wbdir);
        FILE *f = fopen(wp, "w"); if (!f) { fprintf(stderr, "FAIL open words.txt\n"); bad++; break; }
        fprintf(f, "CANON=name:foo|ALIAS=foo|WEIGHT=0.5|SOURCE=seed\n");
        fclose(f);
        WbCtx ctx; memset(&ctx, 0, sizeof(ctx)); ctx.apply = 1; ctx.report = stdout;
        ctx.n_seeds_added = 0;
        wb_add_seed(&ctx, tmpdir, "name:foo", "foo");
        if (ctx.n_seeds_added != 0) { fprintf(stderr, "FAIL wb_add_seed dedup added=%d\n", ctx.n_seeds_added); bad++; }
        wb_add_seed(&ctx, tmpdir, "name:bar", "bar");
        if (ctx.n_seeds_added != 1) { fprintf(stderr, "FAIL wb_add_seed new added=%d\n", ctx.n_seeds_added); bad++; }
    } while (0);

    /* cleanup */
    char cmd[512]; snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmpdir);
    system(cmd);

    printf(bad ? "selftest FAILED (%d)\n" : "selftest ok (parsing, dedup)\n", bad);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    static WbCtx ctx; const char *house = NULL, *report = NULL, *pals_root = NULL; FILE *rf = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) return selftest();
        else if (!strcmp(argv[i], "--apply")) ctx.apply = 1;
        else if (!strcmp(argv[i], "--report") && i + 1 < argc) report = argv[++i];
        else if (!strcmp(argv[i], "--pals-root") && i + 1 < argc) pals_root = argv[++i];
        else if (argv[i][0] != '-' && !house) house = argv[i];
        else { fprintf(stderr, "unknown argument: %s\n", argv[i]); return 2; }
    }
    if (!house && !pals_root) { fprintf(stderr, "usage: wordbank_ensure_op.+x <house_root> [--apply] [--report FILE] [--pals-root DIR] [--selftest]\n"); return 2; }
    if (report && !(rf = fopen(report, "w"))) { fprintf(stderr, "cannot write %s\n", report); return 2; }
    ctx.report = rf ? rf : stdout;
    fprintf(ctx.report, "wordbank_ensure_op  %s\n", ctx.apply ? "APPLY" : "DRY RUN (nothing is written)");
    {
        char pat[WB_BUF]; glob_t g; size_t gi, n_roots = 0;
        if (pals_root) snprintf(pat, sizeof(pat), "%s", pals_root);
        else snprintf(pat, sizeof(pat), "%s/xyzfs/users/*/home/livedesk/pals", house);
        if (glob(pat, 0, NULL, &g) == 0) {
            for (gi = 0; gi < g.gl_pathc; gi++) {
                DIR *d = opendir(g.gl_pathv[gi]); struct dirent *e; char *names[4096]; size_t nn = 0, k;
                if (!d) continue;
                n_roots++;
                fprintf(ctx.report, "pals root: %s\n", g.gl_pathv[gi]);
                while ((e = readdir(d)) && nn < 4096) {
                    char child[WB_BUF];
                    if (e->d_name[0] == '.') continue;
                    if (!wb_join(child, sizeof(child), g.gl_pathv[gi], e->d_name)) continue;
                    if (wb_is_dir(child)) names[nn++] = strdup(child);
                }
                closedir(d);
                for (k = 0; k < nn; k++) for (size_t m = k + 1; m < nn; m++) if (strcmp(names[k], names[m]) > 0) { char *t = names[k]; names[k] = names[m]; names[m] = t; }
                for (k = 0; k < nn; k++) { wb_ensure(names[k], &ctx, 0); free(names[k]); }
            }
            globfree(&g);
        }
        if (!n_roots) { fprintf(stderr, "no pals root found (%s)\n", pat); return 2; }
    }
    fprintf(ctx.report, "\nSUMMARY (%s)\n  entities found:            %d  (top-level %d, items inside inventories %d)\n  already have a bank:      %d\n"
        "  banks %s:         %d\n  seeds added:              %d\n  errors:                   %d\n",
        ctx.apply ? "APPLIED" : "dry run", ctx.n_entities, ctx.n_entities - ctx.n_nested, ctx.n_nested,
        ctx.n_have_bank, ctx.apply ? "created" : "to create", ctx.n_new_bank, ctx.n_seeds_added, ctx.n_errors);
    if (rf) fclose(rf);
    return ctx.n_errors ? 1 : 0;
}
