#define _GNU_SOURCE
/* tear_on_op - the network-menu cell "tear-on": TEAR-IT Ranch and Arena as a BOOK in the rpg-edit HQ board (2D/3D), shape of @.apps/hai-pal (owner 2026-11-24).
 *   tear_on_op --open <house>          project both pages from the chain, make tear-on the editor's last book, then: editor running -> switch it through its inbox (CONFIRM_START_MAP, wait, CONFIRM_SET_DESK);
 *                                      editor not running -> start it (button.sh run, it opens the last book).  Prints one status line.
 *   tear_on_op project <house>         rewrite the ranch and arena pages from the chain (read-only projection, safe to repeat)
 *   tear_on_op do <house> <ROW>        append ONE row (pipes inside) to the chain through tear_chain_op mine (miner _ranch), then project
 *   tear_on_op demo <house>            only when the chain is empty: five pets, faucets, an arena with fighters (so there is something to see). Rows, not files.
 * The rules are NOT here: they are ops/+x/tear_chain_op.+x of &.hq-apps/tear-offline (graded byte for byte against chain.js).  Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tear_on_op.+x tear_on_op.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define PL 1024
static char house[PL], chain[PL], pages[PL], atlas[PL], chainop[PL], editapp[PL], inbox[PL], worldst[PL], boardpid[PL];
static int run(char *const av[], char *out, size_t os) {
    int p[2]; if (pipe(p)) return 99; pid_t c = fork();
    if (c == 0) { dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t r; while (n + 1 < os && (r = read(p[0], out + n, os - n - 1)) > 0) n += (size_t)r; out[n] = 0; close(p[0]);
    int st = 0; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 98; }
static void paths(const char *h) {
    snprintf(house, sizeof house, "%s", h); snprintf(chain, sizeof chain, "%s/@.apps/tear-on/ledger/web_chain.txt", h); snprintf(pages, sizeof pages, "%s/@.apps/tear-on/tear-on-pchq/pieces/system/maps/tear-on", h);
    snprintf(atlas, sizeof atlas, "%s/@.apps/tear-on/atlas/cells.rgba", h); snprintf(chainop, sizeof chainop, "%s/&.hq-apps/tear-offline/ops/+x/tear_chain_op.+x", h); snprintf(editapp, sizeof editapp, "%s/@.apps/rpg-edit-hq", h);
    snprintf(inbox, sizeof inbox, "%s/rpg-edit-hq-pchq/pieces/system/widget_cmds/inbox.txt", editapp); snprintf(worldst, sizeof worldst, "%s/rpg-edit-hq-pchq/pieces/world_01/state.txt", editapp); snprintf(boardpid, sizeof boardpid, "%s/rpg-edit-hq-pchq/pieces/system/board_window.pid", editapp); }
static void mkdir_p(const char *path) { char t[PL + 8]; snprintf(t, sizeof t, "%s", path); for (char *p = t + 1; *p; p++) if (*p == 47) { *p = 0; mkdir(t, 0755); *p = 47; } mkdir(t, 0755); }
static int ensure_chain(void) { char d[PL + 8]; snprintf(d, sizeof d, "%s/@.apps/tear-on/ledger", house); mkdir_p(d); FILE *f = fopen(chain, "a"); if (!f) return 1; fclose(f); return 0; }
static int project(void) {
    if (access(chainop, X_OK)) { fprintf(stderr, "tear-on: tear_chain_op is not built (%s)\n", chainop); return 1; } if (ensure_chain()) { fprintf(stderr, "tear-on: cannot create the chain file\n"); return 1; }
    mkdir_p(pages); const char *pg[2] = { "ranch", "arena" };
    for (int i = 0; i < 2; i++) { char dir[PL + 16], out[512]; snprintf(dir, sizeof dir, "%s/%s", pages, pg[i]); char *av[] = { chainop, "desk", chain, (char *)pg[i], dir, atlas, NULL }; int c = run(av, out, sizeof out); if (c != 0) { fprintf(stderr, "tear-on: projecting %s failed: %s", pg[i], out); return 1; } }
    return 0; }
static int do_row(const char *row) {
    char ts[24], out[512]; snprintf(ts, sizeof ts, "%ld", (long)time(NULL)); if (ensure_chain()) return 1;
    char *av[] = { chainop, "mine", chain, "_ranch", ts, (char *)row, NULL }; int c = run(av, out, sizeof out); char *e = strchr(out, '\n'); if (e) *e = 0; printf("%s\n", out); return c; }
static long chain_size(void) { struct stat st; return stat(chain, &st) ? -1 : (long)st.st_size; }
static int demo(void) {
    if (ensure_chain() || chain_size() > 0) { printf("demo: the chain already has rows; nothing seeded\n"); return 0; }
    const char *rows[] = { "PET|rex|blue dragon dreams|you|1", "PET|mina|moon garden|you|2", "PET|tobo|iron fist|you|3", "PET|ivy|ivy words|you|4", "PET|kai|kai words|you|5",
        "FAUCET|rex|1000|9000", "FAUCET|mina|1000|9000", "FAUCET|tobo|1000|9000", "FAUCET|ivy|1000|9000", "FAUCET|kai|1000|9000",
        "ARENA|arena1|ffa|you|10", "JOIN|arena1|rex|you|11", "JOIN|arena1|mina|you|12", "FILL|arena1|you|13" };
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) { if (do_row(rows[i])) { fprintf(stderr, "demo: row %u refused\n", i); return 1; } }
    return project(); }
static int board_alive(void) { FILE *f = fopen(boardpid, "r"); if (!f) return 0; long p = 0; if (fscanf(f, "%ld", &p) != 1) p = 0; fclose(f); return p > 1 && kill((pid_t)p, 0) == 0; }
static int world_map_is_tear_on(void) { FILE *f = fopen(worldst, "r"); if (!f) return 0; char l[256]; int ok = 0; while (fgets(l, sizeof l, f)) if (!strncmp(l, "map_id=tear-on", 14)) ok = 1; fclose(f); return ok; }
static void inbox_add(const char *line) { FILE *f = fopen(inbox, "a"); if (f) { fprintf(f, "%s\n", line); fclose(f); } }
static int open_editor(void) {
    if (project()) return 1; char out[512], st[PL + 32], mp[PL + 64]; snprintf(st, sizeof st, "%s/state/last_book.txt", editapp); snprintf(mp, sizeof mp, "%s/rpg-edit-hq-pchq/pieces/system/maps", editapp); char es[PL + 32]; snprintf(es, sizeof es, "%s/ops/edit_last.sh", editapp);
    char *sv[] = { "/bin/sh", es, "save", st, "tear-on", "ranch", "--house", house, "--maps", mp, NULL }; if (run(sv, out, sizeof out) != 0) { fprintf(stderr, "tear-on: edit_last save refused: %s", out); return 1; }
    if (board_alive()) { inbox_add("CONFIRM_START_MAP:tear-on"); for (int i = 0; i < 25 && !world_map_is_tear_on(); i++) sleep(2); if (!world_map_is_tear_on()) { printf("tear-on: the editor did not switch books in 50 s (it may be busy); the book is saved as its last book\n"); return 0; } sleep(4); inbox_add("CONFIRM_SET_DESK:ranch"); printf("tear-on: the running rpg-edit switched to book tear-on, page ranch\n"); return 0; }
    char bs[PL + 16]; snprintf(bs, sizeof bs, "%s/button.sh", editapp); char *rv[] = { "/bin/sh", bs, "run", NULL }; run(rv, out, sizeof out); printf("tear-on: rpg-edit is starting on book tear-on (the editor takes about a minute)\n"); return 0; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s --open|project|demo <house>   |   do <house> <ROW>\n", argv[0]); return 2; } paths(argv[2]);
    if (!strcmp(argv[1], "--open")) return open_editor();
    if (!strcmp(argv[1], "project")) { int c = project(); if (!c) printf("tear-on: pages projected\n"); return c; }
    if (!strcmp(argv[1], "demo")) return demo();
    if (!strcmp(argv[1], "do") && argc >= 4) { int c = do_row(argv[3]); if (c == 0 && project()) return 1; return c; }
    fprintf(stderr, "unknown command %s\n", argv[1]); return 2; }
