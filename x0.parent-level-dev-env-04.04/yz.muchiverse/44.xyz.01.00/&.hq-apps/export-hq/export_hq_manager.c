#define _POSIX_C_SOURCE 200809L
/* export_hq_manager.c - real manager for "export-hq", the picker GUI
 * wrapped around &.hq-apps/create-package/create-package.sh.
 *
 * 2026-09-15, direct live request ("yes lets do the the gui" - the GUI
 * option chosen over a thin toy.pdl-only wrapper for create-package.sh,
 * which was itself built the same day per an earlier direct request:
 * "is there a x11-hq app we could make create-package, by letting us
 * choose from all possible package candidates... export it as an
 * independent package").
 *
 * Same proven contract as signup-hq (this file is a close structural
 * copy of signup_hq_manager.c - request.txt poll loop, write plain
 * key=value + <repeat>-friendly N_x_field rows to state/ui.txt, exit
 * when the parent renderer's module_parent.pid is gone):
 *   - launched as a <module> child of khtpm_core_render.+x
 *   - argv[1] = house_root
 *   - every ~150ms: read one line from #.desktop/export_hq/request.txt
 *     (truncate it), act, then write state/ui.txt
 *
 * Candidate list is NOT reinvented - same real, live scan
 * create-package.sh itself does (and the taskbar's own Toys dropdown,
 * khtpm_taskbar_manager.c's livedesk_build_toys_menu(): house_root +
 * @.apps + &.widgits + &.hq-apps, one level deep, opt-in by a real
 * toy.pdl file's presence). Packaging itself is NOT reimplemented
 * either - this GUI is a real, thin frontend that shells out to the
 * SAME create-package.sh a bare `sh` invocation would use, so there is
 * exactly one real packaging implementation, not two that can drift.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>

#define PB 1024
#define MAX_CAND 128

static char g_house[PB];
static char g_pkg[PB];        /* <house>/&.hq-apps/export-hq          */
static char g_state[PB];      /* <house>/#.desktop/export_hq          */
static char g_req[PB];        /* <state>/request.txt                  */
static char g_out[PB];        /* <pkg>/state/ui.txt                   */
static char g_result[PB];     /* <state>/result.txt (packaging output) */

typedef struct {
    char id[128];
    char title[160];
    int  selected;
} Cand;

static Cand g_cand[MAX_CAND];
static int  g_n_cand = 0;

static char g_dest[PB] = "";
static char g_status[512] = "";
static int  g_packaging = 0;
static long g_pkg_started = 0;

static void chomp(char *s) {
    size_t l = strlen(s);
    while (l && (s[l-1] == '\n' || s[l-1] == '\r')) s[--l] = 0;
}
static long now_s(void) { return (long)time(NULL); }

static size_t slurp(const char *p, char *b, size_t n) {
    FILE *f = fopen(p, "r");
    if (!f) { b[0] = 0; return 0; }
    size_t r = fread(b, 1, n - 1, f);
    fclose(f); b[r] = 0; return r;
}

static void xesc(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const char *p = in; *p && o < n - 7; p++) {
        switch (*p) {
            case '&':  memcpy(out + o, "&amp;", 5);  o += 5; break;
            case '<':  memcpy(out + o, "&lt;", 4);   o += 4; break;
            case '>':  memcpy(out + o, "&gt;", 4);   o += 4; break;
            case '"':  memcpy(out + o, "&quot;", 6); o += 6; break;
            case '\'': memcpy(out + o, "&#39;", 5);  o += 5; break;
            default: out[o++] = *p;
        }
    }
    out[o] = 0;
}

/* real, pipe-delimited "SECTION|KEY|VALUE" reader, tolerant of either
 * "SECTION" or "META" in column 1 - same convention this session
 * already used in pchq_board_projector.c's read_pdl_kv() and
 * create-package.sh's own pdl_val(). */
static void pdl_val(const char *path, const char *key, char *out, size_t outsz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        char *k = p1 + 1;
        while (*k == ' ') k++;
        size_t kl = (size_t)(p2 - k);
        while (kl > 0 && k[kl - 1] == ' ') kl--;
        if (kl != klen || strncmp(k, key, klen) != 0) continue;
        char *v = p2 + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        size_t vl = strlen(v);
        while (vl > 0 && v[vl - 1] == ' ') v[--vl] = '\0';
        snprintf(out, outsz, "%s", v);
        break;
    }
    fclose(f);
}

