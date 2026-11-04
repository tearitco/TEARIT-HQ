/* petslike_placer.c - the BRUSH PLACER as a shared, text-included tool (owner 2026-11-04: "modular/sharable like events"). Lifted out of rpg-edit-hq with no behaviour change.
 * What it does: the brush armed in a palettes bar (<app>/board/state/brush.txt: key, glyph, sprite = a palette sprite dir holding sprite.csv) is placed as a normal furniture
 * row. The sprite becomes a generated one-tile sheet state/vox/!v_<hash>.png plus one catalog row in state/voxels.pdl, so the shared 2D painter and the 3D export draw it like
 * any item (2D/3D parity; undo = the REMOVE row, move = the MOVE row). The engine's brush placer (bv_menu_input.c: Enter in brush mode appends "x y z" to
 * <session>/pieces/display/place_requests.txt) is polled by placer_tick(); the `voxel <x> <y>` verb places at a cell. A request is refused (voxel_log.txt says why) unless the
 * board shows THIS app's own book (world_01 map_id == PET_APP).
 * A skin opts in by (1) text-including this file in a section that comes after the core's item/room helpers, (2) forward-declaring the four hooks at the top of its file,
 * (3) calling placer_load_voxels() where it loads its catalog, placer_tick() from its daemon tick, placer_verb(...) from its verb dispatcher and placer_sheet(name, p, cap)
 * from its sheet-path hook ("!v_..." names). Needs from the core: ST, APP, PCHQ, PET_APP, cat/ncat/MAXI, Item, item_idx, check_place, room_idx, rooms/nroom, vroom, nid,
 * append_line, read_file, split_bar, load_data, replay, export3d. COST: one stat of voxels.pdl and one of place_requests.txt per daemon tick; nothing else. ===== */
/* ===== VOXELS (owner 2026-11-02, RPG-EDIT-HQ-PALETTES-SIDEBAR-RESEARCH-2026-11-02.md step 4): the brush armed in the palettes bar (board/state/brush.txt: key, glyph, sprite = a palette
 * sprite dir holding sprite.csv) is placed as a normal furniture row. The sprite becomes a generated one-tile sheet state/vox/!v_<hash>.png and one catalog row in state/voxels.pdl, so the shared 2D
 * painter and the 3D export draw it like any item (2D/3D parity, undo = the REMOVE row, move = the MOVE row). A room is a flat grid: one block high, z is not used yet. ===== */
