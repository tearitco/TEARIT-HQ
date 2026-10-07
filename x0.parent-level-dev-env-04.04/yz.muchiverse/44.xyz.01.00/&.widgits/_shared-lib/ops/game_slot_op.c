/* game_slot_op - save-game / load-game SLOTS for the player (tb "9 player" dropdown -> save-game / load-game -> 16 slots).
 *
 * v1 is AUDITABLE and NON-DESTRUCTIVE (owner request 2026-10-06: slots will later record "played progress" - events done, what was created/destroyed,
 * scores, menu state - so scripted, time/event-dependent play like RPG Maker can be re-audited; shipping as a playtest needs this):
 *   save N : records slot N = a SHA-256 manifest of the user's whole entity tree (every file: sha|size|path), a meta.pdl, and one line in the
 *            append-only savegames/ledger.txt. It copies and changes NO entity data.
 *   load N : compares the CURRENT entity tree against slot N's manifest (same / changed / missing / new), writes savegames/last_load.txt with the
 *            changed paths, records savegames/loaded_slot.txt and a ledger line. It RESTORES NOTHING yet: overwriting entities that are running is the
 *            next step and needs the owner's design (see design doc, "save slots"). Reporting honestly is the point of v1.
 *
 * Usage: game_slot_op.+x <savegames_dir> <pals_dir> save|load <N 1-16> [--now-ms MS]
 * Files (all under <savegames_dir>, per user, so they live in the user's data branch, not in code):
 *   slot_NN/meta.pdl      SLOT | key | value   (n, saved_ms, saved_at, files, bytes, manifest_sha)
 *   slot_NN/manifest.txt  <sha256>|<size>|<path relative to pals_dir>, sorted by path
 *   ledger.txt            <epoch_ms>|save|N|files=F|manifest=H   and   <epoch_ms>|load|N|same=A|changed=B|missing=C|new=D   (append only)
 *   last_load.txt, loaded_slot.txt
 * Exit: 0 ok, 1 slot empty (load) / failed, 2 usage. Output: one summary line on stdout. Marker rule: nothing here reads mtime. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "../khtpm_phone.c"   /* ph_sha_hex: the house's one SHA-256 */

#define GP 4096
typedef struct { char sha[65]; long size; char *path; } Ent;
static Ent *E; static int NE, CAP; static char ROOT[GP]; static size_t ROOTLEN;

static int cmp_ent(const void *a, const void *b) { return strcmp(((const Ent *)a)->path, ((const Ent *)b)->path); }
static void add_ent(const char *sha, long size, const char *path) {
    if (NE == CAP) { CAP = CAP ? CAP * 2 : 2048; E = realloc(E, (size_t)CAP * sizeof(Ent)); if (!E) { fprintf(stderr, "out of memory\n"); exit(1); } }
    snprintf(E[NE].sha, sizeof(E[NE].sha), "%s", sha); E[NE].size = size; E[NE].path = strdup(path); NE++;
}
static int visit(const char *fpath, const struct stat *sb, int type, struct FTW *ftw) {
    (void)ftw; if (type != FTW_F || !S_ISREG(sb->st_mode)) return 0;
    const char *rel = fpath + ROOTLEN; while (*rel == '/') rel++;
    char sha[65] = "-";
    if (sb->st_size <= 32L * 1024 * 1024) {                       /* bigger files: size only (marker rule: growth is the signal) */
        FILE *f = fopen(fpath, "rb");
        if (f) { unsigned char *buf = malloc((size_t)sb->st_size + 1); size_t n = buf ? fread(buf, 1, (size_t)sb->st_size, f) : 0; fclose(f);
                 if (buf) { ph_sha_hex(buf, n, sha); free(buf); } }
    }
    add_ent(sha, (long)sb->st_size, rel);
    return 0;
}
static void free_ents(void) { for (int i = 0; i < NE; i++) free(E[i].path); free(E); E = NULL; NE = CAP = 0; }
static int scan(const char *root) { snprintf(ROOT, sizeof(ROOT), "%s", root); ROOTLEN = strlen(ROOT); free_ents(); if (nftw(ROOT, visit, 32, FTW_PHYS) != 0) return -1; qsort(E, (size_t)NE, sizeof(Ent), cmp_ent); return 0; }
static long long now_ms(void) { struct timeval tv; gettimeofday(&tv, NULL); return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000; }
static int put_atomic(const char *path, const char *data, size_t len) {
    char tmp[GP + 8]; snprintf(tmp, sizeof(tmp), "%s.tmp", path); FILE *f = fopen(tmp, "wb"); if (!f) return -1;
    if (fwrite(data, 1, len, f) != len) { fclose(f); return -1; } fclose(f); return rename(tmp, path);
}
static void ledger(const char *sg, const char *line) { char p[GP]; snprintf(p, sizeof(p), "%s/ledger.txt", sg); FILE *f = fopen(p, "a"); if (f) { fputs(line, f); fputc('\n', f); fclose(f); } }

