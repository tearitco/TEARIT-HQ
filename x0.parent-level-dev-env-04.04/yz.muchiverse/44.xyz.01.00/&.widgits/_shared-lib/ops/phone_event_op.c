/* phone_event_op - the "phone.on-message" event SOURCE: one pass that plays an entity's `on-message` event page for every new text in its inbox
 * (Texting design 3.5e, owner 2026-11-16: "the auto reply should use events"). It decides nothing about WHAT to say: the page does (registry command
 * phone_reply -> phone_send_op), so a reply is an ordinary outbox line that the router delivers. This op only finds new messages and runs the page.
 *
 * Usage: phone_event_op.+x <server_dir> <house> [--tunables FILE] [--now-ms N]
 *   <server_dir> holds phones.index (number|uid|label|entity_dir|phone_dir, written by phone_ensure_op), tunables.conf, and gets:
 *     event_cursors.txt  (<number>|<byte offset into that phone's inbox>, written here)
 *     event_replies.txt  (<epoch_ms>|<phone number>|<from number>|<verdict>, the pair-cap ledger; trimmed to the last 10 minutes when over 256 KiB)
 *     observations.log   (one row per decision: <epoch_ms>|reply|<joint>=<value>|<inputs>|<verdict>)
 *     no_reply.txt       (optional, one phone number per line: phones that never get the DEFAULT page, e.g. a human's own phone)
 *   The page: <entity_dir> (its event_pkg/ has pages/) (the entity's OWN page wins, even a silent one); else, when reply_default=1 and the phone is
 *   not in no_reply.txt, the shared <server_dir>/_TEMPLATE/default-reply-event (same layout); else nothing runs.
 *   It runs  bash <house>/&.widgits/events-hq/ops/play_event.sh <pkg> <house> on-message  with MUCHI_TARGET_ENT, MUCHI_PHONE (the phone dir),
 *   MUCHI_MSG_FROM (sender number), MUCHI_MSG_TEXT in the environment, with a 20 s watchdog (fork+exec+waitpid, never popen).
 * Joints (tunables.conf, read every pass): reply_enabled (default 1), reply_default (default 0: nobody gets the default page until the owner opts in),
 *   reply_max_per_pair_per_min (default 3), reply_batch_max (default 20 messages per pass).
 * Rules: only kind `say` lines addressed to that phone; a line whose text starts with "[auto]" never triggers a page (two bots never chain); change detection
 * is by inbox SIZE against a saved byte cursor, never mtime (a smaller file = rotated, resync from 0); a half-written last line waits for the next pass;
 * this op writes NO inbox and NO outbox (the page's phone_send_op writes the outbox, the router writes inboxes: one writer per file).
 * Exit: 0 pass completed, 2 usage / unreadable index. Prints: played=N skipped_auto=N capped=N nopage=N failed=N */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAXP 4096
#define PB 1024
typedef struct { char number[16], entity[PB], phone[PB]; long long cur; } Ph;
static Ph P[MAXP]; static int NP;
static char SD[PB], HOUSE[PB], TUN[PB + 32];
static long long NOW;

