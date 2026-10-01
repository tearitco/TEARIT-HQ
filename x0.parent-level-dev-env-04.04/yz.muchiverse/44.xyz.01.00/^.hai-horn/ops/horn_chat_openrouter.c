/* horn_chat_openrouter.c - minimal OpenRouter round-trip for HORN_CHAT
 *
 * Usage: horn_chat_openrouter.+x <house_root> <message_text>
 * Writes message and reply to .horn-sessions/chat_history.txt
 * Outputs reply to stdout
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define PATH_BUF 4352

static const char *OR_MODELS[] = {
    "nvidia/nemotron-3-ultra-550b-a55b:free",
    "dots-studio/dots-3-note-preview:free",
    "poolside/laguna-s-2.1:free",
};
#define OR_N_MODELS (int)(sizeof(OR_MODELS) / sizeof(OR_MODELS[0]))

static void json_escaped(FILE *out, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { fputc('\\', out); fputc(*s, out); }
        else if (*s == '\n') fputs("\\n", out);
        else fputc(*s, out);
    }
}

static int load_openrouter_key(const char *house_root, char *out, size_t outsz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/&.widgits/open-hai/state/openrouter_api_key.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[512] = "";
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return 0; }
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
    if (!buf[0]) return 0;
    snprintf(out, outsz, "%s", buf);
    return 1;
}

static char *openrouter_ask_one(const char *house_root, const char *key,
                                 const char *model, const char *prompt_text) {
    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/horn_or_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/horn_or_response_%d.json", getpid());

    FILE *pf = fopen(request_path, "w");
    if (!pf) return NULL;
    fprintf(pf, "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"", model);
    json_escaped(pf, prompt_text);
    fputs("\"}]}", pf);
    fclose(pf);

    char auth_hdr[600];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", key);
    char data_arg[PATH_BUF + 2];
    snprintf(data_arg, sizeof(data_arg), "@%s", request_path);

    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(response_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { dup2(fd, 1); close(fd); }
        execlp("curl", "curl", "-s", "-m", "60", "-X", "POST",
               "https://openrouter.ai/api/v1/chat/completions",
               "-H", "Content-Type: application/json",
               "-H", auth_hdr,
               "-d", data_arg,
               (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        remove(request_path);
        return NULL;
    }
    remove(request_path);

    char parser_bin[PATH_BUF];
    snprintf(parser_bin, sizeof(parser_bin), "%s/&.widgits/entity-cli/ops/json_parser.+x", house_root);
    char parser_cmd[PATH_BUF * 2];
    snprintf(parser_cmd, sizeof(parser_cmd), "'%s' '%s' 'choices[0].message.content'", parser_bin, response_path);
    FILE *jp = popen(parser_cmd, "r");
    if (!jp) { remove(response_path); return NULL; }

    char buf[4096];
    size_t total = 0;
    buf[0] = '\0';
    char chunk[512];
    while (fgets(chunk, sizeof(chunk), jp)) {
        size_t clen = strlen(chunk);
        if (total + clen >= sizeof(buf) - 1) break;
        memcpy(buf + total, chunk, clen);
        total += clen;
        buf[total] = '\0';
    }
    pclose(jp);
    remove(response_path);
    if (total == 0) return NULL;
    return strdup(buf);
}

static int openrouter_ask(const char *house_root, const char *prompt_text, char **out_reply) {
    char key[512];
    if (!load_openrouter_key(house_root, key, sizeof(key))) return 0;
    for (int i = 0; i < OR_N_MODELS; i++) {
        char *reply = openrouter_ask_one(house_root, key, OR_MODELS[i], prompt_text);
        if (reply) { *out_reply = reply; return 1; }
        fprintf(stderr, "horn: %s failed, trying next\n", OR_MODELS[i]);
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: horn_chat_openrouter.+x <house_root> <message_text>\n");
        return 1;
    }
    const char *house_root = argv[1];
    const char *message = argv[2];

    char *reply = NULL;
    int rc = openrouter_ask(house_root, message, &reply);
    if (rc == 0) {
        fprintf(stderr, "horn: no OpenRouter key at &.widgits/open-hai/state/openrouter_api_key.txt\n");
        return 1;
    }
    if (rc < 0) {
        fprintf(stderr, "horn: all %d OpenRouter models failed - check key or network\n", OR_N_MODELS);
        return 1;
    }

    size_t rl = strlen(reply);
    while (rl > 0 && (reply[rl-1] == '\n' || reply[rl-1] == ' ')) reply[--rl] = '\0';
    char *rp = reply; while (*rp == ' ' || *rp == '\n') rp++;

    time_t now = time(NULL);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    char cwd[PATH_BUF];
    if (getcwd(cwd, sizeof(cwd))) {
        char chat_path[PATH_BUF];
        snprintf(chat_path, sizeof(chat_path), "%s/.horn-sessions/chat_history.txt", cwd);
        FILE *cf = fopen(chat_path, "a");
        if (cf) {
            fprintf(cf, "[%s] USER: %s\n", timebuf, message);
            fprintf(cf, "[%s] HORN: %s\n", timebuf, rp);
            fclose(cf);
        }
    }

    printf("%s\n", rp);
    free(reply);
    return 0;
}
