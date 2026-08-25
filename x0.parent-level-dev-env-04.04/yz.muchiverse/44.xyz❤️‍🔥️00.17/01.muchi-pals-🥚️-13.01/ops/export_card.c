/* export_card - one verb, one binary, no shared headers.
 * Snapshots a hatched pet's current stats/moves into a trading-card-style
 * PNG (stbi_write_png, already vendored and used exactly this way by
 * system/emoji_gen_atlas.c) written to exports/<card_id>.png.
 *
 * card_id is a plain incrementing "<pet_id>-<serial>" string, not a QR
 * payload (dropped per direct instruction) - it's meant as the stand-in
 * for whatever a future lightweight blockchain will validate uniqueness/
 * ownership against, and the issue/destroy gate below (see
 * ops/destroy_card.c) is the local, offline version of what will later
 * be a mint/burn pair on that chain. Refuses to mint a second card while
 * one is still "issued" - destroy_card.c must run first, modeling a
 * one-valid-card-per-pet invariant.
 *
 * Usage: export_card.+x <pet_piece_id>
 * Prints a one-line result message to stdout. */
#define _GNU_SOURCE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../system/lib/stb_image_write.h"
#include "../system/lib/bitmap_font5x7.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_LINES 48
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_SKILLS_IN_POOL 64

#define CARD_W 300
#define CARD_H 440
#define BORDER 8
#define SPRITE_AREA 200

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void append_ledger(const char *piece_id, const char *key, const char *value, const char *trigger) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/master_ledger.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] StateChange: %s %s %s | Trigger: %s\n", ts, piece_id, key, value, trigger);
    fclose(f);
}

typedef struct {
    char id[32];
    char name[64];
    int power;
    int mp_cost;
} SkillEntry;

static int load_skill_pool(SkillEntry *out, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/skills/skill_pool.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    int count = 0;
    while (count < max && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = '\0'; /* CRLF-safe */
        char *id = line;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        *p1 = '\0';
        char *name = p1 + 1;
        char *p2 = strchr(name, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *p3 = strchr(p2 + 1, '|');
        if (!p3) continue;
        int power = atoi(p3 + 1);
        char *p4 = strchr(p3 + 1, '|');
        int mp_cost = p4 ? atoi(p4 + 1) : 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(out[count].id, sizeof(out[count].id), "%s", id);
        snprintf(out[count].name, sizeof(out[count].name), "%s", name);
#pragma GCC diagnostic pop
        out[count].power = power;
        out[count].mp_cost = mp_cost;
        count++;
    }
    fclose(f);
    return count;
}

static const SkillEntry *find_skill(const SkillEntry *pool, int count, const char *id) {
    for (int i = 0; i < count; i++) if (strcmp(pool[i].id, id) == 0) return &pool[i];
    return NULL;
}

/* Same CSV format egg_window.c's load_sprite reads - duplicated here per
 * project convention (no shared header). */
static int load_sprite(const char *csv_path, unsigned char **out_pixels, int *out_res) {
    FILE *f = fopen(csv_path, "r");
    if (!f) return 0;

    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return 0; }

    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return 0; }

    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            pixels[count * 4 + 0] = (unsigned char)r;
            pixels[count * 4 + 1] = (unsigned char)g;
            pixels[count * 4 + 2] = (unsigned char)b;
            pixels[count * 4 + 3] = (unsigned char)a;
            count++;
        }
    }
    fclose(f);

    if (count != res * res) { free(pixels); return 0; }
    *out_pixels = pixels;
    *out_res = res;
    return 1;
}

static void fill_rect(unsigned char *canvas, int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b) {
    for (int y = y0; y < y1 && y < CARD_H; y++) {
        if (y < 0) continue;
        for (int x = x0; x < x1 && x < CARD_W; x++) {
            if (x < 0) continue;
            unsigned char *p = &canvas[(y * CARD_W + x) * 4];
            p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
        }
    }
}

/* Nearest-neighbor upscale of a square sprite into a dst_w x dst_h area
 * of the canvas at (dst_x, dst_y). Alpha>127 pixels copy opaquely;
 * near-transparent sprite pixels are skipped so the card background
 * shows through - a simplification vs. real alpha blending, fine for a
 * static pixel-art export. */
