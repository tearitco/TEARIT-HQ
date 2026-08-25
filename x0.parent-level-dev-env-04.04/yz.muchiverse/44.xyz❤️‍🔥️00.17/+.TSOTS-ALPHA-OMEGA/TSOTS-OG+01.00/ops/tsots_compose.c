/* tsots_compose - render the TSOTS menu or game screen into
 * pieces/apps/player_app/view.txt (${game_map}). Same shape as
 * agy-txt's agy_compose_view: box chrome, ONE visible frame writer
 * (writes ONLY view.txt; the pal loop owns hit_frame).
 *
 * Usage: tsots_compose.+x
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 56
#define MAX_VERSES 6
#define MAX_ANSWER 16

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void read_current_layout(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, (int)out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

/* Load round.txt rows (display order): rank|chap:verse|text */
static int load_round(char refs[MAX_VERSES][64], char texts[MAX_VERSES][MAX_LINE],
                      int ranks[MAX_VERSES]) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/round.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < MAX_VERSES && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        *p1 = '\0';
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        ranks[n] = atoi(line);
        snprintf(refs[n], 64, "%s", p1 + 1);
        snprintf(texts[n], MAX_LINE, "%s", p2 + 1);
        n++;
    }
    fclose(f);
    return n;
}

static void read_solution(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/solution.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    size_t n = fread(out, 1, out_sz - 1, f);
    out[n] = '\0';
    out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static void box_top(FILE *o, const char *title) {
    fprintf(o, "╔");
    int pad = BOX_W - 2;
    int tlen = (int)strlen(title);
    int left = (pad - tlen) / 2;
    if (left < 0) left = 0;
    for (int i = 0; i < left; i++) fputs("═", o);
    fputs(title, o);
    for (int i = left + tlen; i < pad; i++) fputs("═", o);
    fprintf(o, "╗\n");
}

static void box_sep(FILE *o) {
    fprintf(o, "╠");
    for (int i = 0; i < BOX_W; i++) fputs("═", o);
    fprintf(o, "╣\n");
}

static void box_bot(FILE *o) {
    fprintf(o, "╚");
    for (int i = 0; i < BOX_W; i++) fputs("═", o);
    fprintf(o, "╝\n");
}

static int visible_width(const char *content) {
    int vis = 0;
    for (const unsigned char *p = (const unsigned char *)content; *p; ) {
        if (*p < 0x80) { vis++; p++; }
        else if ((*p & 0xE0) == 0xC0) { vis++; p += 2; if (!p[-1]) break; }
        else if ((*p & 0xF0) == 0xE0) { vis++; p += 3; }
        else if ((*p & 0xF8) == 0xF0) { vis++; p += 4; }
        else { vis++; p++; }
    }
    return vis;
}

static void box_row(FILE *o, const char *content) {
    int vis = visible_width(content);
    fprintf(o, "║  %s", content);
    int pad = BOX_W - 2 - vis;
    if (pad < 0) pad = 0;
    for (int i = 0; i < pad; i++) fputc(' ', o);
    fprintf(o, "║\n");
}

/* Word-wrap content into box rows (max content width BOX_W - 2). */
static void box_wrap(FILE *o, const char *content, int indent) {
    const char *p = content;
    int maxw = BOX_W - 2 - indent;
    if (maxw < 8) maxw = 8;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *start = p;
        int w = 0;
        const char *last_space = NULL;
        while (*p && w <= maxw) {
            const char *nxt = p;
            int adv;
            if ((unsigned char)*p < 0x80) { adv = 1; }
            else if (((unsigned char)*p & 0xE0) == 0xC0) { adv = 2; }
            else if (((unsigned char)*p & 0xF0) == 0xE0) { adv = 3; }
            else if (((unsigned char)*p & 0xF8) == 0xF0) { adv = 4; }
            else { adv = 1; }
            w += 1;
            p = nxt + adv;
            if (*nxt == ' ' && p - start < maxw) last_space = p;
            if (w > maxw) break;
        }
        if (w > maxw && last_space && last_space > start + indent) {
            size_t seg_len = (size_t)(last_space - start);
            char seg[MAX_LINE];
            if (seg_len >= sizeof(seg)) seg_len = sizeof(seg) - 1;
            memcpy(seg, start, seg_len);
            seg[seg_len] = '\0';
            char row[MAX_LINE];
            if (indent > 0) { memset(row, ' ', (size_t)indent); snprintf(row + indent, sizeof(row) - indent, "%s", seg); }
            else snprintf(row, sizeof(row), "%s", seg);
            box_row(o, row);
            p = last_space;
        } else {
            size_t seg_len = (size_t)(p - start);
            char seg[MAX_LINE];
            if (seg_len >= sizeof(seg)) seg_len = sizeof(seg) - 1;
            memcpy(seg, start, seg_len);
            seg[seg_len] = '\0';
            char row[MAX_LINE];
            if (indent > 0) { memset(row, ' ', (size_t)indent); snprintf(row + indent, sizeof(row) - indent, "%s", seg); }
            else snprintf(row, sizeof(row), "%s", seg);
            box_row(o, row);
        }
    }
}

static void render_menu(FILE *o) {
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/game_state.txt", project_root);
    char elo[32] = "1000", wins[32] = "0", losses[32] = "0";
    read_kv_str(state_path, "elo", elo, sizeof(elo));
    read_kv_str(state_path, "wins", wins, sizeof(wins));
    read_kv_str(state_path, "losses", losses, sizeof(losses));

    box_top(o, " T S O T S ");
    {
        char row[BOX_W + 32];
        snprintf(row, sizeof(row), "THIS ORDER STILL STANDS");
        box_row(o, row);
    }
    box_sep(o);
    box_wrap(o, "Reorder scrambled verses back into their Bible order (Genesis to Revelation).", 0);
    box_row(o, "");
    {
        char row[BOX_W + 32];
        snprintf(row, sizeof(row), "ELO %s    W%s  L%s", elo, wins, losses);
        box_row(o, row);
    }
    box_row(o, "");
    box_row(o, "[ PLAY ] - Enter to start");
    box_sep(o);
    box_wrap(o, "Answer with 1-9 as they appear in the Bible. More verses as you level up.", 0);
    box_bot(o);
}

