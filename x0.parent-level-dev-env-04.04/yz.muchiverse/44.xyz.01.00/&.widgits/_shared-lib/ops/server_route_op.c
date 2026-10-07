/* server_route_op - the "server.route" event: one pass of the phone router (Q009, HAI-ROBOTS-PHONES-SERVER-DESIGN.md 3f, 4).
 *
 * Usage: server_route_op.+x <server_dir> [--tunables FILE] [--now-ms N]
 *   <server_dir> holds:  phones.index (written by phone_ensure_op / the manager hook: number|uid|label|entity_dir|phone_dir),
 *                        cursors.txt  (<number>|<byte offset into that phone's outbox>, written here),
 *                        ledger.txt   (one row per routed/rejected/unroutable message: <epoch_ms>|<what>|<from>|<to>|<kind>|<ref>),
 *                        observations.log (one row per decision: <epoch_ms>|route|<joint>=<value>|<inputs>|<verdict>  - the dataset tomom/a heuristic can learn from),
 *                        tunables.conf (name=value, '#' comments; defaults below; read every pass so editing it needs no recompile).
 *   Joints: route_max_msgs_per_min (default 60, per sender, counted from the ledger), route_batch_max (default 100 messages per pass).
 * Rules (house): this op is the ONLY writer of every inbox.txt; it never writes an outbox. Change detection is by append-only SIZE growth against a saved byte
 * cursor, never mtime; a smaller file than the cursor means "rotated: resync from 0". The sender's number in a message must equal the number of the phone it came
 * from (no spoofing). Unknown recipient -> a 'fail' message back to the sender, never a crash. A throttled message is left in the outbox for a later pass.
 * Exit: 0 pass completed, 2 usage / unreadable index. Prints: routed=N rejected=N unroutable=N throttled=N */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAXP 8192
#define PB 4352
typedef struct { char number[16]; char outbox[PB], inbox[PB], hist[PB]; long long cursor; int sent_window; } Phone;
static Phone P[MAXP]; static int NP;

static long long now_ms(void) { struct timespec t; clock_gettime(CLOCK_REALTIME, &t); return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static void append_line(const char *path, const char *line) { FILE *f = fopen(path, "a"); if (f) { fputs(line, f); fclose(f); } }
static int find_phone(const char *number) { for (int i = 0; i < NP; i++) if (!strcmp(P[i].number, number)) return i; return -1; }
static int join(char *out, size_t n, const char *a, const char *b) { size_t la = strlen(a), lb = strlen(b); if (la + 1 + lb + 1 > n) { out[0] = 0; return 0; } memcpy(out, a, la); out[la] = '/'; memcpy(out + la + 1, b, lb + 1); return 1; }

static long tun(const char *file, const char *name, long dflt) {
    char line[256]; FILE *f = fopen(file, "r"); long v = dflt; size_t nl = strlen(name);
    if (!f) return dflt;
    while (fgets(line, sizeof(line), f)) {
        char *s = line; while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || strncmp(s, name, nl)) continue;
        s += nl; while (*s == ' ') s++;
        if (*s != '=') continue;
        s++;
        while (*s == ' ') s++;
        v = atol(s);
    }
    fclose(f); return v;
}

