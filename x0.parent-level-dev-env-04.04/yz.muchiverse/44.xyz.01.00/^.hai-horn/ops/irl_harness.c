/* irl_harness.c - IRL harness: run prompts through HORN + HALO, grade responses
 *
 * Usage: irl_harness.+x <house_root> <prompt_file> <output_dir>
 * Reads prompts from file (one per line), runs both models, grades, writes training data
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>

#define PATH_BUF 4352
#define MAX_PROMPTS 1000
#define MAX_PROMPT_LEN 2048

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
    snprintf(request_path, sizeof(request_path), "/tmp/irl_request_%d_%ld.json", getpid(), time(NULL));
    snprintf(response_path, sizeof(response_path), "/tmp/irl_response_%d_%ld.json", getpid(), time(NULL));

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
    char parser_cmd[PATH_BUF * 3];
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

static char *openrouter_ask(const char *house_root, const char *prompt_text, char **out_reply) {
    char key[512];
    if (!load_openrouter_key(house_root, key, sizeof(key))) return NULL;
    for (int i = 0; i < OR_N_MODELS; i++) {
        char *reply = openrouter_ask_one(house_root, key, OR_MODELS[i], prompt_text);
        if (reply) { *out_reply = reply; return reply; }
        fprintf(stderr, "irl: %s failed, trying next\n", OR_MODELS[i]);
    }
    return NULL;
}

static char *run_horn(const char *house_root, const char *hai_horn_dir, const char *prompt) {
    char horn_bin[PATH_BUF];
    snprintf(horn_bin, sizeof(horn_bin), "%s/ops/horn_chat_openrouter.+x", hai_horn_dir);
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "HORN_DIR=\"%s\" '%s' \"%s\" \"%s\" 2>&1", hai_horn_dir, horn_bin, house_root, prompt);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char buf[4096];
    size_t total = 0;
    buf[0] = '\0';
    char chunk[512];
    while (fgets(chunk, sizeof(chunk), fp)) {
        size_t clen = strlen(chunk);
        if (total + clen >= sizeof(buf) - 1) break;
        memcpy(buf + total, chunk, clen);
        total += clen;
        buf[total] = '\0';
    }
    pclose(fp);
    if (total == 0) return NULL;
    return strdup(buf);
}

static char *run_halo(const char *house_root, const char *hai_horn_dir, const char *prompt) {
    char halo_bin[PATH_BUF];
    snprintf(halo_bin, sizeof(halo_bin), "%s/ops/horn_chat_openrouter.+x", hai_horn_dir);
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "HORN_DIR=\"%s\" '%s' \"%s\" \"%s\" 2>&1", hai_horn_dir, halo_bin, house_root, prompt);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char buf[4096];
    size_t total = 0;
    buf[0] = '\0';
    char chunk[512];
    while (fgets(chunk, sizeof(chunk), fp)) {
        size_t clen = strlen(chunk);
        if (total + clen >= sizeof(buf) - 1) break;
        memcpy(buf + total, chunk, clen);
        total += clen;
        buf[total] = '\0';
    }
    pclose(fp);
    if (total == 0) return NULL;
    return strdup(buf);
}

static char *grade_responses(const char *house_root, const char *prompt, const char *horn_resp, const char *halo_resp) {
    char grading_prompt[8192];
    snprintf(grading_prompt, sizeof(grading_prompt),
        "You are an expert evaluator. Compare two AI responses to the same prompt and judge which is better.\n\n"
        "PROMPT: %s\n\n"
        "RESPONSE A (HORN_CHAT - direct OpenRouter):\n%s\n\n"
        "RESPONSE B (HALO_CHAT - OpenRouter + Concept Bank pipeline):\n%s\n\n"
        "Evaluate on these criteria (1-5 each):\n"
        "1. Accuracy - factually correct\n"
        "2. Clarity - easy to understand\n"
        "3. Completeness - addresses the prompt fully\n"
        "4. Usefulness - practical value to user\n"
        "5. Concept awareness - references relevant concepts/knowledge\n\n"
        "Respond with EXACTLY this format:\n"
        "WINNER: A|B|TIE\n"
        "SCORE_A: <1-5> <1-5> <1-5> <1-5> <1-5>\n"
        "SCORE_B: <1-5> <1-5> <1-5> <1-5> <1-5>\n"
        "REASON: <one sentence explaining the decision>",
        prompt, horn_resp, halo_resp);

    char *grade = NULL;
    openrouter_ask(house_root, grading_prompt, &grade);
    return grade;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: irl_harness.+x <house_root> <hai_horn_dir> <prompt_file> [output_dir]\n");
        return 1;
    }
    const char *house_root = argv[1];
    const char *hai_horn_dir = argv[2];
    const char *prompt_file = argv[3];
    const char *output_dir = argc > 4 ? argv[4] : hai_horn_dir;

    char prompts[MAX_PROMPTS][MAX_PROMPT_LEN];
    int n_prompts = 0;
    FILE *pf = fopen(prompt_file, "r");
    if (!pf) {
        fprintf(stderr, "Cannot open prompt file: %s\n", prompt_file);
        return 1;
    }
    char line[MAX_PROMPT_LEN];
    while (fgets(line, sizeof(line), pf) && n_prompts < MAX_PROMPTS) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] && line[0] != '#') {
            snprintf(prompts[n_prompts], sizeof(prompts[0]), "%s", line);
            n_prompts++;
        }
    }
    fclose(pf);

    if (n_prompts == 0) {
        fprintf(stderr, "No prompts found in %s\n", prompt_file);
        return 1;
    }

    char output_path[PATH_BUF];
    snprintf(output_path, sizeof(output_path), "%s/irl_training_data.jsonl", output_dir);
    FILE *out = fopen(output_path, "a");
    if (!out) {
        fprintf(stderr, "Cannot open output: %s\n", output_path);
        return 1;
    }

    printf("IRL Harness: %d prompts, output to %s\n", n_prompts, output_path);

    for (int i = 0; i < n_prompts; i++) {
        printf("\n[%d/%d] %s\n", i+1, n_prompts, prompts[i]);

        char *horn_resp = run_horn(house_root, hai_horn_dir, prompts[i]);
        if (!horn_resp) {
            fprintf(stderr, "  HORN failed\n");
            continue;
        }
        printf("  HORN: %.80s...\n", horn_resp);

        char *halo_resp = run_halo(house_root, hai_horn_dir, prompts[i]);
        if (!halo_resp) {
            fprintf(stderr, "  HALO failed\n");
            free(horn_resp);
            continue;
        }
        printf("  HALO: %.80s...\n", halo_resp);

        char *grade = grade_responses(house_root, prompts[i], horn_resp, halo_resp);
        if (!grade) {
            fprintf(stderr, "  Grading failed\n");
            free(horn_resp);
            free(halo_resp);
            continue;
        }
        printf("  GRADE: %.80s...\n", grade);

        time_t now = time(NULL);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

        fprintf(out, "{\"timestamp\":\"%s\",\"prompt\":\"", timebuf);
        json_escaped(out, prompts[i]);
        fprintf(out, "\",\"horn_response\":\"");
        json_escaped(out, horn_resp);
        fprintf(out, "\",\"halo_response\":\"");
        json_escaped(out, halo_resp);
        fprintf(out, "\",\"grading\":\"");
        json_escaped(out, grade);
        fprintf(out, "\"}\n");
        fflush(out);

        free(horn_resp);
        free(halo_resp);
        free(grade);

        sleep(1);  // rate limit
    }

    fclose(out);
    printf("\nDone. Training data written to %s\n", output_path);
    return 0;
}