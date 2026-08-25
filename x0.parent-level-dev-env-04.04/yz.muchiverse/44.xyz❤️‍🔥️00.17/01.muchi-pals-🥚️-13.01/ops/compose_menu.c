/* compose_menu - one verb, one binary, no shared headers.
 * Renders the current menu screen (pieces/system/menu_state.txt) into
 * plain text at pieces/display/current_frame.txt, for the renderer
 * process to print. Visual style (boxed ASCII panel, numbered/cursor
 * options list, title banner) matches real TPMOS's .chtpm layout files
 * (pieces/chtpm/layouts/ in the 1.TPMOS reference tree) without adopting
 * their markup engine - this is hand-printed text, same as mutaclsym's
 * compose_frame.c. Does NOT touch a terminal itself. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 60 /* interior width between the | | borders */

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void border(FILE *out) {
    fputc('+', out);
    for (int i = 0; i < BOX_W; i++) fputc('=', out);
    fputc('+', out);
    fputc('\n', out);
}

static void line(FILE *out, const char *content) {
    int len = (int)strlen(content);
    if (len > BOX_W) len = BOX_W;
    fprintf(out, "|%.*s", len, content);
    for (int i = len; i < BOX_W; i++) fputc(' ', out);
    fputc('|', out);
    fputc('\n', out);
}

static void blank(FILE *out) { line(out, ""); }

static void title(FILE *out, const char *text) {
    /* space the title out letter by letter, matching the TPMOS layouts'
     * "F U Z Z P E T   D A S H B O A R D" style, then center it. */
    char spaced[BOX_W + 1] = "";
    int p = 0;
    for (const char *c = text; *c && p < BOX_W - 1; c++) {
        spaced[p++] = *c;
        if (c[1]) spaced[p++] = ' ';
    }
    spaced[p] = '\0';
    int pad = (BOX_W - (int)strlen(spaced)) / 2;
    char padded[BOX_W + 1] = "";
    for (int i = 0; i < pad && i < BOX_W; i++) padded[i] = ' ';
    snprintf(padded + (pad > 0 ? pad : 0), sizeof(padded) - (pad > 0 ? pad : 0), "%s", spaced);
    line(out, padded);
}

/* Bracket-cursor + bracket-label style, matching real 1.TPMOS's live
 * renderer EXACTLY (pieces/chtpm/plugins/chtpm_parser.c - confirmed via
 * its compiled +x binary, the actual thing that renders these panels;
 * the sibling chtmp_player.c has no equivalent digit-handling and no
 * +x artifact, i.e. dead code). Its interactive-row branch:
 *   asprintf(&line, "%s %d. [%s]", pref, display_num, scratch_substituted);
 * where pref is "[>]"/"[ ]"/"[^]" and display_num is a running counter
 * over every interactive row in the WHOLE panel (continues past 9 with
 * no cap - confirmed live in a captured frame with "10. [checkers]",
 * "11. [chtmgl-alpha]", ... "25. [Back to Main Menu]"). No index<9
 * restriction here for the same reason - see menu_input.c's matching
 * multi-digit accumulator, which is what actually lets a 2+ digit
 * number be typed and selected. */
