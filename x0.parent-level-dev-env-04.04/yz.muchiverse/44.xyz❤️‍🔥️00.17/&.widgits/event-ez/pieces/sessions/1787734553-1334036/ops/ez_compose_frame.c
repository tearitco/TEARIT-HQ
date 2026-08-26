/* ez_compose_frame - Event-EZ: "click nav buttons, fill in cli-io
 * blanks" event authoring, 4th event-editor variant (2026-08-05).
 *
 * CHTPM digit_accum: 1-4 behavior buttons, 5=ez_target cli_io,
 * 6=ez_speed cli_io, 7=Save. Continuous, never restart per section.
 *
 * Usage: ez_compose_frame.+x
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

/* REAL FIX 2026-08-05, direct user correction ("play is meaningless to
 * me as user/dev guide if i cant see the script in the event screen so
 * i dont know wht ur doing... if i asked for more visibility thats
 * always priority"): before this, the window only ever showed the
 * CURRENTLY TYPED field values (blank on a fresh reopen) plus a
 * transient "Saved: ..." message that vanished on the next render.
 * There was no way to SEE what's actually on disk - what Play would
 * really run - just by looking at the window. This reads page_1's own
 * real condition.pdl (pipe-delimited SECTION|KEY|VALUE format) and
 * event.ir.pdl (NODE rows) straight off disk and renders them verbatim,
 * every single frame, regardless of what's currently typed. */
static void read_pdl_value(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        char *v_end = v + strlen(v);
        while (v_end > v && v_end[-1] == ' ') v_end--;
        *v_end = '\0';
        snprintf(out, out_sz, "%s", v);
        break;
    }
    fclose(f);
}

/* REAL, 2026-08-05: Gallery<->Page navigation - see design doc's own
 * "Full nested flow, RPG-Maker-MV-accurate" section. Each page gets its
 * own real, generated .chtpm file (event_ez_page_N.chtpm); a screen
 * tells which page it is by reading chtpm_parser_pal.c's own real,
 * pre-existing pieces/display/current_layout.txt ("EXPORT CURRENT
 * LAYOUT FOR MODULE HEARTBEAT", written on every screen switch,
 * confirmed via source - not invented here). Returns 0 if we're on the
 * gallery (no page_N in the active layout's own filename). Superseded
 * by current_screen_kind() below, which also classifies WHICH kind of
 * per-page screen is active, not just the page number. */

/* REAL, 2026-08-05, direct instruction ("this is at this point a proving
 * grounds for our ability to do event management mirroring rpg maker" -
 * see visual-event-compiler-pal.md §7): the Page screen now hosts a
 * genuine, growing COMMAND LIST (Show Choices, Change Gold - the exact 2
 * real RPG Maker commands confirmed in #.ref/menu/event.commands.1.txt)
 * instead of the old flat Chase/Flee/Wander/Idle demo. Each real command
 * lives on its OWN generated screen (same "page number encoded in
 * filename, read back via current_layout.txt" trick current_page_number()
 * already proves), so this reads current_layout.txt ONCE and classifies
 * which of 3 real screen kinds is active. */
typedef enum { SCREEN_GALLERY, SCREEN_PAGE, SCREEN_CMDPICK, SCREEN_CMD_CHANGE_GOLD, SCREEN_CMD_SHOW_TEXT, SCREEN_CMD_SHOW_CHOICES } ScreenKind;

static ScreenKind current_screen_kind(int *page_n_out) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r");
    *page_n_out = 0;
    if (!f) return SCREEN_GALLERY;
    char line[PATH_BUF];
    ScreenKind kind = SCREEN_GALLERY;
    if (fgets(line, sizeof(line), f)) {
        char *p = strstr(line, "page_");
        if (p) {
            *page_n_out = atoi(p + 5);
            if (strstr(line, "_cmdpick")) kind = SCREEN_CMDPICK;
            else if (strstr(line, "_cmd_change_gold")) kind = SCREEN_CMD_CHANGE_GOLD;
            /* REAL FIX 2026-08-24: _cmd_show_text/_cmd_show_choices used
             * to fall through to SCREEN_PAGE, so every compose tick while
             * on those parameter screens overwrote view.txt with the
             * PAGE screen's compose (the param layout file itself was
             * never regenerated either). Classify them like Change
             * Gold's own screen - static .chtpm, no per-tick view
             * rewrite (see main()'s dispatch comment below). */
            else if (strstr(line, "_cmd_show_text")) kind = SCREEN_CMD_SHOW_TEXT;
            else if (strstr(line, "_cmd_show_choices")) kind = SCREEN_CMD_SHOW_CHOICES;
            else kind = SCREEN_PAGE;
        }
    }
    fclose(f);
    return kind;
}