/* Same real 4-root scan create-package.sh does - see that file's own
 * header comment for why this is never a separately maintained
 * registry. */
static void scan_root(const char *root) {
    DIR *d = opendir(root);
    if (!d) return;
    struct dirent *e;
    while (g_n_cand < MAX_CAND && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char toy_pdl[PB];
        snprintf(toy_pdl, sizeof(toy_pdl), "%s/%s/toy.pdl", root, e->d_name);
        if (access(toy_pdl, F_OK) != 0) continue;
        Cand *c = &g_cand[g_n_cand];
        snprintf(c->id, sizeof(c->id), "%s", e->d_name);
        pdl_val(toy_pdl, "title", c->title, sizeof(c->title));
        if (!c->title[0]) snprintf(c->title, sizeof(c->title), "%s", e->d_name);
        c->selected = 0;
        g_n_cand++;
    }
    closedir(d);
}

static void rescan_candidates(void) {
    g_n_cand = 0;
    char apps[PB], widgits[PB], hqapps[PB];
    snprintf(apps, sizeof(apps), "%s/@.apps", g_house);
    snprintf(widgits, sizeof(widgits), "%s/&.widgits", g_house);
    snprintf(hqapps, sizeof(hqapps), "%s/&.hq-apps", g_house);
    scan_root(g_house);
    scan_root(apps);
    scan_root(widgits);
    scan_root(hqapps);
}

static Cand *find_cand(const char *id) {
    for (int i = 0; i < g_n_cand; i++)
        if (strcmp(g_cand[i].id, id) == 0) return &g_cand[i];
    return NULL;
}

/* ---------- packaging (shell out to the one real implementation) --- */

static void kick_off_package(void) {
    if (!g_dest[0]) { snprintf(g_status, sizeof(g_status), "Enter a destination path first."); return; }
    char names[PB] = "";
    int any = 0;
    for (int i = 0; i < g_n_cand; i++) {
        if (!g_cand[i].selected) continue;
        strncat(names, " ", sizeof(names) - strlen(names) - 1);
        strncat(names, g_cand[i].id, sizeof(names) - strlen(names) - 1);
        any = 1;
    }
    if (!any) { snprintf(g_status, sizeof(g_status), "Select at least one candidate first."); return; }

    unlink(g_result);
    char cmd[PB * 3];
    snprintf(cmd, sizeof(cmd),
        "setsid nohup sh -c '\"%s/&.hq-apps/create-package/create-package.sh\"%s \"%s\" "
        ">\"%s\" 2>&1; echo \"__EXIT:$?\" >>\"%s\"' >/dev/null 2>&1 &",
        g_house, names, g_dest, g_result, g_result);
    int rc = system(cmd);
    (void)rc;
    g_packaging = 1;
    g_pkg_started = now_s();
    snprintf(g_status, sizeof(g_status), "Packaging...");
}

static void poll_package_result(void) {
    if (!g_packaging) return;
    char buf[4096];
    size_t n = slurp(g_result, buf, sizeof(buf));
    if (n == 0) {
        if (now_s() - g_pkg_started > 60) {
            g_packaging = 0;
            snprintf(g_status, sizeof(g_status), "Packaging timed out after 60s - check %s manually.", g_result);
        }
        return;
    }
    char *marker = strstr(buf, "__EXIT:");
    if (!marker) return;   /* still writing */
    int code = atoi(marker + 7);
    g_packaging = 0;
    if (code == 0) {
        snprintf(g_status, sizeof(g_status), "Packaged to %s - run its launch.sh to test.", g_dest);
    } else {
        *marker = '\0';
        char tail[240];
        size_t bl = strlen(buf);
        snprintf(tail, sizeof(tail), "%s", buf + (bl > 200 ? bl - 200 : 0));
        snprintf(g_status, sizeof(g_status), "Packaging FAILED (exit %d): %s", code, tail);
    }
}

/* ---------- request handling --------------------------------------- */

