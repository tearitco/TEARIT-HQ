/* ui_mode_op - switch a window between EDIT and PLAY by flipping its sidebar state keys from data (B1 of PROPOSAL-PLAY-MODE-SEATS-AND-GAME-SIDEBARS-2026-11-07; owner decisions 2026-11-07: Player is the
 * edit/play switch, bars are declared in layouts.pdl, slots stay flexible).
 *

 * Usage:  ui_mode_op <sides.txt> --bars <layouts.pdl> [--bars <layouts.pdl>]... [--memory <file>] [--var NAME=VALUE]... [--dry] [--no-settings] <edit|play|show>

 *   --no-settings  switch the bars only: SETTING rows are validated but never read or applied (a test of the bars must not touch a live window's chat flag)
 *   --var     $NAME in a SETTING row is replaced by VALUE (for example HOUSE, APP);  --dry  report what a SETTING would apply and run nothing
 *   <sides.txt>   the window's key=value file (the board template loads it as a vars file; a bar row is shown with show="${key}": "1" = shown, EMPTY = hidden, the side_toggle convention)
 *   layouts.pdl   rows `BAR | id | key | slot | modes | default | group | note`
 *                   id       a name, unique across all the files          key      the state key in sides.txt, unique
 *                   slot     left | right | top | bottom                   modes    edit, play or edit,play (a bar is shown only in the modes it lists)

 *                   default  on | off, or per mode like edit=off,play=on: used the first time a mode is entered   group    optional. Bars with the same slot and the same non-empty group are EXCLUSIVE (the first one that
 *                            would be on wins, so pets and palettes can share the left slot); an EMPTY group stacks freely. The slot policy is data, not code.
 *   --memory      where each bar's last state per mode is kept (default: sides_mode_memory.pdl beside sides.txt); rows `MEM | <mode> | <key> | on|off`. It is a USER PREFERENCE, never game state:
 *                 delete it and the defaults apply again.
 * What a switch does: (1) remember the state of every bar of the CURRENT mode (the key `ui_mode`, default edit), (2) for the new mode show each bar that lists it, at its remembered state or its
 * default, and hide every bar that does not, (3) write `ui_mode`, `mode_edit` and `mode_play` (1 or EMPTY) so a row can be gated on the mode itself. Keys that no BAR declares are kept as they are.

 * SETTING rows (see the Setting type): `SETTING | id | read_file | apply | edit_default | play_default | note` keeps a value that lives outside sides.txt (for example which chat the right bar shows) per mode, and puts it back by running the game's own verb; exit 1 if an apply failed (sides.txt is still written).
 * `show` prints the current mode and writes nothing; `--dry` does the whole switch in memory, prints what it would do and writes NOTHING (not sides.txt, not the memory file) and runs no apply. Switching to the mode you are already in changes nothing (idempotent).
 * Exit: 0 ok | 2 usage, an unreadable or bad layouts file, an unknown mode. Output: `UIMODE <from>-><to> on=<ids> off=<ids>`.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/ui_mode_op.+x ops/ui_mode_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAXB 64
#define LINE 512
typedef struct { char id[32], key[48], slot[12], group[24]; int edit, play, def_edit, def_play; } Bar;
static Bar B[MAXB]; static int nb;
static char kvk[128][64], kvv[128][256]; static int nkv;       /* sides.txt, order kept */
static char mem_mode[256][8], mem_key[256][48]; static int mem_on[256], nmem;
/* SETTING | id | read_file | apply | edit_default | play_default | note: a value the window keeps OUTSIDE sides.txt (for example which chat the right bar shows). It is read from read_file (first line), remembered per mode, and put back by running `apply` (argv split on spaces, %v = the value, NO shell) through the game's own verb. A default of - leaves it alone. */
typedef struct { char id[32], file[1024], apply[1024], def_edit[32], def_play[32]; } Setting;
static Setting SG[16]; static int nsg;
static char memv_mode[64][8], memv_id[64][32], memv_val[64][32]; static int nmemv;
static char VARN[8][24], VARV[8][1024]; static int nvar; static int dry; static int nosettings;

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *s, char **f, int max) { int n = 0; char *p = s; f[n++] = p; for (; *p; p++) if (*p == '|' && n < max) { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int die2(const char *m) { fprintf(stderr, "ui_mode_op: %s\n", m); return 2; }
static int word_ok(const char *s) { if (!*s) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '-')) return 0; return 1; }

