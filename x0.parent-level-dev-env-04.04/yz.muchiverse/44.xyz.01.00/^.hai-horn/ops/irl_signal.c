/* irl_signal.c - aggregate IRL grading results into training signal
 *
 * Reads irl_training_data.jsonl (JSONL), computes aggregate scores per model,
 * produces weight deltas for Concept Bank or tomom training data.
 *
 * Usage: irl_signal.+x <input_jsonl> <output_signal_file>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAXLINE 16384

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || isspace((unsigned char)s[n-1]))) s[--n] = '\0';
}

/* Extract JSON field value, handling escaped quotes and newlines */
static int extract_json_field(const char *json, const char *field, char *out, size_t outsz) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", field);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return 0;
    p++;
    size_t i = 0;
    while (*p && i < outsz - 1) {
        if (*p == '\\') {
            p++;
            if (*p == '"' || *p == '\\' || *p == '/') {
                out[i++] = *p++;
            } else if (*p == 'n') {
                out[i++] = '\n';
                p++;
            } else if (*p == 't') {
                out[i++] = '\t';
                p++;
            } else if (*p == 'r') {
                out[i++] = '\r';
                p++;
            } else {
                out[i++] = *p++;
            }
        } else if (*p == '"') {
            p++;
            break;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';
    return 1;
}

static int parse_score_line(const char *grading, const char *prefix, int scores[5]) {
    char *p = strstr(grading, prefix);
    if (!p) return 0;
    p += strlen(prefix);
    while (*p == ' ' || *p == ':') p++;
    for (int i = 0; i < 5; i++) {
        while (*p == ' ') p++;
        if (*p >= '1' && *p <= '5') {
            scores[i] = *p - '0';
            p++;
        } else {
            scores[i] = 0;
        }
    }
    return 1;
}

static const char *winner_from_grading(const char *grading) {
    char *p = strstr(grading, "WINNER:");
    if (!p) return "UNKNOWN";
    p += 7;
    while (*p == ' ') p++;
    if (*p == 'A') return "HORN";
    if (*p == 'B') return "HALO";
    if (*p == 'T') return "TIE";
    return "UNKNOWN";
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: irl_signal.+x <input_jsonl> <output_signal_file>\n");
        return 1;
    }
    const char *input_file = argv[1];
    const char *output_file = argv[2];

    FILE *in = fopen(input_file, "r");
    if (!in) {
        fprintf(stderr, "Cannot open %s\n", input_file);
        return 1;
    }

    int horn_wins = 0, halo_wins = 0, ties = 0;
    int horn_scores[5] = {0}, halo_scores[5] = {0};
    int horn_count = 0, halo_count = 0;
    int total = 0;

    char line[MAXLINE];
    while (fgets(line, sizeof(line), in)) {
        trim(line);
        if (!line[0]) continue;

        char grading[MAXLINE] = {0};
        if (!extract_json_field(line, "grading", grading, sizeof(grading))) {
            continue;
        }

        const char *winner = winner_from_grading(grading);
        if (strcmp(winner, "HORN") == 0) horn_wins++;
        else if (strcmp(winner, "HALO") == 0) halo_wins++;
        else ties++;

        int h_scores[5], a_scores[5];
        if (parse_score_line(grading, "SCORE_A:", h_scores)) {
            for (int i = 0; i < 5; i++) horn_scores[i] += h_scores[i];
            horn_count++;
        }
        if (parse_score_line(grading, "SCORE_B:", a_scores)) {
            for (int i = 0; i < 5; i++) halo_scores[i] += a_scores[i];
            halo_count++;
        }
        total++;
    }
    fclose(in);

    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Cannot write %s\n", output_file);
        return 1;
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"total_comparisons\": %d,\n", total);
    fprintf(out, "  \"horn_wins\": %d,\n", horn_wins);
    fprintf(out, "  \"halo_wins\": %d,\n", halo_wins);
    fprintf(out, "  \"ties\": %d,\n", ties);
    fprintf(out, "  \"horn_win_rate\": %.4f,\n", total ? (double)horn_wins / total : 0.0);
    fprintf(out, "  \"halo_win_rate\": %.4f,\n", total ? (double)halo_wins / total : 0.0);

    fprintf(out, "  \"avg_scores_horn\": [");
    for (int i = 0; i < 5; i++) {
        double avg = horn_count ? (double)horn_scores[i] / horn_count : 0.0;
        fprintf(out, "%s%.2f", i ? ", " : "", avg);
    }
    fprintf(out, "],\n");

    fprintf(out, "  \"avg_scores_halo\": [");
    for (int i = 0; i < 5; i++) {
        double avg = halo_count ? (double)halo_scores[i] / halo_count : 0.0;
        fprintf(out, "%s%.2f", i ? ", " : "", avg);
    }
    fprintf(out, "],\n");

    // Compute weight delta for Concept Bank: positive if HALO wins, negative if HORN wins
    double win_diff = (double)(halo_wins - horn_wins) / (total ? total : 1);
    double delta = win_diff * 0.1;  // scale to [-0.1, 0.1]

    fprintf(out, "  \"concept_bank_delta\": %.4f,\n", delta);
    fprintf(out, "  \"signal_timestamp\": %ld\n", (long)time(NULL));
    fprintf(out, "}\n");
    fclose(out);

    printf("Signal computed: HORN=%.2f%% HALO=%.2f%% delta=%.4f\n",
           total ? 100.0 * horn_wins / total : 0,
           total ? 100.0 * halo_wins / total : 0,
           delta);

    return 0;
}