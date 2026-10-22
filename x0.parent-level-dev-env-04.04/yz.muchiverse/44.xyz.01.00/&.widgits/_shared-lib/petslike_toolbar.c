/* petslike_toolbar.c - the header menus of a pets-like window as DATA, in the LyfeDesk taskbar's shape (text-include, prefix ptb_, pure file I/O, no globals).
 *
 * Owner 2026-10-21: "they should have .pdl for headers", "should be similar to lyfedesk". So a window's <app>/toolbar.pdl is written like #.desktop/livedesk_taskbar.pdl
 * (`SECTION | KEY | VALUE` rows, flat keys, TASKBAR-MENUS-DATA-DRIVEN.md section 2): a menu is `<cell>_menu_<N>_label` + `<cell>_menu_<N>_cmd`, and rows can be staged around a list the op
 * builds itself (the pets, the rooms) with `<cell>_menu_pre_<N>_*` / `<cell>_menu_post_<N>_*`, exactly the taskbar's pals_menu_pre / pals_menu_post. Fixed tabs are
 * `tab_<N>_cell` + `tab_<N>_label` (the taskbar's strip_btn_<N>_label).
 *
 *   SECTION | tab_0_cell           | pals
 *   SECTION | tab_0_label          | Pals
 *   SECTION | pals_menu_1_label    | + new pal
 *   SECTION | pals_menu_1_cmd      | op:newpal
 *   SECTION | page_menu_post_1_label | + new room          (after the rooms the op lists)
 *
 * _cmd grammar (the taskbar's, with its `livedesk:` internal form renamed for a window): `op:<verb and args>` = the app op's own verb line; empty = just closes the menu;
 * anything else = a shell string, run as written (the taskbar's bare-string form).
 *
 * ptb_publish() writes ui.txt lines the template uses (bind "tb" tabs, "it" fixed rows):
 *   n_tab tb_N_id (= "tb-<cell>") tb_N_label    n_item it_N_tab it_N_label it_N_act    (act = the full action= string)
 *   and the staged rows as numbered slots per cell: <cell>_pre_<N>_label/_act, <cell>_post_<N>_label/_act (a template lists 4 slots as plain items, show= the label;
 *   a second repeat block into the same dropdown would stack from the top and cover the list)
 * The cell's tab id in the template is `tb-<cell>` (tb-book, tb-page, tb-pals ...). Limits: 8 tabs, 48 rows per group. */
#ifndef PETSLIKE_TOOLBAR_C
#define PETSLIKE_TOOLBAR_C

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PTB_TABS 8
#define PTB_ROWS 48
#define PTB_KV 400

typedef struct { char key[96], val[240]; } PtbKV;
typedef struct { char cell[48], label[160], act[260]; } PtbRow;

static char *ptb_trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
    return s;
}

static const char *ptb_get(const PtbKV *kv, int n, const char *key) {
    for (int i = 0; i < n; i++) if (!strcmp(kv[i].key, key)) return kv[i].val;
    return NULL;
}

/* a "<cell>_menu_[pre_|post_]<N>_label" key: fills cell and stage (0 fixed, 1 pre, 2 post) and the key stem without "_label"; returns 1 if it is one */
static int ptb_menu_label_key(const char *key, char *cell, size_t cn, int *stage) {
    size_t l = strlen(key);
    if (l < 8 || strcmp(key + l - 6, "_label")) return 0;
    const char *m = strstr(key, "_menu_"); if (!m) return 0;
    const char *r = m + 6; *stage = 0;
    if (!strncmp(r, "pre_", 4)) { *stage = 1; r += 4; } else if (!strncmp(r, "post_", 5)) { *stage = 2; r += 5; }
    if (*r < '0' || *r > '9') return 0;
    size_t c = (size_t)(m - key); if (c == 0 || c >= cn) return 0;
    memcpy(cell, key, c); cell[c] = 0; return 1;
}

