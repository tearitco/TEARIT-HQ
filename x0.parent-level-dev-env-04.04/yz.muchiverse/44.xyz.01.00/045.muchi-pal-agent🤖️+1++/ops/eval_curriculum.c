/* ops/eval_curriculum.c - Supervisor op: evaluate one trained curriculum
 * on a LAN node. Runs on THIS box, drives the node over SSH. Generates a
 * few held-out prompts, counts <UNK> tokens (leak gauge), and prints a
 * report row the harness can append to training-report.txt (gap report
 * input for D3 teacher prompting).
 *
 * Usage: eval_curriculum.+x <curriculum> <node> [prompt1] [prompt2] ...
 *   curriculum - store dir on the node (~/iqabod-store/<name>)
 *   node       - linux | mac
 *   prompts    - optional held-out phrases; default:
 *                "good morning" "see you" "how are you today"
 *
 * Prints:
 *   EVAL|<curriculum>|<node>|probe=P|output="<text>"|unk=N|tokens=M
 * per probe, then one summary row:
 *   EVAL_SUM|<curriculum>|<node>|probes=n|max_unk=N|coherent=yes/no
 * Exit 0 on success (even if unk is high), 1 on ssh failure.
 *
 * Self-contained, house style. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 4096
#define MAX_PROBES 8

static const char *node_user(const char *node) {
    if (strcmp(node, "linux") == 0) return "jb@10.0.0.187";
    if (strcmp(node, "mac") == 0)   return "lfs.master@10.0.0.144";
    return NULL;
}

static const char *node_pass(const char *node) {
    if (strcmp(node, "linux") == 0) return "root";
    if (strcmp(node, "mac") == 0)   return "1234";
    return NULL;
}

static char *run_capture(const char *cmd) {
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return NULL;
    char *buf = malloc(131072);
    size_t total = 0, n;
    while ((n = fread(buf + total, 1, 131071 - total, pipe)) > 0) {
        total += n;
        if (total >= 131071) break;
    }
    buf[total] = '\0';
    int rc = pclose(pipe);
    if (rc != 0) { free(buf); return NULL; }
    while (total > 0 && (buf[total-1] == '\n' || buf[total-1] == '\r')) buf[--total] = '\0';
    return buf;
}

static int count_occurrences(const char *haystack, const char *needle) {
    int count = 0;
    const char *p = haystack;
    size_t nlen = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += nlen;
    }
    return count;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: eval_curriculum.+x <curriculum> <node> [prompts...]\n");
        return 1;
    }
    const char *curriculum = argv[1];
    const char *node = argv[2];
    const char *probes[MAX_PROBES];
    int num_probes = 0;
    if (argc > 3) {
        for (int i = 3; i < argc && num_probes < MAX_PROBES; i++)
            probes[num_probes++] = argv[i];
    } else {
        probes[num_probes++] = "good morning";
        probes[num_probes++] = "see you";
        probes[num_probes++] = "how are you today";
    }

    const char *user = node_user(node);
    if (!user) { fprintf(stderr, "Unknown node '%s' (want linux|mac)\n", node); return 1; }
    const char *pass = node_pass(node);

    int max_unk = 0, probe_fail = 0;
    for (int i = 0; i < num_probes; i++) {
        char cmd[MAX_LINE * 3];
        snprintf(cmd, sizeof(cmd),
            "sshpass -p '%s' ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "
            "%s \"cd ~/iqabod-store/%s && ./+x/main_orchestrator.+x generate "
            "curriculum/corpus/corpus.txt 0.7 30 '%s' 2>&1 | grep 'Final generated'\"",
            pass, user, curriculum, probes[i]);
        char *out = run_capture(cmd);
        if (!out) { probe_fail++; printf("EVAL|%s|%s|probe=\"%s\"|ssh_fail\n", curriculum, node, probes[i]); continue; }

        /* Extract text after "Final generated text:" */
        const char *marker = strstr(out, "Final generated text:");
        const char *text = marker ? marker + strlen("Final generated text:") : out;
        int unk = count_occurrences(text, "<UNK>");
        int tokens = count_occurrences(text, " ") + 1;
        if (unk > max_unk) max_unk = unk;
        printf("EVAL|%s|%s|probe=\"%s\"|output=\"%s\"|unk=%d|tokens=%d\n",
               curriculum, node, probes[i], text, unk, tokens);
        free(out);
    }

    printf("EVAL_SUM|%s|%s|probes=%d|ssh_fail=%d|max_unk=%d\n",
           curriculum, node, num_probes, probe_fail, max_unk);
    return probe_fail ? 1 : 0;
}
