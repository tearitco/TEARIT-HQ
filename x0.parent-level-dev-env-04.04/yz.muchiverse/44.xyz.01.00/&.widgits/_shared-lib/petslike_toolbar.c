/* petslike_toolbar.c - the header menus of a pets-like window as DATA (text-include, prefix ptb_, pure file I/O, no globals).
 *
 * Owner 2026-10-21: "they should have .pdl for headers so it should be easy ... get back into reusing / sharing parts better". Every pets-like window
 * (rpg-pet, df-hq, tcg-rpgce, dsr-rpg, mcl-hq, cdda-hq, civ-rpgce, gta-hq) had its toolbar tabs and dropdown rows typed into its own .xhtpm; a new menu meant
 * editing nine templates. Now a window has a toolbar.pdl (house pdl: `SECTION | KEY | VALUE` header, `|`-separated rows, `#` comments):
 *
 *   TAB  | <id> | <label>                       a toolbar tab; its dropdown holds the ITEM rows with the same id
 *   ITEM | <tab id> | <label> | <verb and args>   a dropdown row; the verb is the app op's own verb line (newpal, newroom, goto bedroom ...), empty = just close
 *
 * ptb_publish() turns that into ui.txt lines the template repeats over (bind "tb" for tabs, "it" for rows):
 *   n_tab=N  tb_0_id= tb_0_label=        n_item=M  it_0_tab= it_0_label= it_0_act=      (act = the full action= string: "'<op>' <verb>" or "void")
 * The dynamic menus (the pets, the rooms) stay in the template, fed by the op; this file is for the fixed rows.
 * Limits: 8 tabs, 48 rows. A row of an unknown tab is published anyway (it simply never shows). */
#ifndef PETSLIKE_TOOLBAR_C
#define PETSLIKE_TOOLBAR_C

#include <stdio.h>
#include <string.h>

#define PTB_TABS 8
#define PTB_ITEMS 48

static char *ptb_trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
    return s;
}

/* pdl = <app>/toolbar.pdl, op = the quoted op command the verbs are appended to (e.g. "'/home/.../rpg_pet.+x'"). Appends the ui lines to out; returns bytes written. */
static int ptb_publish(const char *pdl, const char *op, char *out, size_t cap) {
    FILE *f = fopen(pdl, "r");
    int o = 0, nt = 0, ni = 0;
    char line[600], tb[PTB_TABS][2][80], it[PTB_ITEMS][4][240];
    if (!f) { o += snprintf(out + o, cap - o, "n_tab=0\nn_item=0\n"); return o; }
    while (fgets(line, sizeof line, f)) {
        char *f0[5]; int n = 0; char *p = line;
        if (line[0] == '#' || line[0] == '-' || !strchr(line, '|')) continue;
        for (; n < 5; n++) { char *bar = strchr(p, '|'); if (bar) *bar = 0; f0[n] = ptb_trim(p); if (!bar) { n++; break; } p = bar + 1; }
        if (!strcmp(f0[0], "TAB") && n >= 3 && nt < PTB_TABS) { snprintf(tb[nt][0], 80, "%s", f0[1]); snprintf(tb[nt][1], 80, "%s", f0[2]); nt++; }
        else if (!strcmp(f0[0], "ITEM") && n >= 3 && ni < PTB_ITEMS) {
            snprintf(it[ni][0], 240, "%s", f0[1]); snprintf(it[ni][1], 240, "%s", f0[2]);
            if (n >= 4 && f0[3][0]) snprintf(it[ni][2], 240, "%s %s", op, f0[3]); else snprintf(it[ni][2], 240, "void");
            ni++;
        }
    }
    fclose(f);
    o += snprintf(out + o, cap - o, "n_tab=%d\n", nt);
    for (int i = 0; i < nt && (size_t)o < cap; i++) o += snprintf(out + o, cap - o, "tb_%d_id=%s\ntb_%d_label=%s\n", i, tb[i][0], i, tb[i][1]);
    o += snprintf(out + o, cap - o, "n_item=%d\n", ni);
    for (int i = 0; i < ni && (size_t)o < cap; i++) o += snprintf(out + o, cap - o, "it_%d_tab=%s\nit_%d_label=%s\nit_%d_act=%s\n", i, it[i][0], i, it[i][1], i, it[i][2]);
    return o;
}
#endif
