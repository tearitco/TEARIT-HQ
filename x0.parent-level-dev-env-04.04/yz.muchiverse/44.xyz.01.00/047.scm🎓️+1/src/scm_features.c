/* scm_features.c — deterministic situation-vector extraction (v1) */
#include "scm_core.h"

static int has(const char *hay, const char *needle) { return strstr(hay, needle) != NULL; }

void scm_feature_vec(const char *message, ScmCurriculum *c, double *fv, int turn, int rp_on) {
    for (int i = 0; i < c->n_features; i++) fv[i] = 0.0;
    char low[SCM_MAX_LINE];
    snprintf(low, sizeof(low), "%s", message);
    for (int i = 0; low[i]; i++) if (low[i] >= 'A' && low[i] <= 'Z') low[i] += 32;

    for (int i = 0; i < c->n_features; i++) {
        const char *fn = c->features[i];
        if (!strcmp(fn, "greet")) {
            if (has(low, "hello") || has(low, "hi") || has(low, "hey") ||
                has(low, "how are you") || has(low, "good morning") ||
                has(low, "good evening") || has(low, "howdy") || has(low, "yo"))
                fv[i] = 1.0;
        } else if (!strcmp(fn, "question")) {
            int len = (int)strlen(low);
            if (len && low[len-1] == '?') fv[i] = 1.0;
            else if (strncmp(low, "who", 3) == 0 || strncmp(low, "what", 4) == 0 ||
                     strncmp(low, "when", 4) == 0 || strncmp(low, "where", 5) == 0 ||
                     strncmp(low, "why", 3) == 0 || strncmp(low, "how", 3) == 0 ||
                     strncmp(low, "can ", 4) == 0 || strncmp(low, "could", 5) == 0 ||
                     strncmp(low, "do ", 3) == 0 || strncmp(low, "does", 4) == 0 ||
                     strncmp(low, "is ", 3) == 0 || strncmp(low, "are ", 4) == 0)
                fv[i] = 1.0;
        } else if (!strcmp(fn, "neg")) {
            if (has(low, " no") || has(low, "not") || has(low, "n't") ||
                has(low, "bad") || has(low, "hate") || has(low, "sad") ||
                has(low, "angry") || has(low, "tired"))
                fv[i] = 1.0;
        } else if (!strcmp(fn, "repeat")) {
            fv[i] = 0.5;   /* global variety bias; real penalty is per-phrase recency */
        } else if (!strcmp(fn, "topic_roleplay")) {
            fv[i] = (rp_on || strcmp(c->context, "casual") != 0) ? 1.0 : 0.0;
        } else if (!strcmp(fn, "state")) {
            fv[i] = 1.0;   /* v1: free-bandit state; FSM one-hots land here later */
        } else if (!strcmp(fn, "turn")) {
            fv[i] = turn > 0 ? 1.0 : 0.0;
        } else if (!strcmp(fn, "length")) {
            int len = (int)strlen(low);
            fv[i] = len > 40 ? 1.0 : (len > 15 ? 0.5 : 0.0);
        }
    }
}
