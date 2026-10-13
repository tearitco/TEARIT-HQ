/* tpmos_hq.c - show a TPMOS frame in an x11-hq (khtpm) window. C, no GL.
 *
 *   tpmos_hq.+x status            rewrite state/ui.txt from the current TPMOS frame file, print one line
 *   tpmos_hq.+x int               toggle INT: while on, every key typed in the window is handed to TPMOS (see below); off by default
 *   tpmos_hq.+x key <code>        hand ONE raw key code to TPMOS (the same line its keyboard plugin writes), then status
 *   tpmos_hq.+x <house> <pkg>     resident mode (the window's <module>): follow the frame by marker, rewrite ui.txt on each new frame,
 *                                 and forward the window's raw key relay to TPMOS while INT is on
 *
 * INPUT is TPMOS's business: it has its own key handler (pieces/keyboard/plugins/keyboard_input.c, which appends
 * "[YYYY-MM-DD HH:MM:SS] KEY_PRESSED: <code>" to pieces/keyboard/history.txt) and its orchestrator/parser reads that file. This op never decides what a
 * key means; it only appends the same line for each raw key the window saw (printable ASCII as its code, Enter 13, Esc 27, arrows 1000-1003 - the same
 * numbers TPMOS uses). The window's INT relay (khtpm relay= attribute) writes "<code> <ms>" lines to state/interact_relay.txt (and Esc/Enter to
 * state/keyboard/history.txt); this op reads both by size growth and forwards.
 *
 * The TPMOS side is untouched: its orchestrator keeps writing pieces/display/current_frame.txt (ANSI text) and appends a line to
 * pieces/display/frame_changed.txt on every frame; its GL mirror reads the same files. This op is a second reader. Change detection
 * is the marker file's SIZE growth (never mtime); if a TPMOS build has no marker file it falls back to the frame file's size + a
 * cheap checksum. display.pdl (next to the app) holds every path; env TPMOS_HQ_DISPLAY=<file> swaps in another one for testing.
 * ui.txt is written to a temp file and renamed, so the window never reads half a frame. */
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAXROWS 80
static char APP[PATH_MAX], HOUSE[PATH_MAX], ST[PATH_MAX];
static char ROOT[PATH_MAX], FRAME[PATH_MAX], MARKER[PATH_MAX], KEYS[PATH_MAX], BACKEND[32] = "x11-hq";
static int ROWS = 34, POLL_MS = 150;
static char RELAY1[PATH_MAX], RELAY2[PATH_MAX], INTF[PATH_MAX];
static int int_on(void);
static volatile sig_atomic_t quit_flag;
static void bye(int s) { (void)s; quit_flag = 1; }

static long fsize(const char *p) { struct stat s; return stat(p, &s) == 0 ? (long)s.st_size : -1; }
static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }

