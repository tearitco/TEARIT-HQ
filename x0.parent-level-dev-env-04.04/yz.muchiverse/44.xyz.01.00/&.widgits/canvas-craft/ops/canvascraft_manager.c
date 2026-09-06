/* canvascraft_manager.c - Canvas-Craft window backend (PHASE 1).
 *
 * CANVAS-CRAFT-DESIGN.md. Model = &.hq-apps/irc-chat-hq/ops/
 * irc_chat_manager.c: forked by the shared khtpm_core_render.+x as
 *   canvascraft_manager.+x <house_root> <package_dir> [id]
 * publishes <pkg>/canvas-craft_ui.txt (consumed via the xhtpm's
 * vars=), polls <pkg>/canvas-craft_action.txt (seq=/cmd=, written by
 * ops/cc_item.sh).
 *
 * PHASE 1 is read-only: parse the recipe registry, group by tier,
 * run the "highest common denominator" quantity resolver (design §3),
 * publish the left recipe list + the middle recipe card. No inventory,
 * no bench, no CRAFT yet - `have` is always 0.
 *
 * Recipe registry: #.ref/menu/palletes/elements]new=RECIPEZ+]z2🏆.txt
 *   "<Name> <protons> <neutrons> <electrons> <parentA_idx> <parentB_idx>"
 *   - all five == -1  -> primitive (Up_quark, Down_quark, Electron)
 *   - parent fields are 0-BASED indices into the line list
 *   - line 1-3 quark, 4-7 subatomic, 8-125 the 118 elements, 126+ compound
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#define PL        4096
#define MAX_REC   512
#define MAX_INV   256
#define RECIPE_FILE "#.ref/menu/palletes/elements]new=RECIPEZ+]z2🏆.txt"
#define ITEMS_PDL   "canvascraft_items.pdl"   /* relative to pkg_dir */

typedef struct {
    char id[48];       /* slug: lowercase name, non-alnum -> _        */
    char name[48];     /* display name verbatim from the file         */
    int  p, n, e;      /* totals (-1 == primitive)                    */
    int  a, b;         /* parent 0-based line indices (-1 == none)    */
    char tier[12];     /* quark | subatomic | element | compound      */
    int  price;        /* generic placeholder price (design §2 / .pdl) */
} Recipe;

typedef struct {
    int  handle;       /* stable []N badge - never renumbered (design §6.1) */
    int  ridx;         /* index into rec[]                                  */
    long count;
} Stack;

static char house_root[PL];
static char pkg_dir[PL];

static Recipe rec[MAX_REC];
static int    n_rec = 0;
static int    sel   = -1;   /* selected recipe index, -1 = none */

static Stack  inv[MAX_INV];
static int    n_inv = 0;
static int    max_handle = 0;

/* ---- helpers ------------------------------------------------------- */

static void slugify(const char *in, char *out, size_t osz) {
    size_t o = 0;
    for (const char *p = in; *p && o + 1 < osz; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c)) out[o++] = (char)tolower(c);
        else if (o && out[o - 1] != '_') out[o++] = '_';
    }
    while (o && out[o - 1] == '_') o--;
    out[o] = '\0';
}

static int rec_by_id(const char *id) {
    for (int i = 0; i < n_rec; i++)
        if (strcmp(rec[i].id, id) == 0) return i;
    return -1;
}

/* ---- recipe registry -------------------------------------------------- */

static void load_recipes(void) {
    char path[PL];
    snprintf(path, sizeof(path), "%s/%s", house_root, RECIPE_FILE);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "canvascraft: cannot open %s\n", path); return; }

    char line[512];
    n_rec = 0;
    while (n_rec < MAX_REC && fgets(line, sizeof(line), f)) {
        char nm[48];
        int p, n, e, a, b;
        if (sscanf(line, "%47s %d %d %d %d %d", nm, &p, &n, &e, &a, &b) != 6)
            continue;
        Recipe *r = &rec[n_rec];
        snprintf(r->name, sizeof(r->name), "%s", nm);
        slugify(nm, r->id, sizeof(r->id));
        r->p = p; r->n = n; r->e = e; r->a = a; r->b = b;

        int ln = n_rec + 1;                       /* 1-based line number */
        if      (ln <= 3)   snprintf(r->tier, sizeof(r->tier), "quark");
        else if (ln <= 7)   snprintf(r->tier, sizeof(r->tier), "subatomic");
        else if (ln <= 125) snprintf(r->tier, sizeof(r->tier), "element");
        else                snprintf(r->tier, sizeof(r->tier), "compound");
        r->price = 0;                             /* filled by load_items() */
        n_rec++;
    }
    fclose(f);
}

