/* chat_hai_projector.c - compiled UI projector for chat-hai.xhtpm.
 *
 * Replaces the bash projector ops/chat_hai_projector.sh (bash is not an
 * allowed projector - HANDOFF-scope-nav-and-chtpm-port.md §5/§6). Same
 * job: read chat_hai_loop.sh's own live state files and publish a
 * key=value UI file for the shared renderer's static template. This one
 * writes <app>/state/ui.txt (consumed via <window vars="state/ui.txt">)
 * instead of regenerating chat-hai.chtpm markup.
 *
 * Launched as a <module> of chat-hai.xhtpm (generic launch_module():
 * argv = [self, house_root, package_dir]; env KHTPM_PKG = package_dir =
 * the chat-hai app dir, KHTPM_HOUSE = house root). SIGTERM'd when the
 * renderer window closes.
 *
 * Inputs  (all under <app>/ , written by chat_hai_loop.sh + ch_item.sh)
 *   state/sessions/active.txt      name of the live session
 *   state/sessions/<name>.ledger   one per session; "[ts] speaker: text | Trigger: ..."
 *   state/paused.txt               "1" = stopped
 *   state/typing.txt               persona name currently generating, or empty
 *   chat_hai_config.pdl            "SECTION | sleep_between | <secs>"
 * Output  (<app>/state/ui.txt)     written ONLY when the content changes.
 *
 * Old bash projector -> new key mapping (see PROGRESS-chat-hai-xhtpm.md):
 *   <item id="sN" label="> name">        -> session_N_label / session_N_name / sessions_count
 *   <text id="status" label="[running]"> -> status
 *   <item id="pause" label="Stop">       -> pause_label
 *   <item id="speed" label="Speed: 6s">  -> speed_label
 *   <text id="msgM" class="msg-<slug>">  -> msg_M_class / msg_M_text / msgs_count
 *   typing indicator                     -> is_typing / typing_banner
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define UIBUF     (256 * 1024)
#define MAXLINES  8000
#define TAIL_MSGS 60
#define MSG_CAP   600

static char g_app[PATH_MAX];   /* the chat-hai app dir */

/* strip the last `n` path components from `p` in place */
static void up_n(char *p, int n) {
    for (int i = 0; i < n; i++) {
        char *s = strrchr(p, '/');
        if (s) *s = 0;
    }
}

static void resolve_app_dir(void) {
    const char *pkg = getenv("KHTPM_PKG");
    if (pkg && pkg[0]) { snprintf(g_app, sizeof(g_app), "%s", pkg); return; }
    /* fallback: <app>/ops/+x/chat_hai_projector.+x -> up 3 */
    char exe[PATH_MAX];
    ssize_t k = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (k > 0) { exe[k] = 0; up_n(exe, 3); snprintf(g_app, sizeof(g_app), "%s", exe); return; }
    snprintf(g_app, sizeof(g_app), ".");
}

/* read whole small file, NUL-terminate; returns length or -1 */
static long slurp(const char *path, char *buf, size_t cap) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = 0; return -1; }
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = 0;
    return (long)n;
}

/* first line, whitespace-trimmed both ends */
static void read_line1(const char *path, char *out, size_t cap) {
    FILE *f = fopen(path, "r");
    out[0] = 0;
    if (!f) return;
    if (fgets(out, (int)cap, f)) out[strcspn(out, "\r\n")] = 0;
    fclose(f);
    char *p = out; while (*p == ' ' || *p == '\t') p++;
    if (p != out) memmove(out, p, strlen(p) + 1);
    size_t l = strlen(out);
    while (l && (out[l-1] == ' ' || out[l-1] == '\t')) out[--l] = 0;
}

static void put(char *ui, size_t *off, const char *fmt, ...) {
    if (*off >= UIBUF) return;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(ui + *off, UIBUF - *off, fmt, ap);
    va_end(ap);
    if (n > 0) *off += (size_t)n;
}

/* '|' is the frame-dump field separator and newlines break key=value -
 * scrub both from anything that lands in a label. */