static long long now_ms(void) { struct timespec t; clock_gettime(CLOCK_REALTIME, &t); return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static int exists(const char *p) { struct stat s; return stat(p, &s) == 0; }
static long long fsize(const char *p) { struct stat s; return stat(p, &s) ? -1 : (long long)s.st_size; }
static void append_line(const char *path, const char *line) { FILE *f = fopen(path, "a"); if (f) { fputs(line, f); fclose(f); } }
static void obs(const char *joint, long long val, const char *inputs, const char *verdict) {
    char p[PB + 32], l[1024]; snprintf(p, sizeof p, "%s/observations.log", SD);
    snprintf(l, sizeof l, "%lld|reply|%s=%lld|%s|%s\n", NOW, joint, val, inputs, verdict); append_line(p, l);
}
static long long tun(const char *name, long long def) {
    FILE *f = fopen(TUN, "r"); char line[256]; long long v = def; size_t n = strlen(name);
    if (!f) return def;
    while (fgets(line, sizeof line, f)) if (line[0] != '#' && !strncmp(line, name, n) && line[n] == '=') v = atoll(line + n + 1);
    fclose(f); return v;
}
static int in_noreply(const char *number) {
    char p[PB + 32], line[64]; FILE *f; int hit = 0; snprintf(p, sizeof p, "%s/no_reply.txt", SD);
    if (!(f = fopen(p, "r"))) return 0;
    while (fgets(line, sizeof line, f)) { line[strcspn(line, "\r\n")] = 0; if (!strcmp(line, number)) hit = 1; }
    fclose(f); return hit;
}
/* replies already played for this (phone, from) pair in the last 60 s */
static int recent_plays(const char *number, const char *from) {
    char p[PB + 32], line[256]; FILE *f; int n = 0; snprintf(p, sizeof p, "%s/event_replies.txt", SD);
    if (!(f = fopen(p, "r"))) return 0;
    while (fgets(line, sizeof line, f)) {
        char *a = strtok(line, "|"), *b = strtok(NULL, "|"), *c = strtok(NULL, "|"), *d = strtok(NULL, "|\r\n");
        if (a && b && c && d && !strcmp(b, number) && !strcmp(c, from) && !strcmp(d, "played") && atoll(a) >= NOW - 60000) n++;
    }
    fclose(f); return n;
}
static void trim_replies(void) {
    char p[PB + 32], q[PB + 40], line[256]; FILE *f, *o; snprintf(p, sizeof p, "%s/event_replies.txt", SD);
    if (fsize(p) <= 262144) return;
    snprintf(q, sizeof q, "%s.tmp", p); if (!(f = fopen(p, "r"))) return; if (!(o = fopen(q, "w"))) { fclose(f); return; }
    while (fgets(line, sizeof line, f)) if (atoll(line) >= NOW - 600000) fputs(line, o);
    fclose(f); fclose(o); rename(q, p);
}
static int run_page(const char *pkg, const char *ent, const char *phone, const char *from, const char *text) {
    char script[PB + 64]; snprintf(script, sizeof script, "%s/&.widgits/events-hq/ops/play_event.sh", HOUSE);
    pid_t pid = fork(); if (pid < 0) return 125;
    if (pid == 0) {
        int dn = open("/dev/null", O_WRONLY); if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); }
        setenv("MUCHI_TARGET_ENT", ent, 1); setenv("MUCHI_PHONE", phone, 1); setenv("MUCHI_MSG_FROM", from, 1); setenv("MUCHI_MSG_TEXT", text, 1);
        execl("/bin/bash", "bash", script, pkg, HOUSE, "on-message", (char *)0); _exit(127);
    }
    for (int i = 0; i < 200; i++) {
        int st; pid_t w = waitpid(pid, &st, WNOHANG);
        if (w == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : 128;
        usleep(100000);
    }
    kill(pid, SIGKILL); waitpid(pid, NULL, 0); return 124;
}
static void load_cursors(void) {
    char p[PB + 32], line[128]; FILE *f; snprintf(p, sizeof p, "%s/event_cursors.txt", SD);
    if (!(f = fopen(p, "r"))) return;
    while (fgets(line, sizeof line, f)) {
        char *b = strchr(line, '|'); if (!b) continue; *b = 0;
        for (int i = 0; i < NP; i++) if (!strcmp(P[i].number, line)) P[i].cur = atoll(b + 1);
    }
    fclose(f);
}
static void save_cursors(void) {
    char p[PB + 32], q[PB + 40]; FILE *f; snprintf(p, sizeof p, "%s/event_cursors.txt", SD); snprintf(q, sizeof q, "%s.tmp", p);
    if (!(f = fopen(q, "w"))) return;
    for (int i = 0; i < NP; i++) fprintf(f, "%s|%lld\n", P[i].number, P[i].cur);
    fclose(f); rename(q, p);
}
static int load_index(void) {
    char p[PB + 32], line[4 * PB]; FILE *f; snprintf(p, sizeof p, "%s/phones.index", SD);
    if (!(f = fopen(p, "r"))) return 0;
    while (NP < MAXP && fgets(line, sizeof line, f)) {
        char *fld[5]; int n = 0; char *s = line; line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '#' || !line[0]) continue;
        while (n < 5) { fld[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; }
        if (n < 5 || strlen(fld[0]) != 13 || strlen(fld[3]) >= PB || strlen(fld[4]) >= PB) continue;
        snprintf(P[NP].number, sizeof P[NP].number, "%s", fld[0]); snprintf(P[NP].entity, PB, "%s", fld[3]); snprintf(P[NP].phone, PB, "%s", fld[4]); P[NP].cur = 0; NP++;
    }
    fclose(f); return 1;
}