int main(int argc, char **argv) {
    char idx[PB], cur[PB], led[PB], obs[PB], tun_file[PB], line[8192], tmp[PB + 8];
    const char *sd = NULL; long long now = 0; int routed = 0, rejected = 0, unroutable = 0, throttled = 0;
    tun_file[0] = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--tunables") && i + 1 < argc) snprintf(tun_file, sizeof(tun_file), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--now-ms") && i + 1 < argc) now = atoll(argv[++i]);
        else if (argv[i][0] != '-' && !sd) sd = argv[i];
        else { fprintf(stderr, "unknown argument: %s\n", argv[i]); return 2; }
    }
    if (!sd) { fprintf(stderr, "usage: server_route_op.+x <server_dir> [--tunables FILE] [--now-ms N]\n"); return 2; }
    if (!now) now = now_ms();
    join(idx, sizeof(idx), sd, "phones.index"); join(cur, sizeof(cur), sd, "cursors.txt"); join(led, sizeof(led), sd, "ledger.txt"); join(obs, sizeof(obs), sd, "observations.log");
    if (!tun_file[0]) join(tun_file, sizeof(tun_file), sd, "tunables.conf");
    long max_per_min = tun(tun_file, "route_max_msgs_per_min", 60), batch_max = tun(tun_file, "route_batch_max", 100);

    FILE *f = fopen(idx, "r");
    if (!f) { fprintf(stderr, "cannot read %s\n", idx); return 2; }
    while (NP < MAXP && fgets(line, sizeof(line), f)) {   /* number|uid|label|entity_dir|phone_dir */
        char *fld[5]; int n = 0; char *t = line; line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '#' || !line[0]) continue;
        while (n < 5) { fld[n++] = t; char *b = strchr(t, '|'); if (!b) break; *b = 0; t = b + 1; }
        if (n < 5 || strlen(fld[0]) != 13) continue;
        snprintf(P[NP].number, sizeof(P[NP].number), "%s", fld[0]);
        join(P[NP].outbox, PB, fld[4], "outbox.txt"); join(P[NP].inbox, PB, fld[4], "inbox.txt"); join(P[NP].hist, PB, fld[4], "history.txt");
        NP++;
    }
    fclose(f);
    if ((f = fopen(cur, "r"))) {   /* number|offset */
        while (fgets(line, sizeof(line), f)) { char *b = strchr(line, '|'); if (!b) continue; *b = 0; int i = find_phone(line); if (i >= 0) P[i].cursor = atoll(b + 1); }
        fclose(f);
    }
    if ((f = fopen(led, "r"))) {   /* messages each sender already routed in the last 60 s (rate cap state lives in the ledger, not in memory) */
        while (fgets(line, sizeof(line), f)) {
            char *fld[3]; int n = 0; char *t = line;
            while (n < 3) { fld[n++] = t; char *b = strchr(t, '|'); if (!b) break; *b = 0; t = b + 1; }
            if (n >= 3 && !strcmp(fld[1], "route") && atoll(fld[0]) >= now - 60000) { int i = find_phone(fld[2]); if (i >= 0) P[i].sent_window++; }
        }
        fclose(f);
    }

    for (int i = 0; i < NP; i++) {
        struct stat st; int stop_phone = 0;
        if (stat(P[i].outbox, &st) != 0) continue;
        if (st.st_size < P[i].cursor) {   /* rotated or truncated: resync from the start */
            snprintf(line, sizeof(line), "%lld|resync|%.200s|-|-|outbox shrank below cursor\n", now, P[i].number); append_line(led, line); P[i].cursor = 0;
        }
        if (st.st_size == P[i].cursor) continue;
        if (!(f = fopen(P[i].outbox, "r"))) continue;
        fseek(f, (long)P[i].cursor, SEEK_SET);
        while (!stop_phone && fgets(line, sizeof(line), f)) {
            size_t len = strlen(line); char raw[8192], *fld[6]; int n = 0; char *t; char out[8300];
            if (len == 0 || line[len - 1] != '\n') break;                       /* incomplete trailing line: wait for the rest */
            if (routed >= batch_max) { stop_phone = 1; throttled++; snprintf(out, sizeof(out), "%lld|route|route_batch_max=%ld|from=%.200s|batch_full\n", now, batch_max, P[i].number); append_line(obs, out); break; }
            snprintf(raw, sizeof(raw), "%s", line); raw[strcspn(raw, "\r\n")] = 0;
            if (raw[0] == '#' || !raw[0]) { P[i].cursor += (long long)len; continue; }
            t = raw; while (n < 5) { fld[n++] = t; char *b = strchr(t, '|'); if (!b) break; *b = 0; t = b + 1; }
            if (n == 5) fld[n++] = t;
            if (n < 6 || strlen(fld[1]) != 13 || strlen(fld[2]) != 13) {
                snprintf(out, sizeof(out), "%lld|reject|%.200s|-|-|-|malformed\n", now, P[i].number); append_line(led, out); rejected++; P[i].cursor += (long long)len; continue;
            }
            if (strcmp(fld[1], P[i].number)) {   /* sender number must be this phone's own number */
                snprintf(out, sizeof(out), "%lld|reject|%.200s|%.200s|%.200s|%.200s|spoofed-from\n", now, P[i].number, fld[2], fld[3], fld[4]); append_line(led, out);
                snprintf(out, sizeof(out), "%lld|route|route_max_msgs_per_min=%ld|from=%.200s claimed=%.200s|rejected\n", now, max_per_min, P[i].number, fld[1]); append_line(obs, out);
                rejected++; P[i].cursor += (long long)len; continue;
            }
            if (P[i].sent_window >= max_per_min) {   /* leave it in the outbox for a later pass */
                snprintf(out, sizeof(out), "%lld|route|route_max_msgs_per_min=%ld|from=%.200s sent_in_window=%d|throttled\n", now, max_per_min, P[i].number, P[i].sent_window); append_line(obs, out);
                throttled++; stop_phone = 1; break;
            }
            int r = find_phone(fld[2]);
            if (r < 0) {
                snprintf(out, sizeof(out), "%lld|server|%.200s|fail|%.200s|no such number %.200s\n", now, P[i].number, fld[4], fld[2]); append_line(P[i].inbox, out); append_line(P[i].hist, out);
                snprintf(out, sizeof(out), "%lld|unroutable|%.200s|%.200s|%.200s|%.200s\n", now, P[i].number, fld[2], fld[3], fld[4]); append_line(led, out);
                snprintf(out, sizeof(out), "%lld|route|route_max_msgs_per_min=%ld|from=%.200s to=%.200s|unroutable\n", now, max_per_min, P[i].number, fld[2]); append_line(obs, out);
                unroutable++; P[i].cursor += (long long)len; continue;
            }
            append_line(P[r].inbox, line); append_line(P[i].hist, line);        /* the original line, verbatim, into the recipient's inbox and both histories */
            if (r != i) append_line(P[r].hist, line);
            snprintf(out, sizeof(out), "%lld|route|%.200s|%.200s|%.200s|%.200s\n", now, P[i].number, fld[2], fld[3], fld[4]); append_line(led, out);
            snprintf(out, sizeof(out), "%lld|route|route_max_msgs_per_min=%ld|from=%.200s to=%.200s kind=%.200s|routed\n", now, max_per_min, P[i].number, fld[2], fld[3]); append_line(obs, out);
            P[i].sent_window++; routed++; P[i].cursor += (long long)len;
        }
        fclose(f);
    }
    snprintf(tmp, sizeof(tmp), "%s.tmp", cur);
    if ((f = fopen(tmp, "w"))) { for (int i = 0; i < NP; i++) if (P[i].cursor > 0) fprintf(f, "%s|%lld\n", P[i].number, P[i].cursor); fclose(f); rename(tmp, cur); }
    printf("routed=%d rejected=%d unroutable=%d throttled=%d\n", routed, rejected, unroutable, throttled);
    return 0;
}
