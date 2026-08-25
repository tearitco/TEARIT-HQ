/* scm_select.c — the selector: linear score + softmax sample + slot fill */
#include "scm_core.h"

double scm_score_phrase(ScmCurriculum *c, ScmPhrase *p, const double *fv) {
    double s = p->bias;
    for (int i = 0; i < c->n_features; i++) s += fv[i] * p->w[i];
    s += p->meta_delta;
    return s;
}

static unsigned rng_next(unsigned *seed) {
    *seed = *seed * 1103515245u + 12345u;
    return (*seed >> 16) & 0x7fff;
}
static double rng_frac(unsigned *seed) { return (double)rng_next(seed) / 32767.0; }

int scm_select(ScmCurriculum *c, const double *fv, int want, int *idx_out,
               ScmMemory *mem, unsigned *seed) {
    if (c->n_phrases == 0) return 0;
    double *scores = calloc(c->n_phrases, sizeof(double));
    int *used = calloc(c->n_phrases, sizeof(int));
    double temp = c->temperature > 0.001 ? c->temperature : 1.0;
    int got = 0;
    for (int w = 0; w < want && got < c->n_phrases; w++) {
        double mx = -1e30;
        for (int i = 0; i < c->n_phrases; i++) {
            if (used[i]) { scores[i] = -1e30; continue; }
            double s = scm_score_phrase(c, &c->phrases[i], fv);
            s -= scm_mem_recency(mem, c->phrases[i].text) * 0.15;   /* recency penalty */
            scores[i] = s;
            if (s > mx) mx = s;
        }
        double sum = 0.0;
        for (int i = 0; i < c->n_phrases; i++) {
            if (used[i]) { scores[i] = 0; continue; }
            scores[i] = exp((scores[i] - mx) / temp);
            sum += scores[i];
        }
        if (sum <= 0) { for (int i = 0; i < c->n_phrases; i++) { scores[i] = used[i] ? 0 : 1; sum += scores[i]; } }
        double r = rng_frac(seed) * sum;
        double acc = 0; int pick = -1;
        for (int i = 0; i < c->n_phrases; i++) {
            if (used[i]) continue;
            acc += scores[i];
            if (r < acc) { pick = i; break; }
        }
        if (pick < 0) for (int i = 0; i < c->n_phrases; i++) if (!used[i]) { pick = i; break; }
        used[pick] = 1;
        idx_out[got++] = pick;
    }
    free(scores);
    free(used);
    return got;
}

void scm_slot_fill(ScmCurriculum *c, const char *src, char *out, int outsz, unsigned *seed) {
    int w = 0;
    const char *p = src;
    while (*p && w < outsz - 1) {
        if (*p == '{') {
            const char *close = strchr(p, '}');
            if (close && close > p + 1) {
                char name[SCM_MAX_NAME];
                size_t nlen = (size_t)(close - p - 1);
                if (nlen < SCM_MAX_NAME) {
                    memcpy(name, p + 1, nlen);
                    name[nlen] = 0;
                    const char *val = NULL;
                    for (int i = 0; i < c->n_slots; i++) {
                        if (!strcmp(c->slots[i].name, name)) {
                            char vc[SCM_MAX_LINE];
                            snprintf(vc, sizeof(vc), "%s", c->slots[i].values);
                            char *tok = strtok(vc, ",");
                            char *pickval = tok;
                            int count = 0;
                            while (tok) {
                                tok = scm_trim(tok);
                                count++;
                                if (rng_frac(seed) * count < 1.0) pickval = tok;
                                tok = strtok(NULL, ",");
                            }
                            val = pickval;
                            break;
                        }
                    }
                    if (!val) val = "something";
                    while (*val && w < outsz - 1) out[w++] = *val++;
                    p = close + 1;
                    continue;
                }
            }
        }
        out[w++] = *p++;
    }
    out[w] = 0;
}