/* ---- items / prices / starting inventory (design §2, §6.1) ---------- */

/* placeholder economy: unlisted items get a computed price. */
static int default_price(const Recipe *r) {
    if (strcmp(r->tier, "quark") == 0)     return 1;
    if (strcmp(r->tier, "subatomic") == 0) return 5;
    if (strcmp(r->tier, "element") == 0)   return r->p > 0 ? r->p : 1;   /* ~ atomic number */
    return (r->p > 0 ? r->p : 0) + (r->n > 0 ? r->n : 0);               /* compound */
}

static long inv_count(int ridx) {
    if (ridx < 0) return 0;
    for (int i = 0; i < n_inv; i++) if (inv[i].ridx == ridx) return inv[i].count;
    return 0;
}

static void inv_add(int ridx, long count) {   /* merge or new stack */
    if (ridx < 0 || count <= 0) return;
    for (int i = 0; i < n_inv; i++)
        if (inv[i].ridx == ridx) { inv[i].count += count; return; }
    if (n_inv >= MAX_INV) return;
    inv[n_inv].handle = ++max_handle;
    inv[n_inv].ridx   = ridx;
    inv[n_inv].count  = count;
    n_inv++;
}

static void load_items(void) {
    /* every recipe first gets its computed default price */
    for (int i = 0; i < n_rec; i++) rec[i].price = default_price(&rec[i]);

    char path[PL];
    snprintf(path, sizeof(path), "%s/%s", pkg_dir, ITEMS_PDL);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "canvascraft: no %s (using default prices, empty inventory)\n", path); return; }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "ITEM", 4) != 0) continue;
        /* ITEM | id | price | start_count */
        char id[48] = ""; int price = -1; long start = 0;
        char *tok = strtok(p, "|");                      /* "ITEM " */
        tok = strtok(NULL, "|"); if (tok) sscanf(tok, " %47s", id);
        tok = strtok(NULL, "|"); if (tok) price = atoi(tok);
        tok = strtok(NULL, "|"); if (tok) start = atol(tok);
        if (!id[0]) continue;
        int ri = rec_by_id(id);
        if (ri < 0) continue;
        if (price >= 0) rec[ri].price = price;
        if (start > 0)  inv_add(ri, start);
    }
    fclose(f);
}

/* ---- quantity resolver (design §3) --------------------------------- */
/* For recipe X (index sx) with direct parents A,B: fill need_a/need_b
 * with the count of each DIRECT parent (not the expanded primitive
 * tree). *flagged = 1 if we had to fall back to a greedy estimate. */
