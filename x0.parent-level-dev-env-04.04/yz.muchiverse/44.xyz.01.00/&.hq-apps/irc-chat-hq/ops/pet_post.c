#define _GNU_SOURCE
/* pet_post - the ONE door a pet's IRC post goes through (Phase 2, owner 2026-11-04: "caps, a kill switch and a visible audit ledger from day one").
 *   pet_post.+x post <room> <pet_id> <text...>   post as a pet, if the rules allow
 *   pet_post.+x approve <id|first|last> | veto <id|first|last>   release / drop a line that was HELD for a human
 *   pet_post.+x status                            kill switch, hold mode, the caps, pending lines
 * It does not talk to the network: an allowed post is handed to the real chat_post_message.+x (same folder as this binary, or the one named by PET_POST_CHAT_OP), which writes the
 * room ledger and net/outbox.txt exactly as a player's post does. A pet therefore has NO other way out: the window, the harness and any pet code call this op.
 * Files, all under PRISC_PROJECT_ROOT (the chat node's root; default "."):
 *   pet_post.pdl            optional config, `KEY | value` rows: cap_n (posts, default 3), cap_window_s (default 60), max_len (default 280), hold (0|1, default 0 = send at once; 1 = every pet line waits for approve)
 *   pet_post_kill.txt       if this file EXISTS every pet post is refused (touch it = stop all pets; delete it = resume). PET_POST_KILL may name another path (e.g. one file for a whole house).
 *   pet_post_audit.txt      append-only: ts|decision|pet|room|text   decisions: SENT DENY_KILL DENY_CAP DENY_LEN DENY_EMPTY HELD APPROVED VETOED ERROR. Capped (see CAP_BYTES): the older half is dropped.
 *   pet_post_pending.txt    HELD lines: id|ts|pet|room|text
 * The rate cap counts this pet's SENT/APPROVED rows in the last cap_window_s seconds of the audit file, so there is no second state to drift.
 * COST: one process per post, a few small file reads; nothing runs between posts. Files/bytes per hour: audit grows by one line per attempt, capped at 256 KB.
 * Exit: 0 sent/held/approved/vetoed, 3 refused (the decision is on stdout), 2 usage/error. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define CAP_BYTES (256 * 1024)
static char ROOT[PATH_MAX] = ".", SELF_DIR[PATH_MAX] = ".";
static int cap_n = 3, cap_window = 60, max_len = 280, hold = 0;

static void path(char *o, const char *n) { snprintf(o, PATH_MAX, "%s/%s", ROOT, n); }
static long fsz(const char *p) { struct stat s; return stat(p, &s) == 0 ? (long)s.st_size : 0; }
static void clean(char *t) { for (char *p = t; *p; p++) if (*p == '\n' || *p == '\r' || *p == '\t' || *p == '|') *p = ' '; }      /* a pipe or newline in a line would forge another ledger row */
static void audit(const char *dec, const char *pet, const char *room, const char *text) {
    char p[PATH_MAX]; path(p, "pet_post_audit.txt");
    if (fsz(p) > CAP_BYTES) {      /* enforced cap: keep the newer half (a trim that skipped an oversize file would be the bug the house already had once) */
        FILE *f = fopen(p, "r"); if (f) { long n = fsz(p); char *b = malloc((size_t)n + 1); if (b) { size_t r = fread(b, 1, (size_t)n, f); b[r] = 0; char *cut = strchr(b + r / 2, '\n'); fclose(f);
            if (cut) { char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", p); FILE *o = fopen(tmp, "w"); if (o) { fputs(cut + 1, o); fclose(o); rename(tmp, p); } } free(b); } else fclose(f); } }
    FILE *f = fopen(p, "a"); if (!f) return; fprintf(f, "%ld|%s|%s|%s|%s\n", (long)time(NULL), dec, pet, room, text); fclose(f);
}
static void load_cfg(void) {
    char p[PATH_MAX], ln[256]; path(p, "pet_post.pdl"); FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f)) { char *b = strchr(ln, '|'); if (!b || ln[0] == '#') continue; *b = 0; char *k = ln, *v = b + 1; while (*k == ' ') k++; char *e = k + strlen(k); while (e > k && e[-1] == ' ') *--e = 0; int n = atoi(v);
        if (!strcmp(k, "cap_n")) cap_n = n; else if (!strcmp(k, "cap_window_s")) cap_window = n; else if (!strcmp(k, "max_len")) max_len = n; else if (!strcmp(k, "hold")) hold = n; }
    fclose(f);
}
static int killed(void) { const char *e = getenv("PET_POST_KILL"); char p[PATH_MAX]; if (e && e[0]) snprintf(p, sizeof p, "%s", e); else path(p, "pet_post_kill.txt"); return access(p, F_OK) == 0; }
static int recent(const char *pet) {      /* this pet's SENT/APPROVED rows inside the window, from the tail of the audit file */
    char p[PATH_MAX]; path(p, "pet_post_audit.txt"); FILE *f = fopen(p, "r"); if (!f) return 0; long n = fsz(p), off = n > 65536 ? n - 65536 : 0; fseek(f, off, SEEK_SET);
    char ln[1200]; int c = 0; long now = (long)time(NULL); while (fgets(ln, sizeof ln, f)) { long ts = atol(ln); char *a = strchr(ln, '|'); if (!a) continue; char *b = strchr(a + 1, '|'); if (!b) continue; *b = 0; char *dec = a + 1; char *c3 = b + 1; char *d = strchr(c3, '|'); if (!d) continue; *d = 0;
        if (now - ts <= cap_window && !strcmp(c3, pet) && (!strcmp(dec, "SENT") || !strcmp(dec, "APPROVED"))) c++; }
    fclose(f); return c;
}
static int send_line(const char *room, const char *pet, const char *text) {
    char op[PATH_MAX]; const char *e = getenv("PET_POST_CHAT_OP"); if (e && e[0]) snprintf(op, sizeof op, "%s", e); else { snprintf(op, sizeof op, "%s/chat_post_message.+x", SELF_DIR); if (access(op, X_OK) != 0) snprintf(op, sizeof op, "%s/ops/+x/chat_post_message.+x", ROOT); }      /* beside this binary (harness), else the chat node's own ops/+x (the real house) */
    pid_t pid = fork(); if (pid < 0) return -1; if (pid == 0) { execl(op, op, room, pet, text, (char *)NULL); _exit(127); }
    int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : -1;
}
static int pending_take(const char *id0, char *pet, char *room, char *text) {      /* remove line `id` from the pending file, return it; id "first" / "last" = the oldest / newest held line */
    char p[PATH_MAX], tmp[PATH_MAX + 8], ln[1500], idb[40]; const char *id = id0; path(p, "pet_post_pending.txt");
    if (!strcmp(id0, "first") || !strcmp(id0, "last")) { FILE *q = fopen(p, "r"); idb[0] = 0; if (q) { while (fgets(ln, sizeof ln, q)) { char *b = strchr(ln, '|'); if (!b) continue; *b = 0; snprintf(idb, sizeof idb, "%s", ln); if (!strcmp(id0, "first")) break; } fclose(q); } id = idb; } snprintf(tmp, sizeof tmp, "%s.tmp", p); FILE *f = fopen(p, "r"), *o = fopen(tmp, "w"); int found = 0; if (!f || !o) { if (f) fclose(f); if (o) fclose(o); return 0; }
    while (fgets(ln, sizeof ln, f)) { char cp[1500]; snprintf(cp, sizeof cp, "%s", ln); char *f1[5]; int n = 0; for (char *t = strtok(cp, "|\n"); t && n < 5; t = strtok(NULL, "|\n")) f1[n++] = t;
        if (n >= 5 && !strcmp(f1[0], id) && !found) { found = 1; snprintf(pet, 64, "%s", f1[2]); snprintf(room, 64, "%s", f1[3]); snprintf(text, 1024, "%s", f1[4]); continue; } fputs(ln, o); }
    fclose(f); fclose(o); rename(tmp, p); return found;
}
int main(int argc, char **argv) {
    const char *r = getenv("PRISC_PROJECT_ROOT"); if (r && r[0]) snprintf(ROOT, sizeof ROOT, "%s", r);
    { char self[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n > 0) { self[n] = 0; char *s = strrchr(self, '/'); if (s) { *s = 0; snprintf(SELF_DIR, sizeof SELF_DIR, "%s", self); } } else if (argv[0] && strchr(argv[0], '/')) { snprintf(SELF_DIR, sizeof SELF_DIR, "%s", argv[0]); char *s = strrchr(SELF_DIR, '/'); if (s) *s = 0; } }
    load_cfg(); if (argc < 2) { fprintf(stderr, "usage: pet_post post <room> <pet> <text> | approve <id> | veto <id> | status\n"); return 2; }
    if (!strcmp(argv[1], "status")) { char p[PATH_MAX]; path(p, "pet_post_pending.txt"); printf("kill=%s hold=%d cap=%d/%ds max_len=%d pending_bytes=%ld audit_bytes=%ld\n", killed() ? "ON" : "off", hold, cap_n, cap_window, max_len, fsz(p), ({ char a[PATH_MAX]; path(a, "pet_post_audit.txt"); fsz(a); })); return 0; }
    if ((!strcmp(argv[1], "approve") || !strcmp(argv[1], "veto")) && argc >= 3) {
        char pet[64] = "", room[64] = "", text[1024] = ""; if (!pending_take(argv[2], pet, room, text)) { printf("no pending line %s\n", argv[2]); return 2; }
        if (!strcmp(argv[1], "veto")) { audit("VETOED", pet, room, text); printf("VETOED %s\n", argv[2]); return 0; }
        if (killed()) { audit("DENY_KILL", pet, room, text); printf("DENY_KILL (kill switch on; line dropped)\n"); return 3; }
        if (send_line(room, pet, text) != 0) { audit("ERROR", pet, room, text); { char pp[PATH_MAX]; path(pp, "pet_post_pending.txt"); FILE *pf = fopen(pp, "a"); if (pf) { fprintf(pf, "%ld|%ld|%s|%s|%s\n", (long)time(NULL) * 100 + (getpid() % 100), (long)time(NULL), pet, room, text); fclose(pf); } } printf("ERROR chat_post_message failed (line kept pending)\n"); return 2; }
        audit("APPROVED", pet, room, text); printf("APPROVED %s\n", argv[2]); return 0; }
    if (!strcmp(argv[1], "post") && argc >= 5) {
        const char *room = argv[2], *pet = argv[3]; char text[1100] = ""; for (int i = 4; i < argc; i++) { if (i > 4) strncat(text, " ", sizeof text - strlen(text) - 1); strncat(text, argv[i], sizeof text - strlen(text) - 1); } clean(text);
        char rm[64], pt[64]; snprintf(rm, sizeof rm, "%.63s", room); snprintf(pt, sizeof pt, "%.63s", pet); clean(rm); clean(pt);
        if (killed()) { audit("DENY_KILL", pt, rm, text); printf("DENY_KILL\n"); return 3; }
        char *s = text; while (*s == ' ') s++; if (!*s) { audit("DENY_EMPTY", pt, rm, ""); printf("DENY_EMPTY\n"); return 3; }
        if ((int)strlen(s) > max_len) { audit("DENY_LEN", pt, rm, s); printf("DENY_LEN %d > %d\n", (int)strlen(s), max_len); return 3; }
        if (recent(pt) >= cap_n) { audit("DENY_CAP", pt, rm, s); printf("DENY_CAP %d posts in %ds\n", cap_n, cap_window); return 3; }
        if (hold) { char p[PATH_MAX]; path(p, "pet_post_pending.txt"); static long seq; seq = (long)time(NULL) * 100 + (getpid() % 100); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "%ld|%ld|%s|%s|%s\n", seq, (long)time(NULL), pt, rm, s); fclose(f); } audit("HELD", pt, rm, s); printf("HELD %ld\n", seq); return 0; }
        if (send_line(rm, pt, s) != 0) { audit("ERROR", pt, rm, s); printf("ERROR chat_post_message failed\n"); return 2; }
        audit("SENT", pt, rm, s); printf("SENT\n"); return 0; }
    fprintf(stderr, "usage: pet_post post <room> <pet> <text> | approve <id> | veto <id> | status\n"); return 2;
}