static void expand(const char *in, char *out, size_t cap) {      /* $NAME -> its --var value, anything else literal */
    size_t o = 0; for (const char *p = in; *p && o + 1 < cap; ) {
        if (*p == '$') { int hit = 0; for (int i = 0; i < nvar; i++) { size_t l = strlen(VARN[i]); if (!strncmp(p + 1, VARN[i], l)) { for (const char *v = VARV[i]; *v && o + 1 < cap; v++) out[o++] = *v; p += 1 + l; hit = 1; break; } } if (hit) continue; }
        out[o++] = *p++; }
    out[o] = 0;
}
static void read_first_line(const char *path, char *out, size_t cap) { out[0] = 0; FILE *f = fopen(path, "r"); if (!f) return; char b[128]; if (fgets(b, sizeof b, f)) { char *t = trim(b); snprintf(out, cap, "%s", t); } fclose(f); }
static int memv_get(const char *mode, const char *id, char *out, size_t cap) { for (int i = 0; i < nmemv; i++) if (!strcmp(memv_mode[i], mode) && !strcmp(memv_id[i], id)) { snprintf(out, cap, "%s", memv_val[i]); return 1; } return 0; }
static void memv_put(const char *mode, const char *id, const char *val) { for (int i = 0; i < nmemv; i++) if (!strcmp(memv_mode[i], mode) && !strcmp(memv_id[i], id)) { snprintf(memv_val[i], 32, "%s", val); return; } if (nmemv < 64) { snprintf(memv_mode[nmemv], 8, "%s", mode); snprintf(memv_id[nmemv], 32, "%s", id); snprintf(memv_val[nmemv], 32, "%s", val); nmemv++; } }
/* run a setting's apply command with %v = value; returns 0 on success */
static int run_apply(const Setting *s, const char *val) {
    char cmd[2100]; size_t o = 0; for (const char *p = s->apply; *p && o + 1 < sizeof cmd; ) { if (p[0] == '%' && p[1] == 'v') { for (const char *v = val; *v && o + 1 < sizeof cmd; v++) cmd[o++] = *v; p += 2; } else cmd[o++] = *p++; } cmd[o] = 0;
    char *av[16]; int ac = 0; char *sp; for (char *t = strtok_r(cmd, " ", &sp); t && ac < 15; t = strtok_r(NULL, " ", &sp)) av[ac++] = t; av[ac] = NULL; if (!ac) return 1;
    pid_t pid = fork(); if (pid < 0) return 1; if (pid == 0) { int dn = open("/dev/null", O_WRONLY); if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); } execv(av[0], av); _exit(127); }
    for (int i = 0; i < 100; i++) { int st; pid_t r = waitpid(pid, &st, WNOHANG); if (r == pid) return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : 1; usleep(100000); }
    kill(pid, SIGKILL); waitpid(pid, NULL, 0); return 1;
}
static int load_bars(const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) { fprintf(stderr, "ui_mode_op: cannot read %s\n", path); return 2; }
    char ln[LINE], *f[10]; int lineno = 0;
    while (fgets(ln, sizeof ln, fp)) {
        lineno++; if (ln[0] == '#') continue; int n = split(ln, f, 10);
        if (n >= 1 && !strcmp(f[0], "SETTING")) { if (n < 7 || !word_ok(f[1]) || strlen(f[1]) > 31 || nsg >= 16) { fprintf(stderr, "ui_mode_op: %s line %d: SETTING needs id | read_file | apply | edit_default | play_default | note\n", path, lineno); fclose(fp); return 2; } Setting *s = &SG[nsg]; memset(s, 0, sizeof *s); snprintf(s->id, sizeof s->id, "%s", f[1]); expand(f[2], s->file, sizeof s->file); expand(f[3], s->apply, sizeof s->apply); snprintf(s->def_edit, 32, "%s", f[4]); snprintf(s->def_play, 32, "%s", f[5]); if (!s->file[0] || !s->apply[0] || (strcmp(s->def_edit, "-") && !word_ok(s->def_edit)) || (strcmp(s->def_play, "-") && !word_ok(s->def_play))) { fprintf(stderr, "ui_mode_op: %s line %d: a SETTING needs a file, an apply command and word-or-dash defaults\n", path, lineno); fclose(fp); return 2; } for (int i = 0; i < nsg; i++) if (!strcmp(SG[i].id, s->id)) { fprintf(stderr, "ui_mode_op: %s line %d: setting %s is declared twice\n", path, lineno, s->id); fclose(fp); return 2; } nsg++; continue; }
        if (n < 1 || strcmp(f[0], "BAR")) continue;
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
        { char db[64]; snprintf(db, sizeof db, "%s", f[5]); int de = 0, dp = 0, okd = 1; if (!strcmp(db, "on")) de = dp = 1; else if (!strcmp(db, "off")) de = dp = 0; else { char *sp2, *t2; okd = 0; for (t2 = strtok_r(db, ",", &sp2); t2; t2 = strtok_r(NULL, ",", &sp2)) { t2 = trim(t2); if (!strcmp(t2, "edit=on")) { de = 1; okd |= 1; } else if (!strcmp(t2, "edit=off")) { de = 0; okd |= 1; } else if (!strcmp(t2, "play=on")) { dp = 1; okd |= 1; } else if (!strcmp(t2, "play=off")) { dp = 0; okd |= 1; } else { okd = 0; break; } } }
          if (!okd) { fprintf(stderr, "ui_mode_op: %s line %d: default is on, off, or per mode like edit=off,play=on\n", path, lineno); fclose(fp); return 2; } b->def_edit = de; b->def_play = dp; }
        snprintf(b->group, sizeof b->group, "%s", f[6]);
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
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split(ln, f, 6); if (n >= 4 && !strcmp(f[0], "MEMV")) { memv_put(f[1], f[2], f[3]); continue; } if (n >= 4 && !strcmp(f[0], "MEM") && nmem < 256) { snprintf(mem_mode[nmem], 8, "%s", f[1]); snprintf(mem_key[nmem], 48, "%s", f[2]); mem_on[nmem] = !strcmp(f[3], "on"); nmem++; } }
    fclose(fp);
}
static int mem_get(const char *mode, const char *key) { for (int i = 0; i < nmem; i++) if (!strcmp(mem_mode[i], mode) && !strcmp(mem_key[i], key)) return mem_on[i]; return -1; }
static void mem_put(const char *mode, const char *key, int on) { for (int i = 0; i < nmem; i++) if (!strcmp(mem_mode[i], mode) && !strcmp(mem_key[i], key)) { mem_on[i] = on; return; } if (nmem < 256) { snprintf(mem_mode[nmem], 8, "%s", mode); snprintf(mem_key[nmem], 48, "%s", key); mem_on[nmem] = on; nmem++; } }
static int in_mode(const Bar *b, const char *mode) { return !strcmp(mode, "edit") ? b->edit : b->play; }