static void option(FILE *out, int index, int cursor, const char *label) {
    char buf[BOX_W + 1];
    const char *cur = index == cursor ? "[>]" : "[ ]";
    snprintf(buf, sizeof(buf), "  %s %d. [%s]", cur, index + 1, label);
    line(out, buf);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char l[MAX_LINE];
    int val = def;
    while (fgets(l, sizeof(l), f)) {
        char *eq = strchr(l, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(l, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char l[MAX_LINE];
    while (fgets(l, sizeof(l), f)) {
        l[strcspn(l, "\r\n")] = '\0'; /* CRLF-safe - a Windows-touched file can have \r\n endings */
        char *eq = strchr(l, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(l, key) == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

int main(void) {
    resolve_root();

    char menu_path[PATH_BUF], user_path[PATH_BUF], out_path[PATH_BUF];
    snprintf(menu_path, sizeof(menu_path), "%s/pieces/system/menu_state.txt", project_root);
    snprintf(user_path, sizeof(user_path), "%s/pieces/world_01/map_lobby/user_01/state.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/current_frame.txt", project_root);

    char screen[32], msg[256], user_name[64], selected_pet[64];
    read_kv_str(menu_path, "screen", screen, sizeof(screen), "main");
    int cursor = read_kv_int(menu_path, "cursor", 0);
    read_kv_str(menu_path, "last_message", msg, sizeof(msg), "");
    read_kv_str(menu_path, "selected_pet", selected_pet, sizeof(selected_pet), "");
    read_kv_str(user_path, "name", user_name, sizeof(user_name), "user_01");
    int tokens = read_kv_int(user_path, "tokens", 0);

    FILE *out = fopen(out_path, "w");
    if (!out) return 1;

    border(out);
    blank(out);
    title(out, "MUCHI-PALS");
    blank(out);

    if (strcmp(screen, "main") == 0) {
        option(out, 0, cursor, "User");
        option(out, 1, cursor, "Faucet");
        option(out, 2, cursor, "Store");
        option(out, 3, cursor, "Pets");
        option(out, 4, cursor, "Processes");
    } else if (strcmp(screen, "user") == 0) {
        title(out, "USER");
        blank(out);
        char l[512]; /* generous headroom - line() itself re-truncates to BOX_W for display */
        snprintf(l, sizeof(l), "  Name: %s", user_name);
        line(out, l);
        snprintf(l, sizeof(l), "  Tokens: %d", tokens);
        line(out, l);
        blank(out);
        option(out, 0, cursor, "Back");
    } else if (strcmp(screen, "faucet") == 0) {
        title(out, "FAUCET");
        blank(out);
        char l[512]; /* generous headroom - line() itself re-truncates to BOX_W for display */
        snprintf(l, sizeof(l), "  Tokens: %d", tokens);
        line(out, l);
        blank(out);
        option(out, 0, cursor, "Claim Tokens (+10)");
        option(out, 1, cursor, "Coin Flip (stake 10, double or nothing)");
        option(out, 2, cursor, "Back");
    } else if (strcmp(screen, "store") == 0) {
        title(out, "STORE");
        blank(out);
        char l[512]; /* generous headroom - line() itself re-truncates to BOX_W for display */
        snprintf(l, sizeof(l), "  Tokens: %d", tokens);
        line(out, l);
        blank(out);
        option(out, 0, cursor, "Buy Egg (cost: 20 tokens)");
        option(out, 1, cursor, "Back");
    } else if (strcmp(screen, "pets") == 0) {
        title(out, "PETS");
        blank(out);
        char inv_path[PATH_BUF];
        snprintf(inv_path, sizeof(inv_path), "%s/pieces/world_01/map_lobby/user_01/inventory.txt", project_root);
        FILE *inv = fopen(inv_path, "r");
        int row = 0;
        if (inv) {
            char pet_id[64];
            while (fgets(pet_id, sizeof(pet_id), inv)) {
                pet_id[strcspn(pet_id, "\r\n")] = '\0'; /* CRLF-safe */
                if (!pet_id[0]) continue;
                char pet_state[PATH_BUF];
                snprintf(pet_state, sizeof(pet_state), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, pet_id);
                char species[64], emoji[32];
                read_kv_str(pet_state, "species_name", species, sizeof(species), "?");
                read_kv_str(pet_state, "species_emoji", emoji, sizeof(emoji), "?");
                int hatched = read_kv_int(pet_state, "hatched", 0);
                char l[512]; /* generous headroom - line() itself re-truncates to BOX_W for display */
                /* Same "[>]"/"[ ]" + "N. [label]" convention as option()
                 * - these rows bypass that helper (they need extra
                 * per-pet fields inside the label) but must still match
                 * its format exactly, matching real 1.TPMOS's
                 * chtmp_parser.c (see option()'s own comment) and
                 * menu_input.c's matching multi-digit accumulator, which
                 * treats a "pets" screen row exactly like any other
                 * numbered option - no row-count cap here either. */
                const char *cur = row == cursor ? "[>]" : "[ ]";
                char body[256];
                if (hatched) {
                    int hp = read_kv_int(pet_state, "hp", 0);
                    int hp_max = read_kv_int(pet_state, "hp_max", 0);
                    int mp = read_kv_int(pet_state, "mp", 0);
                    int mp_max = read_kv_int(pet_state, "mp_max", 0);
                    int hunger = read_kv_int(pet_state, "hunger", 0);
                    int asleep = read_kv_int(pet_state, "asleep", 0);
                    int poop_count = read_kv_int(pet_state, "poop_count", 0);
                    char flags[32] = "";
                    if (asleep) strcat(flags, " Zzz");
                    if (hunger >= 70) strcat(flags, " hungry");
                    if (poop_count > 0) strcat(flags, " dirty");
                    snprintf(body, sizeof(body), "%s %-6s %s  HP:%d/%d MP:%d/%d%s",
                             emoji, pet_id, species, hp, hp_max, mp, mp_max, flags);
                } else {
                    snprintf(body, sizeof(body), "%s %-6s %s  (unhatched - hatch)", emoji, pet_id, species);
                }
                snprintf(l, sizeof(l), "  %s %d. [%s]", cur, row + 1, body);
                line(out, l);
                row++;
            }
            fclose(inv);
        }
        if (row == 0) line(out, "  No pets yet - visit the Store!");
        blank(out);
        option(out, row, cursor, "Back");
    } else if (strcmp(screen, "pet_detail") == 0) {
        char pet_state[PATH_BUF];
        snprintf(pet_state, sizeof(pet_state), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, selected_pet);
        char species[64], emoji[32], skills[128], card_status[16];
        read_kv_str(pet_state, "species_name", species, sizeof(species), "?");
        read_kv_str(pet_state, "species_emoji", emoji, sizeof(emoji), "?");
        read_kv_str(pet_state, "skills", skills, sizeof(skills), "");
        read_kv_str(pet_state, "card_status", card_status, sizeof(card_status), "none");
        int hp = read_kv_int(pet_state, "hp", 0);
        int hp_max = read_kv_int(pet_state, "hp_max", 0);
        int mp = read_kv_int(pet_state, "mp", 0);
        int mp_max = read_kv_int(pet_state, "mp_max", 0);
        int level = read_kv_int(pet_state, "level", 1);
        int xp = read_kv_int(pet_state, "xp", 0);
        int hunger = read_kv_int(pet_state, "hunger", 0);
        int energy = read_kv_int(pet_state, "energy", 100);
        int poop_count = read_kv_int(pet_state, "poop_count", 0);
        int asleep = read_kv_int(pet_state, "asleep", 0);
        int grid_x = read_kv_int(pet_state, "grid_x", 0);
        int grid_y = read_kv_int(pet_state, "grid_y", 0);
        int pet_z = read_kv_int(pet_state, "z", 0);
        char facing[8];
        read_kv_str(pet_state, "facing", facing, sizeof(facing), "right");

        char title_buf[128];
        snprintf(title_buf, sizeof(title_buf), "%s %s", emoji, selected_pet);
        title(out, title_buf);
        blank(out);
        char l[512]; /* generous headroom - line() itself re-truncates to BOX_W for display */
        snprintf(l, sizeof(l), "  %s  Level %d (XP %d/%d)", species, level, xp, level * 20);
        line(out, l);
        snprintf(l, sizeof(l), "  HP:%d/%d MP:%d/%d", hp, hp_max, mp, mp_max);
        line(out, l);
        snprintf(l, sizeof(l), "  Hunger:%d Energy:%d Poop:%d %s", hunger, energy, poop_count, asleep ? "(asleep)" : "");
        line(out, l);
        snprintf(l, sizeof(l), "  Skills: %s", skills[0] ? skills : "none");
        line(out, l);
        snprintf(l, sizeof(l), "  Card: %s", card_status);
        line(out, l);
        snprintf(l, sizeof(l), "  Position: (%d,%d,%d) Facing: %s", grid_x, grid_y, pet_z, facing);
        line(out, l);
        blank(out);
        option(out, 0, cursor, "Open Window");
        option(out, 1, cursor, "Feed (-5 tokens, -30 hunger)");
        option(out, 2, cursor, "Clean");
        option(out, 3, cursor, asleep ? "Wake" : "Sleep");
        option(out, 4, cursor, "Train (-20 energy)");
        option(out, 5, cursor, "Export Card");
        option(out, 6, cursor, "Destroy Card");
        option(out, 7, cursor, "Back");
    } else if (strcmp(screen, "processes") == 0) {
        title(out, "PROCESSES");
        blank(out);
        char list_path[PATH_BUF];
        snprintf(list_path, sizeof(list_path), "%s/pieces/system/process_list.txt", project_root);
        FILE *lf = fopen(list_path, "r");
        int rows = 0;
        if (lf) {
            char row[MAX_LINE];
            while (fgets(row, sizeof(row), lf)) {
                row[strcspn(row, "\r\n")] = '\0'; /* CRLF-safe */
                if (!row[0]) continue;
                char *p1 = strchr(row, '|');
                char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
                if (!p1 || !p2) continue;
                *p1 = '\0'; *p2 = '\0';
                const char *pet_id = row;
                const char *pid = p1 + 1;
                int alive = atoi(p2 + 1);
                char l[512]; /* generous headroom - line() itself re-truncates to BOX_W for display */
                /* pet_id/pid are genuinely short tokens (a piece id like
                 * "egg_12", a decimal pid) despite gcc only being able to
                 * prove them no longer than row's own MAX_LINE - same
                 * class of warning already suppressed narrowly elsewhere
                 * in this codebase rather than widening l to match. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(l, sizeof(l), "  %-8s pid:%-8s %s", pet_id, pid, alive ? "[alive]" : "[dead]");
#pragma GCC diagnostic pop
                line(out, l);
                rows++;
            }
            fclose(lf);
        }
        if (rows == 0) line(out, "  No pet windows tracked - open one from Pets.");
        blank(out);
        option(out, 0, cursor, "Back");
    }

    if (msg[0]) {
        blank(out);
        char l[512]; /* generous headroom - line() itself re-truncates to BOX_W for display */
        snprintf(l, sizeof(l), "  %s", msg);
        line(out, l);
    }

    blank(out);
    border(out);
    /* Quit is Ctrl+C only (mass-refactor 2026-07-26, direct user
     * instruction: "never use q to quit in any of the refactors") - a
     * literal 'q'/'Q' keystroke used to double as a global quit hotkey
     * here (outside the numbered/multi-digit-accumulator dispatch
     * entirely), which meant that letter could never be typed as
     * ordinary content. Ctrl+C is a real control character, never
     * collides with typed text, and is handled directly by
     * system/keyboard_input.c. Every screen's own "Back" row remains a
     * normal numbered option() - unaffected by this. */
    fprintf(out, "[wasd/arrows] move  [0-9] jump  [enter] select  [b] back  [ctrl+c] quit\n");

    fclose(out);

    /* CHTPM VIEW BRIDGE (see chtpm-to-pal-layout-plan.txt §8 and
     * xyzos-standards.txt §7 for the why - same addition already made to
     * zoo_0000's and mutaclsym's own compose_frame.c): a chtpm layout's
     * own `${game_map}` var is populated by load_vars()'s real,
     * unmodified GENERIC VIEW LOADING logic, which checks
     * pieces/apps/player_app/view.txt as one of its own candidate
     * paths - writing this file is the ONLY thing needed to let a real
     * .chtpm menu shell display this project's live menu, no
     * chtpm_parser_pal.c patch required. Copies the exact content just
     * written to current_frame.txt, byte for byte. */
    {
        FILE *src = fopen(out_path, "r");
        if (src) {
            char view_path[PATH_BUF];
            snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
            FILE *dst = fopen(view_path, "w");
            if (dst) {
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
                fclose(dst);
            }
            fclose(src);
        }
    }

    /* CHTPM RENDER-TRIGGER MARKER: chtpm_parser_pal.c's own main loop
     * only recomposes when one of a fixed set of marker files grows -
     * growing pieces/apps/player_app/state_changed.txt (one of the
     * markers it already checks) makes it re-run load_vars() (which is
     * what re-reads view.txt), so the pets' own ticking shows up even
     * when chtpm itself received no keypress this cycle. */
    {
        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/apps/player_app/state_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
    }

    return 0;
}