/* pdl = <app>/toolbar.pdl, op = the quoted op command the `op:` verbs go to (e.g. "'/home/.../rpg_pet.+x'"). Appends the ui lines to out; returns bytes written. */
static int ptb_publish(const char *pdl, const char *op, char *out, size_t cap) {
    static PtbKV kv[PTB_KV]; static PtbRow rows[3][PTB_ROWS];
    int nkv = 0, o = 0, nrow[3] = {0, 0, 0}, nt = 0;
    char line[600]; FILE *f = fopen(pdl, "r");
    if (!f) { o += snprintf(out + o, cap - o, "n_tab=0\nn_item=0\nn_pre=0\nn_post=0\n"); return o; }
    while (fgets(line, sizeof line, f) && nkv < PTB_KV) {
        if (strncmp(line, "SECTION", 7)) continue;
        char *b1 = strchr(line, '|'); if (!b1) continue; char *b2 = strchr(b1 + 1, '|'); if (!b2) continue;
        *b1 = 0; *b2 = 0;
        char *k = ptb_trim(b1 + 1), *v = ptb_trim(b2 + 1);
        if (!strcmp(k, "KEY")) continue;
        snprintf(kv[nkv].key, sizeof kv[nkv].key, "%s", k); snprintf(kv[nkv].val, sizeof kv[nkv].val, "%s", v); nkv++;
    }
    fclose(f);
    for (int i = 0; i < PTB_TABS; i++) {
        char kc[40], kl[40]; snprintf(kc, sizeof kc, "tab_%d_cell", i); snprintf(kl, sizeof kl, "tab_%d_label", i);
        const char *c = ptb_get(kv, nkv, kc), *l = ptb_get(kv, nkv, kl); if (!c) continue;
        o += snprintf(out + o, cap - o, "tb_%d_id=tb-%s\ntb_%d_label=%s\n", nt, c, nt, l ? l : c); nt++;
    }
    for (int i = 0; i < nkv; i++) {
        char cell[48], ck[100]; int st;
        if (!ptb_menu_label_key(kv[i].key, cell, sizeof cell, &st) || nrow[st] >= PTB_ROWS) continue;
        snprintf(ck, sizeof ck, "%.*s_cmd", (int)(strlen(kv[i].key) - 6), kv[i].key);
        const char *cmd = ptb_get(kv, nkv, ck); PtbRow *r = &rows[st][nrow[st]++];
        snprintf(r->cell, sizeof r->cell, "tb-%s", cell); snprintf(r->label, sizeof r->label, "%s", kv[i].val);
        if (!cmd || !cmd[0]) snprintf(r->act, sizeof r->act, "void");
        else if (!strncmp(cmd, "op:", 3)) snprintf(r->act, sizeof r->act, "%s %s", op, cmd + 3);
        else snprintf(r->act, sizeof r->act, "%s", cmd);
    }
    o += snprintf(out + o, cap - o, "n_tab=%d\nn_item=%d\n", nt, nrow[0]);
    for (int i = 0; i < nrow[0] && (size_t)o + 600 < cap; i++)
        o += snprintf(out + o, cap - o, "it_%d_tab=%s\nit_%d_label=%s\nit_%d_act=%s\n", i, rows[0][i].cell, i, rows[0][i].label, i, rows[0][i].act);
    /* staged rows (pre / post a list the op builds): the template cannot repeat a second block into the same dropdown (each repeat stacks from the top and covers the list),
     * so they are published per cell and stage as numbered slots the template lists as plain items:  <cell>_pre_<N>_label / _act, <cell>_post_<N>_label / _act */
    for (int st = 1; st <= 2; st++) {
        int cnt[16]; char cells[16][48]; int nc = 0;
        for (int i = 0; i < nrow[st] && (size_t)o + 600 < cap; i++) {
            int c = -1; const char *cell = rows[st][i].cell + 3;      /* skip "tb-" */
            for (int k = 0; k < nc; k++) if (!strcmp(cells[k], cell)) c = k;
            if (c < 0 && nc < 16) { c = nc++; snprintf(cells[c], 48, "%s", cell); cnt[c] = 0; }
            if (c < 0) continue;
            o += snprintf(out + o, cap - o, "%s_%s_%d_label=%s\n%s_%s_%d_act=%s\n", cell, st == 1 ? "pre" : "post", cnt[c], rows[st][i].label, cell, st == 1 ? "pre" : "post", cnt[c], rows[st][i].act);
            cnt[c]++;
        }
    }
    return o;
}
#endif
