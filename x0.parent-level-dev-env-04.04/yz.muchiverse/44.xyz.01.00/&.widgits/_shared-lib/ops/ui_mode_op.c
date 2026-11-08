/* ui_mode_op - switch a window between EDIT and PLAY by flipping its sidebar state keys from data (B1 of PROPOSAL-PLAY-MODE-SEATS-AND-GAME-SIDEBARS-2026-11-07; owner decisions 2026-11-07: Player is the
 * edit/play switch, bars are declared in layouts.pdl, slots stay flexible).
 *
 * Usage:  ui_mode_op <sides.txt> --bars <layouts.pdl> [--bars <layouts.pdl>]... [--memory <file>] <edit|play|show>
 *   <sides.txt>   the window's key=value file (the board template loads it as a vars file; a bar row is shown with show="${key}": "1" = shown, EMPTY = hidden, the side_toggle convention)
 *   layouts.pdl   rows `BAR | id | key | slot | modes | default | group | note`
 *                   id       a name, unique across all the files          key      the state key in sides.txt, unique
 *                   slot     left | right | top | bottom                   modes    edit, play or edit,play (a bar is shown only in the modes it lists)
 *                   default  on | off: used the first time a mode is entered   group    optional. Bars with the same slot and the same non-empty group are EXCLUSIVE (the first one that
 *                            would be on wins, so pets and palettes can share the left slot); an EMPTY group stacks freely. The slot policy is data, not code.
 *   --memory      where each bar's last state per mode is kept (default: sides_mode_memory.pdl beside sides.txt); rows `MEM | <mode> | <key> | on|off`. It is a USER PREFERENCE, never game state:
 *                 delete it and the defaults apply again.
 * What a switch does: (1) remember the state of every bar of the CURRENT mode (the key `ui_mode`, default edit), (2) for the new mode show each bar that lists it, at its remembered state or its
 * default, and hide every bar that does not, (3) write `ui_mode`, `mode_edit` and `mode_play` (1 or EMPTY) so a row can be gated on the mode itself. Keys that no BAR declares are kept as they are.
 * `show` prints the current mode and writes nothing. Switching to the mode you are already in changes nothing (idempotent).
 * Exit: 0 ok | 2 usage, an unreadable or bad layouts file, an unknown mode. Output: `UIMODE <from>-><to> on=<ids> off=<ids>`.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/ui_mode_op.+x ops/ui_mode_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXB 64
#define LINE 512
typedef struct { char id[32], key[48], slot[12], group[24]; int edit, play, def_on; } Bar;
static Bar B[MAXB]; static int nb;
static char kvk[128][64], kvv[128][256]; static int nkv;       /* sides.txt, order kept */
static char mem_mode[256][8], mem_key[256][48]; static int mem_on[256], nmem;

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *s, char **f, int max) { int n = 0; char *p = s; f[n++] = p; for (; *p; p++) if (*p == '|' && n < max) { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int die2(const char *m) { fprintf(stderr, "ui_mode_op: %s\n", m); return 2; }
static int word_ok(const char *s) { if (!*s) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '-')) return 0; return 1; }

