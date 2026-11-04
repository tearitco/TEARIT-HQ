/* hai_op.c - the editor's h-ai: locate the live session, publish its context, ask an API model for an event-page edit, propose it, apply it only on the owner's say-so (sprint 2026-11-04,
 * SPRINT-2026-11-04-EDITOR-STANDARDS-HAI.md). A separate process (house rule: fork/exec + files, nothing linked); the editor daemon starts `ask` through kh_spawn.sh.
 *   hai_op <house> locate [--pid N]   print the verified session (window pid alive + ours + bv_session) or a refusal; exit 0 / 3
 *   hai_op <house> context            (re)write state/hai_context.txt (< 2 KB, whole file, no secrets) and print it
 *   hai_op <house> ask <text>         build the request from the context + the event pages the text names, call the API (curl, niced, 90 s cap), then either answer in the chat or write
 *                                     state/hai_proposal.txt (pkg/page/file + the new content) and say so in the chat; the owner types /apply or /no
 *   hai_op <house> apply | discard    write the proposal (after validating it again) / drop it
 * Safety: the model may only call write_event_file(pkg, page, file, content) with file in {condition.pdl, event.ir.pdl} of an EXISTING events/<pkg>/event_pkg/pages/<page>/; content must be a
 * PDL table (SECTION header, dashed line, rows with ' | ') under 8 KB; nothing is written until /apply; the previous file is kept as <file>.hai.bak. The key is read from open-hai/state/raw_groq.txt
 * (gitignored), sent through a curl header file that is deleted after the call, never printed or logged. Quota guard: >= 3 s between asks, at most HAI_DAILY (200) asks per day
 * (Groq free tier: 1000 requests/day, 8000 tokens/min). Files written: state/hai_context.txt (<2 KB), hai_proposal.txt (<9 KB), hai_calls.txt (1 line), hai_chat.txt (capped 24 KB -> 8 KB). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define HAI_DAILY 200
#define MODEL "openai/gpt-oss-20b"
static char HOUSE[PATH_MAX], APP[PATH_MAX];
static void P(char *o, size_t n, const char *rel) { snprintf(o, n, "%s/%s", APP, rel); }
static int rd(const char *p, char *b, size_t n) { FILE *f = fopen(p, "r"); if (!f) { b[0] = 0; return -1; } size_t k = fread(b, 1, n - 1, f); b[k] = 0; fclose(f); return (int)k; }
static int wr(const char *p, const char *s) { char t[PATH_MAX + 8]; snprintf(t, sizeof t, "%s.tmp", p); FILE *f = fopen(t, "w"); if (!f) return -1; fputs(s, f); fclose(f); return rename(t, p); }
static void kv(const char *file, const char *key, char *o, size_t n) { char b[4096]; o[0] = 0; if (rd(file, b, sizeof b) < 0) return; size_t kl = strlen(key); for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, key, kl) && l[kl] == '=') { snprintf(o, n, "%s", l + kl + 1); return; } }
static void chat(const char *who, const char *text) {      /* same file and cap as the daemon's hai_line */
    char p[PATH_MAX]; P(p, sizeof p, "state/hai_chat.txt"); FILE *f = fopen(p, "a"); if (!f) return; char one[600]; size_t oi = 0; for (const unsigned char *c = (const unsigned char *)text; *c && oi + 4 < sizeof one; c++) {      /* the window font has no glyph for the model's typographic characters: map them to ASCII */
        if (c[0] == 0xE2 && c[1] == 0x80 && c[2]) { unsigned char k = c[2]; const char *r = (k == 0x91 || k == 0x93 || k == 0x94 || k == 0x90) ? "-" : (k == 0x98 || k == 0x99) ? "'" : (k == 0x9C || k == 0x9D) ? "\"" : (k == 0xA6) ? "..." : "?"; for (; *r; r++) one[oi++] = *r; c += 2; }
        else if (c[0] >= 0x80) { if (c[0] >= 0xC0) one[oi++] = '?'; }
        else one[oi++] = (*c == '\n' || *c == '\r') ? ' ' : (char)*c; } one[oi] = 0; fprintf(f, "%s: %s\n", who, one); fclose(f);
    struct stat st; if (stat(p, &st) == 0 && st.st_size > 24000) { char *b = malloc(24100); if (!b) return; int k = rd(p, b, 24100); if (k > 8000) { char *q = strchr(b + k - 8000, '\n'); wr(p, q ? q + 1 : b + k - 8000); } free(b); } }