static void resolve_qty(int sx, long *need_a, long *need_b, int *flagged) {
    *need_a = *need_b = 0;
    *flagged = 0;
    if (sx < 0 || sx >= n_rec) return;
    Recipe *X = &rec[sx];
    if (X->a < 0 || X->b < 0 || X->a >= n_rec || X->b >= n_rec) return;
    Recipe *A = &rec[X->a], *B = &rec[X->b];

    /* quark tier: parents have no p/n/e totals (all -1). A "Pair_x" is
     * literally 2 of its one quark; anything else needs 1 of each
     * listed input. */
    if (A->p < 0 || B->p < 0) {
        if (strncmp(X->name, "Pair_", 5) == 0) { *need_a = 2; *need_b = 0; }
        else                                   { *need_a = 1; *need_b = 1; }
        return;
    }

    /* element case: parents are Proton (1,0,1) and Neutron (0,1,0) */
    if (strcmp(A->name, "Proton") == 0 && strcmp(B->name, "Neutron") == 0) {
        *need_a = X->p;
        *need_b = X->n;
        return;
    }

    /* general: smallest qb in [0..X.n] with an exact, non-negative qa
     * on the proton axis that is also consistent on the neutron axis.
     * (Water: A=H(1,0,1) B=O(8,8,8) -> qb=1, qa=2.) */
    int ap = A->p, an = A->n, bp = B->p, bn = B->n;
    for (long qb = 0; qb <= X->n + 1 && qb <= 100000; qb++) {
        long rp = X->p - qb * bp;
        long rn = X->n - qb * bn;
        if (rp < 0 || rn < 0) break;
        if (ap > 0 && rp % ap == 0) {
            long qa = rp / ap;
            if (an == 0 ? rn == 0 : (rn == qa * an)) { *need_a = qa; *need_b = qb; return; }
        } else if (ap == 0 && rp == 0) {
            if (an > 0 && rn % an == 0) { *need_a = rn / an; *need_b = qb; return; }
        }
    }

    /* greedy fallback: split proton total across the two parents' proton
     * weight, flag it so the UI can show the row as approximate. */
    *flagged = 1;
    if (ap > 0) *need_a = (X->p + ap - 1) / ap;
    if (bp > 0) *need_b = (X->p + bp - 1) / bp;
}

/* ---- publish ------------------------------------------------------- */

static void bar10(long have, long need, char *out, size_t osz) {
    int fill = 0;
    if (need > 0) { fill = (int)((have * 10) / need); if (fill > 10) fill = 10; }
    size_t o = 0;
    if (o < osz) out[o++] = '[';
    for (int i = 0; i < 10 && o + 1 < osz; i++) out[o++] = (i < fill) ? '#' : '.';
    if (o + 1 < osz) out[o++] = ']';
    out[o] = '\0';
}

/* Direct ingredients of recipe sx (design §3): fills ridx[]/need[],
 * returns the count (0..4). *flagged set if the resolver guessed. */
typedef struct { long need; int ridx; } Ing;
static int compute_ings(int sx, Ing *ig, int *flagged) {
    *flagged = 0;
    if (sx < 0 || sx >= n_rec) return 0;
    Recipe *X = &rec[sx];
    if (X->a < 0 || X->b < 0) return 0;   /* primitive - not crafted */
    long na, nb;
    resolve_qty(sx, &na, &nb, flagged);
    int ni = 0;
    if (X->a == X->b) {
        if (na + nb > 0) { ig[ni].ridx = X->a; ig[ni].need = na + nb; ni++; }
    } else {
        if (na > 0) { ig[ni].ridx = X->a; ig[ni].need = na; ni++; }
        if (nb > 0) { ig[ni].ridx = X->b; ig[ni].need = nb; ni++; }
    }
    long ee = (strcmp(X->tier, "element") == 0 && X->e > 0) ? X->e : 0;
    if (ee > 0) { ig[ni].ridx = rec_by_id("electron"); ig[ni].need = ee; ni++; }
    if (ni == 0) { ig[0].ridx = X->a; ig[0].need = 1; ni = 1; }
    return ni;
}

static char g_craft_msg[96] = "";   /* last CRAFT result - shown in the bench */

