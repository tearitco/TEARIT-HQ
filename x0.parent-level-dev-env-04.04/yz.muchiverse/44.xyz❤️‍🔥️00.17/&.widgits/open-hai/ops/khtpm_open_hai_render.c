/* khtpm_open_hai_render.c — the taskbar's cell 14 ("ai") real window.
 * Design doc: au11-hq/OPEN-HAI-GUI-DESIGN.md (read that FIRST — this
 * file implements its §3/§4/§5/§7/§9 decisions, doesn't re-derive
 * them).
 *
 * v1 scope (direct instruction, 2026-08-12): raw Ollama HTTP as the
 * MAIN backend (10.0.0.144:11434, see #.Z.HUMAN_LLM/.MAC-ACCESS.txt),
 * agent-45 relay kept as a documented-but-not-yet-wired "legacy hook"
 * (see g_backend_mode below) rather than built out fully this pass.
 *
 * REAL FIX 2026-08-12, direct instruction ("lets make sure we have
 * transcript scrolling so we can audit history... i want a way i can
 * read previous historic chat with sidebar option or something (and
 * delete it if i want)"): added real disk-persisted chat sessions
 * (sidebar list, load/delete) and real transcript scrolling. Scroll
 * convention is PORTED, not invented - user's own correction: "it
 * just uses a [] up and [] down nav button to scroll view up or down
 * its nothing spectacular" - matches wraith-alpha's own fs scroll
 * pattern (1.TPMOS_c_+rmmp.0103.0001/.../wraith_project_input.c):
 * scroll_offset + a fixed visible-window size + nav-badge-numbered up/
 * down buttons, NOT tpmos's own separate joystick/GL-thumb scrollbar
 * (gl_desktop.c) - that one doesn't fit this file's plain X11/nav
 * shape at all, deliberately not used.
 *
 * Because the sidebar now grows with saved sessions, nav is a real
 * dynamic array (g_nav[]) built fresh each frame - same shape as db-hq/
 * events-hq's own Elem->nav_index assignment, adapted to this file's
 * flat (non-tag-tree) layout.
 *
 * Reuses every proven house pattern rather than reinventing:
 * managed window + _MOTIF_WM_HINTS (NOT override_redirect - the real
 * keyboard-focus fix, !.HOUSE_STDS.md #21), RGB compose->present
 * (XGetImage->XPutImage, same as db-hq/events-hq/entities), wraith-
 * alpha nav (bracket badges, digit-jump, HOUSE_STDS #22), Xft text
 * (no GL, HOUSE_STDS #24's non-fatal XSetErrorHandler).
 *
 * Layout is hand-computed x/y/w/h (khtpm_css_parser.c has no flex/grid
 * - same constraint db-hq/events-hq already work within; this file
 * doesn't use the CSS engine at all yet, see OPEN-HAI-GUI-DESIGN.md §9).
 *
 * Usage: khtpm_open_hai_render.+x <house_root>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define PATH_BUF 4096
#define WIN_W 1000
#define WIN_H 680
#define SIDEBAR_W 240
#define TOPBAR_H 44
#define COMPOSER_H 64
#define COMPOSER_MAX_LINES 6 /* grows the composer box as you type wrapped lines, up to this many visible at once before it auto-scrolls (keeps the cursor's line visible) instead of running text off the edge - direct report 2026-08-13: "text input i want it to wrap new line and user input can scroll up instead of dissapearing off the side of the screen" */
#define CHROME_H 28
#define MAX_MSGS 512
#define MSG_LEN 8192
#define MAX_SESSIONS 64
#define MAX_NAV 96
#define INPUT_BUF_LEN 4096
#define LINE_H 19 /* 18 + ~5% (readability - user: lines were "completely stacked") */
#define MAX_FLAT_LINES 4096

/* Audit artifacts live in the house tree (xyzfs, not /tmp) so a human can
 * audit every run: frames, receipts, payloads, responses, tool outputs,
 * runtime log. Same pattern as board-viewer writing receipts into its own
 * pieces/ dir. */
#define AUDIT_DIR_REL "&.widgits/open-hai/pieces/audit"
#define AUDIT_EMOJI_REL "&.widgits/open-hai/pieces/registry/emoji_assets"

static char g_house_root[PATH_BUF];
static char g_sessions_root[PATH_BUF];
static char g_audit_dir[PATH_BUF];
static char g_pid_path[PATH_BUF];
/* PER-INSTANCE DATA ROOT (2026-08-24, cursword chat): non-empty means
 * this instance runs redirected (sessions/state/audit/pid under
 * <data_root>) - launch_module() forwards it to the manager and
 * init_ipc_paths() roots its state files there. Empty = plain open-hai. */
static char g_data_root[PATH_BUF];
static char g_emoji_dir[PATH_BUF];
static int g_running = 1; /* global so the real nav-indexed close button (NAV_CLOSE) can set it from activate_focused() */
static unsigned g_frame = 0; /* redraw counter - drives the thinking animation */

/* ---------- non-fatal X error handler (HOUSE_STDS #24) ---------- */
static int nonfatal_x_error(Display *dpy, XErrorEvent *ev) {
    (void)dpy;
    fprintf(stderr, "open-hai: non-fatal X error, code=%d request=%d\n", ev->error_code, ev->request_code);
    return 0;
}

/* ---------- transcript (chat message log) - DISPLAY ONLY (Stage 2d
 * shell/manager split, 2026-08-16, same real mechanism proven on db-hq/
 * events-hq/chat-hai - see local-2do-15.txt's own open-hai entry). All
 * business logic that used to live here (session persistence, the real
 * Ollama call, tool detection+execution) moved to khtpm_open_hai_manager.c
 * near-verbatim. This shell now only READS what the manager publishes
 * and WRITES requests - it never touches transcript.txt/session dirs/
 * the network/tool execution directly anymore. ---------- */
typedef struct { int is_user; char text[MSG_LEN]; } ChatMsg;
static ChatMsg g_msgs[MAX_MSGS];
static int g_n_msgs = 0;

static char g_session_dir[PATH_BUF] = "";

typedef struct { char dir[PATH_BUF]; char label[80]; } SessionEntry;
static SessionEntry g_sessions[MAX_SESSIONS];
static int g_n_sessions = 0;

static void escape_line(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 2 < outsz; i++) {
        if (in[i] == '\\') { out[o++] = '\\'; out[o++] = '\\'; }
        else if (in[i] == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else out[o++] = in[i];
    }
    out[o] = '\0';
}

/* ---------- Stage 2c REAL PORT (2026-08-16, direct instruction "we need
 * to get it over with"): transcript.txt/sessions.state.txt now go
 * through REAL generated .chtpm markup + a real parse_chtpm(), not
 * ad-hoc line splitting - satisfies the "full port, express dynamic
 * content as generated .chtpm" choice for open-hai's DATA LOADING stage.
 * Round-trip verified standalone first (scratchpad test_chtpm_
 * transcript.c, 3 real session transcripts, zero mismatches) before
 * being wired in here. Scope boundary, deliberate: the actual draw
 * functions (draw_transcript/draw_sidebar/etc, real multi-line-wrap +
 * emoji-tile text layout) still consume the resulting g_msgs[]/
 * g_sessions[] arrays, not a live Elem tree - rewriting 128 functions'
 * worth of drawing code to walk an Elem tree is a separate, much larger
 * change than "make data loading real", not attempted here. Scalars
 * (active_session.txt, busy.state.txt - single values, not lists) stay
 * plain files - chtpm markup adds nothing for a single scalar. Own tiny
 * struct/parser (not khtpm_render_core.c's Elem - no CSS/style needed
 * for this internal data-loading step, avoids an unnecessary shared
 * dependency), same minimal shape as every other khtpm app's own
 * hand-rolled parser. */
#define IPC_MAX_ELEMS 600
#define IPC_MAX_CHILDREN 600
typedef struct IpcElem {
    char tag[16];
    char id[PATH_BUF];
    char label[MSG_LEN];
    struct IpcElem *children[IPC_MAX_CHILDREN];
    int n_children;
} IpcElem;
static IpcElem g_ipc_pool[IPC_MAX_ELEMS];
static int g_ipc_n = 0;
static IpcElem *ipc_elem_new(const char *tag) {
    if (g_ipc_n >= IPC_MAX_ELEMS) return NULL;
    IpcElem *e = &g_ipc_pool[g_ipc_n++];
    memset(e, 0, sizeof(*e));
    snprintf(e->tag, sizeof(e->tag), "%s", tag);
    return e;
}
static void ipc_skip_ws(const char **p) { while (**p && isspace((unsigned char)**p)) (*p)++; }
static void ipc_attr_value(const char **p, char *out, size_t outsz) {
    ipc_skip_ws(p);
    if (**p != '"') { out[0] = '\0'; return; }
    (*p)++;
    size_t n = 0;
    while (**p && **p != '"') { if (n + 1 < outsz) out[n++] = **p; (*p)++; }
    out[n] = '\0';
    if (**p == '"') (*p)++;
}
static void ipc_decode_xml(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (strncmp(r, "&quot;", 6) == 0) { *w++ = '"'; r += 6; }
        else if (strncmp(r, "&lt;", 4) == 0) { *w++ = '<'; r += 4; }
        else if (strncmp(r, "&gt;", 4) == 0) { *w++ = '>'; r += 4; }
        else if (strncmp(r, "&#10;", 5) == 0) { *w++ = '\n'; r += 5; }
        else if (strncmp(r, "&amp;", 5) == 0) { *w++ = '&'; r += 5; }
        else *w++ = *r++;
    }
    *w = '\0';
}
static void ipc_apply_attr(IpcElem *e, const char *name, const char *val) {
    if (strcmp(name, "role") == 0 || strcmp(name, "dir") == 0) {
        char decoded[PATH_BUF]; snprintf(decoded, sizeof(decoded), "%s", val);
        ipc_decode_xml(decoded);
        snprintf(e->id, sizeof(e->id), "%s", decoded);
    } else if (strcmp(name, "text") == 0 || strcmp(name, "label") == 0) {
        char decoded[MSG_LEN]; snprintf(decoded, sizeof(decoded), "%s", val);
        ipc_decode_xml(decoded);
        snprintf(e->label, sizeof(e->label), "%s", decoded);
    }
}
static const char *ipc_parse_element(const char *p, IpcElem *parent) {
    ipc_skip_ws(&p);
    if (*p != '<') return p;
    p++;
    char tag[16]; size_t tn = 0;
    while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '/') { if (tn + 1 < sizeof(tag)) tag[tn++] = *p; p++; }
    tag[tn] = '\0';
    IpcElem *e = ipc_elem_new(tag);
    if (!e) return p + strlen(p);
    if (parent && parent->n_children < IPC_MAX_CHILDREN) parent->children[parent->n_children++] = e;
    for (;;) {
        ipc_skip_ws(&p);
        if (*p == '/' && p[1] == '>') { p += 2; return p; }
        if (*p == '>') { p++; break; }
        if (!*p) return p;
        char attr[16]; size_t an = 0;
        while (*p && *p != '=' && !isspace((unsigned char)*p) && *p != '>' && *p != '/') { if (an + 1 < sizeof(attr)) attr[an++] = *p; p++; }
        attr[an] = '\0';
        ipc_skip_ws(&p);
        static char val[MSG_LEN];
        val[0] = '\0';
        if (*p == '=') { p++; ipc_attr_value(&p, val, sizeof(val)); }
        if (attr[0]) ipc_apply_attr(e, attr, val);
    }
    for (;;) {
        ipc_skip_ws(&p);
        if (!*p) return p;
        if (p[0] == '<' && p[1] == '/') { const char *end = strchr(p, '>'); return end ? end + 1 : p + strlen(p); }
        p = ipc_parse_element(p, e);
    }
}
static IpcElem *ipc_parse_chtpm_string(const char *buf) {
    g_ipc_n = 0;
    const char *p = buf;
    IpcElem *root = NULL;
    while (*p) {
        ipc_skip_ws(&p);
        if (!*p) break;
        if (*p != '<') break;
        if (!root) { root = ipc_elem_new("__root"); p = ipc_parse_element(p, root); }
        else p = ipc_parse_element(p, root);
    }
    if (root && root->n_children > 0) return root->children[0];
    return root;
}
static void ipc_escape_xml(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 6 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '&') { memcpy(out + o, "&amp;", 5); o += 5; }
        else if (c == '<') { memcpy(out + o, "&lt;", 4); o += 4; }
        else if (c == '>') { memcpy(out + o, "&gt;", 4); o += 4; }
        else if (c == '"') { memcpy(out + o, "&quot;", 6); o += 6; }
        else if (c == '\n') { memcpy(out + o, "&#10;", 5); o += 5; }
        else out[o++] = (char)c;
    }
    out[o] = '\0';
}

static void unescape_line(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < outsz; i++) {
        if (in[i] == '\\' && in[i + 1] == 'n') { out[o++] = '\n'; i++; }
        else if (in[i] == '\\' && in[i + 1] == '\\') { out[o++] = '\\'; i++; }
        else out[o++] = in[i];
    }
    out[o] = '\0';
}

static int g_scroll_follow_bottom = 1;

