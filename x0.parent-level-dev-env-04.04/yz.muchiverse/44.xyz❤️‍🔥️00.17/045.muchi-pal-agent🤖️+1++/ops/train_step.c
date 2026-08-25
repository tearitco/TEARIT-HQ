/* ops/train_step.c - Supervisor op: train one curriculum on a LAN node.
 * Runs on THIS box (orchestration only, no compute) and drives the node
 * over SSH (sshpass). The actual vocab_only+train runs on the node where
 * the payload lives (storage rule: pointer here, payload on nodes).
 *
 * The node needs a config.txt with learning_rate=0.1 (see lan-paths.txt /
 * curricula.pdl note) - without it the default 1e-5 never leaves the
 * random floor and with it iqabod learns (loss 4.38 -> 0.0006 proven).
 *
 * Usage: train_step.+x <curriculum> <node> [epochs]
 *   curriculum  - name of the store dir on the node (~/iqabod-store/<name>)
 *   node        - linux | mac  (see lan-paths.txt for the ssh map)
 *   epochs      - optional, default 100 (the store config.txt default)
 *
 * Prints one report line (pdl-ish, for the harness):
 *   TRAIN|<curriculum>|<node>|epochs=N|final_loss=%.4f|last_epoch=M
 * Exit 0 on success, 1 on ssh/remote failure, 2 on missing loss output.
 *
 * Self-contained, no shared headers - matching every other op in this
 * project. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 4096

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

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: train_step.+x <curriculum> <node> [epochs]\n");
        fprintf(stderr, "  node = linux | mac (see lan-paths.txt)\n");
        return 1;
    }
    const char *curriculum = argv[1];
    const char *node = argv[2];
    const char *epochs = (argc > 3) ? argv[3] : "100";

    const char *user = node_user(node);
    if (!user) { fprintf(stderr, "Unknown node '%s' (want linux|mac)\n", node); return 1; }
    const char *pass = node_pass(node);

    char cmd[MAX_LINE * 3];
    snprintf(cmd, sizeof(cmd),
        "sshpass -p '%s' ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "
        "%s \"cd ~/iqabod-store/%s && ./+x/main_orchestrator.+x train corpus.txt %s 2>&1\"",
        pass, user, curriculum, epochs);

    char *out = run_capture(cmd);
    if (!out) { fprintf(stderr, "ssh/remote failure for %s@%s\n", node, curriculum); return 1; }

    /* Pull the last "Average Loss: X" from the tail of output. */
    float final_loss = -1.0f;
    int last_epoch = 0;
    const char *p = out;
    const char *hit;
    while ((hit = strstr(p, "Average Loss:")) != NULL) {
        if (sscanf(hit, "Average Loss: %f", &final_loss) == 1) {
            last_epoch = 0;
        }
        p = hit + 1;
    }
    /* Try to recover the epoch number from "Epoch E/E, Average Loss: ..." */
    {
        const char *q = out;
        const char *h;
        while ((h = strstr(q, "Epoch ")) != NULL) {
            int e = 0, t = 0;
            if (sscanf(h, "Epoch %d/%d, Average Loss: %f", &e, &t, &final_loss) == 3)
                last_epoch = e;
            q = h + 1;
        }
    }
    /* Cross-check: read loss.txt last line for the definitive epoch number. */
    {
        char loss_cmd[MAX_LINE * 2];
        snprintf(loss_cmd, sizeof(loss_cmd),
            "sshpass -p '%s' ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "
            "%s \"cd ~/iqabod-store/%s && tail -1 loss.txt 2>/dev/null\"",
            pass, user, curriculum);
        char *last = run_capture(loss_cmd);
        if (last) {
            int e = 0;
            float l = -1.0f;
            if (sscanf(last, "%d,training,epoch_avg,token,%f", &e, &l) == 2) {
                last_epoch = e;
                final_loss = l;
            }
            free(last);
        }
    }

    if (final_loss < 0.0f) {
        printf("TRAIN|%s|%s|ERROR|no_loss_parsed\n", curriculum, node);
        free(out);
        return 2;
    }

    printf("TRAIN|%s|%s|epochs=%d|final_loss=%.4f\n", curriculum, node, last_epoch, final_loss);
    free(out);
    return 0;
}