static int pid_ok(long pid) { char p[64], b[1024]; snprintf(p, sizeof p, "/proc/%ld/cmdline", pid); FILE *f = fopen(p, "r"); if (!f) return 0; size_t k = fread(b, 1, sizeof b - 1, f); fclose(f); for (size_t i = 0; i < k; i++) if (!b[i]) b[i] = ' '; b[k] = 0; return strstr(b, "khtpm_core_render") && strstr(b, "rpg-edit-hq"); }
static int locate(long hint, char *why, size_t wn, long *pid_out, char *sess, size_t sn) {
    char f[PATH_MAX], b[64]; P(f, sizeof f, "board/module_parent.pid"); rd(f, b, sizeof b); long pid = atol(b); if (pid <= 0) { snprintf(why, wn, "no window pid file: the editor window is not open"); return 3; }
    if (hint > 0 && hint != pid) { snprintf(why, wn, "refused: pid %ld is not this editor's window (it is %ld)", hint, pid); return 3; }
    if (!pid_ok(pid)) { snprintf(why, wn, "refused: window pid %ld is not a live rpg-edit-hq window", pid); return 3; }
    P(f, sizeof f, "board/state/ui.txt"); kv(f, "bv_session", sess, sn); if (!sess[0] || access(sess, F_OK) != 0) { snprintf(why, wn, "window %ld is live but has no engine session yet", pid); *pid_out = pid; return 4; }
    *pid_out = pid; snprintf(why, wn, "ok"); return 0; }
/* --- the active-session pointer (#.desktop/active_session.pdl): one row per app, rewritten whole, at most 16 rows; the toolbar h-ai reads it to find the editor session it should talk about.
 * Row: ACTIVE | <app> | <window pid> | <epoch> | <context file>. A reader never trusts a row: it re-checks the pid against /proc (alive + the right program) before using it. */
