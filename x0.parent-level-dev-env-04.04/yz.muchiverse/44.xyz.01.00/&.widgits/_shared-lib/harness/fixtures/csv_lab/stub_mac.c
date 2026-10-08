/* stub_mac - test double for the Mac Ollama endpoint (POST /api/chat), used ONLY by the csv_lab harnesses. Listens on 127.0.0.1:<port> (never 11434).
 * Usage: stub_mac <port> [ok|slow|http500|empty]. ok: answers a judge prompt ("N<TAB>score<TAB>reason" for each row; score 2 when the Chinese column holds BAD, else 5).
 * slow: waits 6 s before answering (timeout test). http500: answers 500. empty: answers 200 with an empty message. Appends one byte per request to $CSV_STUB_COUNT.
 * Build: gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o stub_mac stub_mac.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int main(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *mode = argc > 2 ? argv[2] : "ok", *cnt = getenv("CSV_STUB_COUNT");
    int s = socket(AF_INET, SOCK_STREAM, 0), one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons((unsigned short)atoi(argv[1])); a.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(s, (struct sockaddr *)&a, sizeof a) || listen(s, 8)) return 2;
    for (;;) {
        int c = accept(s, NULL, NULL); if (c < 0) continue;
        static char req[1 << 18]; size_t n = 0; req[0] = 0;
        for (;;) {
            ssize_t r = read(c, req + n, sizeof req - 1 - n); if (r <= 0) break; n += (size_t)r; req[n] = 0;
            char *he = strstr(req, "\r\n\r\n"); char *cl = strcasestr(req, "content-length:");
            if (he && cl && n >= (size_t)(he - req) + 4 + (size_t)atol(cl + 15)) break;
        }
        if (cnt) { FILE *f = fopen(cnt, "a"); if (f) { fputc('x', f); fclose(f); } }
        if (!strcmp(mode, "slow")) sleep(6);
        if (!strcmp(mode, "http500")) { const char *e = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"; if (write(c, e, strlen(e)) < 0) {} close(c); continue; }
        static char out[1 << 16]; size_t o = 0; out[0] = 0;
        if (strcmp(mode, "empty")) {
            char *p = strstr(req, "ROWS (N"); p = p ? strstr(p, "\\n") : NULL;   /* rows in the JSON body are separated by a literal backslash-n */
            while (p) {
                p += 2; if (!(*p >= '0' && *p <= '9')) break;
                int num = atoi(p); char *end = strstr(p, "\\n"); size_t L = end ? (size_t)(end - p) : strlen(p); char row[2048]; if (L >= sizeof row) L = sizeof row - 1; memcpy(row, p, L); row[L] = 0;
                int bad = strstr(row, "BAD") != NULL;
                o += (size_t)snprintf(out + o, sizeof out - o, "%d\\t%d\\t%s\\n", num, bad ? 2 : 5, bad ? "wrong sense" : "fine");
                p = end;
            }
        }
        static char body[1 << 17]; int bl = snprintf(body, sizeof body, "{\"model\":\"stub\",\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},\"done\":true,\"prompt_eval_count\":%d,\"eval_count\":%d}", out, 111, 22);
        char hdr[256]; int hl = snprintf(hdr, sizeof hdr, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", bl);
        if (write(c, hdr, (size_t)hl) < 0) {}
        if (write(c, body, (size_t)bl) < 0) {}
        close(c);
    }
}
