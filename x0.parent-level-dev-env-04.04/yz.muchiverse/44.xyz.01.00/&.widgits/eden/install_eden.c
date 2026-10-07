/* install_eden - installs the Eden conductor as a playable DESK BUTTON into a livedesk folder. Self-contained C, no shell, no system(), no exec.
 *
 * Usage: install_eden --livedesk <livedesk_dir> --house <house_root> [--apply]
 *   --livedesk  the folder that holds pals/ (xyzfs/users/<uuid>/home/livedesk); required, no default (refused without it, exit 2)
 *   --house     the house root that holds the built template + programs: <house>/&.widgits/eden (conductor/, common_events/, ops/+x/eden_op.+x),
 *               livedesk-clock/ops/+x/lc_clock.+x, _shared-lib/ops/+x/game_snapshot_op.+x, _shared-lib/system/+x/prisc+x.+x, digipet/ops/+x/event_page_op.+x
 *   default is a DRY RUN: nothing is written, the plan goes to stderr. --apply writes.
 * Creates (G = <livedesk>/eden_game, ABSOLUTE in ctl.sh):
 *   G/game/conductor/            copy of eden/conductor (+ ops/+x/eden_op.+x; wiring.pdl gets daemon=1, prisc, event_runner rows appended)
 *   G/lc.+x  G/gso  G/prisc      copies of lc_clock, game_snapshot_op, the shared prisc
 *   G/store/                     (empty; save slots land in store/checkpoints)
 *   G/common_events/eden_day_tick                    the Day Tick common event
 *   G/&.widgits/digipet/ops/+x/event_page_op.+x      what the menu rows (and the clock's event runner) exec
 *   <livedesk>/pals/eden_button/ pal.pdl (PAL | name | eden_button, PAL | glyph | 🔘; NO hash/uid: identity and phone are minted on first spawn), glyph.txt,
 *                                meta.pdl (METHOD rows -> `sh -c 'exec sh "$0/ctl.sh" <trigger>'`, $0 = the pal dir), ctl.sh (exports the env, execs event_page_op G/game/conductor G --trigger <t>,
 *                                or eden_op directly for the slot rows)
 * Then prints ONE line to stdout, the DESK row, for the CALLER to append to a desk page file: this tool never edits a desk file.
 * Never overwrites: an existing G or pal = refusal (exit 2, nothing written). Each tree is built under a temp name and renamed into place.
 * Exit: 0 ok (or dry run ok), 1 error (missing source, I/O), 2 usage / refusal. The toy (taskbar Toys entry) is NOT installed. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define P 4096
static int g_apply, g_files;
static char LD[P], HR[P], G[P], PAL[P], GT[P], PT[P];

static void say(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }
static int lex(const char *p, struct stat *st) { return lstat(p, st) == 0; }
static int exists(const char *p) { struct stat st; return lex(p, &st); }
static int isdir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static int isfile(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }
static void pjoin(char *out, const char *a, const char *b) { snprintf(out, P, "%s/%s", a, b); }

static int mkdirs(const char *path) {
    char t[P]; snprintf(t, sizeof t, "%s", path);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; if (mkdir(t, 0755) && errno != EEXIST) return -1; *p = '/'; }
    return (mkdir(t, 0755) && errno != EEXIST) ? -1 : 0;
}
static int copy_file(const char *src, const char *dst) {
    struct stat st; if (stat(src, &st)) return -1;
    FILE *in = fopen(src, "rb"); if (!in) return -1; FILE *out = fopen(dst, "wb"); if (!out) { fclose(in); return -1; }
    char buf[65536]; size_t n; int rc = 0;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
    fclose(in); if (fclose(out)) rc = -1; chmod(dst, st.st_mode & 0777); g_files++; return rc;
}
static int copy_tree(const char *src, const char *dst) {
    if (mkdirs(dst)) return -1;
    DIR *d = opendir(src); if (!d) return -1;
    struct dirent *e; int rc = 0;
    while ((e = readdir(d))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char s[P], t[P]; pjoin(s, src, e->d_name); pjoin(t, dst, e->d_name); struct stat st; if (lstat(s, &st)) { rc = -1; break; }
        if (S_ISDIR(st.st_mode)) { if (copy_tree(s, t)) { rc = -1; break; } } else if (S_ISREG(st.st_mode)) { if (copy_file(s, t)) { rc = -1; break; } } /* symlinks and specials are not copied */ }
    closedir(d); return rc;
}
static int rm_tree(const char *p) {
    struct stat st; if (lstat(p, &st)) return 0;
    if (S_ISDIR(st.st_mode)) { DIR *d = opendir(p); if (d) { struct dirent *e; while ((e = readdir(d))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; char c[P]; pjoin(c, p, e->d_name); rm_tree(c); } closedir(d); } return rmdir(p); }
    return unlink(p);
}
static int put(const char *path, const char *text, int mode) { FILE *f = fopen(path, "w"); if (!f) return -1; fputs(text, f); if (fclose(f)) return -1; chmod(path, mode); return 0; }
static int append(const char *path, const char *text) { FILE *f = fopen(path, "a"); if (!f) return -1; fputs(text, f); return fclose(f); }
/* one copy source -> destination under a root, creating parents */
static int cp_into(const char *root, const char *rel, const char *src) { char d[P], dir[P]; pjoin(d, root, rel); snprintf(dir, sizeof dir, "%s", d); char *sl = strrchr(dir, '/'); if (sl) *sl = 0; if (mkdirs(dir)) return -1; return copy_file(src, d); }