static void handle_request(void) {
    char line[PB];
    FILE *f = fopen(g_req, "r");
    if (!f) return;
    int got = (fgets(line, sizeof(line), f) != NULL);
    fclose(f);
    { FILE *c = fopen(g_req, "w"); if (c) fclose(c); }
    if (!got) return;
    chomp(line);
    if (!line[0]) return;

    if (strncmp(line, "toggle:", 7) == 0) {
        Cand *c = find_cand(line + 7);
        if (c) c->selected = !c->selected;
    } else if (strncmp(line, "setdest:", 8) == 0) {
        const char *v = line + 8;
        while (*v == ' ') v++;
        snprintf(g_dest, sizeof(g_dest), "%s", v);
    } else if (strcmp(line, "package:") == 0) {
        if (!g_packaging) kick_off_package();
    } else if (strcmp(line, "rescan:") == 0) {
        int prev_n = g_n_cand;
        char prev_ids[MAX_CAND][128];
        int prev_sel[MAX_CAND];
        for (int i = 0; i < prev_n; i++) {
            snprintf(prev_ids[i], sizeof(prev_ids[i]), "%s", g_cand[i].id);
            prev_sel[i] = g_cand[i].selected;
        }
        rescan_candidates();
        for (int i = 0; i < g_n_cand; i++)
            for (int j = 0; j < prev_n; j++)
                if (strcmp(g_cand[i].id, prev_ids[j]) == 0) { g_cand[i].selected = prev_sel[j]; break; }
    }
}

/* ---------- state projection ---------------------------------------
 * Static export-hq.xhtpm does layout; this manager only ever writes
 * plain key=value + n_cand/c_N_* rows (CHTPM-ARCHITECTURE-FIX.md). */

static char *g_last = NULL;

static void write_state(void) {
    char buf[16384];
    size_t o = 0;
    #define K(...) do { o += (size_t)snprintf(buf + o, sizeof(buf) - o, __VA_ARGS__); } while (0)

    K("n_cand=%d\n", g_n_cand);
    for (int i = 0; i < g_n_cand; i++) {
        char t_esc[192], id_esc[192];
        xesc(g_cand[i].title, t_esc, sizeof(t_esc));
        xesc(g_cand[i].id, id_esc, sizeof(id_esc));
        K("c_%d_id=%s\n", i, id_esc);
        K("c_%d_mark=%s\n", i, g_cand[i].selected ? "\xE2\x9C\x85" : "\xE2\xAC\x9C");   /* checkmark / white box */
        K("c_%d_title=%s\n", i, t_esc);
    }
    char dest_esc[PB], status_esc[600];
    xesc(g_dest, dest_esc, sizeof(dest_esc));
    xesc(g_status, status_esc, sizeof(status_esc));
    K("dest=%s\n", dest_esc);
    K("status=%s\n", status_esc);
    K("packaging=%d\n", g_packaging);
    #undef K

    if (g_last && strcmp(g_last, buf) == 0) return;
    free(g_last);
    g_last = strdup(buf);

    char tmp[PB];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_out);
    FILE *wf = fopen(tmp, "w");
    if (!wf) return;
    fputs(buf, wf);
    fclose(wf);
    rename(tmp, g_out);
}

/* ---------- parent-alive -------------------------------------------- */

static int parent_alive(void) {
    char p[PB], b[64];
    snprintf(p, sizeof(p), "%s/module_parent.pid", g_pkg);
    if (slurp(p, b, sizeof(b)) == 0) return 1;
    long pid = atol(b);
    if (pid <= 0) return 1;
    return kill((int)pid, 0) == 0 || errno == EPERM;
}

/* ---------- main ------------------------------------------------- */

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <house_root>\n", argv[0]); return 1; }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);
    { size_t l = strlen(g_house); if (l > 1 && g_house[l - 1] == '/') g_house[l - 1] = 0; }

    snprintf(g_pkg, sizeof(g_pkg), "%s/&.hq-apps/export-hq", g_house);
    { char sd[PB]; snprintf(sd, sizeof(sd), "%s/state", g_pkg); mkdir(sd, 0777); }
    snprintf(g_out, sizeof(g_out), "%s/state/ui.txt", g_pkg);

    snprintf(g_state, sizeof(g_state), "%s/#.desktop/export_hq", g_house);
    mkdir(g_state, 0777);
    snprintf(g_req, sizeof(g_req), "%s/request.txt", g_state);
    snprintf(g_result, sizeof(g_result), "%s/result.txt", g_state);
    { FILE *c = fopen(g_req, "w"); if (c) fclose(c); }

    rescan_candidates();
    write_state();

    for (;;) {
        handle_request();
        poll_package_result();
        write_state();
        if (!parent_alive()) break;
        struct timespec ts = { 0, 150 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
    return 0;
}