static void add_msg(int is_user, const char *text) {
    if (g_n_msgs >= MAX_MSGS) {
        memmove(&g_msgs[0], &g_msgs[1], sizeof(ChatMsg) * (MAX_MSGS - 1));
        g_n_msgs = MAX_MSGS - 1;
    }
    g_msgs[g_n_msgs].is_user = is_user;
    snprintf(g_msgs[g_n_msgs].text, sizeof(g_msgs[g_n_msgs].text), "%s", text);
    g_n_msgs++;
}

/* ---------- Stage 2d IPC: request.txt (write) + state files (read) ---------- */
static char g_state_dir[PATH_BUF];
static char g_request_path[PATH_BUF];
static char g_sessions_state_path[PATH_BUF];
static char g_active_session_path[PATH_BUF];
static char g_pending_tool_state_path[PATH_BUF];
static char g_busy_state_path[PATH_BUF];
static char g_settings_path[PATH_BUF];

static time_t g_transcript_mtime = 0;
static time_t g_sessions_state_mtime = 0;
static time_t g_active_session_mtime = 0;
static time_t g_pending_tool_state_mtime = 0;
static time_t g_busy_state_mtime = 0;

static void init_ipc_paths(void) {
    if (g_data_root[0]) snprintf(g_state_dir, sizeof(g_state_dir), "%s/state", g_data_root);
    else snprintf(g_state_dir, sizeof(g_state_dir), "%s/&.widgits/open-hai/state", g_house_root);
    snprintf(g_request_path, sizeof(g_request_path), "%s/request.txt", g_state_dir);
    snprintf(g_sessions_state_path, sizeof(g_sessions_state_path), "%s/sessions.state.txt", g_state_dir);
    snprintf(g_active_session_path, sizeof(g_active_session_path), "%s/active_session.txt", g_state_dir);
    snprintf(g_pending_tool_state_path, sizeof(g_pending_tool_state_path), "%s/pending_tool.state.txt", g_state_dir);
    snprintf(g_busy_state_path, sizeof(g_busy_state_path), "%s/busy.state.txt", g_state_dir);
    snprintf(g_settings_path, sizeof(g_settings_path), "%s/settings.pdl", g_state_dir);
}

static void write_request(const char *line) {
    FILE *f = fopen(g_request_path, "w");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fclose(f);
}

/* Rebuilds g_msgs wholesale from the active session's own transcript.txt
 * - same tail-the-real-file shape chat-hai's own load_ledger() already
 * uses (transcript.txt IS the real data now, no redundant "message
 * state" file needed, unlike db-hq/events-hq's synthetic state files -
 * those were needed because THEIR underlying data wasn't already a
 * flat append log; open-hai's already was). Returns 1 if it changed. */
static int load_transcript_if_changed(void) {
    if (!g_session_dir[0]) return 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/transcript.txt", g_session_dir);
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (st.st_mtime == g_transcript_mtime) return 0;
    g_transcript_mtime = st.st_mtime;

    g_n_msgs = 0;
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    /* REAL PORT (2026-08-16): build real <transcript><msg role=".." text=
     * ".."/>...</transcript> markup from the raw lines, THEN parse it
     * back with ipc_parse_chtpm_string() and populate g_msgs from the
     * resulting element tree - real generate+parse round trip, not a
     * direct line->array copy. Round-trip verified standalone first
     * (scratchpad test_chtpm_transcript.c, 3 real transcripts, zero
     * mismatches) before being wired in here. */
    size_t cap = (size_t)MAX_MSGS * (size_t)MSG_LEN + 4096;
    char *markup = malloc(cap);
    if (!markup) { fclose(f); return 1; }
    size_t mlen = 0;
    mlen += (size_t)snprintf(markup + mlen, cap - mlen, "<transcript>\n");
    char line[MSG_LEN * 2];
    static char esc[MSG_LEN * 2];
    while (fgets(line, sizeof(line), f) && mlen + MSG_LEN * 2 < cap) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n < 2 || line[1] != '|') continue;
        int is_user = (line[0] == 'U');
        char text[MSG_LEN];
        unescape_line(line + 2, text, sizeof(text));
        ipc_escape_xml(text, esc, sizeof(esc));
        mlen += (size_t)snprintf(markup + mlen, cap - mlen, "  <msg role=\"%s\" text=\"%s\"/>\n",
                                  is_user ? "user" : "ai", esc);
    }
    mlen += (size_t)snprintf(markup + mlen, cap - mlen, "</transcript>\n");
    fclose(f);

    IpcElem *root = ipc_parse_chtpm_string(markup);
    free(markup);
    if (!root) return 1;
    for (int i = 0; i < root->n_children; i++) {
        IpcElem *m = root->children[i];
        int is_user = (strcmp(m->id, "user") == 0);
        add_msg(is_user, m->label);
    }
    return 1;
}

static int load_sessions_state_if_changed(void) {
    struct stat st;
    if (stat(g_sessions_state_path, &st) != 0) return 0;
    if (st.st_mtime == g_sessions_state_mtime) return 0;
    g_sessions_state_mtime = st.st_mtime;

    g_n_sessions = 0;
    FILE *f = fopen(g_sessions_state_path, "r");
    if (!f) return 1;
    /* REAL PORT (2026-08-16): same real generate-markup+parse_chtpm()
     * round trip as load_transcript_if_changed() above, not a direct
     * line->array copy. */
    char markup[MAX_SESSIONS * (PATH_BUF + 256) + 64];
    size_t mlen = 0;
    mlen += (size_t)snprintf(markup + mlen, sizeof(markup) - mlen, "<sessions>\n");
    char line[PATH_BUF + 128];
    char esc_dir[PATH_BUF * 2], esc_label[512];
    while (g_n_sessions < MAX_SESSIONS && fgets(line, sizeof(line), f) && mlen + PATH_BUF + 256 < sizeof(markup)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *bar = strchr(line, '|');
        if (!bar) continue;
        *bar = '\0';
        ipc_escape_xml(line, esc_dir, sizeof(esc_dir));
        ipc_escape_xml(bar + 1, esc_label, sizeof(esc_label));
        mlen += (size_t)snprintf(markup + mlen, sizeof(markup) - mlen,
                                  "  <session dir=\"%s\" label=\"%s\"/>\n", esc_dir, esc_label);
        g_n_sessions++;
    }
    fclose(f);
    mlen += (size_t)snprintf(markup + mlen, sizeof(markup) - mlen, "</sessions>\n");
    (void)mlen;

    g_n_sessions = 0;
    IpcElem *root = ipc_parse_chtpm_string(markup);
    if (!root) return 1;
    for (int i = 0; i < root->n_children && g_n_sessions < MAX_SESSIONS; i++) {
        IpcElem *s = root->children[i];
        SessionEntry *se = &g_sessions[g_n_sessions];
        snprintf(se->dir, sizeof(se->dir), "%s", s->id);
        snprintf(se->label, sizeof(se->label), "%s", s->label);
        g_n_sessions++;
    }
    return 1;
}

/* Returns 1 if the active session changed (caller should force a
 * transcript reload - g_transcript_mtime alone won't catch a session
 * SWITCH to a file with a coincidentally-identical mtime). */
static int load_active_session_if_changed(void) {
    struct stat st;
    if (stat(g_active_session_path, &st) != 0) return 0;
    if (st.st_mtime == g_active_session_mtime) return 0;
    g_active_session_mtime = st.st_mtime;

    FILE *f = fopen(g_active_session_path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int changed = 0;
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, g_session_dir) != 0) {
            snprintf(g_session_dir, sizeof(g_session_dir), "%s", line);
            g_transcript_mtime = 0; /* force reload even if the new file's mtime happens to match */
            changed = 1;
        }
    }
    fclose(f);
    return changed;
}

/* ---------- tool approve/deny - DISPLAY ONLY, manager owns detection/
 * execution now (khtpm_open_hai_manager.c's own detect_tool()/tool_*()) ---------- */
typedef struct {
    char name[32];
    char arg[512];
} PendingToolDisplay;
static PendingToolDisplay g_pending_tool;
static int g_tool_pending = 0;

static int load_pending_tool_state_if_changed(void) {
    struct stat st;
    int exists = (stat(g_pending_tool_state_path, &st) == 0);
    time_t mtime = exists ? st.st_mtime : 0;
    if (mtime == g_pending_tool_state_mtime && (exists || !g_tool_pending)) return 0;
    g_pending_tool_state_mtime = mtime;

    int was_pending = g_tool_pending;
    g_tool_pending = 0;
    if (exists) {
        FILE *f = fopen(g_pending_tool_state_path, "r");
        if (f) {
            char line[700];
            if (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\r\n")] = '\0';
                if (line[0]) {
                    char *bar1 = strchr(line, '|');
                    if (bar1) {
                        *bar1 = '\0';
                        snprintf(g_pending_tool.name, sizeof(g_pending_tool.name), "%s", line);
                        char *bar2 = strchr(bar1 + 1, '|');
                        if (bar2) *bar2 = '\0';
                        snprintf(g_pending_tool.arg, sizeof(g_pending_tool.arg), "%s", bar1 + 1);
                        g_tool_pending = 1;
                    }
                }
            }
            fclose(f);
        }
    }
    return was_pending != g_tool_pending;
}

static int g_pending = 0; /* mirrors the manager's own busy.state.txt - DISPLAY only now, no child process here */

static int load_busy_state_if_changed(void) {
    struct stat st;
    if (stat(g_busy_state_path, &st) != 0) return 0;
    if (st.st_mtime == g_busy_state_mtime) return 0;
    g_busy_state_mtime = st.st_mtime;
    int was = g_pending;
    FILE *f = fopen(g_busy_state_path, "r");
    g_pending = 0;
    if (f) {
        char line[8];
        if (fgets(line, sizeof(line), f)) g_pending = (atoi(line) != 0);
        fclose(f);
    }
    return was != g_pending;
}

/* ---------- backend/model SELECTION (stays shell-side - cheap, purely
 * local UI action, writes model.txt; the manager reads that file fresh
 * before every send, so a switch takes effect on the very next message
 * with zero extra IPC - see khtpm_open_hai_manager.c's own current_model()
 * comment). The actual network CALL moved to the manager entirely. ---------- */
/* REAL FIX 2026-08-16, direct live report ("i didn't see model change
 * in model changer. are u sure u did it right?") - this file has its
 * OWN, SEPARATE copy of BackendMode/g_models[] from khtpm_open_hai_
 * manager.c's (real drift this house's own shell/manager split doesn't
 * automatically prevent - each half owns its own copy of anything it
 * needs, per khtpm-merge-how2.md's real documented shell/manager
 * pattern). Adding OpenRouter/TokenRouter to the MANAGER's list alone
 * (done earlier this session) made real API dispatch work correctly
 * (confirmed via a real tool-call round trip) but left THIS list -
 * the one cycle_model() and the sidebar's own model-name display
 * actually read - unaware of either backend, so the UI silently fell
 * back to g_models[0] ("stable-code:latest") whenever model.txt held
 * a name this list didn't recognize (see this file's own model.txt
 * loader a few lines down - real fallback-on-no-match behavior, not a
 * crash, which is exactly why this looked like "nothing happened"
 * instead of an obvious error). Real fix: mirror the manager's own 2
 * real, live-tested entries here too - both lists must be kept in
 * sync by hand until/unless a real shared-source-of-truth refactor
 * happens (not attempted here - real, separate, larger change). */
typedef enum { BACKEND_OLLAMA_RAW = 0, BACKEND_AGENT45_LEGACY = 1, BACKEND_HARNECIENT = 2, BACKEND_OPENROUTER = 3, BACKEND_TOKENROUTER = 4 } BackendMode;
static BackendMode g_backend_mode = BACKEND_OLLAMA_RAW;
static char g_model_name[128] = "stable-code:latest";

typedef struct { const char *name; BackendMode mode; } ModelEntry;
static const ModelEntry g_models[] = {
    { "stable-code:latest", BACKEND_HARNECIENT },
    { "gemma3:1b", BACKEND_HARNECIENT },
    { "gemma3:270m", BACKEND_HARNECIENT },
    { "llama3-groq-tool-use:8b", BACKEND_OLLAMA_RAW },
    { "llama2:latest", BACKEND_HARNECIENT },
    { "google/gemma-4-26b-a4b-it:free", BACKEND_OPENROUTER },
    /* REAL FIX 2026-08-18, mirrored from khtpm_open_hai_manager.c's own
     * g_models[] same-day fix (see that file's own comment for the
     * live curl test that found these) - this file's own header
     * comment two-hop-warns that BOTH copies must be updated by hand;
     * done here in the SAME pass, not left to drift again. */
    { "nvidia/nemotron-3.5-lightning:free", BACKEND_OPENROUTER },
    { "cohere/north-mini-code:free", BACKEND_OPENROUTER },
    { "qwen/qwen3.8-max-free", BACKEND_TOKENROUTER }
};
static int g_model_idx = 0;
static const int g_n_models = sizeof(g_models) / sizeof(g_models[0]);


/* ---------- input state ---------- */
static int g_armed = 0;
static char g_input_buf[INPUT_BUF_LEN] = "";
static int g_input_len = 0;
static int g_composer_h = COMPOSER_H; /* grows with wrapped input, recomputed each redraw() - see COMPOSER_MAX_LINES */

