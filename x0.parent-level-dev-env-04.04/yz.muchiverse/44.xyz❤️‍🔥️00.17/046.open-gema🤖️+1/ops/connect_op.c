#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Usage: ./connect_op <api_url> <input_file> <output_file>
int main(int argc, char *argv[]) {
    if (argc < 4) return 1;
    char *url = argv[1];
    char *input_file = argv[2];
    char *output_file = argv[3];

    char *input_arg = NULL;
    if (asprintf(&input_arg, "@%s", input_file) == -1) return 1;

    // 600s timeout, -sS (silent but show errors on stderr) - matching
    // gem-dev_manager.c's own start_ai_query() exactly, ported from its
    // own real, hard-won debugging handoff (#.dox/groq-api-handoff.txt)
    // against this SAME model (llama3-groq-tool-use:8b) on this SAME Mac
    // (10.0.0.144:11434). The old 30s figure ("groq-ollama (8B) in <1s")
    // was wrong in practice - that handoff doc explicitly treats a curl
    // timeout (exit 28) as a real, expected failure mode for this exact
    // model, not a rare edge case - an 8B model's cold model-load time
    // under concurrent load can easily exceed 30s even though a WARM
    // model answers in under a second.
    char *curl_args[] = {"curl", "-sS", "--max-time", "600", "-H", "Content-Type: application/json", url, "-d", input_arg, "-o", output_file, NULL};
    
    pid_t pid = fork();
    if (pid == 0) {
        execvp("curl", curl_args);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    free(input_arg);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    /* REAL BUG, LIVE-CAUGHT (2026-07-21): a connection failure (refused,
     * unreachable, timed out) means curl never reaches the point of
     * opening -o's destination at all - output_file is left completely
     * untouched, and nothing downstream (check_response.c) could tell
     * "this attempt failed" apart from "this attempt's response just
     * happens to still be sitting in a file from a PREVIOUS, unrelated
     * call" - send_message.c now clears output_file before every attempt
     * specifically so that ambiguity can't happen, but check_response.c
     * still needs curl's own real exit status to report an honest error
     * ("could not connect") instead of the previous generic "empty or
     * unparseable response" for what's actually a network failure, not a
     * malformed API reply. Sidecar file, same convention as every other
     * *.tmp/*.status handoff in this project - see send_message.c's own
     * header comment on why files are the only handoff mechanism ops have. */
    char *status_path = NULL;
    if (asprintf(&status_path, "%s.status", output_file) != -1) {
        FILE *sf = fopen(status_path, "w");
        if (sf) { fprintf(sf, "%d\n", exit_code); fclose(sf); }
        free(status_path);
    }

    return exit_code;
}
