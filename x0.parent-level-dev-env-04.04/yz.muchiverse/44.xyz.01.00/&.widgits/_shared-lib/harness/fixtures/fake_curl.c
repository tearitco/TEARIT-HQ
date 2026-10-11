/* fake_curl - test stand-in for curl (Q010 horn_chat_backend harness). Put its folder first on PATH as `curl`.
 * Env: FAKE_CURL_RULES = file of lines  <url-substring>TAB<http-status>TAB<curl-exit>TAB<body>  (first match wins; `*` matches all)
 *      FAKE_CURL_LOG   = file; one "CALL <url> m=<-m value> payload=<request body>" line is appended per call (never the headers)
 * Behaves like curl -sS ... -o FILE -w %{http_code}: body to FILE (or stdout without -o), status printed to stdout when -w is given.
 * A nonzero curl-exit prints a curl-style complaint to stderr and writes no body. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *url = "", *out = NULL, *m = "", *data = NULL; int want_w = 0;
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "http", 4)) url = argv[i];
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-m") && i + 1 < argc) m = argv[++i];
        else if (!strcmp(argv[i], "-w")) { want_w = 1; i++; }
        else if (!strcmp(argv[i], "--data-binary") && i + 1 < argc) data = argv[++i];
    }
    char payload[8192] = "";
    if (data && data[0] == '@') { FILE *pf = fopen(data + 1, "r"); if (pf) { size_t n = fread(payload, 1, sizeof payload - 1, pf); payload[n] = 0; fclose(pf); } }
    const char *lp = getenv("FAKE_CURL_LOG");
    if (lp) { FILE *lf = fopen(lp, "a"); if (lf) { fprintf(lf, "CALL %s m=%s payload=%s\n", url, m, payload); fclose(lf); } }
    const char *rp = getenv("FAKE_CURL_RULES");
    FILE *rf = rp ? fopen(rp, "r") : NULL;
    char line[8192]; int status = 200, cx = 0; char body[8192] = "";
    while (rf && fgets(line, sizeof line, rf)) {
        char *t1 = strchr(line, '\t'); if (!t1) continue; *t1++ = 0;
        char *t2 = strchr(t1, '\t'); if (!t2) continue; *t2++ = 0;
        char *t3 = strchr(t2, '\t'); if (!t3) continue; *t3++ = 0;
        if (strcmp(line, "*") && !strstr(url, line)) continue;
        status = atoi(t1); cx = atoi(t2); snprintf(body, sizeof body, "%s", t3);
        body[strcspn(body, "\r\n")] = 0;
        break;
    }
    if (rf) fclose(rf);
    if (cx) { fprintf(stderr, "curl: (%d) fake curl failure\n", cx); return cx; }
    if (out) { FILE *of = fopen(out, "w"); if (of) { fputs(body, of); fclose(of); } } else puts(body);
    if (want_w) printf("%d", status);
    return 0;
}