static int load_bars(const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) { fprintf(stderr, "ui_mode_op: cannot read %s\n", path); return 2; }
    char ln[LINE], *f[10]; int lineno = 0;
    while (fgets(ln, sizeof ln, fp)) {
        lineno++; if (ln[0] == '#') continue; int n = split(ln, f, 10); if (n < 1 || strcmp(f[0], "BAR")) continue;
        if (n < 7) { fprintf(stderr, "ui_mode_op: %s line %d: BAR needs id | key | slot | modes | default | group | note\n", path, lineno); fclose(fp); return 2; }
        if (nb >= MAXB) { fclose(fp); return die2("too many BAR rows"); }
        Bar *b = &B[nb]; memset(b, 0, sizeof *b);
        if (!word_ok(f[1]) || !word_ok(f[2]) || strlen(f[1]) > 31 || strlen(f[2]) > 47) { fprintf(stderr, "ui_mode_op: %s line %d: id and key are letters, digits, _ or - only\n", path, lineno); fclose(fp); return 2; }
        snprintf(b->id, sizeof b->id, "%s", f[1]); snprintf(b->key, sizeof b->key, "%s", f[2]);
        if (strcmp(f[3], "left") && strcmp(f[3], "right") && strcmp(f[3], "top") && strcmp(f[3], "bottom")) { fprintf(stderr, "ui_mode_op: %s line %d: slot is left, right, top or bottom\n", path, lineno); fclose(fp); return 2; }
        snprintf(b->slot, sizeof b->slot, "%s", f[3]);
        char mb[64]; snprintf(mb, sizeof mb, "%s", f[4]); char *sp, *t; int any = 0;
        for (t = strtok_r(mb, ",", &sp); t; t = strtok_r(NULL, ",", &sp)) { t = trim(t); if (!strcmp(t, "edit")) b->edit = 1; else if (!strcmp(t, "play")) b->play = 1; else { fprintf(stderr, "ui_mode_op: %s line %d: a mode is edit or play, not \"%s\"\n", path, lineno, t); fclose(fp); return 2; } any = 1; }
        if (!any) { fprintf(stderr, "ui_mode_op: %s line %d: a bar needs at least one mode\n", path, lineno); fclose(fp); return 2; }
        if (strcmp(f[5], "on") && strcmp(f[5], "off")) { fprintf(stderr, "ui_mode_op: %s line %d: default is on or off\n", path, lineno); fclose(fp); return 2; }
        b->def_on = !strcmp(f[5], "on"); snprintf(b->group, sizeof b->group, "%s", f[6]);
        for (int i = 0; i < nb; i++) if (!strcmp(B[i].id, b->id) || !strcmp(B[i].key, b->key)) { fprintf(stderr, "ui_mode_op: %s line %d: id or key \"%s\" is declared twice\n", path, lineno, b->id); fclose(fp); return 2; }
        nb++;
    }
    fclose(fp); return 0;
}
static int kv_find(const char *k) { for (int i = 0; i < nkv; i++) if (!strcmp(kvk[i], k)) return i; return -1; }
static void kv_set(const char *k, const char *v) { int i = kv_find(k); if (i < 0) { if (nkv >= 128) return; i = nkv++; snprintf(kvk[i], sizeof kvk[i], "%s", k); } snprintf(kvv[i], sizeof kvv[i], "%s", v); }
static void load_kv(const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) return; char ln[LINE];
    while (fgets(ln, sizeof ln, fp)) { char *e = strchr(ln, '='); if (!e) continue; *e = 0; char *v = e + 1; size_t l = strlen(v); while (l && (v[l - 1] == '\n' || v[l - 1] == '\r')) v[--l] = 0; kv_set(ln, v); }
    fclose(fp);
}
static int write_atomic(const char *path, const char *text) { char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *o = fopen(tmp, "w"); if (!o) return 1; fputs(text, o); fclose(o); return rename(tmp, path) ? 1 : 0; }
static void load_mem(const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) return; char ln[LINE], *f[6];
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split(ln, f, 6); if (n >= 4 && !strcmp(f[0], "MEM") && nmem < 256) { snprintf(mem_mode[nmem], 8, "%s", f[1]); snprintf(mem_key[nmem], 48, "%s", f[2]); mem_on[nmem] = !strcmp(f[3], "on"); nmem++; } }
    fclose(fp);
}
static int mem_get(const char *mode, const char *key) { for (int i = 0; i < nmem; i++) if (!strcmp(mem_mode[i], mode) && !strcmp(mem_key[i], key)) return mem_on[i]; return -1; }
static void mem_put(const char *mode, const char *key, int on) { for (int i = 0; i < nmem; i++) if (!strcmp(mem_mode[i], mode) && !strcmp(mem_key[i], key)) { mem_on[i] = on; return; } if (nmem < 256) { snprintf(mem_mode[nmem], 8, "%s", mode); snprintf(mem_key[nmem], 48, "%s", key); mem_on[nmem] = on; nmem++; } }
static int in_mode(const Bar *b, const char *mode) { return !strcmp(mode, "edit") ? b->edit : b->play; }