static unsigned vx_crc(unsigned c, const unsigned char *b, size_t n) { static unsigned t[256]; if (!t[1]) for (unsigned i = 0; i < 256; i++) { unsigned k = i; for (int j = 0; j < 8; j++) k = k & 1 ? 0xEDB88320u ^ (k >> 1) : k >> 1; t[i] = k; } c = ~c; while (n--) c = t[(c ^ *b++) & 255] ^ (c >> 8); return ~c; }
static void vx_be(unsigned char *o, unsigned v) { o[0] = v >> 24; o[1] = v >> 16; o[2] = v >> 8; o[3] = v; }
static void vx_chunk(FILE *f, const char *ty, const unsigned char *d, unsigned n) { unsigned char h[8]; vx_be(h, n); memcpy(h + 4, ty, 4); fwrite(h, 1, 8, f); if (n) fwrite(d, 1, n, f); unsigned c = vx_crc(0, (const unsigned char *)ty, 4); c = vx_crc(c, d, n); unsigned char t[4]; vx_be(t, c); fwrite(t, 1, 4, f); }
/* an uncompressed (stored) PNG of w x h RGBA, w*h*4 + h small enough for one stored block (48x48 here) */
static int vx_png(const char *path, const unsigned char *px, int w, int h) {
    size_t raw = (size_t)h * (1 + (size_t)w * 4); if (raw > 65000) return 0; unsigned char *z = (unsigned char *)malloc(raw + 16), *r = (unsigned char *)malloc(raw); if (!z || !r) { free(z); free(r); return 0; }
    for (int y = 0; y < h; y++) { r[(size_t)y * (1 + w * 4)] = 0; memcpy(r + (size_t)y * (1 + w * 4) + 1, px + (size_t)y * w * 4, (size_t)w * 4); }
    unsigned a = 1, b = 0; for (size_t i = 0; i < raw; i++) { a = (a + r[i]) % 65521; b = (b + a) % 65521; }
    size_t n = 0; z[n++] = 0x78; z[n++] = 1; z[n++] = 1; z[n++] = raw & 255; z[n++] = raw >> 8; z[n++] = ~raw & 255; z[n++] = (~raw >> 8) & 255;
    unsigned char *out = (unsigned char *)malloc(raw + 32); memcpy(out, z, n); memcpy(out + n, r, raw); n += raw; unsigned char ad[4]; vx_be(ad, (b << 16) | a); memcpy(out + n, ad, 4); n += 4;
    char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "wb"); if (!f) { free(z); free(r); free(out); return 0; }
    static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 }; fwrite(sig, 1, 8, f); unsigned char ih[13]; vx_be(ih, w); vx_be(ih + 4, h); ih[8] = 8; ih[9] = 6; ih[10] = ih[11] = ih[12] = 0;
    vx_chunk(f, "IHDR", ih, 13); vx_chunk(f, "IDAT", out, (unsigned)n); vx_chunk(f, "IEND", NULL, 0); int ok = fclose(f) == 0 && rename(tmp, path) == 0; free(z); free(r); free(out); return ok;
}
/* the armed brush -> its item index (generated on first use), -1 + msg when nothing is armed or the sprite cannot be read */
static int vx_item(char *msg, size_t cap) {
    char bp[PATH_MAX], bb[4200] = "", sp[PATH_MAX] = "", gl[64] = ""; snprintf(bp, sizeof bp, "%s/board/state/brush.txt", APP); read_file(bp, bb, sizeof bb);
    for (char *l = strtok(bb, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "sprite=", 7)) snprintf(sp, sizeof sp, "%s", l + 7); else if (!strncmp(l, "glyph=", 6)) snprintf(gl, sizeof gl, "%s", l + 6); }
    if (!sp[0]) { snprintf(msg, cap, "no brush armed: pick a tile in the palettes bar"); return -1; }
    unsigned long h = 5381; for (const char *q = sp; *q; q++) h = h * 33 + (unsigned char)*q; char key[40], sheetn[40]; snprintf(key, sizeof key, "v_%08lx", h & 0xffffffffUL); snprintf(sheetn, sizeof sheetn, "!v_%08lx", h & 0xffffffffUL);
    int at = item_idx(key); if (at >= 0) return at;
    char cp[PATH_MAX]; snprintf(cp, sizeof cp, "%s/sprite.csv", sp); FILE *f = fopen(cp, "r"); if (!f) { snprintf(msg, cap, "cannot read the brush sprite"); return -1; }
    static unsigned char px[48 * 48 * 4]; static unsigned char src[128 * 128 * 4]; int res = 48, n = 0; char ln[200];
    while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') { if (!strncmp(ln, "# resolution=", 13)) res = atoi(ln + 13); continue; } if (ln[0] == 'r') continue; int r, g, b, a; if (res < 1 || res > 128 || n >= res * res) break; if (sscanf(ln, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) { src[n * 4] = r; src[n * 4 + 1] = g; src[n * 4 + 2] = b; src[n * 4 + 3] = a; n++; } }
    fclose(f); if (n < res * res || res < 1) { snprintf(msg, cap, "the brush sprite is not a full csv"); return -1; }
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) memcpy(px + (y * 48 + x) * 4, src + (((size_t)y * res / 48) * res + (size_t)x * res / 48) * 4, 4);
    char dp[PATH_MAX], pp[PATH_MAX]; snprintf(dp, sizeof dp, "%s/vox", ST); mkdir(dp, 0755); snprintf(pp, sizeof pp, "%s/vox/%s.png", ST, sheetn);
    if (!vx_png(pp, px, 48, 48)) { snprintf(msg, cap, "cannot write the voxel sheet"); return -1; }
    if (ncat >= MAXI) { snprintf(msg, cap, "catalog full"); return -1; }
    char row[400]; snprintf(row, sizeof row, "ITEM | %s | %.30s | 0 | %s | 0 | 0 | 1 | 1 | floor | decor", key, gl[0] ? gl : "voxel", sheetn); append_line("voxels.pdl", row);
    Item *it = &cat[ncat]; memset(it, 0, sizeof *it); snprintf(it->key, 32, "%s", key); snprintf(it->label, 40, "%.30s", gl[0] ? gl : "voxel"); snprintf(it->sheet, 24, "%s", sheetn); it->w = it->h = 1; snprintf(it->effect, 12, "decor"); return ncat++;
}
static void export3d(char *msg, size_t cap);      /* defined later in the shared core: a placement rewrites the 3D book (maps/rpg-edit-hq) so the engine shows it */
/* place the armed brush at cell x,y of the viewed room: BUY (price 0) + PLACE rows in house.txt, then replay so the daemon's own list is current */
static void vx_place_in(int room, int x, int y, char *msg, size_t cap) {
    int i = vx_item(msg, cap); if (i < 0) return; if (room < 0 || room >= nroom) { snprintf(msg, cap, "not placed: no such room"); return; } const char *bad = check_place(room, i, x, y, 0); if (bad[0]) { snprintf(msg, cap, "not placed: %s", bad); return; }
    char b[300]; long now = (long)time(NULL); snprintf(b, sizeof b, "BUY|%ld|%s|0", now, cat[i].key); append_line("house.txt", b);
    snprintf(b, sizeof b, "PLACE|%d|%ld|%s|%d|%d|%s", nid, now, cat[i].key, x, y, rooms[room].id); append_line("house.txt", b); snprintf(msg, cap, "placed %s as #%d at %d,%d in %s", cat[i].label, nid, x, y, rooms[room].id); replay();
    { char em[200]; export3d(em, sizeof em); }
}
static void vx_place(int x, int y, char *msg, size_t cap) { vx_place_in(vroom, x, y, msg, cap); }