/* REAL, 2026-08-05, direct instruction ("this is at this point a proving
 * grounds for our ability to do event management mirroring rpg maker"):
 * replaced the old flat Chase/Flee/Wander/Idle demo with a real, growing
 * COMMAND LIST - ${command_list_rows_N} (N baked in at generation time,
 * NOT a single shared key) is compose_gallery()'s own PRECOMPUTED naked-
 * var injection (see build_command_list_rows()'s own header comment for
 * the real staleness bug this precompute-at-gallery-time pattern fixes),
 * populated from the page's own real event.ir.pdl NODE rows plus a
 * trailing "[+] Add Command" row that hrefs to this page's own generated
 * Command Picker screen. */
static void write_page_layout(const char *layouts_dir, int n) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/event_ez_page_%d.chtpm", layouts_dir, n);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
        "<panel time_reactive=\"true\">\n"
        "    <module>system/prisc+x pal/main_loop_chtpm.pal</module>\n"
        "    <interact src=\"pieces/apps/player_app/interact_relay.txt\" />\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"| EVENT PAGE %d - ${pkg_name}               |\" /><br/>\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"When does this page run? (RMMV Trigger)\" /><br/>\n"
        "    <cli_io id=\"ez_trigger\" label=\"Trigger: on_click / on_spawn / parallel\" target_id=\"ez_trigger\" /><br/>\n"
        "    <button label=\"Save Trigger\" onClick=\"KEY:5\" /><br/>\n"
        "    <text label=\"------------------------------------------\" /><br/>\n"
        "    <text label=\"-- EVENT COMMANDS (runs top to bottom) --\" /><br/>\n"
        "    <text label=\"Pick New Event Command to add a line.\" /><br/>\n"
        "    ${command_list_rows_%d}\n"
        "    <button label=\"Clear All Commands on This Page\" onClick=\"KEY:7\" /><br/>\n"
        "    <text label=\"------------------------------------------\" /><br/>\n"
        "    <button label=\"< Back to Event Pages\" href=\"pieces/chtpm/layouts/event_ez.chtpm\" /><br/>\n"
        "    <text label=\"${last_message}\" /><br/>\n"
        "</panel>\n",
        n, n);
    fclose(f);
}

/* Real, shared Command Picker screen for page N - lists real command
 * TYPES this house's own event.pal actually supports (Change Gold,
 * Show Text - each has a real op binary and a real compile branch; Show
 * Choices is a real, planned follow-up, not listed yet so there's no
 * dead/non-functional button). Picking one navigates straight to that
 * type's own Parameter screen for THIS page (plain href, page number
 * carried in the target filename, same trick every other screen here
 * already uses). */
static void write_cmdpick_layout(const char *layouts_dir, int n) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/event_ez_page_%d_cmdpick.chtpm", layouts_dir, n);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
        "<panel time_reactive=\"true\">\n"
        "    <module>system/prisc+x pal/main_loop_chtpm.pal</module>\n"
        "    <interact src=\"pieces/apps/player_app/interact_relay.txt\" />\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"| NEW EVENT COMMAND (page %d)              |\" /><br/>\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"Choose a command type (RMMV-style list).\" /><br/>\n"
        "    <text label=\"Only working types are listed.\" /><br/>\n"
        "    <button label=\"Show Text...\" href=\"pieces/chtpm/layouts/event_ez_page_%d_cmd_show_text.chtpm\" /><br/>\n"
        "    <button label=\"Change Gold...\" href=\"pieces/chtpm/layouts/event_ez_page_%d_cmd_change_gold.chtpm\" /><br/>\n"
        "    <button label=\"< Back\" href=\"pieces/chtpm/layouts/event_ez_page_%d.chtpm\" /><br/>\n"
        "</panel>\n",
        n, n, n, n);
    fclose(f);
}

/* Real Show Text parameter screen for page N - literal message text plus
 * optional speaker name. Save is KEY:8 (see ez_menu_input.c): appends a
 * type=show_text NODE row to event.ir.pdl, recompiles event.pal fresh,
 * materializes msg_<id>.txt (speaker line first, word-wrapped body) and
 * a cmd_<id>.sh wrapper that hands the msg file to the shared
 * mr_show_text.+x op. */