static void pointer_path(char *o, size_t n) { snprintf(o, n, "%s/#.desktop/active_session.pdl", HOUSE); }
static void pointer_set(long pid, const char *ctxfile) {
    char pf[PATH_MAX], old[6000], out[6400]; pointer_path(pf, sizeof pf); rd(pf, old, sizeof old); size_t k = 0; int rows = 0;
    k += snprintf(out + k, sizeof out - k, "SECTION | KEY | VALUE\n----------------------------------------\n");
    char *sv; for (char *l = strtok_r(old, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { if (strncmp(l, "ACTIVE |", 8) || strstr(l, "| rpg-edit-hq |")) continue; if (rows++ >= 14 || k + strlen(l) + 2 > sizeof out - 400) break; k += snprintf(out + k, sizeof out - k, "%s\n", l); }
    snprintf(out + k, sizeof out - k, "ACTIVE | rpg-edit-hq | %ld | %ld | %s\n", pid, (long)time(NULL), ctxfile); wr(pf, out); }
static void context(char *out, size_t on) {
    char why[200], sess[PATH_MAX] = "", f[PATH_MAX], book[200], room[64], brush[200], pos[64]; long pid = 0; int rc = locate(0, why, sizeof why, &pid, sess, sizeof sess);
    P(f, sizeof f, "state/last_book.txt"); rd(f, book, sizeof book); book[strcspn(book, "\r\n")] = 0; if (!strncmp(book, "book=", 5)) memmove(book, book + 5, strlen(book + 4));
    P(f, sizeof f, "state/trainer.txt"); kv(f, "room", room, sizeof room);
    P(f, sizeof f, "board/state/brush.txt"); kv(f, "sprite", brush, sizeof brush); if (!brush[0]) kv(f, "glyph", brush, sizeof brush);
    char xp[PATH_MAX]; snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", sess[0] ? sess : "/nonexistent"); char px[24] = "", py[24] = ""; if (sess[0]) { snprintf(xp, sizeof xp, "%s/rpg-edit-hq-pchq/pieces/xelector_01/state.txt", APP); kv(xp, "pos_x", px, sizeof px); kv(xp, "pos_y", py, sizeof py); }
    snprintf(pos, sizeof pos, "%s,%s", px, py);
    char md[16]; P(f, sizeof f, "state/flag_chatmode.txt"); rd(f, md, sizeof md); md[strcspn(md, "\r\n")] = 0;
    snprintf(out, on, "app=rpg-edit-hq\nwindow_pid=%ld\nsession_status=%s\nbv_session=%s\nbook=%s\nroom=%s\nselector=%s\narmed_brush=%s\nchat_mode=%s\nupdated=%ld\n", pid, rc == 0 ? "live" : why, sess, book, room, pos, brush, md, (long)time(NULL));
    P(f, sizeof f, "state/hai_context.txt"); wr(f, out);
    if (rc == 0) pointer_set(pid, f); }
static int shape_ok(const char *c) { if (strlen(c) > 8000 || strncmp(c, "SECTION", 7)) return 0; const char *nl = strchr(c, '\n'); if (!nl || strncmp(nl + 1, "----", 4)) return 0; int rows = 0; char *cp = strdup(nl + 1); int ok = 1; char *sv; for (char *l = strtok_r(cp, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { if (!strncmp(l, "----", 4) || !l[strspn(l, " \t")]) continue; if (!strstr(l, " | ") && !strstr(l, "|")) { ok = 0; break; } rows++; } free(cp); return ok && rows > 0; }
static int name_ok(const char *s) { if (!*s || strlen(s) > 60) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '-')) return 0; return 1; }
static int target(const char *pkg, const char *page, const char *file, char *path, size_t pn, char *why, size_t wn) {
    if (!name_ok(pkg) || !name_ok(page)) { snprintf(why, wn, "bad event or page name"); return 0; } if (strcmp(file, "condition.pdl") && strcmp(file, "event.ir.pdl")) { snprintf(why, wn, "file %s is not allowed (only condition.pdl, event.ir.pdl)", file); return 0; }
    char d[PATH_MAX]; snprintf(d, sizeof d, "%s/events/%s/event_pkg/pages/%s", APP, pkg, page); struct stat st; if (stat(d, &st) || !S_ISDIR(st.st_mode)) { snprintf(why, wn, "no such event page %s/%s", pkg, page); return 0; }
    snprintf(path, pn, "%s/%s", d, file); return 1; }
/* --- tiny JSON string extraction (the response is one object; we only need a few string fields) --- */
static int jstr(const char *j, const char *key, char *o, size_t n) {      /* first "key":"value" at or after j; unescapes \n \" \\ \t \uXXXX(ASCII only) */
    char pat[80]; snprintf(pat, sizeof pat, "\"%s\"", key); const char *p = strstr(j, pat); if (!p) return 0; p += strlen(pat); while (*p == ' ' || *p == ':') p++; if (*p != '"') return 0; p++; size_t k = 0;
    while (*p && *p != '"' && k + 1 < n) { if (*p == '\\') { p++; if (*p == 'n') o[k++] = '\n'; else if (*p == 't') o[k++] = '\t'; else if (*p == 'r') {} else if (*p == 'u') { unsigned v = 0; sscanf(p + 1, "%4x", &v); o[k++] = v < 128 ? (char)v : '?'; p += 4; } else o[k++] = *p; p++; } else o[k++] = *p++; } o[k] = 0; return 1; }
static void jesc(FILE *f, const char *s) { for (; *s; s++) { if (*s == '"' || *s == '\\') fprintf(f, "\\%c", *s); else if (*s == '\n') fputs("\\n", f); else if (*s == '\r') {} else if (*s == '\t') fputs("\\t", f); else if ((unsigned char)*s < 32) {} else fputc(*s, f); } }
static int quota(void) {      /* 1 = ok, 0 = refuse; the counter file is one line "YYYY-MM-DD count epoch" */
    char f[PATH_MAX], b[128], day[16]; time_t now = time(NULL); struct tm tm; localtime_r(&now, &tm); strftime(day, sizeof day, "%Y-%m-%d", &tm); P(f, sizeof f, "state/hai_calls.txt"); rd(f, b, sizeof b);
    char d0[16] = ""; int cnt = 0; long last = 0; sscanf(b, "%15s %d %ld", d0, &cnt, &last); if (strcmp(d0, day)) cnt = 0; if (now - last < 3 && !strcmp(d0, day)) return 0; if (cnt >= HAI_DAILY) return 0;
    snprintf(b, sizeof b, "%s %d %ld\n", day, cnt + 1, (long)now); wr(f, b); return 1; }
static int name_words(const char *name, const char *low) {      /* every '_'-separated word of the event name occurs as a whole word in the (lower-cased, space padded) text: "dawn phase event" finds phase_dawn */
    char nm[80]; snprintf(nm, sizeof nm, "%s", name); int any = 0; char *sv; for (char *t = strtok_r(nm, "_", &sv); t; t = strtok_r(NULL, "_", &sv)) { char w[90]; snprintf(w, sizeof w, " %s ", t); if (!strstr(low, w)) return 0; any = 1; } return any; }
static void pages_text(const char *text, char *o, size_t on) {      /* the event pages the user's words name (a whole-word match of an events/<pkg> directory), capped */
    char d[PATH_MAX]; snprintf(d, sizeof d, "%s/events", APP); DIR *dir = opendir(d); size_t k = 0; o[0] = 0; char list[800] = ""; int nl = 0; if (!dir) return; struct dirent *e;
    char low[400]; snprintf(low, sizeof low, " %s ", text); for (char *c = low; *c; c++) { if (*c >= 'A' && *c <= 'Z') *c += 32; if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-' || *c == ' ')) *c = ' '; }
    while ((e = readdir(dir))) { if (e->d_name[0] == '.') continue; if (nl + strlen(e->d_name) + 2 < sizeof list) { nl += snprintf(list + nl, sizeof list - nl, "%s ", e->d_name); }
        if (!name_words(e->d_name, low) || k > on - 3000) continue; char pg[PATH_MAX]; snprintf(pg, sizeof pg, "%s/%s/event_pkg/pages", d, e->d_name); DIR *pd = opendir(pg); if (!pd) continue; struct dirent *q;
        while ((q = readdir(pd))) { if (q->d_name[0] == '.') continue; const char *fs[] = { "condition.pdl", "event.ir.pdl" }; for (int i = 0; i < 2; i++) { char fp[PATH_MAX], b[3000]; snprintf(fp, sizeof fp, "%s/%s/%s", pg, q->d_name, fs[i]); if (rd(fp, b, sizeof b) >= 0 && k < on - 3100) k += snprintf(o + k, on - k, "\nEVENT %s PAGE %s FILE %s:\n%s", e->d_name, q->d_name, fs[i], b); } } closedir(pd); }
    closedir(dir); if (k + 900 < on) snprintf(o + k, on - k, "\nEvents in this book: %s\n", list); }
static int run_curl(const char *body_path, const char *out_path, const char *hdr_path) {
    char hat[PATH_MAX + 2]; snprintf(hat, sizeof hat, "@%s", hdr_path); pid_t p = fork(); if (p < 0) return -1; if (p == 0) { int dn = open("/dev/null", O_WRONLY); (void)dn; char *av[] = { "nice", "-n", "15", "curl", "-s", "--max-time", "90", "-o", (char *)out_path, "-H", hat, "-H", "Content-Type: application/json", "--data-binary", NULL, "https://api.groq.com/openai/v1/chat/completions", NULL };
        char at[PATH_MAX + 2]; snprintf(at, sizeof at, "@%s", body_path); av[14] = at; execvp("nice", av); _exit(127); }
    int st = 0; waitpid(p, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
static void ask(const char *text) {
    char ctx[2200]; context(ctx, sizeof ctx); chat("you", text);
    if (strstr(ctx, "session_status=live") == NULL) { chat("h-ai", "I cannot find the live editor session (see state/hai_context.txt); open the editor window first."); return; }
    if (!quota()) { chat("h-ai", "slow down: at most one ask per 3 s and 200 a day (free tier guard)."); return; }
    char kf[PATH_MAX], key[200]; snprintf(kf, sizeof kf, "%s/&.widgits/open-hai/state/raw_groq.txt", HOUSE); rd(kf, key, sizeof key); key[strcspn(key, "\r\n ")] = 0; if (strncmp(key, "gsk_", 4)) { chat("h-ai", "no API key configured (open-hai/state/raw_groq.txt)."); return; }
    char hdr[PATH_MAX], body[PATH_MAX], out[PATH_MAX]; P(hdr, sizeof hdr, "state/.hai_hdr"); P(body, sizeof body, "state/.hai_req.json"); P(out, sizeof out, "state/.hai_resp.json");
    { int fd = open(hdr, O_WRONLY | O_CREAT | O_TRUNC, 0600); if (fd < 0) return; FILE *hf = fdopen(fd, "w"); fprintf(hf, "Authorization: Bearer %s\n", key); fclose(hf); }
    static char pages[9000]; pages_text(text, pages, sizeof pages);
    FILE *bf = fopen(body, "w"); if (!bf) { unlink(hdr); return; }
    fprintf(bf, "{\"model\":\"%s\",\"temperature\":0,\"max_tokens\":1500,\"tools\":[{\"type\":\"function\",\"function\":{\"name\":\"write_event_file\",\"description\":\"Propose the full new content of one file of one event page. The owner reviews it before it is applied.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"pkg\":{\"type\":\"string\"},\"page\":{\"type\":\"string\"},\"file\":{\"type\":\"string\",\"enum\":[\"condition.pdl\",\"event.ir.pdl\"]},\"content\":{\"type\":\"string\"}},\"required\":[\"pkg\",\"page\",\"file\",\"content\"]}}}],\"messages\":[{\"role\":\"system\",\"content\":\"", MODEL);
    jesc(bf, "You are h-ai inside the rpg-edit-hq game editor. Event pages are pipe-separated PDL tables: a 'SECTION | KEY | VALUE' header, a dashed line, then rows. To change an event call write_event_file with the COMPLETE new file, keeping every row you were not asked to change exactly as it is. The pkg argument is an event name from the EVENT lines (for example buy), NEVER the book name; page is the PAGE name shown for that event (for example page_1). Never touch anything else. If the request is not about editing an event of this book, answer in one or two plain sentences and call no tool.\nEditor context:\n");
    jesc(bf, "\nNode rows look like: NODE | id=1 type=rpg_edit_hq_verb | verb=phase|arg=dawn   and   NODE | id=3 type=pet_says | pet=ember|chat=Good morning|voice=Good morning, everyone. To make an event do one more thing, append a NODE row with the next free id (keep every existing row exactly as it is). You may write only node types rpg_edit_hq_verb (verb and arg) and pet_says (pet = a pet id below, chat = the text shown in the chat WITHOUT the pet's name (the game adds the name itself), voice = the spoken line); any other existing node type must be kept unchanged. Plain text only: no quotes, dollar signs or backslashes inside values.\nPet ids: ");
    { char sd[PATH_MAX]; snprintf(sd, sizeof sd, "%s/state", APP); DIR *pd = opendir(sd); char pl[500] = ""; if (pd) { struct dirent *e; while ((e = readdir(pd))) if (!strncmp(e->d_name, "pet_", 4) && strstr(e->d_name, ".txt") && strlen(pl) + strlen(e->d_name) < 440) { char nm[64]; snprintf(nm, sizeof nm, "%.*s", (int)(strlen(e->d_name) - 8), e->d_name + 4); strcat(pl, nm); strcat(pl, " "); } closedir(pd); } jesc(bf, pl); }
    jesc(bf, "\n");
    jesc(bf, ctx); jesc(bf, pages); fprintf(bf, "\"},{\"role\":\"user\",\"content\":\""); jesc(bf, text); fprintf(bf, "\"}]}"); fclose(bf);
    int rc = run_curl(body, out, hdr); unlink(hdr); unlink(body);
    static char resp[24000]; int n = rd(out, resp, sizeof resp); unlink(out);
    if (rc != 0 || n <= 0) { char m[120]; snprintf(m, sizeof m, "the API call failed (curl %d); nothing changed.", rc); chat("h-ai", m); return; }
    char err[300]; if (jstr(resp, "message", err, sizeof err) && strstr(resp, "\"error\"")) { char ef[PATH_MAX]; P(ef, sizeof ef, "state/hai_last_error.txt"); resp[400] = 0; wr(ef, resp); char m[360]; snprintf(m, sizeof m, "the API refused the call: %.200s", err); chat("h-ai", m); return; }
    char *tc = strstr(resp, "\"tool_calls\""); if (!tc) { char a[500]; if (!jstr(resp, "content", a, sizeof a)) snprintf(a, sizeof a, "(no answer)"); chat("h-ai", a); return; }
    static char args[12000]; if (!jstr(tc, "arguments", args, sizeof args)) { chat("h-ai", "the model's edit could not be read; nothing changed."); return; }
    char pkg[64] = "", page[64] = "", file[40] = ""; static char content[9000]; jstr(args, "pkg", pkg, sizeof pkg); jstr(args, "page", page, sizeof page); jstr(args, "file", file, sizeof file); jstr(args, "content", content, sizeof content);
    char path[PATH_MAX], why[200]; if (!target(pkg, page, file, path, sizeof path, why, sizeof why)) { char m[300]; snprintf(m, sizeof m, "rejected the model's edit: %s", why); chat("h-ai", m); return; }
    if (!shape_ok(content)) { chat("h-ai", "rejected the model's edit: the content is not a valid PDL table."); return; }
    char old[9000]; rd(path, old, sizeof old); int ol = 0, nl2 = 0; for (char *c = old; *c; c++) if (*c == '\n') ol++; for (char *c = content; *c; c++) if (*c == '\n') nl2++;
    char pf[PATH_MAX]; P(pf, sizeof pf, "state/hai_proposal.txt"); static char pb[9400]; snprintf(pb, sizeof pb, "pkg=%s\npage=%s\nfile=%s\n---\n%s", pkg, page, file, content); wr(pf, pb);
    char m[400]; snprintf(m, sizeof m, "proposal: %s/%s %s (%d -> %d lines). Type /apply to write it (old file kept as .hai.bak) or /no.", pkg, page, file, ol, nl2); chat("h-ai", m); }
/* --- the page compiler (apply of an event.ir.pdl): regenerate event.pal and the cmd_<id>.sh wrappers the way the existing pages have them. Only two node types may be WRITTEN here:
 *   rpg_edit_hq_verb | verb=<name from verbs.pdl, not a destructive one>|arg=<one word>      pet_says | pet=<id>|chat=<chat line>|voice=<spoken line>
 * any other node type must already have its cmd_<id>.sh (it is kept as it is); values with a quote, backtick, $, backslash or newline are refused. Returns 1 ok, 0 refused (why filled). */
static int plain_val(const char *v) { if (!*v || strlen(v) > 160) return 0; for (; *v; v++) if (*v == '\'' || *v == '`' || *v == '$' || *v == '\\' || *v == '\n' || (unsigned char)*v < 32) return 0; return 1; }
static int verb_ok(const char *v) { static const char *bad[] = { "reset", "newgame", "delpal", "remove", "load", "save", NULL }; for (int i = 0; bad[i]; i++) if (!strcmp(v, bad[i])) return 0; char f[PATH_MAX], b[12000]; P(f, sizeof f, "verbs.pdl"); rd(f, b, sizeof b); char *sv; for (char *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { char nm[48]; if (sscanf(l, "VERB | %47s |", nm) == 1 && !strcmp(nm, v)) return 1; } return 0; }
static int compile_page(const char *dir, const char *ir, char *why, size_t wn) {
    char pal[8000] = "# event.pal - COMPILED from event.ir.pdl by hai_op (the h-ai apply step); wrappers cmd_<id>.sh\n"; size_t pk = strlen(pal); int nodes = 0; char *cp = strdup(ir); char *sv;
    for (char *l = strtok_r(cp, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { if (strncmp(l, "NODE", 4)) continue; int id = 0; char type[48] = ""; const char *a = strchr(l, '|'); if (!a || sscanf(a + 1, " id=%d type=%47s", &id, type) != 2 || id < 1 || id > 99) { snprintf(why, wn, "a NODE row is not 'NODE | id=N type=T | ...'"); free(cp); return 0; }
        const char *fields = strchr(a + 1, '|'); if (!fields) fields = ""; else fields++;
        char cmdp[PATH_MAX]; snprintf(cmdp, sizeof cmdp, "%s/cmd_%d.sh", dir, id); char body[1400] = "";
        const char *head = "#!/bin/sh\ncd \"$(dirname \"$0\")/../../..\" || exit 1\nENT=\"${MUCHI_TARGET_ENT:-$PWD}\"\nD=\"$ENT\"\nwhile [ \"$D\" != \"/\" ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\n";
        if (!strcmp(type, "rpg_edit_hq_verb") || !strcmp(type, "pet_says")) {
            char kvs[6][2][170]; int nk = 0; char *f2 = strdup(fields), *sv2; for (char *t = strtok_r(f2, "|", &sv2); t && nk < 6; t = strtok_r(NULL, "|", &sv2)) { while (*t == ' ') t++; char *eq = strchr(t, '='); if (!eq) continue; *eq = 0; snprintf(kvs[nk][0], 170, "%s", t); char *v = eq + 1; size_t vl = strlen(v); while (vl && v[vl - 1] == ' ') v[--vl] = 0; snprintf(kvs[nk][1], 170, "%s", v); nk++; } free(f2);
            const char *g[3] = { "", "", "" };
            for (int i = 0; i < nk; i++) { if (!strcmp(type, "pet_says")) { if (!strcmp(kvs[i][0], "pet")) g[0] = kvs[i][1]; else if (!strcmp(kvs[i][0], "chat")) g[1] = kvs[i][1]; else if (!strcmp(kvs[i][0], "voice")) g[2] = kvs[i][1]; } else { if (!strcmp(kvs[i][0], "verb")) g[0] = kvs[i][1]; else if (!strcmp(kvs[i][0], "arg")) g[1] = kvs[i][1]; } }
            if (!strcmp(type, "pet_says")) { if (!plain_val(g[0]) || !plain_val(g[1]) || !plain_val(g[2])) { snprintf(why, wn, "node %d: pet_says needs pet, chat, voice (plain text, no quotes or $)", id); free(cp); return 0; } char pf[PATH_MAX]; snprintf(pf, sizeof pf, "%s/state/pet_%s.txt", APP, g[0]); if (access(pf, F_OK) != 0) { snprintf(why, wn, "node %d: no pet called %s", id, g[0]); free(cp); return 0; }
                snprintf(body, sizeof body, "%sexec \"$D/@.apps/rpg-edit-hq/ops/+x/rpg_edit_hq.+x\" pet_says '%s' '%s' '%s' \"$RPG_OTHER\"\n", head, g[0], g[1], g[2]); }
            else { if (!plain_val(g[0]) || !verb_ok(g[0]) || (g[1][0] && !plain_val(g[1]))) { snprintf(why, wn, "node %d: verb '%s' is not allowed (verbs.pdl names only, no destructive ones)", id, g[0]); free(cp); return 0; }
                if (g[1][0]) snprintf(body, sizeof body, "%sexec \"$D/@.apps/rpg-edit-hq/ops/+x/rpg_edit_hq.+x\" '%s' '%s'\n", head, g[0], g[1]); else snprintf(body, sizeof body, "%sexec \"$D/@.apps/rpg-edit-hq/ops/+x/rpg_edit_hq.+x\" '%s'\n", head, g[0]); }
            char old[1500]; if (rd(cmdp, old, sizeof old) >= 0 && strcmp(old, body)) { char bak[PATH_MAX + 12]; snprintf(bak, sizeof bak, "%s.hai.bak", cmdp); wr(bak, old); }
            if (wr(cmdp, body) != 0) { snprintf(why, wn, "could not write cmd_%d.sh", id); free(cp); return 0; } chmod(cmdp, 0755);
        } else if (access(cmdp, F_OK) != 0) { snprintf(why, wn, "node %d has type %s, which only the events-hq editor can create", id, type); free(cp); return 0; }
        if (pk + 40 < sizeof pal) pk += snprintf(pal + pk, sizeof pal - pk, "exec cmd_%d.sh\n", id); nodes++; }
    free(cp); if (!nodes) { snprintf(why, wn, "a page needs at least one NODE row"); return 0; }
    snprintf(pal + pk, sizeof pal - pk, "halt\n"); char pp[PATH_MAX], pold[8000], pbak[PATH_MAX + 12]; snprintf(pp, sizeof pp, "%s/event.pal", dir); if (rd(pp, pold, sizeof pold) >= 0) { snprintf(pbak, sizeof pbak, "%s.hai.bak", pp); wr(pbak, pold); }
    if (wr(pp, pal) != 0) { snprintf(why, wn, "could not write event.pal"); return 0; } return 1; }
static int apply(int do_it) {
    char pf[PATH_MAX], b[9400]; P(pf, sizeof pf, "state/hai_proposal.txt"); if (rd(pf, b, sizeof b) <= 0) { chat("h-ai", "nothing to apply."); return 1; }
    if (!do_it) { unlink(pf); chat("h-ai", "dropped the proposal."); return 0; }
    char pkg[64] = "", page[64] = "", file[40] = ""; kv(pf, "pkg", pkg, sizeof pkg); kv(pf, "page", page, sizeof page); kv(pf, "file", file, sizeof file); char *c = strstr(b, "\n---\n"); if (!c) { chat("h-ai", "the proposal is damaged; dropped."); unlink(pf); return 1; } c += 5;
    char path[PATH_MAX], why[200]; if (!target(pkg, page, file, path, sizeof path, why, sizeof why) || !shape_ok(c)) { chat("h-ai", "the proposal no longer validates; dropped."); unlink(pf); return 1; }
    char old[9000], bak[PATH_MAX + 12]; if (rd(path, old, sizeof old) >= 0) { snprintf(bak, sizeof bak, "%s.hai.bak", path); wr(bak, old); }
    { size_t cl = strlen(c); if (cl && c[cl - 1] != '\n' && cl + 2 < sizeof b) { c[cl] = '\n'; c[cl + 1] = 0; } }
    if (wr(path, c) != 0) { chat("h-ai", "could not write the file."); return 1; }
    int compiled = 0; if (!strcmp(file, "event.ir.pdl")) { char dir[PATH_MAX]; snprintf(dir, sizeof dir, "%s/events/%s/event_pkg/pages/%s", APP, pkg, page); if (!compile_page(dir, c, why, sizeof why)) { char bk[PATH_MAX + 12]; snprintf(bk, sizeof bk, "%s.hai.bak", path); char o2b[9000]; if (rd(bk, o2b, sizeof o2b) >= 0) wr(path, o2b); char m[360]; snprintf(m, sizeof m, "not applied: %s (the old page is back).", why); chat("h-ai", m); unlink(pf); return 1; } compiled = 1; }
    unlink(pf); char m[240]; snprintf(m, sizeof m, "applied to %s/%s %s%s.", pkg, page, file, compiled ? " and recompiled the page" : ""); chat("h-ai", m); return 0; }
/* where: the toolbar h-ai's question "which editor session is live?": newest pointer row first; each row is VERIFIED (window pid alive and the right program) before its context is printed. */
static int where(void) {
    char pf[PATH_MAX], b[6000]; pointer_path(pf, sizeof pf); if (rd(pf, b, sizeof b) <= 0) { printf("no editor session is registered (open the editor first)\n"); return 3; }
    struct { long ep; char app[40]; long pid; char ctx[PATH_MAX]; } r[16]; int n = 0; char *sv;
    for (char *l = strtok_r(b, "\n", &sv); l && n < 16; l = strtok_r(NULL, "\n", &sv)) { if (strncmp(l, "ACTIVE |", 8)) continue; char app[40]; long pid, ep; char ctx[PATH_MAX]; if (sscanf(l, "ACTIVE | %39s | %ld | %ld | %4000[^\n]", app, &pid, &ep, ctx) == 4) { r[n].ep = ep; snprintf(r[n].app, sizeof r[n].app, "%s", app); r[n].pid = pid; snprintf(r[n].ctx, sizeof r[n].ctx, "%s", ctx); n++; } }
    int skipped = 0;
    for (int pass = 0; pass < n; pass++) { int best = -1; for (int i = 0; i < n; i++) if (r[i].pid && (best < 0 || r[i].ep > r[best].ep)) best = i; if (best < 0) break;
        long pid = r[best].pid; r[best].pid = 0;
        if (!strcmp(r[best].app, "rpg-edit-hq") && pid_ok(pid)) { char c[2200]; if (rd(r[best].ctx, c, sizeof c) > 0) { printf("session: %s (window pid %ld, verified live; %d stale pointer row(s) skipped)\n%s", r[best].app, pid, skipped, c); return 0; } }
        skipped++; }
    printf("no live editor session: %d pointer row(s) checked, none verified\n", n); return 3; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: hai_op <house> locate [--pid N] | context | ask <text> | apply | discard\n"); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); snprintf(APP, sizeof APP, "%s/@.apps/rpg-edit-hq", HOUSE); const char *v = argv[2];
    if (!strcmp(v, "locate")) { long hint = (argc >= 5 && !strcmp(argv[3], "--pid")) ? atol(argv[4]) : 0; char why[200], sess[PATH_MAX] = ""; long pid = 0; int rc = locate(hint, why, sizeof why, &pid, sess, sizeof sess); if (rc == 0) printf("OK pid=%ld session=%s\n", pid, sess); else printf("%s\n", why); return rc; }
    if (!strcmp(v, "where")) return where();
    if (!strcmp(v, "context")) { char c[2200]; context(c, sizeof c); fputs(c, stdout); return 0; }
    if (!strcmp(v, "ask") && argc >= 4) { ask(argv[3]); return 0; }
    if (!strcmp(v, "apply")) return apply(1); if (!strcmp(v, "discard")) return apply(0);
    fprintf(stderr, "hai_op: unknown verb\n"); return 2; }