static void load_display(void) {
    char p[PATH_MAX]; const char *env = getenv("TPMOS_HQ_DISPLAY");
    snprintf(p, sizeof p, "%s", env && env[0] ? env : "");
    if (!p[0]) snprintf(p, sizeof p, "%s/display.pdl", APP);
    char root[PATH_MAX] = "", frame[PATH_MAX] = "pieces/display/current_frame.txt", marker[PATH_MAX] = "pieces/display/frame_changed.txt", keys[PATH_MAX] = "pieces/keyboard/history.txt";
    FILE *f = fopen(p, "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof line, f)) {
            char *a, *b, *c;
            if (line[0] == '#' || strncmp(line, "DISPLAY", 7)) continue;
            line[strcspn(line, "\r\n")] = 0;
            a = strchr(line, '|'); if (!a) continue; b = a + 1; c = strchr(b, '|'); if (!c) continue; *c++ = 0;
            while (*b == ' ') b++; { char *e = b + strlen(b); while (e > b && e[-1] == ' ') *--e = 0; }
            while (*c == ' ') c++; { char *e = c + strlen(c); while (e > c && e[-1] == ' ') *--e = 0; }
            if (!strcmp(b, "backend")) snprintf(BACKEND, sizeof BACKEND, "%s", c);
            else if (!strcmp(b, "tpmos_root")) snprintf(root, sizeof root, "%s", c);
            else if (!strcmp(b, "frame_file")) snprintf(frame, sizeof frame, "%s", c);
            else if (!strcmp(b, "marker_file")) snprintf(marker, sizeof marker, "%s", c);
            else if (!strcmp(b, "keys_file")) snprintf(keys, sizeof keys, "%s", c);
            else if (!strcmp(b, "rows")) { ROWS = atoi(c); if (ROWS < 4) ROWS = 4; if (ROWS > MAXROWS) ROWS = MAXROWS; }
            else if (!strcmp(b, "poll_ms")) { POLL_MS = atoi(c); if (POLL_MS < 30) POLL_MS = 30; }
        }
        fclose(f);
    }
    char abs[PATH_MAX];
    if (root[0] == '/') snprintf(abs, sizeof abs, "%s", root); else snprintf(abs, sizeof abs, "%s/%s", HOUSE, root);
    if (!realpath(abs, ROOT)) snprintf(ROOT, sizeof ROOT, "%s", abs);
    snprintf(FRAME, sizeof FRAME, "%s/%s", ROOT, frame);
    snprintf(MARKER, sizeof MARKER, "%s/%s", ROOT, marker);
    snprintf(KEYS, sizeof KEYS, "%s/%s", ROOT, keys);
}

/* Copy src into dst without ANSI escapes (ESC [ ... letter), tabs to a space, and drop a trailing CR. */
static void strip_ansi(const char *src, char *dst, size_t cap) {
    size_t n = 0;
    for (const char *s = src; *s && n + 1 < cap; s++) {
        if (*s == 27) { if (s[1] == '[') { s += 2; while (*s && !((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))) s++; if (!*s) break; } continue; }
        if (*s == '\t') dst[n++] = ' '; else if (*s != '\r') dst[n++] = *s;
    }
    dst[n] = 0;
}

static void write_ui(void) {
    char tmp[PATH_MAX], out[PATH_MAX];
    snprintf(out, sizeof out, "%s/ui.txt", ST); snprintf(tmp, sizeof tmp, "%s/ui.txt.tmp", ST);
    FILE *o = fopen(tmp, "w"); if (!o) return;
    FILE *f = fopen(FRAME, "r");
    char *rows[MAXROWS]; int n = 0;
    char line[4096], clean[4096];
    if (f) {
        while (n < ROWS && fgets(line, sizeof line, f)) { line[strcspn(line, "\n")] = 0; strip_ansi(line, clean, sizeof clean); rows[n++] = strdup(clean); }
        fclose(f);
    }
    long fs = fsize(FRAME), ms = fsize(MARKER);
    fprintf(o, "title=TPMOS (%s)  -  %s\n", BACKEND, f ? "frame" : "no frame yet");
    fprintf(o, "backend=%s\n", BACKEND);
    fprintf(o, "status_line=%s  |  %d lines  |  frames seen (marker bytes): %ld  |  keys: %s\n", f ? "following TPMOS" : "waiting for pieces/display/current_frame.txt", n, ms < 0 ? 0 : ms, KEYS);
    fprintf(o, "frame_bytes=%ld\n", fs < 0 ? 0 : fs);
    fprintf(o, "n_rows=%d\n", n);
    fprintf(o, "interact_class=%s\ninteract_label=%s\nrp_h1=%s\nrp_h2=%s\n", int_on() ? "interact-active" : "", int_on() ? "ON - typing goes to TPMOS" : "off", RELAY1, RELAY2);
    for (int i = 0; i < ROWS; i++) fprintf(o, "f%d=%s\n", i, i < n ? (rows[i][0] ? rows[i] : " ") : " ");
    fclose(o);
    rename(tmp, out);
    for (int i = 0; i < n; i++) free(rows[i]);
}

