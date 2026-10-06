/* khtpm_inventory.c - an entity's inventory: list, selected slot, Take, Place.
 *
 * Text-included canonical helper (same family as khtpm_page_rows.c and
 * khtpm_move_range.c): pure file I/O, no drawing, no environment policy.
 *
 * MODEL (the desk's, unchanged - see 18.pc-hq/CURSWORD-POSSESSION-DESIGN.md):
 *   - An entity's inventory is its own <entity_dir>/inventory/ directory.
 *   - An item IS an entity: a whole entity package directory. Drag-and-drop on
 *     the desk is already a plain rename of that directory (khtpm_entity.c,
 *     fe_drop.sh), so Take and Place are the same operation, in two directions:
 *       Take  = rename <target entity dir>  -> <taker>/inventory/<name>
 *       Place = rename <taker>/inventory/<name> -> <dest parent dir>/<name>
 *   - Nothing is deleted. A taken entity still exists, as an item.
 *   - Slot order is alphabetical by directory name, so it is stable across
 *     runs and across the desk and pc-hq. The selected slot (the Minecraft
 *     hotbar's highlighted slot; Place uses it) is <entity_dir>/inventory_slot.txt
 *     as "slot=N", per entity, so each entity remembers its own.
 *
 * What lives elsewhere (environment glue, not here):
 *   - the page-file row (pgr_remove_row / pgr_append_row, khtpm_page_rows.c)
 *   - spawning a placed desk entity's process, drawing the HUD
 *   - hearts / hunger: a future vitals reader sits beside this file in
 *     khtpm_possess.c; HOOK: inv_ledger() below is where an item with an
 *     effect (food, damage) would also report to it.
 *   - HOOK (later, owner request 2026-10-05): the inventory HUD gets a button
 *     that opens a Minecraft-style inventory menu in x11-hq. It reads exactly
 *     inv_list() + inv_slot_get(); nothing here needs to change for it.
 *
 * Functions are static + unused-tolerant. Prefix: inv_. */
#ifndef KHTPM_INVENTORY_C
#define KHTPM_INVENTORY_C

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__GNUC__)
#define INV_UNUSED __attribute__((unused))
#else
#define INV_UNUSED
#endif

#define INV_PATH 4400
#define INV_NAME 128
#define INV_MAX  64          /* slots listed; a hotbar shows the first few */

/* Return codes (negative = refused or failed, nothing moved). */
#define INV_OK          0
#define INV_E_ARGS     -1
#define INV_E_NOTDIR   -2    /* source is not a directory */
#define INV_E_CYCLE    -3    /* would move an entity into itself / its own inventory */
#define INV_E_NAMES    -4    /* no free destination name */
#define INV_E_RENAME   -5    /* rename() failed (errno kept), e.g. EXDEV */
#define INV_E_EMPTY    -6    /* no such item / empty inventory */

static const char *inv_base(const char *path) {
    const char *b = strrchr(path, '/');
    return b ? b + 1 : path;
}

static int inv_is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

INV_UNUSED static void inv_dir(const char *entity_dir, char *out, size_t n) {
    snprintf(out, n, "%s/inventory", entity_dir);
}

static int inv_name_cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

/* Item names, sorted, directories only, dot entries skipped. Returns the count
 * (0 for a missing or empty inventory). */
INV_UNUSED static int inv_list(const char *entity_dir, char (*names)[INV_NAME], int max) {
    char inv[INV_PATH];
    DIR *d;
    struct dirent *e;
    int n = 0;
    inv_dir(entity_dir, inv, sizeof(inv));
    if (!(d = opendir(inv))) return 0;
    while ((e = readdir(d)) && n < max) {
        char full[INV_PATH];
        if (e->d_name[0] == '.') continue;
        snprintf(full, sizeof(full), "%s/%s", inv, e->d_name);
        if (!inv_is_dir(full)) continue;
        snprintf(names[n++], INV_NAME, "%s", e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, INV_NAME, inv_name_cmp);
    return n;
}

/* ---- selected slot (the hotbar highlight) -------------------------------- */
static void inv_slot_path(const char *entity_dir, char *out, size_t n) {
    snprintf(out, n, "%s/inventory_slot.txt", entity_dir);
}

/* Selected slot, clamped into [0, count-1] (0 when the inventory is empty). */
INV_UNUSED static int inv_slot_get(const char *entity_dir, int count) {
    char p[INV_PATH], line[64];
    int slot = 0;
    FILE *f;
    inv_slot_path(entity_dir, p, sizeof(p));
    if ((f = fopen(p, "r"))) {
        if (fgets(line, sizeof(line), f) && !strncmp(line, "slot=", 5)) slot = atoi(line + 5);
        fclose(f);
    }
    if (count <= 0) return 0;
    if (slot < 0) slot = 0;
    if (slot >= count) slot = count - 1;
    return slot;
}

INV_UNUSED static int inv_slot_set(const char *entity_dir, int slot, int count) {
    char p[INV_PATH];
    FILE *f;
    if (count > 0) { if (slot < 0) slot = 0; if (slot >= count) slot = count - 1; } else slot = 0;
    inv_slot_path(entity_dir, p, sizeof(p));
    if (!(f = fopen(p, "w"))) return 0;
    fprintf(f, "slot=%d\n", slot);
    fclose(f);
    return slot;
}

/* Move the selection by delta (+1 next, -1 previous), wrapping. Returns the new slot. */
INV_UNUSED static int inv_slot_step(const char *entity_dir, int delta, int count) {
    int s;
    if (count <= 0) return 0;
    s = inv_slot_get(entity_dir, count) + delta;
    s %= count;
    if (s < 0) s += count;
    return inv_slot_set(entity_dir, s, count);
}

/* ---- stopping a running desk entity -------------------------------------
 * A desk entity is a live khtpm_entity process holding its own directory. To
 * Take it from outside, stop it first, then rename (the entity's own drag-drop
 * does the same in the other order: rename, then exit). SIGTERM is the
 * process's normal shutdown path. The pid in <dir>/module_parent.pid is only
 * signalled if /proc says that pid really is a khtpm_entity for THIS dir, so a
 * stale pid file can never terminate an unrelated process. pc-hq entities have
 * no process and no pid file: this is a no-op for them. Returns 1 if a process
 * was stopped, 0 if there was nothing to stop. */
static int inv_pid_alive(int pid) {
    char p[64], buf[256];
    FILE *f;
    snprintf(p, sizeof(p), "/proc/%d/stat", pid);
    if (!(f = fopen(p, "r"))) return 0;
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return 0; }
    fclose(f);
    { char *rp = strrchr(buf, ')'); if (rp && rp[1] == ' ' && rp[2] == 'Z') return 0; }  /* zombie */
    return 1;
}