/* ---------- dynamic nav array (db-hq/events-hq convention, adapted to
 * this file's flat non-tag-tree layout) - rebuilt fresh every redraw
 * since the sidebar's session list can grow/shrink. ---------- */
typedef enum { NAV_NEWCHAT, NAV_SESSION, NAV_SCROLL_UP, NAV_SCROLL_DOWN, NAV_COMPOSER, NAV_CLOSE, NAV_TOOL_APPROVE, NAV_TOOL_DENY, NAV_MODEL, NAV_STATS, NAV_SESS_UP, NAV_SESS_DOWN, NAV_SETTINGS, NAV_SETTING_SOUND } NavKind;
typedef struct { NavKind kind; int session_idx; int x, y, w, h; } NavItem;
static NavItem g_nav[MAX_NAV];
static int g_n_nav = 0;
static int g_focus_nav = 1;
/* Session history scroll offset (2026-08-16, direct instruction: "session
 * history can take up a square and leave room for the other features" -
 * bounded HISTORY list + its own up/down scroll, same g_scroll_offset
 * shape the transcript already uses). Unit: session rows. */
static int g_session_scroll_offset = 0;
/* Press-to-confirm guard for "+ New chat" (2026-08-16, direct report:
 * "1.new chat gets accidently pressed" - it USED to be nav index 1, the
 * easiest slot to hit by accident; it now sits lower in the sidebar AND
 * needs a second Enter to actually create a session). */
static int g_newchat_confirm = 0;
/* Per-app settings (2026-08-16, direct instruction: open-hai plays a
 * tone when a message is posted, toggleable via a Settings button in
 * the topbar). Stored in state/settings.pdl (house SECTION|key|value
 * shape); the manager reads the same file to decide whether to play
 * the incoming-message tone. mtime-gated here like every other state
 * file so a hand-edit or the manager never desyncs the GUI toggle. */
static int g_sound_on = 1;
static int g_settings_open = 0;
static time_t g_settings_mtime = 0;

/* ---------- settings.pdl (2026-08-16, Settings submenu) ----------
 * Per-app settings, house .pdl shape (SECTION | key | value) - same
 * file the manager reads to decide whether to play the incoming-
 * message tone, so the GUI toggle and the actual sound never desync.
 * Missing/malformed file = defaults (sound ON), never hard-fails. */
static int load_settings_if_changed(void) {
    struct stat st;
    if (stat(g_settings_path, &st) != 0) return 0;
    if (st.st_mtime == g_settings_mtime) return 0;
    g_settings_mtime = st.st_mtime;
    FILE *f = fopen(g_settings_path, "r");
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *bar = strchr(line, '|');
        if (!bar) continue;
        char *v = strchr(bar + 1, '|');
        if (!v) continue;
        char *key = bar + 1;
        char *val = v + 1;
        *v = '\0'; /* cut the key at the second bar - leaves the rest intact for atoi() */
        while (*key == ' ') key++;
        char *vend = key + strlen(key);
        while (vend > key && vend[-1] == ' ') *--vend = '\0';
        if (strcmp(key, "sound_on") == 0) g_sound_on = (atoi(val) != 0);
    }
    fclose(f);
    return 1;
}

static void write_settings(void) {
    FILE *f = fopen(g_settings_path, "w");
    if (!f) return;
    fprintf(f, "# open-hai settings.pdl - edited via the Settings submenu\n"
              "# (topbar, next to Stats). The manager plays the incoming-\n"
              "# message tone only while sound_on is 1.\n");
    fprintf(f, "SECTION | sound_on | %d\n", g_sound_on);
    fclose(f);
    struct stat st;
    if (stat(g_settings_path, &st) == 0) g_settings_mtime = st.st_mtime;
}

static int nav_add(NavKind kind, int session_idx) {
    if (g_n_nav >= MAX_NAV) return -1;
    g_nav[g_n_nav].kind = kind;
    g_nav[g_n_nav].session_idx = session_idx;
    g_nav[g_n_nav].x = g_nav[g_n_nav].y = g_nav[g_n_nav].w = g_nav[g_n_nav].h = 0;
    g_n_nav++;
    return g_n_nav; /* 1-based nav index of the item just added */
}

/* REAL FIX 2026-08-12, direct report ("hai doesn't have mouse working
 * yet unlike db-hq"): db-hq has real click-to-select-and-activate hit
 * testing (khtpm_hq_render.c's own hit_test()/handle_click()) that
 * this file never got - ButtonPress here only ever checked the chrome
 * bar for window-dragging, nothing else. Every nav item now records
 * its own clickable rect right after nav_add() (same 1-based index),
 * so a real ButtonPress can hit-test against g_nav[] directly - see
 * handle_click() near main()'s event loop. */
static void nav_set_rect(int nav_index, int x, int y, int w, int h) {
    if (nav_index < 1 || nav_index > g_n_nav) return;
    g_nav[nav_index - 1].x = x; g_nav[nav_index - 1].y = y;
    g_nav[nav_index - 1].w = w; g_nav[nav_index - 1].h = h;
}

/* ---------- transcript scroll (ported convention - see file header
 * comment for the real source: wraith-alpha's fs scroll_offset/
 * VISIBLE_ENTRIES shape, NOT tpmos's separate joystick/GL scrollbar).
 * Unit is FLAT WRAPPED LINES (not messages, since messages vary in
 * wrapped height) - flattened fresh each redraw into g_flat_lines[]. */
static int g_scroll_offset = 0; /* 0 = following the bottom/newest */

typedef enum { FSTYLE_NORMAL = 0, FSTYLE_BULLET, FSTYLE_SUBTEXT } FlatStyle;
typedef struct { char text[512]; int is_header; const char *role; int style; } FlatLine;
static FlatLine g_flat[MAX_FLAT_LINES];
static int g_n_flat = 0;

/* ---------- window / GC / fonts ---------- */
static Display *dpy;
static Window win;
static GC gc;
static Pixmap buf;
static XftDraw *xftdraw_buf;
static Atom wm_delete;
static int g_win_x = 200, g_win_y = 280;
static int g_win_w = WIN_W, g_win_h = WIN_H;
static int g_dragging = 0, g_drag_start_x, g_drag_start_y, g_drag_win_x0, g_drag_win_y0;
static int g_resizing = 0, g_resize_start_x, g_resize_start_y, g_resize_w0, g_resize_h0;
#define RESIZE_GRIP 20
#define MIN_WIN_W 620
#define MIN_WIN_H 420

static XftFont *font_ui = NULL, *font_small = NULL, *font_mono = NULL;
static XftFont *font_ui_bold = NULL, *font_ui_italic = NULL;

static XftColor col_text, col_muted, col_accent, col_danger, col_user, col_assistant;
static XftColor col_user_bright, col_assistant_bright, col_bullet, col_subtext;
static Colormap cmap;
static Visual *vis;

static void load_fonts(void) {
    font_ui = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans-9");
    font_small = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans-8");
    font_mono = XftFontOpenName(dpy, DefaultScreen(dpy), "Monospace-9");
    font_ui_bold = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans:bold:size=9");
    font_ui_italic = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans:oblique:size=9");
    if (!font_ui_bold) font_ui_bold = font_ui;
    if (!font_ui_italic) font_ui_italic = font_ui;
}

/* Classifies a wrapped transcript line for the hierarchy typography
 * (bold/underline on bullet points, italic+dim on their indented
 * subtext): a BULLET line starts (after any indent) with a list marker
 * (- * + bullet en/em-dash or a numbered "1."/"1)"), a SUBTEXT line is
 * one that is indented and not itself a bullet, everything else NORMAL. */
static FlatStyle classify_line(const char *s) {
    if (!s) return FSTYLE_NORMAL;
    const char *p = s;
    int indented = 0;
    while (*p == ' ' || *p == '\t') { p++; indented = 1; }
    if (!*p) return FSTYLE_NORMAL;
    const unsigned char *u = (const unsigned char *)p;
    int is_dash = (*p == '-' || *p == '*' || *p == '+');
    int is_utf8_marker = (u[0] == 0xE2 && u[1] == 0x80 &&
                          (u[2] == 0xA2 || u[2] == 0xA5 || u[2] == 0xA6 ||
                           u[2] == 0x93 || u[2] == 0x94 || u[2] == 0x99));
    if (is_dash || is_utf8_marker) {
        const char *after = is_utf8_marker ? p + 3 : p + 1;
        if (*after == ' ' || *after == '\t' || !*after) return FSTYLE_BULLET;
    }
    if (*p >= '0' && *p <= '9') {
        const char *q = p + 1;
        while (*q >= '0' && *q <= '9') q++;
        if ((*q == '.' || *q == ')' || *q == ']') && (q[1] == ' ' || q[1] == '\t' || !q[1]))
            return FSTYLE_BULLET;
    }
    return indented ? FSTYLE_SUBTEXT : FSTYLE_NORMAL;
}

static void xft_color(const char *hex, XftColor *out) {
    XRenderColor rc = {0,0,0,0xffff};
    unsigned int r=0,g=0,b=0;
    if (hex[0] == '#') sscanf(hex+1, "%02x%02x%02x", &r,&g,&b);
    rc.red = (unsigned short)(r*257); rc.green = (unsigned short)(g*257); rc.blue = (unsigned short)(b*257);
    XftColorAllocValue(dpy, vis, cmap, &rc, out);
}

/* ---------- REAL emoji rendering (ported from chtpm's own pipeline,
 * direct instruction: "chtpm uses a function to convert emoji to .csv
 * first use that"). The .csv is made once per emoji by chtpm's
 * emoji_gen_atlas (FreeType-rasterizes one emoji -> 64x64 PNG) followed
 * by emoji_xtract (-> 16x16 r,g,b,a voxel CSV), dropped into
 * <house>/&.widgits/open-hai/pieces/registry/emoji_assets/<hex-cp>/
 * voxels_16.csv - pre-generated, not per-frame, exactly like
 * chtpm_rgb_render.c's own load_emoji_voxels()/blit_emoji_tile(). At
 * runtime we load those CSVs once and blit the tiles inline while
 * Xft draws the plain-text runs around them. ---------- */
#define EMOJI_TILE 16
#define EMOJI_ADV 18 /* advance per emoji, keeps wrap math in text_width() honest */
typedef struct {
    unsigned int cp;                          /* base unicode codepoint (hex dir name) */
    unsigned char px[EMOJI_TILE * EMOJI_TILE * 4]; /* r,g,b,a per pixel */
} EmojiTile;
static EmojiTile g_emoji_tiles[512];
static int g_emoji_n = 0;
static int g_px_rshift, g_px_gshift, g_px_bshift; /* TrueColor packing (vis masks) */

static int utf8_decode(const unsigned char *s, unsigned int *cp) {
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) { *cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); return 2; }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) { *cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); return 3; }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) { *cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); return 4; }
    *cp = 0xFFFD; return 1;
}

static int mask_shift(unsigned long m) {
    int s = 0;
    while (m && !(m & 1UL)) { m >>= 1; s++; }
    return s;
}

static int load_emoji_tiles(void) {
    DIR *d = opendir(g_emoji_dir);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) && g_emoji_n < 512) {
        if (e->d_name[0] == '.') continue;
        char csv[PATH_BUF];
        snprintf(csv, sizeof(csv), "%s/%s/voxels_16.csv", g_emoji_dir, e->d_name);
        FILE *f = fopen(csv, "r");
        if (!f) continue;
        unsigned int cp = (unsigned int)strtoul(e->d_name, NULL, 16);
        EmojiTile *t = &g_emoji_tiles[g_emoji_n];
        memset(t, 0, sizeof(*t));
        t->cp = cp;
        char line[64];
        int npix = 0;
        while (fgets(line, sizeof(line), f) && npix < EMOJI_TILE * EMOJI_TILE) {
            if (line[0] == '#' || line[0] == '\n') continue;
            if (line[0] == 'r' && line[1] == ',') continue; /* "r,g,b,a" header */
            int r, g, b, a;
            if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
                size_t o = (size_t)npix * 4;
                t->px[o] = (unsigned char)r; t->px[o + 1] = (unsigned char)g;
                t->px[o + 2] = (unsigned char)b; t->px[o + 3] = (unsigned char)a;
                npix++;
            }
        }
        fclose(f);
        if (npix == EMOJI_TILE * EMOJI_TILE) g_emoji_n++;
    }
    closedir(d);
    return g_emoji_n;
}

static const EmojiTile *emoji_for_cp(unsigned int cp) {
    for (int i = 0; i < g_emoji_n; i++) if (g_emoji_tiles[i].cp == cp) return &g_emoji_tiles[i];
    return NULL;
}

static void blit_emoji_tile(const EmojiTile *t, int x, int ytop) {
    for (int yy = 0; yy < EMOJI_TILE; yy++) {
        for (int xx = 0; xx < EMOJI_TILE; xx++) {
            size_t o = ((size_t)yy * EMOJI_TILE + xx) * 4;
            if (t->px[o + 3] < 128) continue; /* skip transparent pixels */
            unsigned long px = ((((unsigned long)t->px[o]) << g_px_rshift) & vis->red_mask) |
                               ((((unsigned long)t->px[o + 1]) << g_px_gshift) & vis->green_mask) |
                               ((((unsigned long)t->px[o + 2]) << g_px_bshift) & vis->blue_mask);
            XSetForeground(dpy, gc, px);
            XDrawPoint(dpy, buf, gc, x + xx, ytop + yy);
        }
    }
}