typedef struct { const char *rel_dst; char src[P]; } Prog;
static void abs_house(char *out, const char *rel) { pjoin(out, HR, rel); }

/* the trigger rows of the pal menu: label | ctl.sh argument */
static const char *ROWS[][2] = {
    { "Start game", "begin" }, { "Stop game", "stop" }, { "Next day", "nextday" }, { "Status", "status" }, { "Pause", "pause" }, { "Resume", "resume" },
    { "Save 1", "save1" }, { "Save 2", "save2" }, { "Save 3", "save3" }, { "Load 1", "load1" }, { "Load 2", "load2" }, { "Load 3", "load3" },
    { "Slot next", "slot-next" }, { "Slot prev", "slot-prev" }, { "Slot +10", "slot-plus10" },
    { "Save slot...", "save-slot" }, { "Resave slot...", "resave-slot" }, { "Load slot...", "load-slot" }, { NULL, NULL } };

static int build_pal(const char *dir) {
    char p[P], text[16384]; size_t o = 0;
    if (mkdirs(dir)) return -1;
    pjoin(p, dir, "pal.pdl"); if (put(p, "PAL | name | eden_button\nPAL | glyph | \xf0\x9f\x94\x98\n", 0644)) return -1;
    pjoin(p, dir, "glyph.txt"); if (put(p, "\xf0\x9f\x94\x98\n", 0644)) return -1;
    o += snprintf(text + o, sizeof text - o, "SECTION      | KEY                | VALUE\n----------------------------------------\nMETA         | piece_id           | eden-button\nSTATE        | kind                 | deskpal\nSTATE        | glyph                | \xf0\x9f\x94\x98\nSTATE        | menu_stay_open     | 1\nMETA         | cli_io_action      | ${PKG}/cli.sh\n");
    for (int i = 0; ROWS[i][0]; i++) o += snprintf(text + o, sizeof text - o, "METHOD       | %-20s | sh -c 'exec sh \"$0/ctl.sh\" %s'\n", ROWS[i][0], ROWS[i][1]);
    /* the standard desk-pal rows every entity menu carries (same strings as asa/robot meta.pdl), then Close/Cancel */
    o += snprintf(text + o, sizeof text - o, "METHOD       | %-20s | sh -c 'exec \"$1/&.widgits/events-hq/button.sh\" \"$0\" \"$1\"'\n", "Events (hq)");
    o += snprintf(text + o, sizeof text - o, "METHOD       | %-20s | sh -c 'exec xdg-open \"$0\"'\n", "Dir");
    o += snprintf(text + o, sizeof text - o, "METHOD       | %-20s | sh -c 'H=\"$1\"; I=\"$H/&.widgits/file-explorer/instances/inv-$(basename \"$(dirname \"$0\")\")-$(basename \"$0\")\"; mkdir -p \"$0/inventory\" \"$I\"; printf \"mode=LOAD\\nstart_dir=%%s/inventory\\n\" \"$0\" > \"$I/fe_request.txt\"; exec sh \"$H/&.widgits/file-explorer/button.sh\" run-instance \"$I\"'\n", "Inventory");
    o += snprintf(text + o, sizeof text - o, "METHOD       | %-20s | CLOSE\nMETHOD       | %-20s | void\n", "Close", "Cancel");
    pjoin(p, dir, "meta.pdl"); if (put(p, text, 0644)) return -1;
    /* ctl.sh: G is ABSOLUTE (written by the installer). The first argument is a conductor trigger (an event page), or one of the slot verbs which go straight to eden_op. */
    o = snprintf(text, sizeof text,
        "#!/bin/sh\n# ctl.sh - generated by install_eden. Usage: ctl.sh <trigger>. Menu rows run `sh -c 'exec sh \"$0/ctl.sh\" <trigger>' <pal_dir> <house_root>`.\n"
        "G='%s'\nC=\"$G/game/conductor\"\nexport EVENT_PAGE_PRISC=\"$G/prisc\"\nexport LC_CLOCK_NO_POPUP=1\nexport LC_CLOCK_EVENT_RUNNER=\"$G/&.widgits/digipet/ops/+x/event_page_op.+x\"\n"
        "case \"$1\" in\n"
        "  slot-next) exec \"$C/ops/+x/eden_op.+x\" \"$C\" slot next ;;\n  slot-prev) exec \"$C/ops/+x/eden_op.+x\" \"$C\" slot prev ;;\n  slot-plus10) exec \"$C/ops/+x/eden_op.+x\" \"$C\" slot +10 ;;\n"
        "  save-slot|resave-slot|load-slot) exec \"$C/ops/+x/eden_op.+x\" \"$C\" \"$1\" ;;\n"
        "  '') echo 'usage: ctl.sh <trigger>' >&2; exit 2 ;;\nesac\n"
        "exec \"$G/&.widgits/digipet/ops/+x/event_page_op.+x\" \"$C\" \"$G\" --trigger \"$1\"\n", G);
    pjoin(p, dir, "ctl.sh"); if (put(p, text, 0755)) return -1;
    /* cli.sh: the Cli-io handler (the converter turns `META | cli_io_action` into a real <cli_io>; the renderer runs it as `cli.sh <pal_dir> <house_root> <typed text>`).
     * Standard: 02-architecture/ENTITY-MENU-CLI-IO-STANDARD.md. Every line typed is appended to cli_commands.txt; answers go to cli_reply.txt (append-only). */
    pjoin(p, dir, "cli.sh");
    return put(p, "#!/bin/sh\n# cli.sh - Cli-io handler of the eden button, generated by install_eden. argv: <pal_dir> <house_root> <typed text>\n"
        "pal=\"$1\"; text=\"$3\"\nprintf '%s\\n' \"$text\" >> \"$pal/cli_commands.txt\"\nset -- $text\n"
        "case \"$1\" in\n  start|begin) t=begin ;; stop) t=stop ;; next|nextday) t=nextday ;; status) t=status ;; pause) t=pause ;; resume) t=resume ;;\n"
        "  save1|save2|save3|load1|load2|load3) t=$1 ;;\n  save|load) case \"${2:-}\" in 1|2|3) t=\"$1$2\" ;; *) t= ;; esac ;;\n  *) t= ;;\nesac\n"
        "if [ -z \"$t\" ]; then echo \"unknown: $text (try: start stop next status pause resume save1-3 load1-3)\" >> \"$pal/cli_reply.txt\"; exit 0; fi\n"
        "sh \"$pal/ctl.sh\" \"$t\" >> \"$pal/cli_reply.txt\" 2>&1\n", 0755);
}