static void write_cmd_show_text_layout(const char *layouts_dir, int n) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/event_ez_page_%d_cmd_show_text.chtpm", layouts_dir, n);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
        "<panel time_reactive=\"true\">\n"
        "    <module>system/prisc+x pal/main_loop_chtpm.pal</module>\n"
        "    <interact src=\"pieces/apps/player_app/interact_relay.txt\" />\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"| Show Text (page %d)                      |\" /><br/>\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"Show a message box to the player when\" /><br/>\n"
        "    <text label=\"this event page runs.\" /><br/>\n"
        "    <text label=\"HOW TO: Enter on field, type message,\" /><br/>\n"
        "    <text label=\"Esc when done, then OK to save.\" /><br/>\n"
        "    <cli_io id=\"ez_st_text\" label=\"Message text\" target_id=\"ez_st_text\" /><br/>\n"
        "    <cli_io id=\"ez_st_speaker\" label=\"Speaker name (optional)\" target_id=\"ez_st_speaker\" /><br/>\n"
        "    <button label=\"OK — Save Command\" onClick=\"KEY:8\" /><br/>\n"
        "    <button label=\"< Back\" href=\"pieces/chtpm/layouts/event_ez_page_%d.chtpm\" /><br/>\n"
        "    <text label=\"${last_message}\" /><br/>\n"
        "</panel>\n",
        n, n);
    fclose(f);
}

/* Real Change Gold parameter screen for page N - one real cli_io field
 * (signed amount), Save compiles a real new command onto this page's own
 * event.ir.pdl/event.pal (see ez_menu_input.c's own KEY:6) and returns to
 * the Page screen with the new command row visible + a fresh "[+] Add
 * Command" slot below it - same "always one more empty slot" growth
 * pattern the page list itself already uses. */
static void write_cmd_change_gold_layout(const char *layouts_dir, int n) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/event_ez_page_%d_cmd_change_gold.chtpm", layouts_dir, n);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
        "<panel time_reactive=\"true\">\n"
        "    <module>system/prisc+x pal/main_loop_chtpm.pal</module>\n"
        "    <interact src=\"pieces/apps/player_app/interact_relay.txt\" />\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"| Change Gold (page %d)                    |\" /><br/>\n"
        "    <text label=\"+==========================================+\" /><br/>\n"
        "    <text label=\"Add gold to this character (use -N to spend).\" /><br/>\n"
        "    <text label=\"HOW TO: Enter on field, type number, Esc,\" /><br/>\n"
        "    <text label=\"then OK to save the command.\" /><br/>\n"
        "    <cli_io id=\"ez_cg_amount\" label=\"Gold amount (e.g. 25 or -5)\" target_id=\"ez_cg_amount\" /><br/>\n"
        "    <button label=\"OK — Save Command\" onClick=\"KEY:6\" /><br/>\n"
        "    <button label=\"< Back\" href=\"pieces/chtpm/layouts/event_ez_page_%d.chtpm\" /><br/>\n"
        "    <text label=\"${last_message}\" /><br/>\n"
        "</panel>\n",
        n, n);
    fclose(f);
}

/* Sanitize a dynamically-injected button label: no double-quotes (would
 * break the injected attribute string), no embedded newlines. */
static void sanitize(const char *in, char *out, size_t n) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < n; i++) {
        char c = in[i];
        if (c == '"') c = '\'';
        if (c == '\n' || c == '\r') c = ' ';
        out[j++] = c;
    }
    out[j] = '\0';
}

/* REAL FIX 2026-08-06, user CPU throttle ("toolbar/muchi/events >30fps"):
 * This used to ALWAYS append to BOTH frame_changed.txt AND
 * ez_screen_changed.txt on every compose. main_loop_chtpm.pal only
 * re-runs ez_compose_frame when ez_screen_changed grows — so each
 * compose re-armed the next compose → ~25–33 full recomposes/sec forever
 * (live-measured), which kept chtpm_rgb_render (~40% CPU) + gl_mirror
 * (~20%) + parser (~10%) pinned hot even while idle.
 *
 * Contract now:
 *   - ez_screen_changed: owned ONLY by ez_menu_input bump() (real
 *     KEY side-effects). Compose never touches it.
 *   - frame_changed: only when view.txt content actually changed, so
 *     rgb_render/parser don't re-blit an identical frame. */