static void write_ui(void) {
    char tmp[PL], dst[PL];
    snprintf(dst, sizeof(dst), "%s/canvas-craft_ui.txt", pkg_dir);
    snprintf(tmp, sizeof(tmp), "%s/canvas-craft_ui.txt.tmp", pkg_dir);

    /* --- compute the middle card into locals (one emit, no dup keys) --- */
    char sel_id[48] = "", sel_name[48] = "", sel_tier[16] = "";
    char sel_pne[64] = "", sel_note[80] = "";
    int  sel_show = 0;
    struct { char nm[48]; long need; int ridx; } ig[4];
    int ni = 0;

    if (sel >= 0 && sel < n_rec) {
        Recipe *X = &rec[sel];
        sel_show = 1;
        snprintf(sel_id,   sizeof(sel_id),   "%s", X->id);
        snprintf(sel_name, sizeof(sel_name), "%s", X->name);
        snprintf(sel_tier, sizeof(sel_tier), "%s", X->tier);
        if (X->p >= 0) snprintf(sel_pne, sizeof(sel_pne), "%dp / %dn / %de", X->p, X->n, X->e);
        else           snprintf(sel_pne, sizeof(sel_pne), "primitive");

        if (X->a < 0 || X->b < 0) {
            snprintf(sel_note, sizeof(sel_note), "primitive - mined / spawned, not crafted");
        } else {
            Ing g[4]; int flagged;
            ni = compute_ings(sel, g, &flagged);
            for (int i = 0; i < ni; i++) {
                ig[i].ridx = g[i].ridx; ig[i].need = g[i].need;
                snprintf(ig[i].nm, sizeof(ig[i].nm), "%s",
                         (g[i].ridx >= 0 && g[i].ridx < n_rec) ? rec[g[i].ridx].name : "?");
            }
            if (flagged) snprintf(sel_note, sizeof(sel_note), "counts are an estimate (recipe data is approximate)");
        }
    }

    /* --- emit --- */
    FILE *f = fopen(tmp, "w");
    if (!f) return;

    fprintf(f, "sel_show=%s\n", sel_show ? "1" : "");
    fprintf(f, "sel_empty=%s\n", sel_show ? "" : "1");
    fprintf(f, "sel_id=%s\n",   sel_id);
    fprintf(f, "sel_name=%s\n", sel_name);
    fprintf(f, "sel_tier=%s\n", sel_tier);
    fprintf(f, "sel_yield=%s\n", sel_show ? "1" : "");
    fprintf(f, "sel_pne=%s\n",  sel_pne);
    fprintf(f, "sel_note=%s\n", sel_note);
    fprintf(f, "craft_msg=%s\n", g_craft_msg);

    fprintf(f, "n_ing=%d\n", ni);
    char bar[16];
    for (int i = 0; i < ni; i++) {
        long have = inv_count(ig[i].ridx);
        bar10(have, ig[i].need, bar, sizeof(bar));
        fprintf(f, "ing_%d_name=%s\ning_%d_have=%ld\ning_%d_need=%ld\ning_%d_bar=%s\n",
                i, ig[i].nm, i, have, i, ig[i].need, i, bar);
    }

    /* right column: inventory (design §6.1 - stable []handle, priced) */
    fprintf(f, "n_inv=%d\n", n_inv);
    fprintf(f, "inv_empty=%s\n", n_inv ? "" : "1");
    for (int i = 0; i < n_inv; i++) {
        Recipe *r = &rec[inv[i].ridx];
        fprintf(f, "iv_%d_handle=%d\n", i, inv[i].handle);
        fprintf(f, "iv_%d_id=%s\n",     i, r->id);
        fprintf(f, "iv_%d_name=%s\n",   i, r->name);
        fprintf(f, "iv_%d_count=%ld\n", i, inv[i].count);
        fprintf(f, "iv_%d_price=%d\n",  i, r->price);
        fprintf(f, "iv_%d_cls=%s\n",    i, (inv[i].ridx == sel) ? "cc-active" : "");
    }

    fprintf(f, "n_recipes=%d\n", n_rec);
    const char *last_tier = "";
    for (int i = 0; i < n_rec; i++) {
        Recipe *r = &rec[i];
        int hdr = strcmp(r->tier, last_tier) != 0;
        last_tier = r->tier;
        fprintf(f, "rc_%d_id=%s\n", i, r->id);
        fprintf(f, "rc_%d_name=%s%s\n", i, hdr ? "" : "  ", r->name);
        fprintf(f, "rc_%d_tier=%s\n", i, r->tier);
        fprintf(f, "rc_%d_cls=%s\n", i, (i == sel) ? "cc-active" : "");
    }

    fclose(f);
    rename(tmp, dst);
}

/* ---- action relay ------------------------------------------------------ */