static void sanitize(char *s) {
    for (char *c = s; *c; c++) {
        if (*c == '|') *c = '/';
        else if (*c == '\n' || *c == '\r' || *c == '\t') *c = ' ';
    }
}

static int cmpstr(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/* sleep_between from chat_hai_config.pdl ("SECTION | sleep_between | N") */
static int read_speed(void) {
    char path[PATH_MAX], buf[8192];
    snprintf(path, sizeof(path), "%s/chat_hai_config.pdl", g_app);
    if (slurp(path, buf, sizeof(buf)) < 0) return 6;
    for (char *ln = strtok(buf, "\n"); ln; ln = strtok(NULL, "\n")) {
        /* split on '|' into <=3 fields, trim each */
        char *f[3] = {0}; int nf = 0;
        char *p = ln;
        while (nf < 3) {
            f[nf++] = p;
            char *bar = strchr(p, '|');
            if (!bar) break;
            *bar = 0; p = bar + 1;
        }
        if (nf < 3) continue;
        for (int i = 0; i < nf; i++) {
            char *s = f[i]; while (*s == ' ' || *s == '\t') s++;
            f[i] = s;
            size_t l = strlen(s);
            while (l && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = 0;
        }
        if (strcmp(f[1], "sleep_between") == 0) {
            int v = atoi(f[2]);
            return v > 0 ? v : 6;
        }
    }
    return 6;
}

static void project(char *ui, size_t *off) {
    char path[PATH_MAX], buf[UIBUF];

    /* --- active session --- */
    char active[256];
    snprintf(path, sizeof(path), "%s/state/sessions/active.txt", g_app);
    read_line1(path, active, sizeof(active));
    if (!active[0]) snprintf(active, sizeof(active), "main");

    /* --- paused / typing --- */
    char paused[16], typing[256];
    snprintf(path, sizeof(path), "%s/state/paused.txt", g_app);
    read_line1(path, paused, sizeof(paused));
    snprintf(path, sizeof(path), "%s/state/typing.txt", g_app);
    read_line1(path, typing, sizeof(typing));
    sanitize(typing);
    int is_paused = (strcmp(paused, "1") == 0);
    int is_typing = (!is_paused && typing[0] != 0);

    if (is_paused)
        put(ui, off, "status=[stopped]\n");
    else if (is_typing)
        put(ui, off, "status=[running] %s is typing...\n", typing);
    else
        put(ui, off, "status=[running]\n");

    put(ui, off, "is_typing=%d\n", is_typing);
    put(ui, off, "typing_banner=%s is typing...\n", is_typing ? typing : "");
    put(ui, off, "pause_label=%s\n", is_paused ? "Start" : "Stop");
    put(ui, off, "speed_label=Speed: %ds (click to cycle)\n", read_speed());
    put(ui, off, "active_name=%s\n", active);

    /* --- session list --- */
    char *names[512]; int nn = 0;
    snprintf(path, sizeof(path), "%s/state/sessions", g_app);
    DIR *d = opendir(path);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && nn < 512) {
            const char *e = strrchr(de->d_name, '.');
            if (!e || strcmp(e, ".ledger") != 0) continue;
            size_t blen = e - de->d_name;
            char *nm = malloc(blen + 1);
            memcpy(nm, de->d_name, blen); nm[blen] = 0;
            names[nn++] = nm;
        }
        closedir(d);
    }
    qsort(names, nn, sizeof(char *), cmpstr);
    for (int i = 0; i < nn; i++) {
        char label[300];
        snprintf(label, sizeof(label), "%s%s",
                 strcmp(names[i], active) == 0 ? "> " : "  ", names[i]);
        sanitize(label);
        put(ui, off, "session_%d_name=%s\n", i, names[i]);
        put(ui, off, "session_%d_label=%s\n", i, label);
    }
    put(ui, off, "sessions_count=%d\n", nn);

    /* --- transcript of the active session --- */
    snprintf(path, sizeof(path), "%s/state/sessions/%s.ledger", g_app, active);
    long len = slurp(path, buf, sizeof(buf));
    int m = 0;
    if (len >= 0) {
        char *lines[MAXLINES]; int nl = 0;
        for (char *ln = strtok(buf, "\n"); ln && nl < MAXLINES; ln = strtok(NULL, "\n"))
            lines[nl++] = ln;
        int start = nl > TAIL_MSGS ? nl - TAIL_MSGS : 0;
        for (int i = start; i < nl; i++) {
            char *line = lines[i];
            if (!line[0]) continue;
            /* strip "[ts] " prefix */
            char *rest = strstr(line, "] ");
            rest = rest ? rest + 2 : line;
            /* speaker : text */
            char *colon = strchr(rest, ':');
            char speaker[128], text[MSG_CAP];
            if (colon) {
                size_t sl = (size_t)(colon - rest);
                if (sl >= sizeof(speaker)) sl = sizeof(speaker) - 1;
                memcpy(speaker, rest, sl); speaker[sl] = 0;
                char *tp = colon + 1;
                while (*tp == ' ') tp++;
                snprintf(text, sizeof(text), "%s", tp);
            } else {
                speaker[0] = 0;
                snprintf(text, sizeof(text), "%s", rest);
            }
            /* drop trailing " | Trigger: ..." */
            char *trig = strstr(text, " | Trigger:");
            if (trig) *trig = 0;
            /* slug of speaker: lowercase alnum only */
            char slug[64]; int sj = 0;
            for (char *c = speaker; *c && sj < 63; c++)
                if (isalnum((unsigned char)*c)) slug[sj++] = (char)tolower((unsigned char)*c);
            slug[sj] = 0;
            if (!slug[0]) snprintf(slug, sizeof(slug), "other");

            char row[MSG_CAP + 160];
            snprintf(row, sizeof(row), "%s%s%s",
                     speaker[0] ? speaker : "", speaker[0] ? ": " : "", text);
            sanitize(row);
            /* keep labels short - long rows bloat / risk the frame-dump
             * round-trip and never fully render in a chat row anyway. */
            if (strlen(row) > 200) {
                int cut = 197;
                /* don't sever a UTF-8 multibyte sequence */
                while (cut > 0 && ((unsigned char)row[cut] & 0xC0) == 0x80) cut--;
                row[cut] = '.'; row[cut+1] = '.'; row[cut+2] = '.'; row[cut+3] = 0;
            }
            put(ui, off, "msg_%d_class=chat-msg msg-%s\n", m, slug);
            put(ui, off, "msg_%d_text=%s\n", m, row);
            m++;
        }
    }
    put(ui, off, "msgs_count=%d\n", m);
    put(ui, off, "has_msgs=%d\n", m > 0 ? 1 : 0);
    put(ui, off, "empty_hint=%s\n", m > 0 ? "" : "(no messages in this session yet)");

    for (int i = 0; i < nn; i++) free(names[i]);
}

int main(void) {
    resolve_app_dir();

    char out[PATH_MAX], tmp[PATH_MAX];
    snprintf(out, sizeof(out), "%s/state/ui.txt", g_app);
    snprintf(tmp, sizeof(tmp), "%s/state/ui.txt.tmp", g_app);

    /* make sure state/ exists so the first write lands */
    { char sd[PATH_MAX]; snprintf(sd, sizeof(sd), "%s/state", g_app); mkdir(sd, 0777); }

    static char ui[UIBUF], last[UIBUF];
    last[0] = 0;

    for (;;) {
        size_t off = 0;
        ui[0] = 0;
        project(ui, &off);
        if (strcmp(ui, last) != 0) {
            FILE *f = fopen(tmp, "w");
            if (f) { fputs(ui, f); fclose(f); rename(tmp, out); }
            memcpy(last, ui, off + 1 < UIBUF ? off + 1 : UIBUF);
            last[UIBUF - 1] = 0;
        }
        usleep(400000);
    }
    return 0;
}