static void ping_frame_if_view_changed(const char *view_path) {
    static char last_fp[64] = "";
    char fp[64] = "empty";
    FILE *vf = fopen(view_path, "r");
    if (vf) {
        /* Cheap stable fingerprint: size + FNV-ish over first 2KB. */
        unsigned long long h = 14695981039346656037ULL;
        char buf[2048];
        size_t n = fread(buf, 1, sizeof(buf), vf);
        long sz = 0;
        if (fseek(vf, 0, SEEK_END) == 0) sz = ftell(vf);
        fclose(vf);
        for (size_t i = 0; i < n; i++) {
            h ^= (unsigned char)buf[i];
            h *= 1099511628211ULL;
        }
        snprintf(fp, sizeof(fp), "%ld:%llx", sz, (unsigned long long)h);
    }
    if (strcmp(fp, last_fp) == 0) return;
    snprintf(last_fp, sizeof(last_fp), "%s", fp);

    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

/* Real, on-disk page count: how many pages/page_N/ dirs actually exist. */
static int count_real_pages(const char *pkg_dir) {
    if (!pkg_dir[0]) return 0;
    int n = 1;
    char path[PATH_BUF];
    while (1) {
        snprintf(path, sizeof(path), "%s/pages/page_%d", pkg_dir, n);
        struct stat st;
        if (stat(path, &st) != 0) break;
        n++;
    }
    return n - 1;
}

/* REAL, 2026-08-05: reads this page's own real event.ir.pdl NODE rows
 * (type=change_gold today, type=show_choices once that pass lands) and
 * renders each as a real, plain <text> row (commands aren't clickable
 * once saved - editing an existing command isn't in scope for this
 * pass, only appending new ones), then a real "[+] Add Command" button
 * hrefing to this page's own Command Picker screen - same "always one
 * more empty slot at the end" growth pattern the page list itself uses.
 *
 * REAL FIX 2026-08-05, root-caused via live k3 testing (a fresh, empty
 * page's own "[+] Add Command" button never appeared on first visit):
 * chtpm_parser_pal's own ${var} naked-substitution only re-runs at the
 * EXACT instant of an href-commit (parse_chtm(), called synchronously
 * inside that SAME process) - but ez_compose_frame.+x (a SEPARATE
 * process, only invoked by the PAL script's own loop) computes this
 * page's real command rows slightly AFTER that instant, so the value
 * gui_state.txt held AT COMMIT TIME was still stale/empty from
 * whichever screen the player was on before. No later render re-
 * triggers parse_chtm() just because gui_state.txt changed (confirmed
 * via source - only active_target_id/piece_methods/input_text changes,
 * or wait_for_view_change, force a reparse). Same real fix already used
 * for the .chtpm FILES themselves (pre-generate every reachable page's
 * own file at Gallery-compose time, before it's ever clicked): this
 * function is now ALSO called once per real page, AT GALLERY-COMPOSE
 * TIME, writing a per-page-NUMBERED var (command_list_rows_N, not a
 * single shared key) - so by the time a page is actually clicked into,
 * gui_state.txt already holds ITS OWN correct value from the last
 * Gallery visit (guaranteed to have happened - there's no way to reach
 * a page except through the Gallery first). */
static void build_command_list_rows(const char *pkg_dir, int page_n, char *rows_out, size_t rows_out_sz) {
    rows_out[0] = '\0';
    size_t used = 0;
    char ir_path[PATH_BUF];
    snprintf(ir_path, sizeof(ir_path), "%s/pages/page_%d/event.ir.pdl", pkg_dir, page_n);
    FILE *irf = fopen(ir_path, "r");
    int shown = 0;
    if (irf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), irf)) {
            char *tp = strstr(line, "type=");
            if (strncmp(line, "NODE", 4) != 0 || !tp) continue;
            char type_buf[48] = "";
            char *t = tp + 5, *sp = strchr(t, ' ');
            char *pipe = strchr(t, '|');
            size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
            if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
            memcpy(type_buf, t, len);
            type_buf[len] = '\0';
            if (strcmp(type_buf, "change_gold") == 0) {
                char *ap = strstr(line, "amount=");
                char amount[32] = "";
                if (ap) {
                    snprintf(amount, sizeof(amount), "%s", ap + 7);
                    amount[strcspn(amount, "\r\n|")] = '\0';
                }
                shown++;
                char label[96], clean[96];
                snprintf(label, sizeof(label), "• Change Gold: %s", amount[0] ? amount : "?");
                sanitize(label, clean, sizeof(clean));
                int wrote = snprintf(rows_out + used, rows_out_sz - used,
                                      "<text label=\"%s\" /><br/>", clean);
                if (wrote < 0 || (size_t)wrote >= rows_out_sz - used) break;
                used += (size_t)wrote;
            } else if (strcmp(type_buf, "show_text") == 0) {
                /* REAL FIX 2026-08-24: type=show_text NODE rows existed in
                 * IR but were silently skipped here, so a saved Show Text
                 * never appeared in the page's own command list. Render
                 * first-line preview like Change Gold's row does. */
                char *xp = strstr(line, "text=");
                char txt[256] = "";
                if (xp) {
                    snprintf(txt, sizeof(txt), "%s", xp + 5);
                    txt[strcspn(txt, "\r\n|")] = '\0';
                }
                /* collapse escaped newlines for the one-line preview */
                for (char *q = txt; *q; q++) {
                    if (q[0] == '\\' && q[1] == 'n') { q[0] = ' '; q[1] = ' '; }
                }
                shown++;
                char label[128], clean[128];
                snprintf(label, sizeof(label), "• Show Text: %.44s%s",
                         txt, strlen(txt) > 44 ? "…" : "");
                sanitize(label, clean, sizeof(clean));
                int wrote = snprintf(rows_out + used, rows_out_sz - used,
                                      "<text label=\"%s\" /><br/>", clean);
                if (wrote < 0 || (size_t)wrote >= rows_out_sz - used) break;
                used += (size_t)wrote;
            } else {
                continue; /* trigger/ret bookkeeping rows, skip */
            }
        }
        fclose(irf);
    }
    int wrote = snprintf(rows_out + used, rows_out_sz - used,
                          "<button label=\"New Event Command...\" href=\"pieces/chtpm/layouts/event_ez_page_%d_cmdpick.chtpm\" /><br/>",
                          page_n);
    if (wrote > 0 && (size_t)wrote < rows_out_sz - used) used += (size_t)wrote;
}