INV_UNUSED static int inv_stop_entity(const char *entity_dir) {
#ifdef __linux__
    char p[INV_PATH], cmd[INV_PATH + 512];
    FILE *f;
    int pid = 0, i;
    size_t got;
    snprintf(p, sizeof(p), "%s/module_parent.pid", entity_dir);
    if (!(f = fopen(p, "r"))) return 0;
    if (fscanf(f, "%d", &pid) != 1) pid = 0;
    fclose(f);
    if (pid <= 1 || !inv_pid_alive(pid)) return 0;
    snprintf(p, sizeof(p), "/proc/%d/cmdline", pid);
    if (!(f = fopen(p, "r"))) return 0;
    got = fread(cmd, 1, sizeof(cmd) - 1, f);
    fclose(f);
    cmd[got] = '\0';
    for (i = 0; i + 1 < (int)got; i++) if (cmd[i] == '\0') cmd[i] = ' ';   /* argv -> one string */
    if (!strstr(cmd, "khtpm_entity") || !strstr(cmd, entity_dir)) return 0;
    kill(pid, SIGTERM);
    for (i = 0; i < 25 && inv_pid_alive(pid); i++) usleep(100000);          /* up to 2.5 s */
    return 1;
#else
    (void)entity_dir;
    return 0;
#endif
}

/* ---- Take / Place --------------------------------------------------------- */

/* Free destination name in `parent`: base, else base_2 .. base_9. */
static int inv_free_name(const char *parent, const char *base, char *out, size_t n) {
    char p[INV_PATH];
    int i;
    snprintf(out, n, "%s", base);
    snprintf(p, sizeof(p), "%s/%s", parent, out);
    if (access(p, F_OK) != 0) return 1;
    for (i = 2; i <= 9; i++) {
        snprintf(out, n, "%s_%d", base, i);
        snprintf(p, sizeof(p), "%s/%s", parent, out);
        if (access(p, F_OK) != 0) return 1;
    }
    return 0;
}

/* One line per inventory event, appended to ledger_path (NULL = no ledger):
 *   <unix ts>|<verb>|<holder>|<item>
 * HOOK: verbs are inv_take / inv_place today. A future vitals/effects layer
 * (eating, damage) reads the same ledger instead of a second event bus. */
INV_UNUSED static void inv_ledger(const char *ledger_path, const char *verb,
                                  const char *holder, const char *item) {
    FILE *f;
    if (!ledger_path || !ledger_path[0] || !(f = fopen(ledger_path, "a"))) return;
    fprintf(f, "%ld|%s|%s|%s\n", (long)time(NULL), verb, holder, item);
    fclose(f);
}

/* Take: move the entity directory `target_dir` into taker_dir/inventory/.
 * Refuses (nothing moved) if the target is not a directory, is the taker
 * itself, contains the taker, or the taker's inventory already contains it.
 * name_out (optional) receives the item's name inside the inventory. The caller
 * stops a running desk entity first (inv_stop_entity) and fixes the page row. */