/* text -> drawable segments (text runs vs emoji tiles); zero-width
 * joiners / variation selectors / ZWSP are consumed silently so no
 * tofu box ever renders for them. */
typedef struct { const char *s; int len; int is_emoji; const EmojiTile *tile; } DrawSeg;
static int build_segs(const char *text, DrawSeg *segs, int maxsegs) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)text;
    const unsigned char *run = p;
    while (*p) {
        unsigned int cp; int clen = utf8_decode(p, &cp);
        int zero_w = (cp == 0xFE0F || cp == 0x200D || cp == 0x200C || cp == 0x200B);
        const EmojiTile *t = zero_w ? NULL : emoji_for_cp(cp);
        if (t || zero_w) {
            if (p > run && n < maxsegs) { segs[n].s = (const char *)run; segs[n].len = (int)(p - run); segs[n].is_emoji = 0; segs[n].tile = NULL; n++; }
            if (t && n < maxsegs) { segs[n].s = (const char *)p; segs[n].len = clen; segs[n].is_emoji = 1; segs[n].tile = t; n++; }
            p += clen; run = p;
        } else {
            p += clen;
        }
    }
    if (p > run && n < maxsegs) { segs[n].s = (const char *)run; segs[n].len = (int)(p - run); segs[n].is_emoji = 0; segs[n].tile = NULL; n++; }
    return n;
}

static int text_run_advance(XftFont *f, const char *s, int len) {
    XGlyphInfo gi;
    XftTextExtentsUtf8(dpy, f, (const FcChar8 *)s, len, &gi);
    return gi.xOff;
}

static void draw_text(XftFont *f, XftColor *c, int x, int y, const char *s) {
    if (!s || !*s) return;
    DrawSeg segs[512];
    int n = build_segs(s, segs, 512);
    int sx = x;
    int tile_top = y - 13; /* baseline y; tile sits up in the line box */
    for (int i = 0; i < n; i++) {
        if (segs[i].is_emoji) {
            blit_emoji_tile(segs[i].tile, sx, tile_top);
            sx += EMOJI_ADV;
        } else {
            XftDrawStringUtf8(xftdraw_buf, c, f, sx, y, (const FcChar8 *)segs[i].s, segs[i].len);
            sx += (int)text_run_advance(f, segs[i].s, segs[i].len);
        }
    }
}

static int text_width(XftFont *f, const char *s) {
    DrawSeg segs[512];
    int n = build_segs(s, segs, 512);
    int w = 0;
    for (int i = 0; i < n; i++) {
        if (segs[i].is_emoji) w += EMOJI_ADV;
        else w += text_run_advance(f, segs[i].s, segs[i].len);
    }
    return w;
}

/* word-wraps a message body into `out[]` lines of at most max_px width,
 * returns line count. Simple greedy wrap, good enough for chat text. */
static int wrap_text(XftFont *f, const char *text, int max_px, char out[][512], int max_lines) {
    int n = 0;
    char word[256];
    char line[512] = "";
    const char *p = text;
    while (*p && n < max_lines) {
        int wi = 0;
        while (*p && *p != ' ' && *p != '\n' && wi < 255) word[wi++] = *p++;
        word[wi] = '\0';
        int hard_break = (*p == '\n');
        char trial[600];
        if (line[0]) snprintf(trial, sizeof(trial), "%s %s", line, word);
        else snprintf(trial, sizeof(trial), "%s", word);
        if (text_width(f, trial) > max_px && line[0]) {
            snprintf(out[n++], 512, "%s", line);
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", trial);
        }
        if (*p == ' ') p++;
        if (hard_break) {
            if (n < max_lines) snprintf(out[n++], 512, "%s", line);
            line[0] = '\0';
            p++;
        }
    }
    if (line[0] && n < max_lines) snprintf(out[n++], 512, "%s", line);
    return n;
}

/* Flattens g_msgs into g_flat[] (one entry per wrapped visual line,
 * plus a role-header entry before each message) - the scroll window
 * indexes into THIS array, so scroll units are stable regardless of
 * variable message heights. Must be called before either drawing the
 * transcript or computing scroll clamps. */
static void flatten_transcript(int max_px) {
    g_n_flat = 0;
    for (int i = 0; i < g_n_msgs && g_n_flat < MAX_FLAT_LINES - 40; i++) {
        if (g_n_flat < MAX_FLAT_LINES) {
            g_flat[g_n_flat].is_header = 1;
            g_flat[g_n_flat].role = g_msgs[i].is_user ? "You" : "open-hai";
            g_flat[g_n_flat].text[0] = '\0';
            g_flat[g_n_flat].style = FSTYLE_NORMAL;
            g_n_flat++;
        }
        char lines[32][512];
        int nlines = wrap_text(font_ui, g_msgs[i].text, max_px, lines, 32);
        for (int l = 0; l < nlines && g_n_flat < MAX_FLAT_LINES; l++) {
            g_flat[g_n_flat].is_header = 0;
            g_flat[g_n_flat].role = NULL;
            g_flat[g_n_flat].style = classify_line(lines[l]);
            snprintf(g_flat[g_n_flat].text, sizeof(g_flat[g_n_flat].text), "%s", lines[l]);
            g_n_flat++;
        }
        if (g_n_flat < MAX_FLAT_LINES) { g_flat[g_n_flat].is_header = 0; g_flat[g_n_flat].role = NULL; g_flat[g_n_flat].text[0] = '\0'; g_flat[g_n_flat].style = FSTYLE_NORMAL; g_n_flat++; } /* blank spacer line between messages */
    }
}

/* REAL FIX 2026-08-16 (Stage 2d shell/manager split): this used to
 * detect_tool()/send_to_ollama()/start_tool_job() directly, and persist
 * the user's own message itself. All of that is the manager's job now
 * (khtpm_open_hai_manager.c's own handle_submit()) - this just writes the
 * request; the next transcript.txt poll shows the user's message (and
 * whatever the manager does in response) once the manager actually
 * processes it - same async-request-then-poll shape as db-hq/events-hq's
 * own action.txt round trip. g_pending/g_tool_pending here are DISPLAY
 * mirrors of the manager's own busy.state.txt/pending_tool.state.txt -
 * still correct to guard on, just no longer authoritative in-process
 * state. */
static void submit_composer(void) {
    if (!g_input_len) return;
    if (g_pending) {
        g_input_buf[0] = '\0';
        g_input_len = 0;
        g_armed = 0;
        return;
    }
    if (g_tool_pending) { /* resolve the pending approve/deny first */
        g_input_buf[0] = '\0';
        g_input_len = 0;
        g_armed = 0;
        return;
    }

    char prompt[INPUT_BUF_LEN];
    snprintf(prompt, sizeof(prompt), "%s", g_input_buf);
    char esc[INPUT_BUF_LEN * 2 + 8];
    escape_line(prompt, esc, sizeof(esc));
    char req[INPUT_BUF_LEN * 2 + 16];
    snprintf(req, sizeof(req), "SEND|%s", esc);
    write_request(req);

    g_input_buf[0] = '\0';
    g_input_len = 0;
    g_armed = 0;
    g_scroll_follow_bottom = 1;
}

static void new_chat(void) {
    write_request("NEWSESSION");
    g_input_buf[0] = '\0';
    g_input_len = 0;
    g_scroll_follow_bottom = 1;
}

/* ---------- draw ---------- */
static int draw_badge(int nav_index, int is_scroll_arrow, int x, int y);

/* REAL FIX 2026-08-16 (direct report: "for h-ai stats and even x button
 * are too off screen 2 the right"): draw_close_button()/the Stats badge
 * in draw_topbar() both right-align a "[state]N. label" pair against
 * g_win_w using a FIXED width guess (e.g. g_win_w - 56) - real nav
 * indices in a house with real history routinely reach 2+ digits, so the
 * guessed width was too narrow and the real (wider) text ran past the
 * window edge / into its neighbor. Real fix: measure the badge+label
 * pair's ACTUAL width first (same text_run_advance() real-measurement
 * approach draw_badge() itself now uses), then right-align from that. */
static int right_aligned_badge_label(int nav_index, int is_scroll_arrow, const char *label,
                                      XftColor *label_col, int right_x, int y) {
    char badge[16];
    int on = (nav_index == g_focus_nav);
    snprintf(badge, sizeof(badge), "[%s]%d.", on ? (g_armed && !is_scroll_arrow ? "^" : ">") : " ", nav_index);
    int badge_w = text_run_advance(font_small, badge, (int)strlen(badge));
    int label_w = text_run_advance(font_small, label, (int)strlen(label));
    int label_x = right_x - label_w;
    int badge_x = label_x - 6 - badge_w;
    draw_badge(nav_index, is_scroll_arrow, badge_x, y);
    draw_text(font_small, label_col, label_x, y, label);
    return badge_x; /* left edge, for the caller's nav_set_rect() */
}

/* REAL FIX 2026-08-12, direct instruction ("don't use esc to close.
 * use a real x nav as is standard"): closing used to be Escape-only,
 * not the real nav-indexed close convention every other khtpm window
 * (db-hq, events-hq) already uses - a real, numbered, clickable/
 * digit-jumpable close button in the chrome bar, same as "close button
 * gets the LAST nav index" fixed for db-hq earlier this same session
 * (see !.HOUSE_STDS.md - "everything gets a number", including close).
 * Escape now only disarms the composer (same as before), it does NOT
 * close the window anymore - use the real close button/nav item like
 * every other window here. */
static void draw_chrome_bar(void) {
    XSetForeground(dpy, gc, 0x1a1a1a);
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, CHROME_H);
    draw_text(font_small, &col_muted, 10, CHROME_H - 9, "open-hai — [Backspace] on a chat row deletes it");
}

/* Close button gets the LAST nav index, drawn far right of the chrome
 * bar - called at the very end of redraw()'s nav-building pass, after
 * every other real nav item already has its index, matching db-hq's
 * own established close-button convention exactly. */
/* REAL FIX 2026-08-12, direct correction ("db-hq and hai are using
 * nav index in not quite the std the std is [].<#> not [<#>]"):
 * verified against the actual real reference
 * (1.TPMOS_c_+rmmp.0103.0001/projects/wraith-alpha/ops/
 * wraith_parser_alpha.c ~line 2221-2224/2283) - the bracket holds
 * ONLY the state glyph (`[^]`/`[>]`/`[]`/`[ ]`), the number is a
 * SEPARATE suffix drawn after the closing bracket with a trailing
 * period (e.g. `[>]1.`), NOT embedded inside the brackets as `[>1]`.
 * Same fix applied to khtpm_hq_render.c (db-hq) and
 * khtpm_events_hq_render.c (events-hq) - all three now match the real
 * standard, see !.HOUSE_STDS.md #22's own correction. */
static void draw_close_button(void) {
    int nav_close = nav_add(NAV_CLOSE, -1);
    XftColor *c = (nav_close == g_focus_nav) ? &col_danger : &col_muted;
    /* is_scroll_arrow=1 here only to keep this badge's original ">"-only
     * glyph (never "^") - not an actual scroll arrow, just reusing that
     * flag's existing effect on draw_badge()'s glyph choice. */
    int badge_x = right_aligned_badge_label(nav_close, 1, "x", c, g_win_w - 8, CHROME_H - 9);
    nav_set_rect(nav_close, badge_x - 4, 0, g_win_w - (badge_x - 4), CHROME_H);
}

/* REAL FIX 2026-08-16 (direct report: "some of the other numbers and
 * options up and down are jumbled with their words too close to their
 * numbers"): every call site used to place its label at a fixed pixel
 * offset from the badge's x (e.g. x+44), tuned for a single-digit index.
 * Session/history counts here routinely exceed 9, so "[ ]10." etc is
 * measurably wider than "[ ]1." was - the fixed offsets ran the label
 * straight into the badge's own digits, exactly as reported live (see
 * the open-hai-frame.png dump: "[ ]1008-15 02:13" with zero gap). Real
 * fix: return the badge's ACTUAL measured pixel width (text_run_advance,
 * same real-measurement approach wrap_lines()/measure_text_px() already
 * use elsewhere in this house) so every caller can compute its label's x
 * from the real width instead of a guessed constant. */
static int draw_badge(int nav_index, int is_scroll_arrow, int x, int y) {
    char badge[16];
    int on = (nav_index == g_focus_nav);
    snprintf(badge, sizeof(badge), "[%s]%d.", on ? (g_armed && !is_scroll_arrow ? "^" : ">") : " ", nav_index);
    XftColor *c = on ? &col_accent : &col_text;
    draw_text(font_small, c, x, y, badge);
    return text_run_advance(font_small, badge, (int)strlen(badge));
}