int main(int argc, char **argv) {
    if (argc < 5) return die2("usage: ui_mode_op <sides.txt> --bars <layouts.pdl> [--bars ...] [--memory <file>] <edit|play|show>");
    const char *sides = argv[1], *memfile = NULL, *want = NULL; char defmem[4200];
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--bars") && i + 1 < argc) { int rc = load_bars(argv[++i]); if (rc) return rc; }
        else if (!strcmp(argv[i], "--memory") && i + 1 < argc) memfile = argv[++i];
        else if (!want && argv[i][0] != '-') want = argv[i];
        else return die2("bad argument");
    }
    if (!want || (strcmp(want, "edit") && strcmp(want, "play") && strcmp(want, "show"))) return die2("the mode is edit, play or show");
    if (nb == 0) return die2("no BAR rows: pass --bars <layouts.pdl>");
    if (!memfile) { char sd[4200]; snprintf(sd, sizeof sd, "%s", sides); char *sl = strrchr(sd, '/'); if (sl) *sl = 0; else strcpy(sd, "."); snprintf(defmem, sizeof defmem, "%s/sides_mode_memory.pdl", sd); memfile = defmem; }
    load_kv(sides); load_mem(memfile);
    int ci = kv_find("ui_mode"); const char *from = (ci >= 0 && !strcmp(kvv[ci], "play")) ? "play" : "edit";
    if (!strcmp(want, "show")) { printf("UIMODE %s\n", from); return 0; }
    /* 1 remember the bars of the mode we are leaving (also when staying: then step 2 restores exactly this) */
    for (int i = 0; i < nb; i++) if (in_mode(&B[i], from)) { int k = kv_find(B[i].key); mem_put(from, B[i].key, k >= 0 && kvv[k][0] ? 1 : 0); }
    /* 2 decide the new state of every declared bar */
    int on[MAXB]; for (int i = 0; i < nb; i++) { on[i] = 0; if (in_mode(&B[i], want)) { int m = mem_get(want, B[i].key); on[i] = m >= 0 ? m : B[i].def_on; } }
    for (int i = 0; i < nb; i++) if (on[i] && B[i].group[0]) for (int j = 0; j < i; j++) if (on[j] && !strcmp(B[j].slot, B[i].slot) && !strcmp(B[j].group, B[i].group)) { on[i] = 0; break; }
    /* 3 write */
    char ids_on[1024] = "", ids_off[1024] = "";
    for (int i = 0; i < nb; i++) { kv_set(B[i].key, on[i] ? "1" : ""); char *d = on[i] ? ids_on : ids_off; size_t l = strlen(d); snprintf(d + l, 1024 - l, "%s%s", l ? "," : "", B[i].id); }
    kv_set("ui_mode", want); kv_set("mode_edit", !strcmp(want, "edit") ? "1" : ""); kv_set("mode_play", !strcmp(want, "play") ? "1" : "");
    static char out[128 * 330]; out[0] = 0; for (int i = 0; i < nkv; i++) { size_t l = strlen(out); snprintf(out + l, sizeof out - l, "%s=%s\n", kvk[i], kvv[i]); }
    if (write_atomic(sides, out)) return die2("cannot write sides.txt");
    static char mo[256 * 90 + 200]; snprintf(mo, sizeof mo, "# sides_mode_memory.pdl - a user PREFERENCE (which bars were open in each mode), written by ui_mode_op; delete it and the defaults apply. Never game state.\n");
    for (int i = 0; i < nmem; i++) { size_t l = strlen(mo); snprintf(mo + l, sizeof mo - l, "MEM | %s | %s | %s\n", mem_mode[i], mem_key[i], mem_on[i] ? "on" : "off"); }
    if (write_atomic(memfile, mo)) return die2("cannot write the memory file");
    printf("UIMODE %s->%s on=%s off=%s\n", from, want, ids_on, ids_off); return 0;
}
