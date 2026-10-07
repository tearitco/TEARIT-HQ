/* inventory_op - Take / Place / slot / projection for an entity's inventory.
 *
 * The one command the desk and pc-hq both call; all logic is the shared
 * khtpm_inventory.c (+ khtpm_page_rows.c for the page row). See
 * 18.pc-hq/CURSWORD-POSSESSION-DESIGN.md.
 *
 *   inventory_op list    <entity_dir>
 *   inventory_op slot    <entity_dir> get|next|prev|<N>
 *   inventory_op project <entity_dir> <out_file>
 *   inventory_op take    <house> <taker_dir> <target_dir> [ledger]
 *   inventory_op place   <house> <taker_dir> <dest_parent> <cx> <cy> [name] [ledger] [--spawn]
 *
 * take:  stop the target's process if it has one (desk), move its directory
 *        into <taker>/inventory/, remove its row from the open page, and keep
 *        that row inside the item (taken_row.txt) so Place restores glyph/index.
 * place: move the item (named, else the selected slot) out to <dest_parent>,
 *        append a page row at cell (cx,cy), and with --spawn start it as a
 *        desk entity (khtpm_entity.+x, same launch the taskbar uses).
 * Output: one line, "ok ..." or "err <code> <what>"; exit 0 / 1.
 *
 * Build: gcc -O2 -I../../_shared-lib -o inventory_op.+x inventory_op.c */
#define _GNU_SOURCE
#include "khtpm_page_rows.c"
#include "khtpm_inventory.c"

static int err(int code, const char *what) {
    printf("err %d %s\n", code, what);
    return 1;
}

static void rel_to_house(const char *house, const char *abs, char *out, size_t n) {
    size_t hl = strlen(house);
    if (!strncmp(abs, house, hl) && abs[hl] == '/') snprintf(out, n, "%s", abs + hl + 1);
    else snprintf(out, n, "%s", abs);
}

static void spawn_desk_entity(const char *house, const char *dir) {
    char exe[INV_PATH];
    pid_t pid;
    snprintf(exe, sizeof(exe), "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_entity.+x", house);
    if (access(exe, X_OK) != 0) return;
    pid = fork();
    if (pid == 0) {
        setsid();
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl(exe, exe, dir, (char *)NULL);
        _exit(1);
    }
}

int main(int argc, char **argv) {
    const char *cmd = argc > 1 ? argv[1] : "";
    if (!strcmp(cmd, "list") && argc >= 3) {
        char names[INV_MAX][INV_NAME];
        int n = inv_list(argv[2], names, INV_MAX), i, sel = inv_slot_get(argv[2], n);
        for (i = 0; i < n; i++) printf("%s%d %s\n", i == sel ? "*" : " ", i, names[i]);
        return 0;
    }
    if (!strcmp(cmd, "slot") && argc >= 4) {
        char names[INV_MAX][INV_NAME];
        int n = inv_list(argv[2], names, INV_MAX), s;
        if (!strcmp(argv[3], "get")) s = inv_slot_get(argv[2], n);
        else if (!strcmp(argv[3], "next")) s = inv_slot_step(argv[2], 1, n);
        else if (!strcmp(argv[3], "prev")) s = inv_slot_step(argv[2], -1, n);
        else s = inv_slot_set(argv[2], atoi(argv[3]), n);
        printf("ok slot=%d count=%d\n", s, n);
        return 0;
    }
    if (!strcmp(cmd, "project") && argc >= 4) {
        int n = inv_project(argv[2], argv[3]);
        if (n < 0) return err(INV_E_RENAME, "cannot write projection");
        printf("ok count=%d\n", n);
        return 0;
    }
    if (!strcmp(cmd, "take") && argc >= 5) {
        const char *house = argv[2], *taker = argv[3], *target = argv[4];
        const char *ledger = argc > 5 ? argv[5] : NULL;
        char tname[INV_NAME], row[2048], got[INV_NAME], rowfile[INV_PATH];
        int rc, stopped;
        snprintf(tname, sizeof(tname), "%s", inv_base(target));
        if (!inv_is_dir(target)) return err(INV_E_NOTDIR, "target is not a directory");
        if (!strcmp(taker, target)) return err(INV_E_CYCLE, "cannot take itself");
        stopped = inv_stop_entity(target);          /* no-op for pc-hq entities */
        rc = inv_take(taker, target, got, sizeof(got));
        if (rc != INV_OK) return err(rc, "take refused");
        /* page row: remove it, keep it with the item */
        if (pgr_remove_row(house, tname, row, sizeof(row)) && row[0]) {
            snprintf(rowfile, sizeof(rowfile), "%s/inventory/%s/taken_row.txt", taker, got);
            FILE *f = fopen(rowfile, "w");
            if (f) { fprintf(f, "%s\n", row); fclose(f); }
        }
        inv_ledger(ledger, "inv_take", inv_base(taker), got);
        printf("ok take item=%s stopped=%d\n", got, stopped);
        return 0;
    }
    if (!strcmp(cmd, "place") && argc >= 7) {
        const char *house = argv[2], *taker = argv[3], *dest_parent = argv[4];
        int cx = atoi(argv[5]), cy = atoi(argv[6]), spawn = 0, i;
        const char *name = NULL, *ledger = NULL;
        char dest[INV_PATH], rel[INV_PATH], glyph[64] = "", rowfile[INV_PATH], line[2048], *fld[10];
        int rc, rowed;
        FILE *f;
        for (i = 7; i < argc; i++) {
            if (!strcmp(argv[i], "--spawn")) spawn = 1;
            else if (!name) name = argv[i];
            else if (!ledger) ledger = argv[i];
        }
        rc = inv_place(taker, name, dest_parent, dest, sizeof(dest));
        if (rc != INV_OK) return err(rc, rc == INV_E_EMPTY ? "nothing to place" : "place refused");
        /* glyph: from the row it had when taken, else its glyph.txt */
        snprintf(rowfile, sizeof(rowfile), "%s/taken_row.txt", dest);
        if ((f = fopen(rowfile, "r"))) {
            if (fgets(line, sizeof(line), f) && pgr_split(line, fld, 10) >= 8) snprintf(glyph, sizeof(glyph), "%s", fld[7]);
            fclose(f);
            remove(rowfile);
        }
        if (!glyph[0]) {
            snprintf(rowfile, sizeof(rowfile), "%s/glyph.txt", dest);
            if ((f = fopen(rowfile, "r"))) { if (fgets(glyph, sizeof(glyph), f)) glyph[strcspn(glyph, "\r\n")] = '\0'; fclose(f); }
        }
        rel_to_house(house, dest, rel, sizeof(rel));
        rowed = pgr_append_row(house, inv_base(dest), rel, cx, cy, glyph);
        if (spawn) spawn_desk_entity(house, dest);
        inv_ledger(ledger, "inv_place", inv_base(taker), inv_base(dest));
        printf("ok place item=%s at=%d,%d row=%d spawned=%d\n", inv_base(dest), cx, cy, rowed, spawn);
        return 0;
    }
    fprintf(stderr, "usage: inventory_op list|slot|project|take|place ...  (see header)\n");
    return 2;
}