/* Model persistence and cycling */
static void load_model_choice(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/model.txt", g_sessions_root);
    FILE *f = fopen(path, "r");
    if (!f) {
        strncpy(g_model_name, g_models[0].name, sizeof(g_model_name) - 1);
        g_model_idx = 0;
        g_backend_mode = g_models[0].mode;
        return;
    }
    char buf[128];
    if (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
        for (int i = 0; i < g_n_models; i++) {
            if (strcmp(buf, g_models[i].name) == 0) {
                g_model_idx = i;
                strncpy(g_model_name, g_models[i].name, sizeof(g_model_name) - 1);
                g_backend_mode = g_models[i].mode;
                fclose(f);
                return;
            }
        }
    }
    fclose(f);
    strncpy(g_model_name, g_models[0].name, sizeof(g_model_name) - 1);
    g_model_idx = 0;
    g_backend_mode = g_models[0].mode;
}

static void save_model_choice(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/model.txt", g_sessions_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", g_model_name);
        fclose(f);
    }
}

static void cycle_model(void) {
    g_model_idx = (g_model_idx + 1) % g_n_models;
    strncpy(g_model_name, g_models[g_model_idx].name, sizeof(g_model_name) - 1);
    g_backend_mode = g_models[g_model_idx].mode;
    save_model_choice();
}

static void draw_sidebar(void) {
    XSetForeground(dpy, gc, 0x141414);
    XFillRectangle(dpy, buf, gc, 0, CHROME_H, SIDEBAR_W, (unsigned)(g_win_h - CHROME_H));

    int y = CHROME_H + 18;
    int bw = 0;

    if (g_tool_pending) {
        int nav_app = nav_add(NAV_TOOL_APPROVE, -1);
        nav_set_rect(nav_app, 0, y - 14, SIDEBAR_W, 24);
        bw = draw_badge(nav_app, 0, 14, y);
        char lbl[120];
        snprintf(lbl, sizeof(lbl), "Approve: %s", g_pending_tool.name);
        draw_text(font_ui, nav_app == g_focus_nav ? &col_accent : &col_text, 14 + bw + 6, y, lbl);
        y += 24;
        int nav_deny = nav_add(NAV_TOOL_DENY, -1);
        nav_set_rect(nav_deny, 0, y - 14, SIDEBAR_W, 24);
        bw = draw_badge(nav_deny, 0, 14, y);
        draw_text(font_ui, nav_deny == g_focus_nav ? &col_danger : &col_text, 14 + bw + 6, y, "Deny");
        y += 26;
    }

    XSetForeground(dpy, gc, 0x2a2a2a);
    XDrawLine(dpy, buf, gc, 0, y, SIDEBAR_W, y);
    y += 18;

    draw_text(font_small, &col_muted, 14, y, "HISTORY (audit — Backspace deletes)");
    y += 20;

    /* REAL FIX 2026-08-16 (direct instruction: "instead of session
     * history taking up full rect, it can take up a square and leave
     * room for the other features"): HISTORY no longer stretches to
     * fill the whole sidebar. It is bounded to roughly a square
     * (SIDEBAR_W x SIDEBAR_W) and gets its own up/down scroll pair
     * (same shape as the transcript's own g_scroll_offset scroll), so
     * sessions beyond the cap are reachable - never silently hidden.
     * The block is laid out bottom-up so it can never overlap the
     * MODEL row at the bottom, at any window height. */
    int history_top = y;
    int newchat_center = (g_win_h - 70) - 26;
    int scroll_center = newchat_center - 50;
    int history_bottom = scroll_center - 8;
    int square_h = history_bottom - history_top;
    if (square_h > SIDEBAR_W) square_h = SIDEBAR_W;
    if (square_h < 40) square_h = 40;
    int max_rows = square_h / 20;
    if (max_rows < 1) max_rows = 1;
    int has_scroll = g_n_sessions > max_rows;
    int max_scroll = g_n_sessions > max_rows ? g_n_sessions - max_rows : 0;
    if (g_session_scroll_offset > max_scroll) g_session_scroll_offset = max_scroll;
    if (g_session_scroll_offset < 0) g_session_scroll_offset = 0;
    int end = g_session_scroll_offset + max_rows;
    if (end > g_n_sessions) end = g_n_sessions;

    for (int i = g_session_scroll_offset; i < end; i++) {
        int nav_i = nav_add(NAV_SESSION, i);
        nav_set_rect(nav_i, 0, y - 14, SIDEBAR_W, 20);
        int is_current = (g_session_dir[0] && strcmp(g_sessions[i].dir, g_session_dir) == 0);
        int bw2 = draw_badge(nav_i, 0, 14, y);
        XftColor *rc = (nav_i == g_focus_nav) ? &col_accent : (is_current ? &col_text : &col_muted);
        char lbl[100];
        snprintf(lbl, sizeof(lbl), "%s%s", is_current ? "* " : "", g_sessions[i].label);
        draw_text(font_small, rc, 14 + bw2 + 6, y, lbl);
        y += 20;
    }
    if (g_n_sessions == 0) {
        draw_text(font_small, &col_muted, 14, y, "(no saved chats yet)");
        y += 20;
    }

    if (has_scroll) {
        int nav_su = nav_add(NAV_SESS_UP, -1);
        nav_set_rect(nav_su, 0, y - 14, SIDEBAR_W, 20);
        int bwsu = draw_badge(nav_su, 1, 14, y);
        draw_text(font_small, nav_su == g_focus_nav ? &col_accent : &col_muted, 14 + bwsu + 6, y,
                  g_session_scroll_offset > 0 ? "older chats" : "(no older chats)");
        y += 20;
        int nav_sd = nav_add(NAV_SESS_DOWN, -1);
        nav_set_rect(nav_sd, 0, y - 14, SIDEBAR_W, 20);
        int bwsd = draw_badge(nav_sd, 1, 14, y);
        draw_text(font_small, nav_sd == g_focus_nav ? &col_accent : &col_muted, 14 + bwsd + 6, y,
                  g_session_scroll_offset < max_scroll ? "newer chats" : "(no newer chats)");
        y += 20;
    }

    /* REAL FIX 2026-08-16 (direct report: "1.new chat gets accidently
     * pressed"): "+ New chat" used to be the FIRST sidebar item = nav
     * index 1 = the easiest slot to hit by accident. It now sits below
     * the bounded HISTORY square, far from the digit-1 slot, AND is
     * guarded by a press-to-confirm step (see activate_focused) so even
     * a deliberate press can't create a session in one shot. */
    y = newchat_center;
    int nav_newchat = nav_add(NAV_NEWCHAT, -1);
    nav_set_rect(nav_newchat, 0, y - 14, SIDEBAR_W, 24);
    int bwnc = draw_badge(nav_newchat, 0, 14, y);
    const char *ncl = g_newchat_confirm ? "New chat? Enter to confirm" : "+ New chat";
    draw_text(font_ui, nav_newchat == g_focus_nav ? &col_accent : &col_text, 14 + bwnc + 6, y, ncl);

    y = g_win_h - 70;
    XSetForeground(dpy, gc, 0x2a2a2a);
    XDrawLine(dpy, buf, gc, 0, y, SIDEBAR_W, y);
    y += 16;
    draw_text(font_small, &col_muted, 14, y, "MODEL");
    y += 18;
    int nav_model = nav_add(NAV_MODEL, -1);
    nav_set_rect(nav_model, 0, y - 14, SIDEBAR_W, 24);
    int bw3 = draw_badge(nav_model, 0, 14, y);
    draw_text(font_small, nav_model == g_focus_nav ? &col_accent : &col_text, 14 + bw3 + 6, y, g_model_name);
    /* Stats moved out of here (2026-08-16, direct report: "the 'stats'
     * button is in inappropriate location visually - lets put it up top
     * right, right of 'h-ai' title subbar thing") - see draw_topbar(). */
}

static void draw_topbar(void) {
    XSetForeground(dpy, gc, 0x0d0d0d); /* darker than the transcript/sidebar below it (user: "toolbar at top should be darker") */
    XFillRectangle(dpy, buf, gc, SIDEBAR_W, CHROME_H, (unsigned)(g_win_w - SIDEBAR_W), TOPBAR_H);
    XSetForeground(dpy, gc, 0x2a2a2a);
    XDrawLine(dpy, buf, gc, SIDEBAR_W, CHROME_H + TOPBAR_H, g_win_w, CHROME_H + TOPBAR_H);
    draw_text(font_ui, &col_text, SIDEBAR_W + 16, CHROME_H + 27, "h-ai");
    /* thinking animation: cycling dots while a request or tool is in
     * flight (g_frame is bumped every redraw, so this visibly animates) */
    if (g_pending) {
        static const char *dots[] = {".", "..", "...", ""};
        char anim[32];
        snprintf(anim, sizeof(anim), "thinking%s", dots[(g_frame / 6) % 4]);
        /* g_pending_is_tool (curl-vs-tool-job color distinction) doesn't
         * exist here anymore (Stage 2d - busy.state.txt is just a plain
         * bool, the manager's own internal distinction isn't published)
         * - always col_muted now, a minor deliberate fidelity loss, not
         * a bug. */
        draw_text(font_small, &col_muted, g_win_w - 130, CHROME_H + 27, anim);
    }
    /* REAL FIX 2026-08-16 (direct report: "the 'stats' button is in
     * inappropriate location visually - lets put it up top right, right
     * of 'h-ai' title subbar thing"): moved out of the sidebar's bottom
     * MODEL block (see that block's own note) into this row, right-
     * aligned, same row as the "h-ai" title. Kept as its own navigable
     * badge (NAV_STATS unchanged) - only its drawn position changed. */
    int nav_stats = nav_add(NAV_STATS, -1);
    XftColor *sc = (nav_stats == g_focus_nav) ? &col_accent : &col_text;
    int badge_x = right_aligned_badge_label(nav_stats, 0, "Stats", sc, g_win_w - 8, CHROME_H + 27);
    nav_set_rect(nav_stats, badge_x - 4, CHROME_H, g_win_w - (badge_x - 4), TOPBAR_H);
    /* Settings button (2026-08-16, direct instruction: "have a setting
     * button, like header toolbar, that can open submenu with things
     * like this, next to stats") - sits just left of Stats, opens the
     * settings submenu (draw_settings_panel) when activated. */
    int nav_settings = nav_add(NAV_SETTINGS, -1);
    XftColor *setc = (nav_settings == g_focus_nav) ? &col_accent : &col_text;
    int set_badge_x = right_aligned_badge_label(nav_settings, 0, "Settings", setc, badge_x - 12, CHROME_H + 27);
    nav_set_rect(nav_settings, set_badge_x - 4, CHROME_H, (badge_x - 12) - (set_badge_x - 4), TOPBAR_H);
}

/* Settings submenu (2026-08-16): a small dropdown panel anchored under
 * the topbar, right-aligned, drawn AFTER the composer so it overlays
 * the transcript area. Each row is a real nav item; today there's one
 * setting (Sound on/off), more rows can be appended here. The panel is
 * closed by the Settings button, Escape, or any non-Settings activation
 * (see activate_focused/handle_key). */
static void draw_settings_panel(void) {
    if (!g_settings_open) return;
    int pw = 220;
    int px = g_win_w - pw - 12;
    int py = CHROME_H + TOPBAR_H + 6;
    int ph = 12 + 22 + 22 + 10;
    XSetForeground(dpy, gc, 0x1a1a1a);
    XFillRectangle(dpy, buf, gc, px, py, (unsigned)pw, (unsigned)ph);
    XSetForeground(dpy, gc, 0x3a3a3a);
    XDrawRectangle(dpy, buf, gc, px, py, (unsigned)pw - 1, (unsigned)ph - 1);
    draw_text(font_small, &col_muted, px + 10, py + 16, "Settings");
    int y = py + 12 + 22;
    int nav_snd = nav_add(NAV_SETTING_SOUND, -1);
    nav_set_rect(nav_snd, px + 4, y - 14, pw - 8, 20);
    char snd[48];
    snprintf(snd, sizeof(snd), "Sound: %s", g_sound_on ? "on" : "off");
    XftColor *rc = (nav_snd == g_focus_nav) ? &col_accent : &col_text;
    int bw = draw_badge(nav_snd, 0, px + 10, y);
    draw_text(font_small, rc, px + 10 + bw + 6, y, snd);
}

static int transcript_geom(int *x0, int *y0, int *w, int *h) {
    *x0 = SIDEBAR_W; *y0 = CHROME_H + TOPBAR_H;
    *w = g_win_w - SIDEBAR_W; *h = g_win_h - CHROME_H - TOPBAR_H - g_composer_h;
    return (*h - 8) / LINE_H; /* visible line count, leaving room for the scroll-arrow row */
}

