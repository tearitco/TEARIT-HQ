/* opencode_ask - talk to a headless opencode server as a full LLM arm.
 *
 * The escalation ladder has a new rung: instead of (or in addition to)
 * llama3 on the Mac, this box can call its OWN opencode instance
 * (opencode serve) over HTTP and get real tool-using agent answers.
 * That is exactly what "meta-orchestrator" dreams about - the small
 * gemma agent can hand a hard question UP to opencode, and opencode can
 * hand instructions back DOWN, same curl-shaped convention as every
 * other op in this family.
 *
 * Why curl, not a socket: connect_op.c already proved the curl op shape
 * on this exact LAN (600s max-time, -sS). This op reuses that shape so
 * the failure semantics are identical and the harness can assert on
 * exit codes the same way.
 *
 * Flow (all HTTP against the headless server):
 *   1. POST /session                      -> {id: ...}        (new thread)
 *   2. POST /session/<id>/message          -> {parts:[{type:text,...}]}
 *   3. print every text part to stdout
 *
 * The server must already be running:
 *   opencode serve --port 4123 --hostname 127.0.0.1
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Usage: opencode_ask.+x "<question>" [server_url]
 *        default server_url = http://127.0.0.1:4123
 *        exit 0 = got a reply, 1 = server down / no text reply */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static char *run_curl(const char *url, const char *json_body) {
    char body_file[] = "/tmp/opencode_ask_XXXXXX";
    char out_file[] = "/tmp/opencode_ask_out_XXXXXX";
    int bfd = mkstemp(body_file);
    int ofd = mkstemp(out_file);
    if (bfd < 0 || ofd < 0) return NULL;
    FILE *bf = fdopen(bfd, "w");
    if (!bf) return NULL;
    fprintf(bf, "%s", json_body ? json_body : "");
    fclose(bf);
    close(ofd);

    char body_arg[1024], *curl_args[16];
    snprintf(body_arg, sizeof(body_arg), "@%s", body_file);
    curl_args[0] = "curl";
    curl_args[1] = "-sS";
    curl_args[2] = "--max-time";
    curl_args[3] = "600";
    curl_args[4] = "-H";
    curl_args[5] = "Content-Type: application/json";
    curl_args[6] = "-X";
    curl_args[7] = "POST";
    curl_args[8] = url;
    curl_args[9] = "-d";
    curl_args[10] = body_arg;
    curl_args[11] = "-o";
    curl_args[12] = out_file;
    curl_args[13] = NULL;

    pid_t pid = fork();
    if (pid == 0) { execvp("curl", curl_args); _exit(127); }
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        unlink(body_file); unlink(out_file);
        return NULL;
    }

    FILE *of = fopen(out_file, "r");
    if (!of) { unlink(body_file); unlink(out_file); return NULL; }
    fseek(of, 0, SEEK_END);
    long sz = ftell(of);
    fseek(of, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(of); unlink(body_file); unlink(out_file); return NULL; }
    size_t got = fread(buf, 1, sz, of);
    buf[got] = '\0';
    fclose(of);
    unlink(body_file);
    unlink(out_file);
    return buf;
}

/* crude JSON string field extractor: finds "key": then captures the
 * quoted string value. Good enough for {id} and {type,text}. */
static int json_str_field(const char *json, const char *key, char *out, size_t out_sz) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return 0;
    p = strchr(p + strlen(needle), ':');
    if (!p) return 0;
    while (*p && *p != '"') p++;
    if (*p != '"') return 0;
    p++;
    size_t n = 0;
    while (*p && *p != '"' && n < out_sz - 1) { out[n++] = *p; p++; }
    out[n] = '\0';
    return n > 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s \"<question>\" [server_url]\n", argv[0]);
        return 1;
    }
    const char *question = argv[1];
    const char *base = (argc >= 3) ? argv[2] : "http://127.0.0.1:4123";

    /* 1. create a fresh session */
    char sess_url[1024];
    snprintf(sess_url, sizeof(sess_url), "%s/session", base);
    char *sess = run_curl(sess_url, "{\"title\":\"opencode_ask\"}");
    if (!sess) {
        fprintf(stderr, "opencode_ask: could not reach server at %s "
                "(is `opencode serve` running?)\n", base);
        return 1;
    }
    char sid[256];
    if (!json_str_field(sess, "id", sid, sizeof(sid))) {
        fprintf(stderr, "opencode_ask: server did not return a session id\n");
        free(sess);
        return 1;
    }
    free(sess);

    /* 2. send the question and wait for the reply.
     * The question goes inside a JSON string: escape " and backslash,
     * then quote it. Without the quotes the body is invalid JSON and
     * the server replies with an error part (no text) - live-caught. */
    char *esc = malloc(strlen(question) * 2 + 1);
    if (!esc) return 1;
    const char *src = question;
    char *dst = esc;
    while (*src) {
        if (*src == '"' || *src == '\\') *dst++ = '\\';
        *dst++ = *src++;
    }
    *dst = '\0';
    char msg_url[1024];
    snprintf(msg_url, sizeof(msg_url), "%s/session/%s/message", base, sid);
    char *json_body = NULL;
    if (asprintf(&json_body,
                 "{\"parts\":[{\"type\":\"text\",\"text\":\"%s\"}]}",
                 esc) < 0) {
        free(esc);
        return 1;
    }
    free(esc);
    char *reply = run_curl(msg_url, json_body);
    free(json_body);
    if (!reply) {
        fprintf(stderr, "opencode_ask: request failed after session created\n");
        return 1;
    }

    /* 3. print the text parts */
    int printed = 0;
    const char *p = reply;
    while ((p = strstr(p, "\"type\":\"text\"")) != NULL) {
        const char *q = strchr(p + strlen("\"type\":\"text\""), ':');
        while (q && *q && *q != '"') q++;
        if (!q || *q != '"') { p += 1; continue; }
        const char *start = q + 1;
        const char *end = strchr(start, '"');
        if (!end) break;
        size_t len = end - start;
        if (len > 0) {
            /* strip escaped \n */
            for (size_t i = 0; i < len; i++) {
                if (start[i] == '\\' && i + 1 < len && start[i+1] == 'n') {
                    putchar('\n');
                    i++;
                } else {
                    putchar(start[i]);
                }
            }
            putchar('\n');
            printed = 1;
        }
        p = end;
    }
    free(reply);

    if (!printed) {
        fprintf(stderr, "opencode_ask: no text reply from server\n");
        return 1;
    }
    return 0;
}