/* the catalog gains the generated voxel rows (state/voxels.pdl, one ITEM row per palette sprite ever placed; same columns as catalog.pdl): call where the skin loads its catalog */
static void placer_load_voxels(void) {
    char p[PATH_MAX], line[512], *f[12]; FILE *fp; snprintf(p, sizeof p, "%s/voxels.pdl", ST);
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp) && ncat < MAXI) { if (split_bar(line, f, 12) == 11 && !strcmp(f[0], "ITEM")) { Item *it = &cat[ncat++]; snprintf(it->key, 32, "%s", f[1]); snprintf(it->label, 40, "%s", f[2]); it->price = atoi(f[3]); snprintf(it->sheet, 24, "%s", f[4]); it->c = atoi(f[5]); it->r = atoi(f[6]); it->w = atoi(f[7]); it->h = atoi(f[8]); it->wall = !strcmp(f[9], "wall"); snprintf(it->effect, 12, "%s", f[10]); } } fclose(fp); }
}
static int placer_verb(const char *v, int argc, char **argv, char *msg, size_t cap) {      /* returns 1 when it handled the verb (voxel <x> <y>) */
    if (!strcmp(v, "voxel") && argc >= 3) { vx_place(atoi(argv[1]), atoi(argv[2]), msg, cap); return 1; }
    return 0;
}
static int placer_sheet(const char *name, char *p, size_t cap) {      /* the sheet path of a generated voxel sheet, 1 when the name is one */
    if (strncmp(name, "!v_", 3)) return 0; snprintf(p, cap, "%s/vox/%s.png", ST, name); return 1;
}
/* once per daemon tick */
static void placer_tick(void) {
    { static long vsz = -1; char vp[PATH_MAX]; struct stat vs; snprintf(vp, sizeof vp, "%s/voxels.pdl", ST); long cur = stat(vp, &vs) == 0 ? (long)vs.st_size : 0;
      if (vsz >= 0 && cur != vsz) { load_data(); replay(); } vsz = cur; }      /* a verb process placed a new voxel kind: the catalog gained a row */
    {   /* the engine's brush placer (bv_menu_input.c: Enter in brush mode appends "x y z" to <session>/pieces/display/place_requests.txt): place each new request, never an old one */
        static char rsess[PATH_MAX] = ""; static long roff = -1; char up[PATH_MAX], sess[PATH_MAX] = "", ub[2048]; snprintf(up, sizeof up, "%s/board/state/ui.txt", APP); read_file(up, ub, sizeof ub);
        for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11);
        if (sess[0]) { char rq[PATH_MAX]; snprintf(rq, sizeof rq, "%s/pieces/display/place_requests.txt", sess); struct stat rs; long sz = stat(rq, &rs) == 0 ? (long)rs.st_size : 0;
            if (strcmp(rsess, sess) || roff < 0 || sz < roff) { snprintf(rsess, sizeof rsess, "%s", sess); roff = sz; }      /* first look at this session: everything already there is history */
            else if (sz > roff) { FILE *rf = fopen(rq, "r"); if (rf) { fseek(rf, roff, SEEK_SET); char ln[80]; while (fgets(ln, sizeof ln, rf)) { int rx, ry, rz; if (sscanf(ln, "%d %d %d", &rx, &ry, &rz) >= 2) { char pm[200], wb[2048], wmap[64] = "", wdesk[64] = "", wp[PATH_MAX]; snprintf(wp, sizeof wp, "%s/pieces/world_01/state.txt", PCHQ); read_file(wp, wb, sizeof wb);
                    for (char *wl = strtok(wb, "\n"); wl; wl = strtok(NULL, "\n")) { if (!strncmp(wl, "map_id=", 7)) snprintf(wmap, sizeof wmap, "%s", wl + 7); else if (!strncmp(wl, "desk_id=", 8)) snprintf(wdesk, sizeof wdesk, "%s", wl + 8); }
                    if (strcmp(wmap, "" PET_APP "")) snprintf(pm, sizeof pm, "not placed: the board shows the book %s, not this editor's own", wmap); else vx_place_in(room_idx(wdesk), rx, ry, pm, sizeof pm); append_line("voxel_log.txt", pm); } } roff = ftell(rf); fclose(rf); } } } }
}