static void draw_transcript(void) {
    int x0, y0, w, h;
    int visible = transcript_geom(&x0, &y0, &w, &h) - 1; /* -1 for the arrow row */
    XSetForeground(dpy, gc, 0x1a1a1a);
    XFillRectangle(dpy, buf, gc, x0, y0, (unsigned)w, (unsigned)h);

    flatten_transcript(w - 32);
    int max_scroll = g_n_flat > visible ? g_n_flat - visible : 0;
    if (g_scroll_follow_bottom) g_scroll_offset = max_scroll;
    if (g_scroll_offset > max_scroll) g_scroll_offset = max_scroll;
    if (g_scroll_offset < 0) g_scroll_offset = 0;

    /* scroll-arrow row (ported convention - bare [▲]/[▼] nav buttons,
     * see file header comment) */
    int nav_up = nav_add(NAV_SCROLL_UP, -1);
    int nav_down = nav_add(NAV_SCROLL_DOWN, -1);
    int ay = y0 + 14;
    nav_set_rect(nav_up, x0 + 8, ay - 12, 90, 20);
    nav_set_rect(nav_down, x0 + 106, ay - 12, 90, 20);
    int bwu = draw_badge(nav_up, 1, x0 + 12, ay);
    draw_text(font_small, nav_up == g_focus_nav ? &col_accent : &col_muted, x0 + 12 + bwu + 6, ay, "up");
    int bwd = draw_badge(nav_down, 1, x0 + 110, ay);
    draw_text(font_small, nav_down == g_focus_nav ? &col_accent : &col_muted, x0 + 110 + bwd + 6, ay, "down");
    char pos[64];
    snprintf(pos, sizeof(pos), "line %d-%d / %d", g_scroll_offset + 1,
             g_scroll_offset + (visible < g_n_flat ? visible : g_n_flat), g_n_flat);
    draw_text(font_small, &col_muted, x0 + w - 140, ay, pos);

    int y = y0 + 34;
    const char *cur_role = NULL;
    for (int i = g_scroll_offset; i < g_n_flat && i < g_scroll_offset + visible; i++) {
        if (g_flat[i].is_header) {
            cur_role = g_flat[i].role;
            draw_text(font_small, &col_muted, x0 + 16, y, g_flat[i].role);
        } else if (g_flat[i].text[0]) {
            XftColor *mc = (cur_role && strcmp(cur_role, "You") == 0)
                               ? &col_user
                               : (cur_role && strcmp(cur_role, "open-hai") == 0)
                                     ? &col_assistant
                                     : &col_text;
            XftFont *mf = font_ui;
            if (g_flat[i].style == FSTYLE_BULLET) {
                mf = font_ui_bold;
                mc = (cur_role && strcmp(cur_role, "You") == 0) ? &col_user_bright
                   : (cur_role && strcmp(cur_role, "open-hai") == 0) ? &col_assistant_bright
                                                                   : &col_bullet;
            } else if (g_flat[i].style == FSTYLE_SUBTEXT) {
                mf = font_ui_italic;
                mc = &col_subtext;
            }
            draw_text(mf, mc, x0 + 16, y, g_flat[i].text);
            if (g_flat[i].style == FSTYLE_BULLET) {
                XSetForeground(dpy, gc, mc->pixel);
                XFillRectangle(dpy, buf, gc, x0 + 16, y + 4,
                               (unsigned)text_width(mf, g_flat[i].text), 1);
            }
        }
        y += LINE_H;
    }
}

/* Available pixel width for wrapped composer text - same margins
 * draw_composer() itself uses (70px left for the nav badge, 20px
 * right breathing room), computed once so update_composer_height()
 * (called BEFORE draw_composer() in redraw(), to size the box that
 * frame) and draw_composer() itself always wrap identically. */
static int composer_wrap_px(void) {
    return (g_win_w - SIDEBAR_W) - 70 - 20;
}

/* Recomputes g_composer_h from the CURRENT input text, called at the
 * top of redraw() before transcript_geom()/draw_transcript() so the
 * transcript's own visible-area math accounts for the composer's real
 * height THIS frame, not last frame's. Grows the box up to
 * COMPOSER_MAX_LINES tall as the input wraps to more lines; beyond
 * that the box stays capped and draw_composer() auto-scrolls to keep
 * the cursor's line visible instead of the input running off the
 * edge - real fix for the direct report "text input i want it to wrap
 * new line and user input can scroll up instead of dissapearing off
 * the side of the screen" (2026-08-13). */
static void update_composer_height(void) {
    if (!g_input_len) { g_composer_h = COMPOSER_H; return; }
    char lines[64][512];
    int nlines = wrap_text(font_ui, g_input_buf, composer_wrap_px(), lines, 64);
    if (nlines < 1) nlines = 1;
    int visible = nlines < COMPOSER_MAX_LINES ? nlines : COMPOSER_MAX_LINES;
    g_composer_h = COMPOSER_H + (visible - 1) * LINE_H;
}

static void draw_composer(void) {
    int x0 = SIDEBAR_W, y0 = g_win_h - g_composer_h;
    int w = g_win_w - SIDEBAR_W;
    XSetForeground(dpy, gc, 0x242424);
    XFillRectangle(dpy, buf, gc, x0 + 12, y0 + 10, (unsigned)(w - 24), (unsigned)(g_composer_h - 20));

    int nav_composer = nav_add(NAV_COMPOSER, -1);
    nav_set_rect(nav_composer, x0 + 12, y0 + 10, w - 24, g_composer_h - 20);
    int bwc = draw_badge(nav_composer, 0, x0 + 20, y0 + 26);
    int text_x = x0 + 20 + bwc + 12; /* +12 not +6 - font_ui text needs a touch more room than font_small labels do */

    if (!g_input_len) {
        const char *placeholder = g_armed ? "" : "Enter to type a message...";
        draw_text(font_ui, &col_muted, text_x, y0 + 26, placeholder);
        if (g_armed) {
            XSetForeground(dpy, gc, 0x22c55e);
            XFillRectangle(dpy, buf, gc, text_x + 2, y0 + 14, 2, 16);
        }
        return;
    }

    char lines[64][512];
    int nlines = wrap_text(font_ui, g_input_buf, composer_wrap_px(), lines, 64);
    if (nlines < 1) nlines = 1;
    int visible = nlines < COMPOSER_MAX_LINES ? nlines : COMPOSER_MAX_LINES;
    int first = nlines - visible; /* auto-follow-bottom: always show the tail, cursor stays visible */

    int ly = y0 + 26;
    for (int i = first; i < nlines; i++) {
        draw_text(font_ui, &col_text, text_x, ly, lines[i]);
        ly += LINE_H;
    }

    if (g_armed) {
        int cx = text_x + text_width(font_ui, lines[nlines - 1]);
        int cy = y0 + 26 + (visible - 1) * LINE_H;
        XSetForeground(dpy, gc, 0x22c55e);
        XFillRectangle(dpy, buf, gc, cx + 2, cy - 12, 2, 16);
    }
}

/* REAL FIX 2026-08-12, direct instruction ("u should use png dump not
 * pil capture. from now on (or receipt) learn 2 rely on receipts"):
 * external xwd/PIL screen capture is unreliable once the real user is
 * actively using their own desktop - a window can be dragged, occluded,
 * or mid-composite exactly when a capture fires, producing bleed-
 * through from whatever's on top (confirmed live this session: an
 * xwd capture returned another real window's content once open-hai got
 * covered). This app's OWN offscreen `buf` pixmap is the actual source
 * of truth - dumping FROM there (same technique db-hq's own
 * dump_frame_png() already proved: standard 0xRRGGBB byte layout, NOT
 * the zeroed mask fields XGetImage returns on a bare Pixmap) can never
 * race with window stacking/occlusion, because it reads what THIS
 * PROCESS drew, not what's visually on screen. A receipt file
 * (matching the `*.receipt.txt` convention already seen elsewhere in
 * this house, e.g. agent-45's own `rgb_frame.receipt.txt`) is written
 * right after, so a caller can poll for ITS existence/mtime instead of
 * guessing a sleep duration or trusting the PNG file's own write to be
 * atomic-enough to read mid-write. */
/* Real, human-readable label for g_nav[idx] (0-based), matching
 * EXACTLY what draw_sidebar()/draw_close_button()/draw_composer()
 * already draw on screen for that item - not a separate guess at what
 * the label "should" be. This is the missing piece flagged in
 * au11-hq/HARNESS-DELEGATION-PIPELINE.md §6 (nav_intent_to_index.sh
 * has no live label source yet) - added 2026-08-13 so a delegated
 * navigation decision (model names an item in plain text) can be
 * resolved against the REAL current labels instead of a hardcoded
 * guess, same "labels/order both drift, always read live" discipline
 * as everything else in this house's nav handling. */
static void nav_label(int idx, char *out, size_t outsz) {
    if (idx < 0 || idx >= g_n_nav) { out[0] = '\0'; return; }
    NavItem *it = &g_nav[idx];
    switch (it->kind) {
        case NAV_NEWCHAT: snprintf(out, outsz, g_newchat_confirm ? "New Chat (confirm)" : "New Chat"); break;
        case NAV_TOOL_APPROVE: snprintf(out, outsz, "Approve: %s", g_pending_tool.name); break;
        case NAV_TOOL_DENY: snprintf(out, outsz, "Deny"); break;
        case NAV_SESS_UP: snprintf(out, outsz, "Older Chats"); break;
        case NAV_SESS_DOWN: snprintf(out, outsz, "Newer Chats"); break;
        case NAV_SESSION:
            if (it->session_idx >= 0 && it->session_idx < g_n_sessions) {
                int is_current = (g_session_dir[0] && strcmp(g_sessions[it->session_idx].dir, g_session_dir) == 0);
                snprintf(out, outsz, "%s%s", is_current ? "* " : "", g_sessions[it->session_idx].label);
            } else snprintf(out, outsz, "Session");
            break;
        case NAV_MODEL: snprintf(out, outsz, "Model: %s", g_model_name); break;
        case NAV_SCROLL_UP: snprintf(out, outsz, "Scroll Up"); break;
        case NAV_SCROLL_DOWN: snprintf(out, outsz, "Scroll Down"); break;
        case NAV_COMPOSER: snprintf(out, outsz, "Composer"); break;
        case NAV_STATS: snprintf(out, outsz, "Stats"); break;
        case NAV_SETTINGS: snprintf(out, outsz, "Settings"); break;
        case NAV_SETTING_SOUND: snprintf(out, outsz, "Sound: %s", g_sound_on ? "on" : "off"); break;
        case NAV_CLOSE: snprintf(out, outsz, "Close"); break;
        default: snprintf(out, outsz, "?"); break;
    }
}

/* Delegation-safe variant of nav_label(): same real label for every
 * kind EXCEPT NAV_SESSION, where the human-facing label embeds
 * arbitrary chat snippet text (e.g. "08-13 03:00 howdy , how are u?")
 * that can and did confuse a small model into echoing noise back
 * instead of naming a real item (HARNESS-DELEGATION-PIPELINE.md §6,
 * live finding 2026-08-13: gemma3:1b replied "Howdy" - resolver failed
 * closed correctly, but the label itself was the real problem). Fixed
 * by giving sessions a clean, content-free ordinal name for the
 * delegation-facing label ONLY - the human-facing nav_label() /
 * on-screen sidebar text is UNCHANGED, still shows the real snippet,
 * since a human benefits from that context and isn't confused by it
 * the way a small model is. */
static void nav_label_delegate_safe(int idx, char *out, size_t outsz) {
    if (idx < 0 || idx >= g_n_nav) { out[0] = '\0'; return; }
    NavItem *it = &g_nav[idx];
    if (it->kind == NAV_SESSION) {
        snprintf(out, outsz, "Session %d", it->session_idx + 1);
        return;
    }
    nav_label(idx, out, outsz);
}

/* Companion file to the PNG receipt: one REAL current nav label per
 * line, 1-based index implied by line number - the live source
 * nav_intent_to_index.sh needs (see that script's own header). Written
 * alongside the receipt on every dump (same 'p' relay trigger), not a
 * separate dump mode - one receipt read gets both counts and labels.
 * Two columns after the index: the human-facing display label (real
 * on-screen text, may contain snippet content), then the delegation-
 * safe label (content-free for noisy kinds like sessions) - a
 * delegation harness should resolve against the THIRD field, a human
 * reading this file for debugging wants the second. */
static void dump_nav_labels(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/open-hai-frame.png.nav-labels.txt", g_audit_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    char lbl[128], safe_lbl[128];
    for (int i = 0; i < g_n_nav; i++) {
        nav_label(i, lbl, sizeof(lbl));
        nav_label_delegate_safe(i, safe_lbl, sizeof(safe_lbl));
        fprintf(f, "%d|%s|%s\n", i + 1, lbl, safe_lbl);
    }
    fclose(f);
}

static void dump_frame_png(void) {
    char frame_path_png[PATH_BUF];
    char frame_path_receipt[PATH_BUF];
    snprintf(frame_path_png, sizeof(frame_path_png), "%s/open-hai-frame.png", g_audit_dir);
    snprintf(frame_path_receipt, sizeof(frame_path_receipt), "%s/open-hai-frame.png.receipt.txt", g_audit_dir);
    dump_nav_labels();
    XSync(dpy, False);
    XImage *img = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
    if (!img) { fprintf(stderr, "open-hai: dump_frame_png: XGetImage failed\n"); return; }
    int w = g_win_w, h = g_win_h;
    unsigned char *rgb = malloc((size_t)w * h * 3);
    if (!rgb) { XDestroyImage(img); return; }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned long px = XGetPixel(img, x, y);
            size_t o = ((size_t)y * w + x) * 3;
            rgb[o] = (unsigned char)((px >> 16) & 0xff);
            rgb[o + 1] = (unsigned char)((px >> 8) & 0xff);
            rgb[o + 2] = (unsigned char)(px & 0xff);
        }
    }
    XDestroyImage(img);
    int ok = stbi_write_png(frame_path_png, w, h, 3, rgb, w * 3);
    free(rgb);
    FILE *rf = fopen(frame_path_receipt, "w");
    if (rf) {
        fprintf(rf, "ok=%d w=%d h=%d t=%ld nav=%d n_nav=%d n_sessions=%d n_msgs=%d tool_pending=%d tool=%s\n",
                ok, w, h, (long)time(NULL), g_focus_nav, g_n_nav, g_n_sessions, g_n_msgs,
                g_tool_pending, g_tool_pending ? g_pending_tool.name : "none");
        fclose(rf);
    }
    fprintf(stderr, ok ? "open-hai: wrote %s (%dx%d)\n" : "open-hai: dump_frame_png: write failed\n", frame_path_png, w, h);
}