static void compose_gallery(const char *state, const char *view, const char *gui,
                             const char *pkg, const char *pkg_dir, const char *msg,
                             char *rows_out, size_t rows_out_sz,
                             char *cmd_rows_kv_out, size_t cmd_rows_kv_out_sz) {
    /* Real page-editor files must exist BEFORE the gallery can href to
     * them - generate every real page's own file plus one trailing
     * blank slot ("always one more empty page," RPG Maker's own
     * convention) fresh every compose. Cheap, idempotent. Also
     * PRECOMPUTES every real page's own command_list_rows_N value here
     * (see build_command_list_rows()'s own header comment for why) -
     * written into gui_state.txt by main() below. */
    char layouts_dir[PATH_BUF];
    snprintf(layouts_dir, sizeof(layouts_dir), "%s/pieces/chtpm/layouts", project_root);
    int n_real = count_real_pages(pkg_dir);
    cmd_rows_kv_out[0] = '\0';
    size_t kv_used = 0;
    for (int i = 1; i <= n_real + 1; i++) {
        write_page_layout(layouts_dir, i);
        write_cmdpick_layout(layouts_dir, i);
        write_cmd_change_gold_layout(layouts_dir, i);
        /* REAL, 2026-08-24: generate the Show Text parameter screen too -
         * previously only its stale hand-made page_1 file existed on
         * disk and the picker never linked to it. */
        write_cmd_show_text_layout(layouts_dir, i);
        char page_cmd_rows[4096];
        build_command_list_rows(pkg_dir, i, page_cmd_rows, sizeof(page_cmd_rows));
        int kv_wrote = snprintf(cmd_rows_kv_out + kv_used, cmd_rows_kv_out_sz - kv_used,
                                 "command_list_rows_%d=%s\n", i, page_cmd_rows);
        if (kv_wrote < 0 || (size_t)kv_wrote >= cmd_rows_kv_out_sz - kv_used) break;
        kv_used += (size_t)kv_wrote;
    }

    /* Real dynamic button injection, SAME proven mechanism the real
     * CHTPM editor's own ${event_content_rows} already uses - a bare
     * ${var} placeholder (not inside a <text> tag) gets substituted
     * with raw markup BEFORE tokenization (parse_chtm()'s own
     * substitute_vars_naked(), confirmed via source), so injected
     * <button href="..."> tags parse as REAL new elements. Using href
     * (a plain literal path, no prefix-matching) rather than onClick
     * here - onClick strings other than a few recognized prefixes are
     * silently rejected by send_command(), a real latent bug already
     * found this session; href has no such restriction. */
    rows_out[0] = '\0';
    size_t used = 0;
    for (int i = 1; i <= n_real + 1; i++) {
        char label[80];
        /* REAL FIX 2026-08-05, root-caused via live k3 testing + a real
         * instrumented chtpm_parser_pal trace (see
         * visual-event-compiler-pal.md): this used to bake its OWN
         * "N. [...]" numbering into the label text - but
         * chtpm_parser_pal's own render_element() ALREADY prepends a
         * real "[ ]"/"[>]" cursor + real auto-numbered "N." for every
         * navigable button automatically. The result was double,
         * nested numbering ("[ ] 1. [1. [on-click]]") - confirmed via
         * live trace this was purely cosmetic (do_jump()/href commit
         * both traced CORRECTLY against the real element every time),
         * not an actual navigation-targeting bug. Labels now carry only
         * the real content - chtpm's own real nav chrome supplies the
         * number and bracket. */
        if (i <= n_real) {
            char cond_path[PATH_BUF], ir_path[PATH_BUF], trig[64] = "";
            int n_cmds = 0;
            snprintf(cond_path, sizeof(cond_path), "%s/pages/page_%d/condition.pdl", pkg_dir, i);
            read_pdl_value(cond_path, "trigger", trig, sizeof(trig));
            snprintf(ir_path, sizeof(ir_path), "%s/pages/page_%d/event.ir.pdl", pkg_dir, i);
            {
                FILE *irf = fopen(ir_path, "r");
                if (irf) {
                    char ln[MAX_LINE];
                    while (fgets(ln, sizeof(ln), irf)) {
                        if (strncmp(ln, "NODE", 4) == 0 && strstr(ln, "type=change_gold")) n_cmds++;
                    }
                    fclose(irf);
                }
            }
            if (!trig[0]) snprintf(trig, sizeof(trig), "on_click");
            /* RMMV-ish: Page N — When: Action Button / n commands */
            snprintf(label, sizeof(label), "Page %d - When: %s (%d cmd)", i, trig, n_cmds);
        } else {
            snprintf(label, sizeof(label), "Page %d - (new empty page)", i);
        }
        char clean[80]; sanitize(label, clean, sizeof(clean));
        int wrote = snprintf(rows_out + used, rows_out_sz - used,
                              "<button label=\"%s\" href=\"pieces/chtpm/layouts/event_ez_page_%d.chtpm\" /><br/>",
                              clean, i);
        if (wrote < 0 || (size_t)wrote >= rows_out_sz - used) break;
        used += (size_t)wrote;
    }

    FILE *o = fopen(view, "w");
    if (!o) return;
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| EVENT EDITOR — %-22.22s|\n", pkg);
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| -- EVENT PAGES (open one to edit) --       |\n");
    for (int i = 1; i <= n_real; i++) {
        char cond_path[PATH_BUF], ir_path[PATH_BUF], trig[64] = "";
        snprintf(cond_path, sizeof(cond_path), "%s/pages/page_%d/condition.pdl", pkg_dir, i);
        snprintf(ir_path, sizeof(ir_path), "%s/pages/page_%d/event.ir.pdl", pkg_dir, i);
        read_pdl_value(cond_path, "trigger", trig, sizeof(trig));
        char summary[64] = "(empty)";
        FILE *irf = fopen(ir_path, "r");
        if (irf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), irf)) {
                if (strncmp(line, "NODE", 4) != 0) continue;
                char *txp = strstr(line, "text=");
                if (!txp) continue;
                char *v = txp + 5;
                v[strcspn(v, "\r\n")] = '\0';
                snprintf(summary, sizeof(summary), "%s", v);
                break;
            }
            fclose(irf);
        }
        char row[96];
        snprintf(row, sizeof(row), "%d. [%s] %.30s", i, trig[0] ? trig : "?", summary);
        fprintf(o, "| %-42.42s|\n", row);
    }
    {
        char row[64];
        snprintf(row, sizeof(row), "%d. [ ] empty  <- click to add", n_real + 1);
        fprintf(o, "| %-42.42s|\n", row);
    }
    fprintf(o, "+==========================================+\n");
    if (msg[0]) fprintf(o, "| %-42.42s|\n", msg);
    else fprintf(o, "| Click a page number to open/create it    |\n");
    fprintf(o, "+==========================================+\n");
    fclose(o);
    (void)state; (void)gui;
}