INV_UNUSED static int inv_take(const char *taker_dir, const char *target_dir, char *name_out, size_t nn) {
    char inv[INV_PATH], dst[INV_PATH], name[INV_NAME], tclean[INV_PATH];
    size_t tl;
    if (!taker_dir || !target_dir || !taker_dir[0] || !target_dir[0]) return INV_E_ARGS;
    if (!inv_is_dir(target_dir)) return INV_E_NOTDIR;
    snprintf(tclean, sizeof(tclean), "%s", target_dir);
    tl = strlen(tclean);
    while (tl > 1 && tclean[tl - 1] == '/') tclean[--tl] = '\0';
    /* taker == target, or taker inside target (moving a parent into its child) */
    if (!strcmp(taker_dir, tclean) ||
        (!strncmp(taker_dir, tclean, tl) && taker_dir[tl] == '/')) return INV_E_CYCLE;
    inv_dir(taker_dir, inv, sizeof(inv));
    /* already in the taker's inventory */
    if (!strncmp(tclean, inv, strlen(inv)) && tclean[strlen(inv)] == '/') return INV_E_CYCLE;
    if (mkdir(inv, 0775) != 0 && errno != EEXIST) return INV_E_RENAME;
    if (!inv_free_name(inv, inv_base(tclean), name, sizeof(name))) return INV_E_NAMES;
    snprintf(dst, sizeof(dst), "%s/%s", inv, name);
    if (rename(tclean, dst) != 0) return INV_E_RENAME;
    if (name_out && nn) snprintf(name_out, nn, "%s", name);
    return INV_OK;
}

/* Place: move taker_dir/inventory/<name> to dest_parent/<name>. name NULL or ""
 * means the selected slot. dest_out (optional) receives the new directory. The
 * caller adds the page row and spawns the process (desk) afterwards. */
INV_UNUSED static int inv_place(const char *taker_dir, const char *name, const char *dest_parent,
                                char *dest_out, size_t dn) {
    char inv[INV_PATH], src[INV_PATH], dst[INV_PATH], pick[INV_NAME], final_name[INV_NAME];
    char names[INV_MAX][INV_NAME];
    int count;
    if (!taker_dir || !dest_parent || !taker_dir[0] || !dest_parent[0]) return INV_E_ARGS;
    count = inv_list(taker_dir, names, INV_MAX);
    if (count <= 0) return INV_E_EMPTY;
    if (name && name[0]) snprintf(pick, sizeof(pick), "%s", name);
    else snprintf(pick, sizeof(pick), "%s", names[inv_slot_get(taker_dir, count)]);
    inv_dir(taker_dir, inv, sizeof(inv));
    snprintf(src, sizeof(src), "%s/%s", inv, pick);
    if (!inv_is_dir(src)) return INV_E_EMPTY;
    if (!inv_is_dir(dest_parent)) return INV_E_NOTDIR;
    if (!inv_free_name(dest_parent, pick, final_name, sizeof(final_name))) return INV_E_NAMES;
    snprintf(dst, sizeof(dst), "%s/%s", dest_parent, final_name);
    if (rename(src, dst) != 0) return INV_E_RENAME;
    if (dest_out && dn) snprintf(dest_out, dn, "%s", dst);
    return INV_OK;
}

/* ---- the visual display ---------------------------------------------------
 * The inventory is also a display (owner, 2026-10-05): a Minecraft-style
 * hotbar above the taskbar on the desk and in pc-hq. This is its data feed:
 * one plain text projection a khtpm panel renders, the house's usual
 * manager -> projection -> generic-renderer split, so both screens draw the
 * same file and only the content differs per entity.
 *
 *   holder=<entity name>
 *   count=<n>            slots filled
 *   selected=<0-based>   the highlighted slot (what Place will use)
 *   slot_<i>=<glyph>|<name>      i = 0..count-1, glyph from the item's glyph.txt
 *
 * HOOK: hearts / hunger lines (heart=, hunger=) are appended here by the
 * vitals reader when it exists (khtpm_possess.c); the panel shows whatever
 * keys are present, so adding them needs no panel change. Returns the count,
 * or -1 if the file could not be written (tmp + rename, never half-written). */
INV_UNUSED static int inv_project(const char *entity_dir, const char *out_path) {
    char names[INV_MAX][INV_NAME], tmp[INV_PATH + 8];
    int n, i;
    FILE *f;
    n = inv_list(entity_dir, names, INV_MAX);
    snprintf(tmp, sizeof(tmp), "%s.tmp", out_path);
    if (!(f = fopen(tmp, "w"))) return -1;
    fprintf(f, "holder=%s\ncount=%d\nselected=%d\n", inv_base(entity_dir), n, inv_slot_get(entity_dir, n));
    for (i = 0; i < n; i++) {
        char gp[INV_PATH], glyph[64] = "?";
        FILE *g;
        snprintf(gp, sizeof(gp), "%s/inventory/%s/glyph.txt", entity_dir, names[i]);
        if ((g = fopen(gp, "r"))) {
            if (fgets(glyph, sizeof(glyph), g)) glyph[strcspn(glyph, "\r\n")] = '\0';
            fclose(g);
            if (!glyph[0]) snprintf(glyph, sizeof(glyph), "?");
        }
        fprintf(f, "slot_%d=%s|%s\n", i, glyph, names[i]);
    }
    fclose(f);
    return rename(tmp, out_path) == 0 ? n : -1;
}

#endif
