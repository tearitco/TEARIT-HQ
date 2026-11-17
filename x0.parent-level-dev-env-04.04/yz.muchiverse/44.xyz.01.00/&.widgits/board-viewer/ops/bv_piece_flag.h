/* One boolean-ish flag of an entity's piece.pdl (muta parity for possession): `SECTION | possessable | 1` or a bare `possessable=1`; the line must contain the flag name, then "true"/" 1"/"=1" is on and
 * "false"/" 0"/"=0" is off, anything else keeps `def`. Shared by bv_menu_input.c (key 9 release, Enter possess) and bv_render_3d.c (a click on the corner map under minimap_click_possessed=release) so both
 * apply the same `de_possessible` lock (2026-11-17; was a private copy in bv_menu_input.c). The includer defines PATH_BUF and MAX_LINE first. Pure, no allocation. */
#ifndef BV_PIECE_FLAG_H
#define BV_PIECE_FLAG_H
static int piece_pdl_flag(const char *host_root, const char *entity_id,
                          const char *flag, int def) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/%s/piece.pdl", host_root, entity_id);
    FILE *f = fopen(p, "r");
    if (!f) return def;
    int r = def; char l[MAX_LINE];
    while (fgets(l, sizeof(l), f)) {
        if (!strstr(l, flag)) continue;
        if (strstr(l, "true") || strstr(l, " 1") || strstr(l, "=1")) r = 1;
        else if (strstr(l, "false") || strstr(l, " 0") || strstr(l, "=0")) r = 0;
        break;
    }
    fclose(f);
    return r;
}
#endif