static void draw_resize_grip(void) {
    int x0 = g_win_w - RESIZE_GRIP - 6, y0 = g_win_h - RESIZE_GRIP - 6;
    XSetForeground(dpy, gc, 0x3a3a3a);
    XDrawLine(dpy, buf, gc, x0, y0 + 14, x0 + 14, y0);
    XDrawLine(dpy, buf, gc, x0, y0 + 8, x0 + 8, y0);
    XDrawLine(dpy, buf, gc, x0 + 6, y0 + 14, x0 + 14, y0 + 6);
}

static void redraw(void) {
    /* check_pending() moved to khtpm_open_hai_manager.c (Stage 2d) - the
     * shell polls that manager's published state instead, see main()'s
     * own loop. */
    g_n_nav = 0; /* rebuilt fresh below - db-hq/events-hq convention */
    g_frame++;

    update_composer_height(); /* before draw_transcript() - its own visible-area math (transcript_geom()) needs g_composer_h for THIS frame, not last frame's */

    XSetForeground(dpy, gc, 0x141414);
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
    draw_chrome_bar();
    draw_sidebar();
    draw_topbar();
    draw_transcript(); /* also (re)builds the flat-line cache used for scroll math */
    draw_composer();
    draw_settings_panel(); /* dropdown over the transcript; nav items land before Close */
    draw_close_button(); /* LAST nav index, drawn after every other real nav item exists */
    draw_resize_grip();

    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
    if (g_focus_nav < 1) g_focus_nav = 1;

    XSync(dpy, False);
    XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
    if (frame) {
        XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
        XDestroyImage(frame);
    } else {
        XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, 0, 0);
    }
    XFlush(dpy);
}

/* ---------- input ---------- */
static void activate_focused(void) {
    if (g_focus_nav < 1 || g_focus_nav > g_n_nav) return;
    NavItem *it = &g_nav[g_focus_nav - 1];
    if (it->kind != NAV_NEWCHAT) g_newchat_confirm = 0;
    if (it->kind != NAV_SETTINGS && it->kind != NAV_SETTING_SOUND) g_settings_open = 0;
    switch (it->kind) {
        case NAV_NEWCHAT:
            /* press-to-confirm guard (2026-08-16, "1.new chat gets
             * accidently pressed"): first Enter arms the confirm state
             * (label flips to "New chat? Enter to confirm"), second
             * Enter actually creates. */
            if (!g_newchat_confirm) { g_newchat_confirm = 1; break; }
            g_newchat_confirm = 0;
            new_chat();
            break;
        case NAV_SESSION:
            if (it->session_idx >= 0 && it->session_idx < g_n_sessions) {
                char req[PATH_BUF + 16];
                snprintf(req, sizeof(req), "LOADSESSION|%s", g_sessions[it->session_idx].dir);
                write_request(req);
                g_scroll_follow_bottom = 1;
            }
            break;
        case NAV_SESS_UP:
            if (g_session_scroll_offset > 0) g_session_scroll_offset--;
            break;
        case NAV_SESS_DOWN:
            g_session_scroll_offset++;
            break;
        case NAV_SCROLL_UP:
            g_scroll_follow_bottom = 0;
            g_scroll_offset -= 3;
            if (g_scroll_offset < 0) g_scroll_offset = 0;
            break;
        case NAV_SCROLL_DOWN:
            g_scroll_follow_bottom = 0;
            g_scroll_offset += 3;
            break;
        case NAV_COMPOSER:
            g_armed = 1;
            break;
        case NAV_TOOL_APPROVE:
            if (g_tool_pending) write_request("APPROVE"); /* manager runs the job + persists the result, see khtpm_open_hai_manager.c */
            break;
        case NAV_TOOL_DENY:
            if (g_tool_pending) write_request("DENY"); /* manager persists the "[tool denied]" note itself now */
            break;
        case NAV_MODEL:
            cycle_model();
            break;
        case NAV_STATS: {
            char cmd[2048];
            char session_id[32] = "";
            if (g_session_dir[0]) {
                sscanf(g_session_dir, "%*[^/]/%31s", session_id);
            }
            snprintf(cmd, sizeof(cmd), "setsid nohup bash '%s/open_session_stats.sh' '%s' '%s' >/dev/null 2>&1 &",
                     g_house_root, session_id, g_house_root);
            system(cmd);
            break;
        }
        case NAV_SETTINGS:
            g_settings_open = !g_settings_open;
            break;
        case NAV_SETTING_SOUND:
            g_sound_on = !g_sound_on;
            write_settings();
            break;
        case NAV_CLOSE:
            g_running = 0;
            break;
    }
}

static void delete_focused_if_session(void) {
    if (g_focus_nav < 1 || g_focus_nav > g_n_nav) return;
    NavItem *it = &g_nav[g_focus_nav - 1];
    if (it->kind != NAV_SESSION) return;
    if (it->session_idx < 0 || it->session_idx >= g_n_sessions) return;
    /* REAL FIX 2026-08-16: request.txt holds only ONE pending line - a
     * second write_request() call right after this one (e.g. a
     * follow-up NEWSESSION) would OVERWRITE it before the manager's
     * next poll ever sees the first, silently dropping the delete. The
     * manager's own DELETESESSION handler now starts a fresh session
     * itself when the deleted one was active (see khtpm_open_hai_manager.c),
     * so this shell only ever sends the one request - no race. */
    char req[PATH_BUF + 16];
    snprintf(req, sizeof(req), "DELETESESSION|%s", g_sessions[it->session_idx].dir);
    write_request(req);
}

/* REAL FIX 2026-08-12 ("hai doesn't have mouse working yet unlike
 * db-hq") - real click-to-select-and-activate, same one-click-does-
 * both shape as db-hq's own handle_click()/hit_test(). Composer is
 * special-cased: a click there should ARM it directly (same as
 * pressing Enter on it), not toggle it off if it was already armed -
 * matches how a text field click behaves everywhere else, not a nav
 * toggle. */
static void handle_mouse_click(int px, int py) {
    for (int i = 0; i < g_n_nav; i++) {
        NavItem *it = &g_nav[i];
        if (it->w <= 0 || it->h <= 0) continue;
        if (px < it->x || px >= it->x + it->w || py < it->y || py >= it->y + it->h) continue;
        g_focus_nav = i + 1;
        if (it->kind == NAV_COMPOSER) { g_armed = 1; return; }
        activate_focused();
        return;
    }
}

static int g_digit_accum = 0; /* multi-digit nav-jump accumulator, house standard (chtpm_parser.c) */

static void handle_key(KeySym ks, char ch) {
    if (g_armed) {
        if (ks == XK_Return || ks == XK_KP_Enter) { submit_composer(); return; }
        if (ks == XK_Escape) { g_armed = 0; return; }
        if (ks == XK_BackSpace) {
            if (g_input_len > 0) { g_input_buf[--g_input_len] = '\0'; }
            return;
        }
        if (ch >= 32 && ch <= 126 && g_input_len < INPUT_BUF_LEN - 1) {
            g_input_buf[g_input_len++] = ch;
            g_input_buf[g_input_len] = '\0';
        }
        return;
    }
    if (ch == 'p') { dump_frame_png(); return; } /* not armed, so 'p' can't collide with composer typing */
    if (ks == XK_Escape) { g_newchat_confirm = 0; g_settings_open = 0; g_digit_accum = 0; return; }
    if (ch >= '0' && ch <= '9') {
        /* digit accumulation, ported from the house standard in
         * chtpm_parser.c (~line 2621): greedy multi-digit jump so a
         * single digit still moves focus instantly when unambiguous,
         * but a run of digits (e.g. "1" then "2" for nav item 12)
         * accumulates instead of the first digit always winning.
         * Previous naive version here always let a bare 1-9 digit win
         * outright, so nav items past index 9 were unreachable via
         * relay digit-jump once several sessions existed. */
        int d = ch - '0';
        int new_val = g_digit_accum * 10 + d;
        if (new_val > 0 && new_val <= g_n_nav) {
            g_digit_accum = new_val;
            g_focus_nav = g_digit_accum;
        } else if (d > 0 && d <= g_n_nav) {
            g_digit_accum = d;
            g_focus_nav = g_digit_accum;
        } else {
            g_digit_accum = 0;
        }
        g_newchat_confirm = 0;
        return; /* digit selects (moves focus) only - Enter activates, same convention as everywhere else in this house (direct confirmation 2026-08-12: "i do expect to press enter. (as usual)") */
    }
    if (ks == XK_Up || ks == XK_Left) { if (g_focus_nav > 1) g_focus_nav--; g_digit_accum = 0; g_newchat_confirm = 0; return; }
    if (ks == XK_Down || ks == XK_Right) { if (g_focus_nav < g_n_nav) g_focus_nav++; g_digit_accum = 0; g_newchat_confirm = 0; return; }
    if (ks == XK_Return || ks == XK_KP_Enter) { g_digit_accum = 0; activate_focused(); return; }
    if (ks == XK_BackSpace) { g_digit_accum = 0; g_newchat_confirm = 0; delete_focused_if_session(); return; }
}

/* ---------- agent relay (same shape as db-hq's own, HOUSE_STDS/
 * testing-guide convention): <house_root>/#.desktop/
 * open_hai_agent_relay.txt, one bare decimal ASCII code per line
 * (digits 48-57, Enter=13, Escape=27, Backspace=8, printable 32-126).
 * This is THE mechanism that makes "you type as human, I inject via
 * relay" (the whole stated point of this GUI) real - dispatched
 * through the SAME handle_key() real KeyPress events use, so relay-
 * driven input and real keyboard input are indistinguishable to the
 * rest of the program. */
static long g_relay_cursor = -1;

static void relay_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/#.desktop/open_hai_agent_relay.txt", g_house_root);
}

static void dispatch_relay_code(int code) {
    if (code == 13) handle_key(XK_Return, 0);
    else if (code == 27) handle_key(XK_Escape, 0);
    else if (code == 8) handle_key(XK_BackSpace, 0);
    else if (code >= 32 && code <= 126) handle_key(0, (char)code);
}

static int poll_agent_relay(void) {
    char path[PATH_BUF];
    relay_path(path, sizeof(path));
    struct stat stt;
    if (stat(path, &stt) != 0) return 0;
    if (g_relay_cursor < 0) { g_relay_cursor = stt.st_size; return 0; }
    if (stt.st_size < g_relay_cursor) { g_relay_cursor = stt.st_size; return 0; }
    if (stt.st_size == g_relay_cursor) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, g_relay_cursor, SEEK_SET);
    char line[32];
    long consumed = g_relay_cursor;
    int n_dispatched = 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (!nl) break;
        *nl = '\0';
        long here = ftell(f);
        int code = atoi(line);
        if (code > 0) { dispatch_relay_code(code); n_dispatched++; }
        consumed = here;
    }
    fclose(f);
    g_relay_cursor = consumed;
    return n_dispatched;
}

/* Real single-instance PID tracking + clean SIGTERM shutdown.
 * FOUND LIVE 2026-08-13: repeated test launches left FIVE concurrent
 * khtpm_open_hai_render processes alive simultaneously, all racing on
 * the same relay file/session dir (root-caused after ~2hrs of
 * "flaky" relay test results that were actually multiple processes
 * fighting over shared state, not a code bug - see
 * _.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13" for the full
 * writeup). `pkill -9 khtpm_open_hai_render` does not reliably match
 * this binary's process given the emoji-laden house-root path in
 * argv - a pidfile + graceful SIGTERM handler + button.sh doing a
 * real pgrep -f kill-before-launch (mirroring
 * *.livedesk-taskbar/ops/run_khtpm_strip.sh's own proven pattern) is
 * the fix, not a bigger hammer. */
/* REAL module launch (Stage 2d, 2026-08-16) - same real fork()+execv()
 * mechanism as db-hq/events-hq/chat-hai's own launch_module(), adapted
 * to THIS app's existing graceful-shutdown convention (g_want_exit +
 * a real end-of-main() cleanup path) instead of copying their _exit(0)-
 * on-signal shape verbatim - open-hai already had its own proven pattern
 * here, no reason to replace it. open-hai has no .chtpm (Group B, no
 * Elem/CSS parser - confirmed in khtpm-merge-how2.md's own STATUS
 * section) so there's no layout tag to read a <module> path from; the
 * path is just a plain string constant here instead. */
static pid_t g_module_pid = -1;

