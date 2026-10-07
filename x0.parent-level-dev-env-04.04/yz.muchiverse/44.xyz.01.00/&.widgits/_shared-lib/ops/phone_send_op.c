/* phone_send_op - the "phone.send" event (Q009, HAI-ROBOTS-PHONES-SERVER-DESIGN.md 3, 3f).
 *
 * Usage: phone_send_op.+x <sender_phone_dir> <to_number> <kind> <ref> <text>
 *   Appends ONE message line to the sender's outbox.txt (the owner-only writer of that file):
 *       <epoch_ms>|<from_number>|<to_number>|<kind>|<ref>|<text>
 *   <from_number> is read from the sender's own phone.pdl (a sender can never claim another number); the server router (server_route_op) delivers it.
 *   Refuses (exit 1, message on stderr, nothing written) on: unknown kind, malformed to_number, '|' or newline in ref/text, empty text, a phone.pdl without a number,
 *   or an outbox already at the sender's own history caps (history_max_lines / history_max_bytes from phone.pdl; rotation is the server's job, a later quest).
 * Exit: 0 sent, 1 refused, 2 usage. Pure file I/O, no network, no shell. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define KINDS "say task ask-human answer status spawn done fail command lease release result"

static int kind_ok(const char *k) {
    char buf[256]; char *t, *save = NULL; snprintf(buf, sizeof(buf), "%s", KINDS);
    for (t = strtok_r(buf, " ", &save); t; t = strtok_r(NULL, " ", &save)) if (!strcmp(t, k)) return 1;
    return 0;
}
static int number_ok(const char *n) {   /* NNN-NNNN-NNNN */
    int i; if (strlen(n) != 13) return 0;
    for (i = 0; i < 13; i++) { if (i == 3 || i == 8) { if (n[i] != '-') return 0; } else if (n[i] < '0' || n[i] > '9') return 0; }
    return 1;
}
/* "PHONE | <key> | <value>" from phone.pdl */
static int pdl_get(const char *dir, const char *key, char *out, size_t n) {
    char p[4352], line[512], want[96]; FILE *f; out[0] = '\0';
    snprintf(p, sizeof(p), "%s/phone.pdl", dir); snprintf(want, sizeof(want), "PHONE        | %s", key);
    if (!(f = fopen(p, "r"))) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PHONE", 5)) continue;
        char *b1 = strchr(line, '|'); if (!b1) continue; char *b2 = strchr(b1 + 1, '|'); if (!b2) continue;
        char k[96]; size_t kl = (size_t)(b2 - b1 - 1); if (kl >= sizeof(k)) continue;
        memcpy(k, b1 + 1, kl); k[kl] = '\0';
        char *ks = k; while (*ks == ' ') ks++; char *ke = ks + strlen(ks); while (ke > ks && ke[-1] == ' ') *--ke = '\0';
        if (strcmp(ks, key)) continue;
        char *v = b2 + 1; while (*v == ' ') v++; v[strcspn(v, "\r\n")] = '\0';
        char *ve = v + strlen(v); while (ve > v && ve[-1] == ' ') *--ve = '\0';
        snprintf(out, n, "%s", v); break;
    }
    fclose(f); (void)want; return out[0] != '\0';
}

int main(int argc, char **argv) {
    char from[32], capl[32], capb[32], outbox[4352], line[8192]; FILE *f; struct stat st;
    long max_lines = 2000, max_bytes = 262144, n_lines = 0; struct timespec ts;
    if (argc != 6) { fprintf(stderr, "usage: phone_send_op.+x <sender_phone_dir> <to_number> <kind> <ref> <text>\n"); return 2; }
    const char *dir = argv[1], *to = argv[2], *kind = argv[3], *ref = argv[4], *text = argv[5];
    if (!pdl_get(dir, "number", from, sizeof(from)) || !number_ok(from)) { fprintf(stderr, "refused: sender phone.pdl has no valid number (%s)\n", dir); return 1; }
    if (!number_ok(to)) { fprintf(stderr, "refused: to_number must look like NNN-NNNN-NNNN\n"); return 1; }
    if (!kind_ok(kind)) { fprintf(stderr, "refused: unknown kind '%s' (allowed: %s)\n", kind, KINDS); return 1; }
    if (strpbrk(ref, "|\r\n") || strpbrk(text, "|\r\n")) { fprintf(stderr, "refused: '|' and newlines are not allowed in ref or text\n"); return 1; }
    if (!text[0]) { fprintf(stderr, "refused: empty text\n"); return 1; }
    if (strlen(text) + strlen(ref) > 4000) { fprintf(stderr, "refused: message too long\n"); return 1; }
    if (pdl_get(dir, "history_max_lines", capl, sizeof(capl))) max_lines = atol(capl);
    if (pdl_get(dir, "history_max_bytes", capb, sizeof(capb))) max_bytes = atol(capb);
    snprintf(outbox, sizeof(outbox), "%s/outbox.txt", dir);
    if (stat(outbox, &st) == 0) {
        if (st.st_size >= max_bytes) { fprintf(stderr, "refused: outbox is at its byte cap (%ld); rotate first\n", max_bytes); return 1; }
        if ((f = fopen(outbox, "r"))) { while (fgets(line, sizeof(line), f)) n_lines++; fclose(f); }
        if (n_lines >= max_lines) { fprintf(stderr, "refused: outbox is at its line cap (%ld); rotate first\n", max_lines); return 1; }
    }
    clock_gettime(CLOCK_REALTIME, &ts);
    if (!(f = fopen(outbox, "a"))) { fprintf(stderr, "refused: cannot append to %s\n", outbox); return 1; }
    fprintf(f, "%lld|%s|%s|%s|%s|%s\n", (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000, from, to, kind, ref, text);
    fclose(f);
    return 0;
}