static int int_on(void) { FILE *f = fopen(INTF, "r"); int v = 0; if (f) { if (fscanf(f, "%d", &v) != 1) v = 0; fclose(f); } return v; }
static void append_key(int code) {
    FILE *k = fopen(KEYS, "a"); if (!k) return;
    time_t t = time(NULL); struct tm tmv; char ts[32]; localtime_r(&t, &tmv); strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(k, "[%s] KEY_PRESSED: %d\n", ts, code); fclose(k);
}
/* Read what the window appended to one relay file since *off; forward each code to TPMOS only while INT is on (offsets always advance). */
static void pump_relay(const char *path, long *off, int forward) {
    long sz = fsize(path);
    if (sz < 0) { *off = 0; return; }
    if (*off < 0 || sz < *off) { *off = sz; return; }          /* first sight or truncated: resync, do not replay old keys */
    if (sz == *off) return;
    FILE *f = fopen(path, "r"); if (!f) return;
    fseek(f, *off, SEEK_SET);
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *p = strstr(line, "KEY_PRESSED:"); int code = 0;
        if (p) code = atoi(p + 12); else code = atoi(line);
        if (forward && code > 0) append_key(code);
    }
    *off = ftell(f); fclose(f);
}

static long frame_stamp(void) {          /* marker size growth; without a marker, frame size + a byte checksum */
    long ms = fsize(MARKER); if (ms >= 0) return ms * 1000003L + 1;
    FILE *f = fopen(FRAME, "r"); if (!f) return -1;
    long sum = 0; int c; while ((c = fgetc(f)) != EOF) sum = sum * 31 + c;
    fclose(f); return sum;
}

static int resident(void) {
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye);
    long last = -2, o1 = -1, o2 = -1; write_ui();
    while (!quit_flag) {
        pump_relay(RELAY1, &o1, int_on()); pump_relay(RELAY2, &o2, int_on());
        long st = frame_stamp();
        if (st != last) { last = st; write_ui(); }
        usleep((useconds_t)POLL_MS * 1000);
    }
    return 0;
}

int main(int argc, char **argv) {
    char exe[PATH_MAX]; if (!realpath(argv[0], exe)) snprintf(exe, sizeof exe, "%s", argv[0]);
    int mode_resident = (argc == 3 && is_dir(argv[1]) && is_dir(argv[2]));
    if (mode_resident) snprintf(APP, sizeof APP, "%s", argv[2]);
    else { for (int up = 0; up < 3; up++) { char *s = strrchr(exe, '/'); if (s) *s = 0; } snprintf(APP, sizeof APP, "%s", exe); }
    snprintf(HOUSE, sizeof HOUSE, "%s", APP); { char *s = strrchr(HOUSE, '/'); if (s) *s = 0; s = strrchr(HOUSE, '/'); if (s) *s = 0; }   /* .../44.xyz.01.00/@.apps/tpmos-hq -> house root */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755);
    snprintf(RELAY1, sizeof RELAY1, "%s/interact_relay.txt", ST); snprintf(INTF, sizeof INTF, "%s/int.txt", ST);
    { char d[PATH_MAX]; snprintf(d, sizeof d, "%s/keyboard", ST); mkdir(d, 0755); snprintf(RELAY2, sizeof RELAY2, "%s/keyboard/history.txt", ST); }
    load_display();
    if (mode_resident) return resident();
    if (argc >= 3 && !strcmp(argv[1], "key")) {
        if (fsize(KEYS) < 0) { FILE *k = fopen(KEYS, "a"); if (!k) { printf("cannot open keys file %s: %s\n", KEYS, strerror(errno)); return 1; } fclose(k); }
        append_key(atoi(argv[2]));
    } else if (argc >= 2 && !strcmp(argv[1], "int")) {
        int nv = int_on() ? 0 : 1; FILE *f = fopen(INTF, "w"); if (f) { fprintf(f, "%d\n", nv); fclose(f); }   /* read BEFORE fopen("w") truncates it */
    } else if (argc >= 2 && strcmp(argv[1], "status")) { printf("usage: tpmos_hq.+x status | int | key <code> | <house> <pkg>\n"); return 1; }
    write_ui();
    printf("backend=%s frame=%s (%ld bytes) keys=%s\n", BACKEND, FRAME, fsize(FRAME), KEYS);
    return 0;
}