static void blit_sprite(unsigned char *canvas, int dst_x, int dst_y, int dst_w, int dst_h,
                         const unsigned char *src, int src_res) {
    for (int y = 0; y < dst_h; y++) {
        int sy = (y * src_res) / dst_h;
        if (sy >= src_res) sy = src_res - 1;
        for (int x = 0; x < dst_w; x++) {
            int sx = (x * src_res) / dst_w;
            if (sx >= src_res) sx = src_res - 1;
            const unsigned char *sp = &src[(sy * src_res + sx) * 4];
            if (sp[3] <= 127) continue;
            int cx = dst_x + x, cy = dst_y + y;
            if (cx < 0 || cy < 0 || cx >= CARD_W || cy >= CARD_H) continue;
            unsigned char *p = &canvas[(cy * CARD_W + cx) * 4];
            p[0] = sp[0]; p[1] = sp[1]; p[2] = sp[2]; p[3] = 255;
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pet_piece_id>\n", argv[0]);
        return 1;
    }
    const char *pet_id = argv[1];
    resolve_root();

    char state_path[PATH_BUF + 32];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, pet_id);

    FILE *f = fopen(state_path, "r");
    if (!f) { printf("Export failed: unknown pet %s.\n", pet_id); return 1; }

    char lines[MAX_LINES][MAX_LINE];
    int nlines = 0;
    char species_name[64] = "?", skills[MAX_LINE] = "";
    int hp = 0, hp_max = 0, mp = 0, mp_max = 0, level = 1, rarity = 1, card_serial = 0;
    char card_status[16] = "none";
    int seen_card_status = 0, seen_card_serial = 0, seen_card_id = 0;

    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            const char *key = lines[nlines];
            char *val = eq + 1;
            if (strcmp(key, "species_name") == 0) { char v[64]; snprintf(v, sizeof(v), "%s", val); v[strcspn(v, "\r\n")] = '\0'; snprintf(species_name, sizeof(species_name), "%s", v); }
            else if (strcmp(key, "hp") == 0) hp = atoi(val);
            else if (strcmp(key, "hp_max") == 0) hp_max = atoi(val);
            else if (strcmp(key, "mp") == 0) mp = atoi(val);
            else if (strcmp(key, "mp_max") == 0) mp_max = atoi(val);
            else if (strcmp(key, "level") == 0) level = atoi(val);
            else if (strcmp(key, "rarity") == 0) rarity = atoi(val);
            else if (strcmp(key, "skills") == 0) { char v[MAX_LINE]; snprintf(v, sizeof(v), "%s", val); v[strcspn(v, "\r\n")] = '\0'; snprintf(skills, sizeof(skills), "%s", v); }
            else if (strcmp(key, "card_status") == 0) { char v[16]; snprintf(v, sizeof(v), "%s", val); v[strcspn(v, "\r\n")] = '\0'; snprintf(card_status, sizeof(card_status), "%s", v); seen_card_status = 1; }
            else if (strcmp(key, "card_serial") == 0) { card_serial = atoi(val); seen_card_serial = 1; }
            else if (strcmp(key, "card_id") == 0) seen_card_id = 1;
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    if (strcmp(card_status, "issued") == 0) {
        printf("Destroy the current card before printing a new one for %s.\n", pet_id);
        return 1;
    }

    card_serial += 1;
    char card_id[80];
    snprintf(card_id, sizeof(card_id), "%s-%d", pet_id, card_serial);

    f = fopen(state_path, "w");
    if (!f) { printf("Export failed: could not write state.\n"); return 1; }
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            const char *key = lines[i];
            int handled = 1;
            if (strcmp(key, "card_status") == 0) fprintf(f, "card_status=issued\n");
            else if (strcmp(key, "card_serial") == 0) fprintf(f, "card_serial=%d\n", card_serial);
            else if (strcmp(key, "card_id") == 0) fprintf(f, "card_id=%s\n", card_id);
            else handled = 0;
            *eq = '=';
            if (!handled) fputs(lines[i], f);
        } else {
            fputs(lines[i], f);
        }
    }
    if (!seen_card_status) fprintf(f, "card_status=issued\n");
    if (!seen_card_serial) fprintf(f, "card_serial=%d\n", card_serial);
    if (!seen_card_id) fprintf(f, "card_id=%s\n", card_id);
    fclose(f);

    append_ledger(pet_id, "card_status", "issued", "export_card");
    char valbuf[16];
    snprintf(valbuf, sizeof(valbuf), "%d", card_serial);
    append_ledger(pet_id, "card_serial", valbuf, "export_card");
    append_ledger(pet_id, "card_id", card_id, "export_card");

    /* --- Render the card PNG --- */
    unsigned char *canvas = calloc((size_t)CARD_W * CARD_H, 4);
    if (!canvas) { printf("Export failed: out of memory rendering card.\n"); return 1; }

    unsigned char bg_r, bg_g, bg_b, border_r, border_g, border_b;
    if (rarity >= 3) { bg_r = 255; bg_g = 240; bg_b = 200; border_r = 180; border_g = 140; border_b = 30; }
    else if (rarity == 2) { bg_r = 200; bg_g = 230; bg_b = 255; border_r = 60; border_g = 110; border_b = 180; }
    else { bg_r = 235; bg_g = 235; bg_b = 235; border_r = 120; border_g = 120; border_b = 120; }

    fill_rect(canvas, 0, 0, CARD_W, CARD_H, border_r, border_g, border_b);
    fill_rect(canvas, BORDER, BORDER, CARD_W - BORDER, CARD_H - BORDER, bg_r, bg_g, bg_b);

    char sprite_path[PATH_BUF + 32];
    snprintf(sprite_path, sizeof(sprite_path), "%s/pieces/world_01/map_lobby/%s/sprite.csv", project_root, pet_id);
    unsigned char *sprite_pixels = NULL;
    int sprite_res = 0;
    if (load_sprite(sprite_path, &sprite_pixels, &sprite_res)) {
        blit_sprite(canvas, (CARD_W - SPRITE_AREA) / 2, 50, SPRITE_AREA, SPRITE_AREA, sprite_pixels, sprite_res);
        free(sprite_pixels);
    }

    unsigned char tr = 30, tg = 30, tb = 30, ta = 255;
    bmfont_draw_text(canvas, CARD_W, CARD_H, 16, 16, pet_id, tr, tg, tb, ta, 2);
    bmfont_draw_text(canvas, CARD_W, CARD_H, 16, 36, species_name, tr, tg, tb, ta, 1);

    char linebuf[128];
    snprintf(linebuf, sizeof(linebuf), "HP:%d/%d", hp, hp_max);
    bmfont_draw_text(canvas, CARD_W, CARD_H, 16, 260, linebuf, tr, tg, tb, ta, 2);
    snprintf(linebuf, sizeof(linebuf), "MP:%d/%d LV:%d", mp, mp_max, level);
    bmfont_draw_text(canvas, CARD_W, CARD_H, 16, 284, linebuf, tr, tg, tb, ta, 1);

    SkillEntry pool[MAX_SKILLS_IN_POOL];
    int pool_count = load_skill_pool(pool, MAX_SKILLS_IN_POOL);
    int move_y = 312, moves_drawn = 0;
    char skills_copy[MAX_LINE];
    snprintf(skills_copy, sizeof(skills_copy), "%s", skills);
    char *tok = strtok(skills_copy, ",");
    while (tok && moves_drawn < 4) {
        const SkillEntry *s = find_skill(pool, pool_count, tok);
        if (s) {
            snprintf(linebuf, sizeof(linebuf), "%s %d", s->name, s->power);
            bmfont_draw_text(canvas, CARD_W, CARD_H, 16, move_y, linebuf, tr, tg, tb, ta, 1);
            move_y += 12;
            moves_drawn++;
        }
        tok = strtok(NULL, ",");
    }

    bmfont_draw_text(canvas, CARD_W, CARD_H, 16, CARD_H - 30, card_id, tr, tg, tb, ta, 2);

    char exports_dir[PATH_BUF];
    snprintf(exports_dir, sizeof(exports_dir), "%s/exports", project_root);
#ifdef _WIN32
    mkdir(exports_dir);
#else
    mkdir(exports_dir, 0755);
#endif

    char out_path[PATH_BUF + 96];
    snprintf(out_path, sizeof(out_path), "%s/%s.png", exports_dir, card_id);

    if (!stbi_write_png(out_path, CARD_W, CARD_H, 4, canvas, CARD_W * 4)) {
        free(canvas);
        printf("Export failed: could not write PNG for %s.\n", card_id);
        return 1;
    }
    free(canvas);

    printf("Exported card %s -> exports/%s.png\n", card_id, card_id);
    return 0;
}