int main(int argc, char **argv) {
    const char *sg, *pals, *verb; int n; long long ms = 0; char slot[GP], p[GP + 64];
    if (argc < 5) { fprintf(stderr, "usage: game_slot_op.+x <savegames_dir> <pals_dir> save|load <N 1-16> [--now-ms MS]\n"); return 2; }
    sg = argv[1]; pals = argv[2]; verb = argv[3]; n = atoi(argv[4]);
    for (int i = 5; i + 1 < argc; i++) if (!strcmp(argv[i], "--now-ms")) ms = atoll(argv[i + 1]);
    if (!ms) ms = now_ms();
    if (n < 1 || n > 16 || (strcmp(verb, "save") && strcmp(verb, "load"))) { fprintf(stderr, "game_slot_op: need save|load and a slot 1-16\n"); return 2; }
    { struct stat st; if (stat(pals, &st) != 0 || !S_ISDIR(st.st_mode)) { fprintf(stderr, "game_slot_op: no pals dir %s\n", pals); return 1; } }
    snprintf(slot, sizeof(slot), "%s/slot_%02d", sg, n);
    if (scan(pals) != 0) { fprintf(stderr, "game_slot_op: cannot scan %s\n", pals); return 1; }

    if (!strcmp(verb, "save")) {
        size_t cap = (size_t)NE * (GP / 8) + 64, len = 0; char *m = malloc(cap); long long bytes = 0; char mh[65], meta[1024], at[32], led[256]; time_t t = (time_t)(ms / 1000);
        if (!m) return 1;
        for (int i = 0; i < NE; i++) { size_t need = strlen(E[i].path) + 100; if (len + need > cap) { cap = (cap + need) * 2; m = realloc(m, cap); if (!m) return 1; }
            len += (size_t)snprintf(m + len, cap - len, "%s|%ld|%s\n", E[i].sha, E[i].size, E[i].path); bytes += E[i].size; }
        ph_sha_hex((const unsigned char *)m, len, mh);
        mkdir(sg, 0755); mkdir(slot, 0755);
        strftime(at, sizeof(at), "%Y-%m-%d %H:%M:%S", localtime(&t));
        snprintf(p, sizeof(p), "%s/manifest.txt", slot); if (put_atomic(p, m, len) != 0) { fprintf(stderr, "game_slot_op: cannot write %s\n", p); free(m); return 1; }
        snprintf(meta, sizeof(meta), "# save slot %d - see game_slot_op.c\nSLOT | n | %d\nSLOT | saved_ms | %lld\nSLOT | saved_at | %s\nSLOT | files | %d\nSLOT | bytes | %lld\nSLOT | manifest_sha | %s\n", n, n, ms, at, NE, bytes, mh);
        snprintf(p, sizeof(p), "%s/meta.pdl", slot); if (put_atomic(p, meta, strlen(meta)) != 0) { free(m); return 1; }
        snprintf(led, sizeof(led), "%lld|save|%d|files=%d|manifest=%s", ms, n, NE, mh); ledger(sg, led);
        printf("saved slot %d: %d files, %lld bytes, manifest %.12s\n", n, NE, bytes, mh);
        free(m); free_ents(); return 0;
    }
    /* load: compare current tree with the slot's manifest; restore nothing */
    {
        char line[GP + 200]; FILE *f; int same = 0, changed = 0, missing = 0, fresh = 0, shown = 0; char *rep = malloc(1 << 20); size_t rl = 0; char led[256]; char *seen;
        snprintf(p, sizeof(p), "%s/manifest.txt", slot);
        if (!(f = fopen(p, "r"))) { fprintf(stderr, "slot %d is empty\n", n); free(rep); free_ents(); return 1; }
        if (!rep) return 1;
        seen = calloc((size_t)NE + 1, 1);
        if (!seen) return 1;
        rep[0] = 0;
        while (fgets(line, sizeof(line), f)) {
            char *a = strchr(line, '|'), *b, *path; long size; line[strcspn(line, "\r\n")] = 0; if (!a) continue; *a = 0; b = strchr(a + 1, '|'); if (!b) continue; *b = 0; path = b + 1; size = atol(a + 1);
            Ent key; key.path = path; Ent *hit = bsearch(&key, E, (size_t)NE, sizeof(Ent), cmp_ent);
            if (!hit) { missing++; if (shown < 200 && rl < (1 << 20) - 600) { rl += (size_t)snprintf(rep + rl, (1 << 20) - rl, "missing|%s\n", path); shown++; } continue; }
            seen[hit - E] = 1;
            if (!strcmp(hit->sha, line) && hit->size == size) same++;
            else { changed++; if (shown < 200 && rl < (1 << 20) - 600) { rl += (size_t)snprintf(rep + rl, (1 << 20) - rl, "changed|%s\n", path); shown++; } }
        }
        fclose(f);
        for (int i = 0; i < NE; i++) if (!seen[i]) { fresh++; if (shown < 200 && rl < (1 << 20) - 600) { rl += (size_t)snprintf(rep + rl, (1 << 20) - rl, "new|%s\n", E[i].path); shown++; } }
        snprintf(p, sizeof(p), "%s/last_load.txt", sg);
        { char head[256]; int hl = snprintf(head, sizeof(head), "# load of slot %d at %lld: same=%d changed=%d missing=%d new=%d (first 200 differences below; NOTHING was restored)\n", n, ms, same, changed, missing, fresh);
          char *all = malloc((size_t)hl + rl + 1); if (all) { memcpy(all, head, (size_t)hl); memcpy(all + hl, rep, rl); put_atomic(p, all, (size_t)hl + rl); free(all); } }
        { char nb[16]; int nl = snprintf(nb, sizeof(nb), "%d\n", n); snprintf(p, sizeof(p), "%s/loaded_slot.txt", sg); put_atomic(p, nb, (size_t)nl); }
        snprintf(led, sizeof(led), "%lld|load|%d|same=%d|changed=%d|missing=%d|new=%d", ms, n, same, changed, missing, fresh); ledger(sg, led);
        printf("loaded slot %d (compare only, nothing restored): same=%d changed=%d missing=%d new=%d\n", n, same, changed, missing, fresh);
        free(rep); free(seen); free_ents(); return 0;
    }
}