static void compose_page(const char *state, const char *view, const char *gui,
                          const char *pkg, const char *pkg_dir, const char *msg, int page_n,
                          char *cmd_rows_out, size_t cmd_rows_out_sz) {
    char saved_trigger[64] = "";
    char cond_path[PATH_BUF], ir_path[PATH_BUF];
    int have_saved_page = 0;
    if (pkg_dir[0]) {
        snprintf(cond_path, sizeof(cond_path), "%s/pages/page_%d/condition.pdl", pkg_dir, page_n);
        snprintf(ir_path, sizeof(ir_path), "%s/pages/page_%d/event.ir.pdl", pkg_dir, page_n);
        struct stat st;
        if (stat(ir_path, &st) == 0) {
            have_saved_page = 1;
            read_pdl_value(cond_path, "trigger", saved_trigger, sizeof(saved_trigger));
        }
    }
    build_command_list_rows(pkg_dir, page_n, cmd_rows_out, cmd_rows_out_sz);

    FILE *o = fopen(view, "w");
    if (!o) return;
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| EVENT-EZ  pkg=%-13.13s page=%-8d|\n", pkg, page_n);
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| trigger=%-33.33s|\n", have_saved_page && saved_trigger[0] ? saved_trigger : "(unset - Save Trigger)");
    fprintf(o, "+==========================================+\n");
    fprintf(o, "| -- COMMANDS (what Play runs) --            |\n");
    fprintf(o, "| %-42.42s|\n", cmd_rows_out[0] ? "(see real page for live list)" : "(none yet - Add Command)");
    fprintf(o, "+==========================================+\n");
    if (msg[0]) fprintf(o, "| %-42.42s|\n", msg);
    else fprintf(o, "| fill Trigger + Save, or Add a Command     |\n");
    fprintf(o, "+==========================================+\n");
    fclose(o);
    (void)gui;
}