static void cleanup_module(void) {
    if (g_module_pid > 0) {
        kill(g_module_pid, SIGTERM);
        waitpid(g_module_pid, NULL, WNOHANG);
        g_module_pid = -1;
    }
}

static void launch_module(const char *src) {
    if (!src || !src[0]) return;
    char full_path[PATH_BUF];
    if (src[0] == '/') snprintf(full_path, sizeof(full_path), "%s", src);
    else snprintf(full_path, sizeof(full_path), "%s/%s", g_house_root, src);

    g_module_pid = fork();
    if (g_module_pid == 0) {
        /* PER-INSTANCE DATA ROOT (2026-08-24): forward --data-root to the
         * manager child so both halves of one instance agree on where
         * sessions/state live; plain launches pass nothing extra. */
        char *cargs[8];
        int n = 0;
        cargs[n++] = full_path;
        cargs[n++] = g_house_root;
        if (g_data_root[0]) { cargs[n++] = "--data-root"; cargs[n++] = g_data_root; }
        cargs[n] = NULL;
        execv(full_path, cargs);
        _exit(1);
    } else if (g_module_pid < 0) {
        fprintf(stderr, "open-hai: launch_module: fork failed for %s\n", full_path);
        g_module_pid = -1;
    }
}

static volatile sig_atomic_t g_want_exit = 0;
/* kill()/waitpid() are async-signal-safe, safe to call directly here -
 * same reliability db-hq/events-hq/chat-hai's own signal handlers rely
 * on (SIGTERM must reliably reap the manager even if the main loop is
 * blocked in select() at the moment the signal arrives). */
static void handle_sigterm(int sig) { (void)sig; cleanup_module(); g_want_exit = 1; }

static void write_pidfile(void) {
    FILE *f = fopen(g_pid_path, "w");
    if (f) { fprintf(f, "%d\n", (int)getpid()); fclose(f); }
}

static void unlink_pidfile(void) {
    remove(g_pid_path);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: khtpm_open_hai_render.+x <house_root> [--data-root <dir>] [--title <label>] [--dump-and-exit]\n"); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    /* PER-INSTANCE DATA ROOT (2026-08-24, cursword chat): optional
     * --data-root redirects sessions/state/audit/pidfile to one
     * self-contained dir so a SECOND instance of this same shared binary
     * can run next to plain open-hai with its OWN session history (same
     * interface/binary rule). --title only names the X window. Emoji tile
     * registry stays house-shared (assets, not data). Forwarded to the
     * manager via launch_module()'s own arg pass-through below; plain
     * button.sh launches (no flags) are byte-for-byte unchanged behavior. */
    char data_root[PATH_BUF] = "";
    const char *title_override = NULL;
    int dump_and_exit = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--dump-and-exit") == 0) dump_and_exit = 1;
        else if (strcmp(argv[i], "--data-root") == 0 && i + 1 < argc) snprintf(data_root, sizeof(data_root), "%s", argv[++i]);
        else if (strcmp(argv[i], "--title") == 0 && i + 1 < argc) title_override = argv[++i];
    }
    int per_instance = data_root[0] == '/';
    if (per_instance) {
        snprintf(g_data_root, sizeof(g_data_root), "%s", data_root);
        snprintf(g_sessions_root, sizeof(g_sessions_root), "%s/sessions", data_root);
        snprintf(g_audit_dir, sizeof(g_audit_dir), "%s/audit", data_root);
        snprintf(g_pid_path, sizeof(g_pid_path), "%s/audit/open-hai.pid", data_root);
    } else {
        snprintf(g_sessions_root, sizeof(g_sessions_root), "%s/&.widgits/open-hai/sessions", g_house_root);
        snprintf(g_audit_dir, sizeof(g_audit_dir), "%s/%s", g_house_root, AUDIT_DIR_REL);
        snprintf(g_pid_path, sizeof(g_pid_path), "%s/%s", g_house_root, AUDIT_DIR_REL "/open-hai.pid");
    }
    /* flag-parse smoke test: resolve paths, print them, touch nothing */
    if (dump_and_exit) {
        printf("house=%s\nsessions=%s\naudit=%s\npidfile=%s\ntitle=%s\n",
               g_house_root, g_sessions_root, g_audit_dir, g_pid_path,
               title_override ? title_override : "open-hai");
        return 0;
    }
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);
    snprintf(g_emoji_dir, sizeof(g_emoji_dir), "%s/%s", g_house_root, AUDIT_EMOJI_REL);
    mkdir(g_audit_dir, 0755);
    mkdir(g_emoji_dir, 0755);
    write_pidfile();

    XSetErrorHandler(nonfatal_x_error);
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "open-hai: cannot open display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    vis = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    cmap = XCreateColormap(dpy, RootWindow(dpy, screen), vis, AllocNone);

    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
    swa.background_pixel = 0x141414;
    swa.border_pixel = 0;

    win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)g_win_w, (unsigned)g_win_h,
                         0, depth, InputOutput, vis,
                         CWColormap | CWEventMask | CWBackPixel | CWBorderPixel, &swa);
    XStoreName(dpy, win, title_override ? title_override : "open-hai");

    /* Managed window + _MOTIF_WM_HINTS, decorations=0 - the real
     * keyboard-focus fix (HOUSE_STDS #21), NOT override_redirect. */
    Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    struct { unsigned long flags, functions, decorations; long input_mode; unsigned long status; } mwm = {2, 0, 0, 0, 0};
    XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)&mwm, 5);

    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    XSizeHints *sh = XAllocSizeHints();
    sh->flags = PMinSize;
    sh->min_width = MIN_WIN_W; sh->min_height = MIN_WIN_H;
    XSetWMNormalHints(dpy, win, sh);
    XFree(sh);

    XMapWindow(dpy, win);
    /* Force the position after mapping - window manager may try to remember
     * old position from previous session, so explicitly set it here. */
    XMoveWindow(dpy, win, g_win_x, g_win_y);

    gc = XCreateGC(dpy, win, 0, NULL);
    buf = XCreatePixmap(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h, (unsigned)depth);
    xftdraw_buf = XftDrawCreate(dpy, buf, vis, cmap);

    load_fonts();
    xft_color("#ececec", &col_text);
    xft_color("#a0a0a0", &col_muted);
    xft_color("#22c55e", &col_accent);
    xft_color("#ef4444", &col_danger);
    xft_color("#4d9fff", &col_user);      /* user messages - blue (unused elsewhere in the palette) */
    xft_color("#c084fc", &col_assistant); /* ai responses - purple (unused elsewhere in the palette) */
    xft_color("#9ecbff", &col_user_bright);      /* lighter blue - bullet points under user msgs */
    xft_color("#e2c4ff", &col_assistant_bright); /* lighter purple - bullet points under ai msgs */
    xft_color("#fdfdfd", &col_bullet);           /* lighter white - bullet points, neutral/sys role */
    xft_color("#8a8a8a", &col_subtext);          /* dim grey - italic indented subtext lines */

    g_px_rshift = mask_shift(vis->red_mask);
    g_px_gshift = mask_shift(vis->green_mask);
    g_px_bshift = mask_shift(vis->blue_mask);
    load_emoji_tiles();

    (void)g_backend_mode; /* referenced when BACKEND_AGENT45_LEGACY gets implemented, see file header */
    mkdir(g_sessions_root, 0755);
    load_model_choice();
    init_ipc_paths();
    /* REAL module launch (Stage 2d, 2026-08-16) - the manager owns
     * session bootstrap/scanning now (khtpm_open_hai_manager.c's own
     * main(), same "load most recent session or start fresh" behavior
     * this shell used to do itself). This shell starts empty and picks
     * up sessions.state.txt/active_session.txt/transcript.txt via its
     * own periodic poll below, same graceful-empty-then-fills-in
     * pattern already proven on db-hq/events-hq. */
    launch_module("&.widgits/open-hai/ops/+x/khtpm_open_hai_manager.+x");
    g_focus_nav = 1;

    redraw();

    /* headless verification aid (same convention as db-hq's own):
     * argv[2]=="--dump-and-exit" dumps one frame + receipt and quits
     * immediately - no need for a live human/relay round trip just to
     * prove the window renders. */
    if (argc > 2 && strcmp(argv[2], "--dump-and-exit") == 0) {
        unlink_pidfile(); /* throwaway process - don't leave/clobber a real instance's pidfile */
        dump_frame_png();
        XftDrawDestroy(xftdraw_buf);
        XFreeGC(dpy, gc);
        XFreePixmap(dpy, buf);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        return 0;
    }

    g_running = 1;
    while (g_running && !g_want_exit) {
        struct timeval tv = {0, 150000};
        fd_set fds; FD_ZERO(&fds); int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
        select(xfd + 1, &fds, NULL, NULL, &tv);

        int need_redraw = g_pending; /* keep polling curl child even with no X events */
        if (poll_agent_relay() > 0) need_redraw = 1;
        /* Stage 2d shell/manager split: pick up khtpm_open_hai_manager.c's
         * latest publishes (all mtime-gated, cheap every tick). Order
         * matters - active-session change must be checked before the
         * transcript reload so a session SWITCH forces a fresh read
         * even if the new file's mtime happens to equal the old one's. */
        if (load_active_session_if_changed()) need_redraw = 1;
        if (load_transcript_if_changed()) { need_redraw = 1; g_scroll_follow_bottom = 1; }
        if (load_sessions_state_if_changed()) need_redraw = 1;
        if (load_pending_tool_state_if_changed()) need_redraw = 1;
        if (load_busy_state_if_changed()) need_redraw = 1;
        if (load_settings_if_changed()) need_redraw = 1;
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            need_redraw = 1;
            if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) { g_running = 0; }
            else if (ev.type == ConfigureNotify) {
                if (ev.xconfigure.width != g_win_w || ev.xconfigure.height != g_win_h) {
                    g_win_w = ev.xconfigure.width; g_win_h = ev.xconfigure.height;
                    XFreePixmap(dpy, buf);
                    buf = XCreatePixmap(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h, (unsigned)depth);
                    XftDrawDestroy(xftdraw_buf);
                    xftdraw_buf = XftDrawCreate(dpy, buf, vis, cmap);
                }
            } else if (ev.type == ButtonPress && ev.xbutton.y < CHROME_H) {
                /* close button lives IN the chrome bar - check it before
                 * falling back to drag, same precedence db-hq's own
                 * handle_click() uses for its synthetic close element. */
                int hit_close = 0;
                for (int i = 0; i < g_n_nav; i++) {
                    if (g_nav[i].kind != NAV_CLOSE) continue;
                    NavItem *it = &g_nav[i];
                    if (ev.xbutton.x >= it->x && ev.xbutton.x < it->x + it->w &&
                        ev.xbutton.y >= it->y && ev.xbutton.y < it->y + it->h) {
                        g_focus_nav = i + 1;
                        activate_focused();
                        hit_close = 1;
                    }
                    break;
                }
                if (!hit_close) {
                    g_dragging = 1; g_drag_start_x = ev.xbutton.x_root; g_drag_start_y = ev.xbutton.y_root;
                    g_drag_win_x0 = g_win_x; g_drag_win_y0 = g_win_y;
                }
            } else if (ev.type == ButtonPress) {
                if (ev.xbutton.x >= g_win_w - RESIZE_GRIP && ev.xbutton.y >= g_win_h - RESIZE_GRIP) {
                    g_resizing = 1; g_resize_start_x = ev.xbutton.x_root; g_resize_start_y = ev.xbutton.y_root;
                    g_resize_w0 = g_win_w; g_resize_h0 = g_win_h;
                } else {
                    handle_mouse_click(ev.xbutton.x, ev.xbutton.y);
                }
            } else if (ev.type == ButtonRelease) {
                g_dragging = 0;
                g_resizing = 0;
            } else if (ev.type == MotionNotify && g_resizing) {
                int nw = g_resize_w0 + (ev.xmotion.x_root - g_resize_start_x);
                int nh = g_resize_h0 + (ev.xmotion.y_root - g_resize_start_y);
                if (nw < MIN_WIN_W) nw = MIN_WIN_W;
                if (nh < MIN_WIN_H) nh = MIN_WIN_H;
                /* ConfigureNotify below owns g_win_w/g_win_h + the pixmap
                 * rebuild (it only rebuilds when the event size differs) */
                XResizeWindow(dpy, win, (unsigned)nw, (unsigned)nh);
            } else if (ev.type == MotionNotify && g_dragging) {
                g_win_x = g_drag_win_x0 + (ev.xmotion.x_root - g_drag_start_x);
                g_win_y = g_drag_win_y0 + (ev.xmotion.y_root - g_drag_start_y);
                XMoveWindow(dpy, win, g_win_x, g_win_y);
            } else if (ev.type == KeyPress) {
                char buf_ch[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, buf_ch, sizeof(buf_ch), &ks, NULL);
                handle_key(ks, n > 0 ? buf_ch[0] : 0);
                /* Escape only disarms the composer now, it does NOT
                 * close the window - real nav-indexed close button is
                 * the standard close mechanism (see draw_close_button()'s
                 * own header comment). */
            }
        }
        if (need_redraw) redraw();
    }

    cleanup_module(); /* covers the normal exit path (close button/wm_delete) - handle_sigterm() already covers the signal path */
    unlink_pidfile();
    XftDrawDestroy(xftdraw_buf);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