static int build_game(const char *dir, const Prog *pr, int np) {
    char s[P], d[P];
    if (mkdirs(dir)) return -1;
    abs_house(s, "&.widgits/eden/conductor"); pjoin(d, dir, "game/conductor"); if (mkdirs(dir) || copy_tree(s, d)) return -1;
    abs_house(s, "&.widgits/eden/common_events/eden_day_tick"); pjoin(d, dir, "common_events/eden_day_tick"); if (copy_tree(s, d)) return -1;
    for (int i = 0; i < np; i++) if (cp_into(dir, pr[i].rel_dst, pr[i].src)) return -1;
    pjoin(d, dir, "store"); if (mkdirs(d)) return -1;
    pjoin(d, dir, "game/conductor/wiring.pdl");
    return append(d, "# --- written by install_eden: clock daemon + the runner env (see the commented rows above)\nWIRING | daemon       | 1\nWIRING | daemon_pid   | ../../eden_daemon.pid\nWIRING | prisc        | ../../prisc\nWIRING | event_runner | ../../&.widgits/digipet/ops/+x/event_page_op.+x\n");
}

/* best-effort finishing steps for the pal (fork+exec, output discarded): returns 0 when the program exited 0 */
static int run_quiet(char *const argv[]) {
    pid_t k = fork(); if (k < 0) return -1;
    if (k == 0) { freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr); execv(argv[0], argv); _exit(127); }
    int st = 0; if (waitpid(k, &st, 0) < 0) return -1;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : -1;
}

