/* tsc_answer - test-harness op. The PLUGGABLE ANSWER SOURCE for the
 * FSM driver (design TSC_P2P_PVP.md sec. 4.2). Given a mode and a
 * prompt/topic, produces ONE valid answer line for the caller:
 *
 *   MODE BOOK       - deterministic keyword lookup in the "book" file
 *                     (a game-reference corpus; pre-parse intent ->
 *                     deterministic tool, the gemma_strategy inversion)
 *   MODE GUESS      - random valid move (the stress test: prove the
 *                     network/ledger layer is honest even for arbitrary
 *                     players)
 *   MODE GEMMA_LAN  - POST to GEMMA_LAN_URL (/api/chat, stream=false),
 *                     a SIMPLE keyword-extractable prompt ("name one
 *                     move: 1-4, one line"), hard fallback to BOOK/GUESS
 *                     if the response is unusable (never structured
 *                     output - gemma can't reliably do it)
 *
 * Answers for TSC PvP are one of: strike heavy heal block. The answer
 * is printed on stdout (one word); exit 0 on success, 1 on unusable.
 *
 * Self-contained, no shared headers.
 * Usage:
 *   tsc_answer.+x <mode> <prompt> [book_file]
 *   modes: book|guess|gemma   (book_file default ../book.txt)
 *   TSC_ANSWER_MODE env also honored when argv[1] is "auto".
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PATH_BUF 4352
#define MAX_LINE 2048

static const char *MOVES[] = { "strike", "heavy", "heal", "block" };
static const int N_MOVES = 4;

/* BOOK: deterministic keyword/topic lookup, pre-LLM. Lines:
 *   <keyword>: <move>
 * First line whose keyword appears in the prompt wins. Falls back to
 * GUESS if nothing matches (a book should not make a harness fail). */
static int answer_book(const char *book_path, const char *prompt,
                       char *out, size_t out_sz) {
    FILE *f = fopen(book_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        const char *kw = line;
        const char *mv = colon + 1;
        while (*mv == ' ' || *mv == '\t') mv++;
        int valid = 0;
        for (int i = 0; i < N_MOVES; i++)
            if (strcmp(mv, MOVES[i]) == 0) valid = 1;
        if (!valid) continue;
        if (strstr(prompt, kw)) {
            snprintf(out, out_sz, "%s", mv);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int answer_guess(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", MOVES[rand() % N_MOVES]);
    return 1;
}

static int answer_gemma(const char *url, const char *model,
                        const char *prompt, char *out, size_t out_sz) {
    char body[2048];
    snprintf(body, sizeof(body),
             "{\"model\":\"%s\",\"stream\":false,\"messages\":[{\"role\":\"user\","
             "\"content\":\"%s. Answer in one line: one of strike, heavy, heal, block.\\n\"}]}",
             model, prompt);
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
             "curl -s --max-time 15 -H 'Content-Type: application/json' -d '%s' %s",
             body, url);
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    char raw[MAX_LINE];
    size_t n = fread(raw, 1, sizeof(raw) - 1, p);
    raw[n] = '\0';
    pclose(p);

    /* Extract the first move keyword that appears anywhere. */
    for (int i = 0; i < N_MOVES; i++) {
        if (strstr(raw, MOVES[i])) {
            snprintf(out, out_sz, "%s", MOVES[i]);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    if (argc < 3) {
        fprintf(stderr, "Usage: tsc_answer.+x <book|guess|gemma|auto> <prompt> [book_file]\n");
        return 1;
    }
    char mode[32];
    snprintf(mode, sizeof(mode), "%s", argv[1]);
    const char *prompt = argv[2];
    const char *book_file = (argc >= 4) ? argv[3] : "../book.txt";

    if (strcmp(mode, "auto") == 0) {
        const char *env = getenv("TSC_ANSWER_MODE");
        if (env && env[0]) snprintf(mode, sizeof(mode), "%s", env);
        else snprintf(mode, sizeof(mode), "guess");
    }

    char answer[64] = "";
    if (strcmp(mode, "book") == 0) {
        if (!answer_book(book_file, prompt, answer, sizeof(answer)))
            answer_guess(answer, sizeof(answer));
    } else if (strcmp(mode, "guess") == 0) {
        answer_guess(answer, sizeof(answer));
    } else if (strcmp(mode, "gemma") == 0) {
        const char *url = getenv("GEMMA_LAN_URL");
        const char *model = getenv("GEMMA_LAN_MODEL");
        if (!url || !url[0]) url = "http://10.0.0.144:11434/api/chat";
        if (!model || !model[0]) model = "gemma3:270m";
        if (!answer_gemma(url, model, prompt, answer, sizeof(answer))) {
            if (!answer_book(book_file, prompt, answer, sizeof(answer)))
                answer_guess(answer, sizeof(answer));
        }
    } else {
        fprintf(stderr, "unknown mode %s\n", mode);
        return 1;
    }

    printf("%s\n", answer);
    return 0;
}