int main(int argc, char **argv) {
    int played = 0, skipped_auto = 0, capped = 0, nopage = 0, failed = 0, batch_left;
    if (argc < 3) { fprintf(stderr, "usage: phone_event_op <server_dir> <house> [--tunables FILE] [--now-ms N]\n"); return 2; }
    snprintf(SD, sizeof SD, "%s", argv[1]); snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); snprintf(TUN, sizeof TUN, "%s/tunables.conf", SD); NOW = now_ms();
    for (int i = 3; i + 1 < argc; i += 2) { if (!strcmp(argv[i], "--tunables")) snprintf(TUN, sizeof TUN, "%s", argv[i + 1]); else if (!strcmp(argv[i], "--now-ms")) NOW = atoll(argv[i + 1]); }
    if (!load_index()) { fprintf(stderr, "phone_event_op: cannot read %s/phones.index\n", SD); return 2; }
    load_cursors();
    if (!tun("reply_enabled", 1)) { obs("reply_enabled", 0, "-", "off"); printf("played=0 skipped_auto=0 capped=0 nopage=0 failed=0 (reply_enabled=0)\n"); return 0; }
    long long cap = tun("reply_max_per_pair_per_min", 3), batch = tun("reply_batch_max", 20), deflt = tun("reply_default", 0);
    batch_left = (int)batch;
    for (int i = 0; i < NP && batch_left > 0; i++) {
        char inbox[PB + 16]; snprintf(inbox, sizeof inbox, "%s/inbox.txt", P[i].phone);
        long long sz = fsize(inbox); if (sz < 0) continue; if (sz < P[i].cur) P[i].cur = 0; if (sz == P[i].cur) continue;
        FILE *f = fopen(inbox, "r"); if (!f) continue; fseek(f, (long)P[i].cur, SEEK_SET);
        char line[4096];
        while (batch_left > 0 && fgets(line, sizeof line, f)) {
            size_t ln = strlen(line); if (ln == 0 || line[ln - 1] != '\n') break;      /* half-written line: wait for the next pass */
            P[i].cur += (long long)ln; line[ln - 1] = 0;
            char *fld[6]; int n = 0; char *s = line;
            while (n < 5) { char *b = strchr(s, '|'); if (!b) break; *b = 0; fld[n++] = s; s = b + 1; }
            if (n < 5) continue;                       /* epoch|from|to|kind|ref|text: five separators or it is not a message */
            fld[5] = s;                                /* text keeps any later '|' */
            if (strcmp(fld[3], "say") || strcmp(fld[2], P[i].number)) continue;
            batch_left--;
            if (!strncmp(fld[5], "[auto]", 6)) { skipped_auto++; obs("auto_guard", 1, fld[1], "skipped-auto"); continue; }
            char own[PB + 16], tmpl[PB + 64], chk[PB + 96], *pkg = NULL;       /* play_event.sh takes the folder that CONTAINS event_pkg/ */
            snprintf(own, sizeof own, "%s", P[i].entity); snprintf(tmpl, sizeof tmpl, "%s/_TEMPLATE/default-reply-event", SD);
            snprintf(chk, sizeof chk, "%s/event_pkg/pages", own);
            if (exists(chk)) pkg = own;
            else { snprintf(chk, sizeof chk, "%s/event_pkg/pages", tmpl); if (deflt && !in_noreply(P[i].number) && exists(chk)) pkg = tmpl; }
            if (!pkg) { nopage++; obs("reply_default", deflt, fld[1], "no-page"); continue; }
            int have = recent_plays(P[i].number, fld[1]);
            if (have >= cap) { capped++; char in[64]; snprintf(in, sizeof in, "%s", fld[1]); obs("reply_max_per_pair_per_min", cap, in, "capped"); continue; }
            int rc = run_page(pkg, P[i].entity, P[i].phone, fld[1], fld[5]);
            char rp[PB + 32], rl[128]; snprintf(rp, sizeof rp, "%s/event_replies.txt", SD);
            snprintf(rl, sizeof rl, "%lld|%s|%s|%s\n", NOW, P[i].number, fld[1], rc == 0 ? "played" : "failed"); append_line(rp, rl);
            if (rc == 0) { played++; obs("reply_max_per_pair_per_min", cap, fld[1], "played"); } else { failed++; obs("page_exit", rc, fld[1], "failed"); }
        }
        fclose(f);
    }
    save_cursors(); trim_replies();
    printf("played=%d skipped_auto=%d capped=%d nopage=%d failed=%d\n", played, skipped_auto, capped, nopage, failed);
    return 0;
}