int main(int argc, char **argv) {
    if (argc < 5) return die2("usage: ui_mode_op <sides.txt> --bars <layouts.pdl> [--bars ...] [--memory <file>] <edit|play|show>");
    const char *sides = argv[1], *memfile = NULL, *want = NULL; char defmem[4200]; const char *barfiles[16]; int nbf = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--bars") && i + 1 < argc && nbf < 16) barfiles[nbf++] = argv[++i];
        else if (!strcmp(argv[i], "--var") && i + 1 < argc && nvar < 8) { char *eq = strchr(argv[++i], '='); if (!eq || eq == argv[i] || (size_t)(eq - argv[i]) > 23) return die2("--var NAME=VALUE"); snprintf(VARN[nvar], 24, "%.*s", (int)(eq - argv[i]), argv[i]); snprintf(VARV[nvar], 1024, "%s", eq + 1); nvar++; }
        else if (!strcmp(argv[i], "--dry")) dry = 1;
        else if (!strcmp(argv[i], "--no-settings")) nosettings = 1;
        else if (!strcmp(argv[i], "--memory") && i + 1 < argc) memfile = argv[++i];
        else if (!want && argv[i][0] != '-') want = argv[i];
        else return die2("bad argument");
    }
    if (!want || (strcmp(want, "edit") && strcmp(want, "play") && strcmp(want, "show"))) return die2("the mode is edit, play or show");
    for (int i = 0; i < nbf; i++) { int rc = load_bars(barfiles[i]); if (rc) return rc; }
    if (nosettings) nsg = 0;   /* bars only: SETTING rows are read (so they are checked) but never captured or applied */
    if (nb == 0) return die2("no BAR rows: pass --bars <layouts.pdl>");
    if (!memfile) { char sd[4200]; snprintf(sd, sizeof sd, "%s", sides); char *sl = strrchr(sd, '/'); if (sl) *sl = 0; else strcpy(sd, "."); snprintf(defmem, sizeof defmem, "%s/sides_mode_memory.pdl", sd); memfile = defmem; }
    load_kv(sides); load_mem(memfile);
    int ci = kv_find("ui_mode"); const char *from = (ci >= 0 && !strcmp(kvv[ci], "play")) ? "play" : "edit";
    if (!strcmp(want, "show")) { printf("UIMODE %s\n", from); return 0; }
    /* 1 remember the bars of the mode we are leaving (also when staying: then step 2 restores exactly this) */
    for (int i = 0; i < nb; i++) if (in_mode(&B[i], from)) { int k = kv_find(B[i].key); if (k >= 0) mem_put(from, B[i].key, kvv[k][0] ? 1 : 0); }   /* a key that was never set has NO state to remember (the declared default applies) */
    char cur_sv[16][32]; for (int i = 0; i < nsg; i++) { char v[64]; read_first_line(SG[i].file, v, sizeof v); snprintf(cur_sv[i], 32, "%s", v); if (v[0] && word_ok(v)) memv_put(from, SG[i].id, v); }   /* a setting with no value has nothing to remember */
    /* 2 decide the new state of every declared bar */
    int on[MAXB]; for (int i = 0; i < nb; i++) { on[i] = 0; if (in_mode(&B[i], want)) { int m = mem_get(want, B[i].key); on[i] = m >= 0 ? m : (!strcmp(want, "edit") ? B[i].def_edit : B[i].def_play); } }
    for (int i = 0; i < nb; i++) if (on[i] && B[i].group[0]) for (int j = 0; j < i; j++) if (on[j] && !strcmp(B[j].slot, B[i].slot) && !strcmp(B[j].group, B[i].group)) { on[i] = 0; break; }
    /* 3 write */
    char ids_on[1024] = "", ids_off[1024] = "";
    for (int i = 0; i < nb; i++) { kv_set(B[i].key, on[i] ? "1" : ""); char *d = on[i] ? ids_on : ids_off; size_t l = strlen(d); snprintf(d + l, 1024 - l, "%s%s", l ? "," : "", B[i].id); }
    kv_set("ui_mode", want); kv_set("mode_edit", !strcmp(want, "edit") ? "1" : ""); kv_set("mode_play", !strcmp(want, "play") ? "1" : "");
    static char out[128 * 330]; out[0] = 0; for (int i = 0; i < nkv; i++) { size_t l = strlen(out); snprintf(out + l, sizeof out - l, "%s=%s\n", kvk[i], kvv[i]); }
    if (!dry && write_atomic(sides, out)) return die2("cannot write sides.txt");
    static char mo[256 * 90 + 200]; snprintf(mo, sizeof mo, "# sides_mode_memory.pdl - a user PREFERENCE (which bars were open in each mode), written by ui_mode_op; delete it and the defaults apply. Never game state.\n");
    for (int i = 0; i < nmem; i++) { size_t l = strlen(mo); snprintf(mo + l, sizeof mo - l, "MEM | %s | %s | %s\n", mem_mode[i], mem_key[i], mem_on[i] ? "on" : "off"); }

    /* the settings: remembered value for the new mode, else its default; `-` leaves it alone; the apply command is the game's own verb */
    int failed = 0; char setmsg[512] = "";
    for (int i = 0; i < nsg; i++) { char val[32]; if (!memv_get(want, SG[i].id, val, sizeof val)) snprintf(val, sizeof val, "%s", !strcmp(want, "edit") ? SG[i].def_edit : SG[i].def_play); if (!strcmp(val, "-") || !strcmp(val, cur_sv[i])) continue;
        size_t l = strlen(setmsg); if (dry) { snprintf(setmsg + l, sizeof setmsg - l, " setting %s=%s (dry)", SG[i].id, val); continue; }
        if (run_apply(&SG[i], val)) { failed++; snprintf(setmsg + l, sizeof setmsg - l, " setting %s=%s FAILED", SG[i].id, val); } else snprintf(setmsg + l, sizeof setmsg - l, " setting %s=%s", SG[i].id, val); }
    { static char mo2[256 * 90 + 64 * 90 + 400]; snprintf(mo2, sizeof mo2, "%s", mo); for (int i = 0; i < nmemv; i++) { size_t l = strlen(mo2); snprintf(mo2 + l, sizeof mo2 - l, "MEMV | %s | %s | %s\n", memv_mode[i], memv_id[i], memv_val[i]); } if (!dry && write_atomic(memfile, mo2)) return die2("cannot write the memory file"); }
    printf("UIMODE %s->%s on=%s off=%s%s\n", from, want, ids_on, ids_off, setmsg); return failed ? 1 : 0;
}
