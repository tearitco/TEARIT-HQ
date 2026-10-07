/* phone_ensure_op - give every entity a phone and a permanent identity (Q005, design HAI-ROBOTS-PHONES-SERVER-DESIGN.md sec 3b/3c).
 *
 * Usage: phone_ensure_op.+x <house_root> [--apply] [--report FILE] [--pals-root DIR] [--index FILE] [--template DIR] [--selftest]
 *   default is a DRY RUN: nothing is written, the report says what would happen.
 *   <house_root>      the 44.xyz.01.00 folder; pals are found at xyzfs/users/<uuid>/home/livedesk/pals/<entity>
 *   --pals-root DIR   scan this one pals folder instead (used to test --apply on a COPY of the pals tree)
 *   --index FILE      where phones.index is appended (default <house_root>/^.hai-server/phones.index)
 *   --selftest        check SHA-256 against known vectors and the number format, then exit
 * Exit: 0 ok, 1 errors (collisions / write failures), 2 usage.
 * The work is in the text-included &.widgits/_shared-lib/khtpm_phone.c (the manager's spawn hook will call the same ph_ensure()). */
#define _GNU_SOURCE
#include <glob.h>
#include "../khtpm_phone.c"

static int selftest(void) {
    char h[65], n[16]; int bad = 0;
    ph_sha_hex((const unsigned char *)"abc", 3, h);
    if (strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) { fprintf(stderr, "FAIL sha256(abc)=%s\n", h); bad++; }
    ph_sha_hex((const unsigned char *)"", 0, h);
    if (strcmp(h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")) { fprintf(stderr, "FAIL sha256(empty)=%s\n", h); bad++; }
    ph_sha_hex((const unsigned char *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, h);
    if (strcmp(h, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")) { fprintf(stderr, "FAIL sha256(448-bit)=%s\n", h); bad++; }
    ph_number("00000000000000010000000000000000ffffffffffffffff0123456789abcdef", 0, n, sizeof(n));
    if (strcmp(n, "000-0000-0001")) { fprintf(stderr, "FAIL number slice0=%s (want 000-0000-0001)\n", n); bad++; }
    ph_number("00000000000000010000000000000000ffffffffffffffff0123456789abcdef", 3, n, sizeof(n));
    if (strcmp(n, "292-1648-6895")) { fprintf(stderr, "FAIL number slice3=%s (want 292-1648-6895)\n", n); bad++; }
    printf(bad ? "selftest FAILED (%d)\n" : "selftest ok (sha256 vectors, number format)\n", bad);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    static PhCtx ctx; const char *house = NULL, *report = NULL, *pals_root = NULL, *index = NULL; FILE *rf = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) return selftest();
        else if (!strcmp(argv[i], "--apply")) ctx.apply = 1;
        else if (!strcmp(argv[i], "--report") && i + 1 < argc) report = argv[++i];
        else if (!strcmp(argv[i], "--pals-root") && i + 1 < argc) pals_root = argv[++i];
        else if (!strcmp(argv[i], "--index") && i + 1 < argc) index = argv[++i];
        else if (!strcmp(argv[i], "--template") && i + 1 < argc) snprintf(ctx.template_dir, sizeof(ctx.template_dir), "%s", argv[++i]);
        else if (argv[i][0] != '-' && !house) house = argv[i];
        else { fprintf(stderr, "unknown argument: %s\n", argv[i]); return 2; }
    }
    if (!house && !pals_root) { fprintf(stderr, "usage: phone_ensure_op.+x <house_root> [--apply] [--report FILE] [--pals-root DIR] [--index FILE] [--selftest]\n"); return 2; }
    if (index) snprintf(ctx.index_path, sizeof(ctx.index_path), "%s", index);
    else snprintf(ctx.index_path, sizeof(ctx.index_path), "%s/^.hai-server/phones.index", house ? house : ".");
    if (!ctx.template_dir[0] && house) { char t[PH_BUF]; ph_join(t, sizeof(t), house, "^.hai-phone/_TEMPLATE"); if (ph_is_dir(t)) snprintf(ctx.template_dir, sizeof(ctx.template_dir), "%s", t); }
    if (report && !(rf = fopen(report, "w"))) { fprintf(stderr, "cannot write %s\n", report); return 2; }
    ctx.report = rf ? rf : stdout;
    ph_load_index(&ctx);
    fprintf(ctx.report, "phone_ensure_op  %s  index=%s\n", ctx.apply ? "APPLY" : "DRY RUN (nothing is written)", ctx.index_path);
    {
        char pat[PH_BUF]; glob_t g; size_t gi, n_roots = 0;
        if (pals_root) snprintf(pat, sizeof(pat), "%s", pals_root);
        else snprintf(pat, sizeof(pat), "%s/xyzfs/users/*/home/livedesk/pals", house);
        if (glob(pat, 0, NULL, &g) == 0) {
            for (gi = 0; gi < g.gl_pathc; gi++) {
                DIR *d = opendir(g.gl_pathv[gi]); struct dirent *e; char *names[4096]; size_t nn = 0, k;
                if (!d) continue;
                n_roots++;
                fprintf(ctx.report, "pals root: %s\n", g.gl_pathv[gi]);
                while ((e = readdir(d)) && nn < 4096) {
                    char child[PH_BUF];
                    if (e->d_name[0] == '.') continue;
                    if (!ph_join(child, sizeof(child), g.gl_pathv[gi], e->d_name)) continue;
                    if (ph_is_dir(child)) names[nn++] = strdup(child);   /* every top-level folder is an entity, with or without pal.pdl (the manager spawns it either way) */
                }
                closedir(d);
                for (k = 0; k < nn; k++) for (size_t m = k + 1; m < nn; m++) if (strcmp(names[k], names[m]) > 0) { char *t = names[k]; names[k] = names[m]; names[m] = t; }
                for (k = 0; k < nn; k++) { ph_ensure(names[k], &ctx, 0); free(names[k]); }
            }
            globfree(&g);
        }
        if (!n_roots) { fprintf(stderr, "no pals root found (%s)\n", pat); return 2; }
    }
    fprintf(ctx.report, "\nSUMMARY (%s)\n  entities found:            %d  (top-level %d, items inside inventories %d)\n  already have a phone:      %d\n"
        "  phones %s:        %d\n  already have entity_uid:   %d\n  new uids from pal hash:    %d  (frozen, continuity with PAL | hash)\n"
        "  new random uids:           %d\n  phone sprites %s:    %d phones' files  (template: %s)\n  errors / collisions:       %d\n",
        ctx.apply ? "APPLIED" : "dry run", ctx.n_entities, ctx.n_entities - ctx.n_nested, ctx.n_nested, ctx.n_have_phone,
        ctx.apply ? "created" : "to create", ctx.n_new_phone, ctx.n_have_uid, ctx.n_new_uid_from_hash, ctx.n_new_uid_random,
        ctx.apply ? "added" : "missing", ctx.apply ? ctx.n_sprite_added : ctx.n_sprite_missing, ctx.template_dir[0] ? ctx.template_dir : "none", ctx.n_errors);
    if (rf) fclose(rf);
    return ctx.n_errors ? 1 : 0;
}
