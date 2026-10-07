/* irl_bootstrap_fsm.c — Hop 1: Gemma's own IRL bootstrap FSM
 *
 * States: IDLE → WATCHING (tail transcript.txt) → JUDGING (gemma3:270m call)
 *         → PROPOSING (draft weights.txt line) → PENDING_REVIEW (human gate)
 *         → IDLE
 *
 * This FSM runs on GEMMA'S transcript history (open-hai), not HORN/HALO.
 * It reproduces the hand-scoring logic via bounded Gemma call (decision_mode=3 shaped).
 * NEVER auto-merges — human review required at PENDING_REVIEW.
 *
 * Usage: irl_bootstrap_fsm.+x <transcript_path> <output_dir> <house_root>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <uuid/uuid.h>

#define PATH_BUF 4352
#define MAXLINE 8192
#define MAX_EXCHANGES 100

typedef enum {
    STATE_IDLE,
    STATE_WATCHING,
    STATE_JUDGING,
    STATE_PROPOSING,
    STATE_PENDING_REVIEW,
    STATE_DONE
} FsmState;

typedef struct {
    char user_text[MAXLINE];
    char assistant_text[MAXLINE];
    char next_user_text[MAXLINE];
    int has_next_user;
} Exchange;

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = '\0';
}

static int load_exchanges(const char *transcript_path, Exchange *exchanges, int max) {
    FILE *f = fopen(transcript_path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAXLINE];
    Exchange *prev = NULL;
    while (fgets(line, sizeof(line), f) && n < max) {
        trim(line);
        if (!line[0]) continue;
        if (strncmp(line, "U|", 2) == 0) {
            if (prev && prev->assistant_text[0]) {
                strncpy(prev->next_user_text, line + 2, sizeof(prev->next_user_text) - 1);
                prev->has_next_user = 1;
            }
            if (n < max) {
                strncpy(exchanges[n].user_text, line + 2, sizeof(exchanges[0].user_text) - 1);
                exchanges[n].assistant_text[0] = '\0';
                exchanges[n].next_user_text[0] = '\0';
                exchanges[n].has_next_user = 0;
                prev = &exchanges[n];
                n++;
            }
        } else if (strncmp(line, "A|", 2) == 0) {
            if (n > 0) {
                strncpy(exchanges[n-1].assistant_text, line + 2, sizeof(exchanges[0].assistant_text) - 1);
            }
        }
    }
    fclose(f);
    return n;
}

static char *gemma_judge(const char *house_root, const char *prompt) {
    char request_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(request_path, sizeof(request_path), "/tmp/fsm_judge_req_%d_%ld.json", getpid(), time(NULL));
    snprintf(response_path, sizeof(response_path), "/tmp/fsm_judge_rsp_%d_%ld.json", getpid(), time(NULL));

    FILE *pf = fopen(request_path, "w");
    if (!pf) return NULL;
    fprintf(pf, "{\"model\":\"gemma3:270m\",\"stream\":false,\"messages\":[{\"role\":\"user\",\"content\":\"");
    for (const char *p = prompt; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', pf); fputc(*p, pf); }
        else if (*p == '\n') fputs("\\n", pf);
        else fputc(*p, pf);
    }
    fputs("\"}]}", pf);
    fclose(pf);

    char connect_bin[PATH_BUF];
    snprintf(connect_bin, sizeof(connect_bin), "%s/&.widgits/entity-cli/ops/connect_op.+x", house_root);
    char connect_cmd[PATH_BUF * 3];
    snprintf(connect_cmd, sizeof(connect_cmd), "'%s' 'http://10.0.0.144:11434/api/chat' '%s' '%s'", connect_bin, request_path, response_path);
    int rc = system(connect_cmd);
    remove(request_path);
    if (rc != 0) { remove(response_path); return NULL; }

    char parser_bin[PATH_BUF];
    snprintf(parser_bin, sizeof(parser_bin), "%s/&.widgits/entity-cli/ops/json_parser.+x", house_root);
    char parser_cmd[PATH_BUF * 2];
    snprintf(parser_cmd, sizeof(parser_cmd), "'%s' '%s' 'message.content'", parser_bin, response_path);
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

static const char *judge_exchange(const char *house_root, const Exchange *ex) {
    char prompt[MAXLINE * 2];
    snprintf(prompt, sizeof(prompt),
        "You are judging a single exchange in a conversation. Determine if the user's NEXT message indicates the assistant's answer was ACCEPTED (user moved on) or CORRECTED (user re-asked, clarified, or said 'no').\n\n"
        "USER: %s\n"
        "ASSISTANT: %s\n"
        "NEXT USER MESSAGE: %s\n\n"
        "Rules:\n"
        "- If next user message REPEATS the same question, says \"no\", \"wrong\", \"not what I meant\", or re-asks → CORRECTED\n"
        "- If next user message MOVES TO A NEW TOPIC, says something different → ACCEPTED\n"
        "- If no next user message → UNKNOWN\n\n"
        "Respond with EXACTLY one word: ACCEPTED or CORRECTED or UNKNOWN",
        ex->user_text, ex->assistant_text, ex->has_next_user ? ex->next_user_text : "(none)");

    char *result = gemma_judge(house_root, prompt);
    if (!result) return "UNKNOWN";
    trim(result);
    if (strstr(result, "ACCEPTED")) { free(result); return "ACCEPTED"; }
    if (strstr(result, "CORRECTED")) { free(result); return "CORRECTED"; }
    free(result);
    return "UNKNOWN";
}

static void write_proposal(const char *output_dir, const Exchange *ex, const char *judgment) {
    char proposal_dir[PATH_BUF];
    snprintf(proposal_dir, sizeof(proposal_dir), "%s/proposals", output_dir);
    mkdir(proposal_dir, 0755);

    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);

    char proposal_path[PATH_BUF];
    snprintf(proposal_path, sizeof(proposal_path), "%s/proposals/%s.txt", output_dir, uuid_str);
    FILE *f = fopen(proposal_path, "w");
    if (!f) return;

    time_t now = time(NULL);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(f, "# IRL Bootstrap Proposal — %s\n", timebuf);
    fprintf(f, "id=%s\n", uuid_str);
    fprintf(f, "exchange_user=\"%s\"\n", ex->user_text);
    fprintf(f, "exchange_assistant=\"%s\"\n", ex->assistant_text);
    fprintf(f, "next_user=\"%s\"\n", ex->has_next_user ? ex->next_user_text : "");
    fprintf(f, "judgment=%s\n", judgment);
    fprintf(f, "status=PENDING_REVIEW\n");
    fprintf(f, "created=%s\n", timebuf);
    fclose(f);

    printf("  → Proposal written: %s\n", proposal_path);
}

static int has_unreviewed_proposals(const char *output_dir) {
    char proposal_dir[PATH_BUF];
    snprintf(proposal_dir, sizeof(proposal_dir), "%s/proposals", output_dir);
    DIR *d = opendir(proposal_dir);
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strstr(de->d_name, ".txt")) {
            char path[PATH_BUF];
            snprintf(path, sizeof(path), "%s/%s", proposal_dir, de->d_name);
            FILE *f = fopen(path, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    if (strstr(line, "status=PENDING_REVIEW")) { fclose(f); closedir(d); return 1; }
                }
                fclose(f);
            }
        }
    }
    closedir(d);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: irl_bootstrap_fsm.+x <transcript_path> <output_dir> <house_root> [--batch]\n");
        return 1;
    }
    const char *transcript_path = argv[1];
    const char *output_dir = argv[2];
    const char *house_root = argv[3];
    int batch_mode = (argc > 4 && strcmp(argv[4], "--batch") == 0);

    mkdir(output_dir, 0755);
    char reviewed_dir[PATH_BUF];
    snprintf(reviewed_dir, sizeof(reviewed_dir), "%s/reviewed", output_dir);
    mkdir(reviewed_dir, 0755);

    FsmState state = STATE_IDLE;
    Exchange exchanges[MAX_EXCHANGES];
    int n_exchanges = 0;
    int current_idx = 0;

    printf("=== IRL Bootstrap FSM (Hop 1) ===\n");
    printf("Transcript: %s\n", transcript_path);
    printf("Output: %s\n", output_dir);
    printf("House: %s\n", house_root);
    if (batch_mode) printf("Mode: BATCH (process all, exit)\n");
    printf("\n");

    while (1) {
        switch (state) {
            case STATE_IDLE:
                printf("[IDLE] Loading transcript...\n");
                n_exchanges = load_exchanges(transcript_path, exchanges, MAX_EXCHANGES);
                printf("  Loaded %d exchanges\n", n_exchanges);
                current_idx = 0;
                state = STATE_WATCHING;
                break;

            case STATE_WATCHING:
                if (current_idx >= n_exchanges) {
                    printf("[WATCHING] All exchanges processed.\n");
                    state = STATE_DONE;
                    break;
                }
                if (exchanges[current_idx].assistant_text[0] == '\0') {
                    current_idx++;
                    continue;
                }
                printf("[WATCHING] Exchange %d/%d\n", current_idx + 1, n_exchanges);
                state = STATE_JUDGING;
                break;

            case STATE_JUDGING: {
                printf("[JUDGING] Calling Gemma...\n");
                const char *judgment = judge_exchange(house_root, &exchanges[current_idx]);
                printf("  Judgment: %s\n", judgment);
                if (strcmp(judgment, "UNKNOWN") == 0) {
                    printf("  Unknown, skipping\n");
                    current_idx++;
                    state = STATE_WATCHING;
                } else {
                    // Store judgment for PROPOSING state
                    exchanges[current_idx].next_user_text[0] = '\0'; // hack: use first char as flag
                    if (strcmp(judgment, "ACCEPTED") == 0) exchanges[current_idx].next_user_text[0] = 'A';
                    else if (strcmp(judgment, "CORRECTED") == 0) exchanges[current_idx].next_user_text[0] = 'C';
                    state = STATE_PROPOSING;
                }
                break;
            }

            case STATE_PROPOSING:
                if (current_idx > 0 && exchanges[current_idx - 1].next_user_text[0]) {
                    const char *judgment = (exchanges[current_idx - 1].next_user_text[0] == 'A') ? "ACCEPTED" : "CORRECTED";
                    write_proposal(output_dir, &exchanges[current_idx - 1], judgment);
                    exchanges[current_idx - 1].next_user_text[0] = '\0'; // clear
                }
                if (batch_mode) {
                    state = STATE_WATCHING;
                } else {
                    state = STATE_PENDING_REVIEW;
                }
                break;

            case STATE_PENDING_REVIEW:
                printf("[PENDING_REVIEW] Waiting for human review...\n");
                printf("  Proposals in %s/proposals/ awaiting review\n", output_dir);
                printf("  Human: move approved proposals to %s/reviewed/, delete rejected\n", output_dir);
                sleep(10);
                if (!has_unreviewed_proposals(output_dir)) {
                    printf("  All proposals reviewed. Continuing...\n");
                    state = STATE_WATCHING;
                }
                break;

            case STATE_DONE:
                printf("[DONE] Processed %d exchanges, proposals in %s/proposals/\n", n_exchanges, output_dir);
                return 0;
        }
    }
    return 0;
}