int main(int argc, char **argv) {
    const char *ld = NULL, *hr = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--livedesk") && i + 1 < argc) ld = argv[++i];
        else if (!strcmp(argv[i], "--house") && i + 1 < argc) hr = argv[++i];
        else if (!strcmp(argv[i], "--apply")) g_apply = 1;
        else { say("install_eden: unknown argument %s\n", argv[i]); return 2; } }
    if (!ld || !hr) { say("usage: install_eden --livedesk <livedesk_dir> --house <house_root> [--apply]   (dry run unless --apply; --livedesk is required)\n"); return 2; }
    if (!realpath(ld, LD) || !isdir(LD)) { say("install_eden: livedesk dir %s does not exist\n", ld); return 2; }
    if (!realpath(hr, HR) || !isdir(HR)) { say("install_eden: house root %s does not exist\n", hr); return 2; }
    if (strchr(LD, '\'')) { say("install_eden: a single quote in the livedesk path is not supported\n"); return 2; }
    pjoin(G, LD, "eden_game"); char pals[P]; pjoin(pals, LD, "pals"); pjoin(PAL, pals, "eden_button");
    if (exists(G)) { say("install_eden: REFUSED, %s already exists (never overwrites); nothing written\n", G); return 2; }
    if (exists(PAL)) { say("install_eden: REFUSED, %s already exists (never overwrites); nothing written\n", PAL); return 2; }
    Prog pr[] = {
        { "game/conductor/ops/+x/eden_op.+x", "&.widgits/eden/ops/+x/eden_op.+x" },
        { "lc.+x", "&.widgits/livedesk-clock/ops/+x/lc_clock.+x" },
        { "gso", "&.widgits/_shared-lib/ops/+x/game_snapshot_op.+x" },
        { "prisc", "&.widgits/_shared-lib/system/+x/prisc+x.+x" },
        { "&.widgits/digipet/ops/+x/event_page_op.+x", "&.widgits/digipet/ops/+x/event_page_op.+x" } };
    int np = (int)(sizeof pr / sizeof pr[0]), bad = 0; char t[P];
    for (int i = 0; i < np; i++) { char rel[P]; snprintf(rel, sizeof rel, "%s", pr[i].src); abs_house(pr[i].src, rel); if (!isfile(pr[i].src)) { say("install_eden: missing built program %s (run the build scripts)\n", pr[i].src); bad = 1; } }
    abs_house(t, "&.widgits/eden/conductor/game.pdl"); if (!isfile(t)) { say("install_eden: missing %s\n", t); bad = 1; }
    abs_house(t, "&.widgits/eden/common_events/eden_day_tick"); if (!isdir(t)) { say("install_eden: missing %s\n", t); bad = 1; }
    if (bad) return 1;
    say("install_eden: %s\n  creating %s (game/conductor, lc.+x, gso, prisc, store/, common_events/eden_day_tick, &.widgits/digipet/ops/+x/event_page_op.+x)\n  creating %s (pal.pdl glyph.txt meta.pdl ctl.sh)\n",
        g_apply ? "APPLY" : "DRY RUN (nothing written)", G, PAL);
    if (!g_apply) { say("  DESK row (printed on --apply): DESK | eden_button | pals/eden_button | 800 | 80 | 10 | 1 | \xf0\x9f\x94\x98 | \n"); return 0; }
    snprintf(GT, sizeof GT, "%s/.eden_game.tmp-%ld", LD, (long)getpid()); snprintf(PT, sizeof PT, "%s/.eden_button.tmp-%ld", LD, (long)getpid());
    if (build_game(GT, pr, np) || build_pal(PT)) { say("install_eden: build failed (%s); cleaning the temp folders, nothing installed\n", strerror(errno)); rm_tree(GT); rm_tree(PT); return 1; }
    if (mkdirs(pals)) { say("install_eden: cannot create %s\n", pals); rm_tree(GT); rm_tree(PT); return 1; }
    if (rename(GT, G)) { say("install_eden: rename to %s failed: %s\n", G, strerror(errno)); rm_tree(GT); rm_tree(PT); return 1; }
    if (rename(PT, PAL)) { say("install_eden: rename to %s failed: %s; removing %s again\n", PAL, strerror(errno), G); rm_tree(PT); rm_tree(G); return 1; }
    say("install_eden: installed %d files into %s and the pal %s\n  append the DESK row below to the desk page file yourself (this tool edits no desk file)\n", g_files, G, PAL);
    /* sprite (without one the window is a plain coloured square) + the generated menu.chtpm chrome; a missing tool is reported, not fatal */
    {
        char gen[P], xt[P], atlas[P], csv[P], conv[P], hh[P], rel[P]; snprintf(hh, sizeof hh, "%s", hr);
        snprintf(gen, sizeof gen, "%s/_.monads/_.livedesk-taskbar/ops/+x/emoji_gen_atlas.+x", hh);
        snprintf(xt, sizeof xt, "%s/_.monads/_.livedesk-taskbar/ops/+x/emoji_xtract.+x", hh);
        snprintf(conv, sizeof conv, "%s/_.monads/_.livedesk-taskbar/ops/meta_to_menu_chtpm.py", hh);
        snprintf(atlas, sizeof atlas, "%s/atlas.png", PAL); snprintf(csv, sizeof csv, "%s/sprite.csv", PAL);
        char *a1[] = { gen, "\xf0\x9f\x94\x98", atlas, NULL }; char *a2[] = { xt, atlas, "0", "64", csv, NULL };
        if (isfile(gen) && isfile(xt) && !run_quiet(a1) && !run_quiet(a2)) say("install_eden: sprite written\n"); else say("install_eden: sprite NOT generated (emoji tools missing or failed): the window will be a plain square\n");
        char *a3[] = { "/usr/bin/python3", "-I", conv, PAL, NULL };
        if (isfile(conv) && !run_quiet(a3)) say("install_eden: menu.chtpm written\n"); else say("install_eden: menu.chtpm NOT generated (run meta_to_menu_chtpm.py on the pal)\n");
        size_t hl = strlen(hh); snprintf(rel, sizeof rel, "%s", strncmp(PAL, hh, hl) == 0 && PAL[hl] == '/' ? PAL + hl + 1 : "pals/eden_button");
        printf("DESK | eden_button | %s | 800 | 80 | 10 | 1 | \xf0\x9f\x94\x98 | \n", rel);
    }
    return 0;
}