static void render_game(FILE *o) {
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/game_state.txt", project_root);
    char elo[32] = "1000", wins[32] = "0", losses[32] = "0", round_raw[32] = "0";
    char status[64] = "none", answer[MAX_ANSWER] = "", last_result[32] = "none", last_delta[16] = "0";
    read_kv_str(state_path, "elo", elo, sizeof(elo));
    read_kv_str(state_path, "wins", wins, sizeof(wins));
    read_kv_str(state_path, "losses", losses, sizeof(losses));
    read_kv_str(state_path, "round", round_raw, sizeof(round_raw));
    read_kv_str(state_path, "status", status, sizeof(status));
    read_kv_str(state_path, "answer", answer, sizeof(answer));
    read_kv_str(state_path, "last_result", last_result, sizeof(last_result));
    read_kv_str(state_path, "last_delta", last_delta, sizeof(last_delta));

    char refs[MAX_VERSES][64];
    char texts[MAX_VERSES][MAX_LINE];
    int ranks[MAX_VERSES];
    int n = load_round(refs, texts, ranks);

    {
        char title[BOX_W + 32];
        snprintf(title, sizeof(title), " R O U N D %s ", round_raw);
        box_top(o, title);
    }
    {
        char row[BOX_W + 32];
        snprintf(row, sizeof(row), "ELO %s    W%s  L%s", elo, wins, losses);
        box_row(o, row);
    }
    box_sep(o);

    if (n < 1) {
        box_row(o, "Dealing verses...");
        box_bot(o);
        return;
    }

    box_wrap(o, "Put these verses in BIBLE order:", 0);
    box_row(o, "");
    for (int d = 0; d < n; d++) {
        char row[BOX_W + 32];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(row, sizeof(row), "[%d] %s", d + 1, texts[d]);
#pragma GCC diagnostic pop
        box_wrap(o, row, 0);
    }
    box_sep(o);

    {
        char row[BOX_W + 32];
        if (answer[0]) {
            char spaced[MAX_ANSWER * 2];
            spaced[0] = '\0';
            for (size_t i = 0; i < strlen(answer); i++) {
                if (i > 0) strncat(spaced, " ", sizeof(spaced) - strlen(spaced) - 1);
                char c[2] = { answer[i], '\0' };
                strncat(spaced, c, sizeof(spaced) - strlen(spaced) - 1);
            }
            snprintf(row, sizeof(row), "ORDER: %s _", spaced);
        } else {
            snprintf(row, sizeof(row), "ORDER: _");
        }
        box_row(o, row);
    }

    if (strcmp(status, "feedback") == 0) {
        box_sep(o);
        if (strcmp(last_result, "correct") == 0) {
            char row[BOX_W + 32];
            snprintf(row, sizeof(row), "[CORRECT!] %s ELO", last_delta);
            box_row(o, row);
        } else if (strcmp(last_result, "wrong") == 0) {
            char row[BOX_W + 32];
            snprintf(row, sizeof(row), "[WRONG!] %s ELO", last_delta);
            box_row(o, row);
        } else {
            box_row(o, "ORDER INCOMPLETE");
        }

        char solution[MAX_ANSWER];
        read_solution(solution, sizeof(solution));
        {
            char row[BOX_W + 32];
            snprintf(row, sizeof(row), "Correct order: %s", solution);
            box_row(o, row);
        }
        /* display indices in true bible order (inverse of the ranks) */
        {
            int inv[MAX_VERSES];
            for (int i = 0; i < n; i++) inv[i] = -1;
            for (int d = 0; d < n; d++) {
                if (ranks[d] >= 0 && ranks[d] < n) inv[ranks[d]] = d;
            }
            char row[MAX_LINE];
            row[0] = '\0';
            for (int r = 0; r < n; r++) {
                char seg[32];
                if (inv[r] >= 0) snprintf(seg, sizeof(seg), "%d ", inv[r] + 1);
                else snprintf(seg, sizeof(seg), "? ");
                if (strlen(row) + strlen(seg) + 1 < sizeof(row)) strncat(row, seg, sizeof(row) - strlen(row) - 1);
            }
            char full[BOX_W + 48];
            snprintf(full, sizeof(full), "Bible sequence: %s", row);
            box_row(o, full);
        }
        /* refs listed in the same positional order as the solution */
        {
            char row[MAX_LINE];
            row[0] = '\0';
            for (int i = 0; i < n; i++) {
                char seg[96];
                snprintf(seg, sizeof(seg), "[%d]%s ", i + 1, refs[i]);
                if (strlen(row) + strlen(seg) + 1 < sizeof(row)) strncat(row, seg, sizeof(row) - strlen(row) - 1);
            }
            box_wrap(o, row, 0);
        }
        box_sep(o);
        box_row(o, "Enter -> next round");
    } else {
        box_row(o, "(1-9 pick, Backspace undo, Enter submit)");
    }
    box_bot(o);
}

int main(void) {
    resolve_root();

    char layout[256];
    read_current_layout(layout, sizeof(layout));

    char view_path[PATH_BUF];
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);

    FILE *o = fopen(view_path, "w");
    if (!o) return 1;

    if (strstr(layout, "game.chtpm") != NULL) {
        render_game(o);
    } else {
        render_menu(o);
    }

    fclose(o);
    return 0;
}