static void do_craft(void) {
    g_craft_msg[0] = '\0';
    if (sel < 0 || sel >= n_rec) { snprintf(g_craft_msg, sizeof(g_craft_msg), "Pick a recipe first."); return; }
    Recipe *X = &rec[sel];
    if (X->a < 0 || X->b < 0) {
        snprintf(g_craft_msg, sizeof(g_craft_msg), "%s is a primitive - can't craft it.", X->name);
        return;
    }
    Ing g[4]; int flagged;
    int ni = compute_ings(sel, g, &flagged);

    /* enough of everything? */
    for (int i = 0; i < ni; i++) {
        long have = inv_count(g[i].ridx);
        if (have < g[i].need) {
            const char *nm = (g[i].ridx >= 0) ? rec[g[i].ridx].name : "?";
            snprintf(g_craft_msg, sizeof(g_craft_msg),
                     "Not enough %s - need %ld more (have %ld / %ld).",
                     nm, g[i].need - have, have, g[i].need);
            return;
        }
    }
    /* consume + produce */
    for (int i = 0; i < ni; i++) {
        for (int s = 0; s < n_inv; s++)
            if (inv[s].ridx == g[i].ridx) { inv[s].count -= g[i].need; break; }
    }
    /* drop empty stacks (keep handles off the free list simple: just compact) */
    int w = 0;
    for (int s = 0; s < n_inv; s++) if (inv[s].count > 0) inv[w++] = inv[s];
    n_inv = w;

    inv_add(sel, 1);   /* yield 1 for now */
    snprintf(g_craft_msg, sizeof(g_craft_msg), "Crafted 1 %s.  (+1 to inventory)", X->name);
}

static void do_cmd(const char *cmd) {
    if (strncmp(cmd, "SELECT_RECIPE:", 14) == 0) {
        int i = rec_by_id(cmd + 14);
        if (i >= 0) { sel = i; g_craft_msg[0] = '\0'; }
    } else if (strcmp(cmd, "CRAFT") == 0) {
        do_craft();
    }
    /* BENCH_* / SEARCH_* / FILTER_* land in later phases */
    write_ui();
}

static void clear_action_file(void) {
    char p[PL];
    snprintf(p, sizeof(p), "%s/canvas-craft_action.txt", pkg_dir);
    FILE *f = fopen(p, "w");
    if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); }
}

static void poll_action(int *last_seq) {
    char p[PL];
    snprintf(p, sizeof(p), "%s/canvas-craft_action.txt", pkg_dir);
    FILE *f = fopen(p, "r");
    if (!f) return;
    char buf[PL];
    size_t nr = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[nr] = '\0';
    int seq = 0;
    char cmd[512]; cmd[0] = '\0';
    for (char *ls = buf; *ls; ) {
        char *le = strchr(ls, '\n');
        size_t ll = le ? (size_t)(le - ls) : strlen(ls);
        if (strncmp(ls, "seq=", 4) == 0) seq = atoi(ls + 4);
        else if (strncmp(ls, "cmd=", 4) == 0) {
            size_t cl = ll - 4; if (cl >= sizeof(cmd)) cl = sizeof(cmd) - 1;
            memcpy(cmd, ls + 4, cl); cmd[cl] = '\0';
        }
        if (!le) break;
        ls = le + 1;
    }
    if (seq > *last_seq && cmd[0]) { *last_seq = seq; do_cmd(cmd); }
}

static void bye(int s) { (void)s; _exit(0); }

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <house_root> <package_dir> [id]\n", argv[0]);
        return 1;
    }
    snprintf(house_root, sizeof(house_root), "%s", argv[1]);
    snprintf(pkg_dir,    sizeof(pkg_dir),    "%s", argv[2]);

    signal(SIGTERM, bye);
    signal(SIGINT,  bye);
    signal(SIGHUP,  bye);

    load_recipes();
    load_items();
    clear_action_file();
    write_ui();

    int last_seq = 0;
    for (;;) {
        usleep(50000);
        poll_action(&last_seq);
    }
    return 0;
}