int main(void) {
    resolve_root();

    char state[PATH_BUF], view[PATH_BUF], gui[PATH_BUF];
    snprintf(state, sizeof(state), "%s/pieces/system/ez_state.txt", project_root);
    snprintf(view, sizeof(view), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(gui, sizeof(gui), "%s/projects/event-ez/manager/gui_state.txt", project_root);

    char pkg[128], msg[MAX_LINE], pkg_dir[PATH_BUF];
    read_kv(state, "pkg_name", pkg, sizeof(pkg));
    read_kv(state, "last_message", msg, sizeof(msg));
    read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
    if (!pkg[0]) snprintf(pkg, sizeof(pkg), "(none)");

    char page_gallery_rows[4096] = "";
    char command_list_rows[4096] = "";
    char command_list_rows_kv[32768] = "";
    int page_n = 0;
    ScreenKind kind = current_screen_kind(&page_n);

    /* REAL FIX 2026-08-06 (user: new command not listed after Save):
     * ALWAYS recompute page_gallery_rows + command_list_rows_N from disk.
     * Cheap, and guarantees:
     *  - Back still has a real page list (not wiped)
     *  - After Change Gold Save, page list shows the new NODE immediately
     *    even while still on the param/page screen (not only after
     *    revisiting the Gallery). */
    {
        char dummy_view[PATH_BUF];
        snprintf(dummy_view, sizeof(dummy_view), "%s/pieces/apps/player_app/view_gallery_blob.txt", project_root);
        compose_gallery(state, dummy_view, gui, pkg, pkg_dir, msg, page_gallery_rows, sizeof(page_gallery_rows),
                         command_list_rows_kv, sizeof(command_list_rows_kv));
    }

    if (kind == SCREEN_PAGE) {
        compose_page(state, view, gui, pkg, pkg_dir, msg, page_n, command_list_rows, sizeof(command_list_rows));
    } else if (kind == SCREEN_GALLERY) {
        /* Gallery chrome goes to the real view.txt (compose_gallery
         * already wrote dummy_view; write the same content to view). */
        char src[PATH_BUF], cmd[PATH_BUF * 2];
        snprintf(src, sizeof(src), "%s/pieces/apps/player_app/view_gallery_blob.txt", project_root);
        snprintf(cmd, sizeof(cmd), "cp -f '%s' '%s' 2>/dev/null", src, view);
        if (system(cmd) != 0) {
            /* fallback: recompose directly into view */
            compose_gallery(state, view, gui, pkg, pkg_dir, msg, page_gallery_rows, sizeof(page_gallery_rows),
                             command_list_rows_kv, sizeof(command_list_rows_kv));
        }
    }
    /* CMDPICK / CHANGE_GOLD: static .chtpm; gui blobs still refreshed above. */

    {
        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s/projects/event-ez/manager'", project_root);
        if (system(cmd) != 0) { /* best-effort, dir likely already exists */ }
    }

    /* Read gui_state, drop our managed keys, rewrite atomically. */
    char keep[64][MAX_LINE];
    int n_keep = 0;
    FILE *rf = fopen(gui, "r");
    if (rf) {
        char line[MAX_LINE];
        while (n_keep < 64 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, "pkg_name=", 9) == 0) continue;
            if (strncmp(line, "last_message=", 13) == 0) continue;
            if (strncmp(line, "page_gallery_rows=", 18) == 0) continue;
            if (strncmp(line, "command_list_rows=", 18) == 0) continue;
            /* REAL FIX: was strncmp(..., 19) which NEVER matched
             * "command_list_rows_N=" (18-char prefix) → unbounded dupes. */
            if (strncmp(line, "command_list_rows_", 18) == 0) continue;
            snprintf(keep[n_keep], MAX_LINE, "%s", line);
            n_keep++;
        }
        fclose(rf);
    }
    /* REAL FIX 2026-08-05, root-caused via live k3 reproduction + a
     * real instrumented chtpm_parser_pal trace (see
     * visual-event-compiler-pal.md / EVENT_SCRIPTING_PROGRESS_AND_GOALS.md
     * "KNOWN BUG"): this used to fopen(gui, "w") directly - a plain
     * truncate-in-place write. chtpm_parser_pal's own load_vars() reads
     * this EXACT file from a SEPARATE, concurrently-running process on
     * every parse_chtm() (this PAL loop re-runs ez_compose_frame every
     * tick that processed a key - main_loop_chtpm.pal's own
     * render_always path). A torn read (parser catching this file mid-
     * truncate, after the old content was wiped but before the new
     * page_gallery_rows= line was written) is EXACTLY what produced the
     * live-observed symptom: page rows sometimes duplicated, sometimes
     * missing entirely, no crash - a classic non-atomic-write race, not
     * a parsing/counting bug in do_jump() itself (which traced correctly
     * every single time once fed a consistent file). Write to a real
     * temp file, then atomically rename() over the real path - same
     * fix shape this house's own livedesk registry files already use. */
    char gui_tmp[PATH_BUF];
    snprintf(gui_tmp, sizeof(gui_tmp), "%s.tmp", gui);
    FILE *g = fopen(gui_tmp, "w");
    if (!g) return 1;
    for (int i = 0; i < n_keep; i++) fputs(keep[i], g);
    fprintf(g, "pkg_name=%s\n", pkg);
    fprintf(g, "last_message=%s\n", msg[0] ? msg : "");
    fprintf(g, "page_gallery_rows=%s\n", page_gallery_rows);
    /* REAL FIX, same crash-interrupted pass: this used to write a single
     * "command_list_rows=" key (page_n() bug's own leftover) - the real
     * fix needs the per-page-NUMBERED blob compose_gallery() precomputes
     * (command_list_rows_kv, one "command_list_rows_N=..." line per real
     * page), not a single shared key. The plain "command_list_rows"
     * (still computed by compose_page() for view.txt) is written too,
     * harmless/unused by any real .chtpm now but kept so view.txt's own
     * informational summary still works. */
    fputs(command_list_rows_kv, g);
    fprintf(g, "command_list_rows=%s\n", command_list_rows);
    fclose(g);
    rename(gui_tmp, gui);

    ping_frame_if_view_changed(view);

    /* Session frame history: ONLY when layout or frame TEXT changes
     * (not every 30ms tick — that produced 30k-line logs where one
     * "Saved..." message looked like thousands of saves). */
    {
        char hist_dir[PATH_BUF], hist_path[PATH_BUF], frame_path[PATH_BUF], layout_path[PATH_BUF];
        char last_path[PATH_BUF];
        snprintf(hist_dir, sizeof(hist_dir), "%s/pieces/debug/frames", project_root);
        snprintf(hist_path, sizeof(hist_path), "%s/session_frame_history.txt", hist_dir);
        {
            FILE *szf = fopen(hist_path, "r");
            if (szf) {
                if (fseek(szf, 0, SEEK_END) == 0) {
                    long sz = ftell(szf);
                    if (sz > 400000L) { /* ~400KB cap — crash risk if unbounded */
                        fclose(szf);
                        szf = NULL;
                        FILE *wf = fopen(hist_path, "w");
                        if (wf) {
                            fprintf(wf, "(session_frame_history truncated — was >400KB)\n");
                            fclose(wf);
                        }
                    }
                }
                if (szf) fclose(szf);
            }
        }

        snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);
        snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
        snprintf(last_path, sizeof(last_path), "%s/pieces/debug/frames/last_logged_frame.txt", project_root);
        char mk[PATH_BUF];
        snprintf(mk, sizeof(mk), "mkdir -p '%s'", hist_dir);
        if (system(mk) == 0) {
            FILE *ff = fopen(frame_path, "r");
            if (ff) {
                char frame_buf[8192];
                size_t n = fread(frame_buf, 1, sizeof(frame_buf) - 1, ff);
                fclose(ff);
                frame_buf[n] = '\0';
                char layout_buf[PATH_BUF] = "";
                FILE *lf = fopen(layout_path, "r");
                if (lf) {
                    if (fgets(layout_buf, sizeof(layout_buf), lf))
                        layout_buf[strcspn(layout_buf, "\r\n")] = '\0';
                    fclose(lf);
                }
                char fingerprint[8700];
                snprintf(fingerprint, sizeof(fingerprint), "%s\n%s", layout_buf, frame_buf);
                char prev[8700] = "";
                FILE *pf = fopen(last_path, "r");
                if (pf) {
                    size_t pn = fread(prev, 1, sizeof(prev) - 1, pf);
                    prev[pn] = '\0';
                    fclose(pf);
                }
                if (strcmp(prev, fingerprint) != 0) {
                    FILE *wf = fopen(last_path, "w");
                    if (wf) { fputs(fingerprint, wf); fclose(wf); }
                    FILE *hf = fopen(hist_path, "a");
                    if (hf) {
                        time_t now = time(NULL);
                        struct tm tm;
                        localtime_r(&now, &tm);
                        fprintf(hf, "\n===== %04d-%02d-%02d %02d:%02d:%02d layout=%s =====\n",
                                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                tm.tm_hour, tm.tm_min, tm.tm_sec, layout_buf);
                        fputs(frame_buf, hf);
                        fclose(hf);
                    }
                }
            }
        }
    }
    return 0;
}
