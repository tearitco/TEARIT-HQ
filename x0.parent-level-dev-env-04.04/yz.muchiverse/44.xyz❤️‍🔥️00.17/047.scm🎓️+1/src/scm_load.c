/* scm_load.c — corpus / weights / meta / intents loaders + router v1 */
#include "scm_core.h"

char *scm_trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) s[--n] = 0;
    return s;
}

int scm_curriculum_load(ScmCurriculum *c, const char *dir) {
    char p[SCM_MAX_LINE];
    memset(c, 0, sizeof(*c));
    snprintf(c->name, sizeof(c->name), "%s", dir);
    snprintf(c->path, sizeof(c->path), "%s", dir);
    c->temperature = 1.0;
    c->seq_len = 1;
    snprintf(c->context, sizeof(c->context), "%s", "casual");
    snprintf(c->fallback, sizeof(c->fallback), "%s", "I'm not sure what to say about that yet.");

    /* curriculum.pdl */
    snprintf(p, sizeof(p), "%s/curriculum.pdl", dir);
    FILE *f = fopen(p, "r");
    if (f) {
        char line[SCM_MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *t = scm_trim(line);
            if (!*t || *t == '#') continue;
            char *colon = strchr(t, ':');
            if (!colon) continue;
            *colon = 0;
            char *key = scm_trim(t), *val = scm_trim(colon + 1);
            if (!strcmp(key, "corpus")) snprintf(c->name, sizeof(c->name), "%s", val);
            else if (!strcmp(key, "reply_shape")) {
                snprintf(c->reply_shape, sizeof(c->reply_shape), "%s", val);
                if (strncmp(val, "sequence", 8) == 0) {
                    int n = atoi(val + 8);
                    c->seq_len = n > 0 ? n : 1;
                    snprintf(c->reply_shape, sizeof(c->reply_shape), "%s", "sequence");
                } else c->seq_len = 1;
            } else if (!strcmp(key, "temperature")) c->temperature = atof(val);
            else if (!strcmp(key, "context")) snprintf(c->context, sizeof(c->context), "%s", val);
            else if (!strcmp(key, "features")) {
                    char *tok = strtok(val, ",");
                while (tok && c->n_features < SCM_MAX_FEATURES) {
                    c->features = realloc(c->features, (c->n_features + 1) * sizeof(char *));
                    c->features[c->n_features] = strdup(scm_trim(tok));
                    c->n_features++;
                    tok = strtok(NULL, ",");
                }
            } else if (!strcmp(key, "fallback")) snprintf(c->fallback, sizeof(c->fallback), "%s", val);
        }
        fclose(f);
    }

    /* weights.txt */
    snprintf(p, sizeof(p), "%s/weights.txt", dir);
    f = fopen(p, "r");
    if (f) {
        char line[SCM_MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *t = scm_trim(line);
            if (!*t || *t == '#') continue;
            char *toks[SCM_MAX_FEATURES + 8]; int nt = 0;
            char *tok = strtok(t, "|");
            while (tok && nt < (SCM_MAX_FEATURES + 8)) { toks[nt++] = scm_trim(tok); tok = strtok(NULL, "|"); }
            if (nt < 1) continue;
            ScmPhrase ph; memset(&ph, 0, sizeof(ph));
            snprintf(ph.text, sizeof(ph.text), "%s", toks[0]);
            snprintf(ph.lock, sizeof(ph.lock), "%s", "-");
            if (nt >= 2) ph.bias = atof(toks[1]);
            if (c->n_features > 0) {
                ph.w = calloc(c->n_features, sizeof(double));
                for (int i = 0; i < c->n_features && 2 + i < nt; i++) ph.w[i] = atof(toks[2 + i]);
            }
            int lockidx = 2 + c->n_features;
            if (nt > lockidx) snprintf(ph.lock, sizeof(ph.lock), "%s", toks[nt - 1]);
            if (c->n_phrases >= c->cap_phrases) {
                c->cap_phrases = c->cap_phrases ? c->cap_phrases * 2 : 64;
                c->phrases = realloc(c->phrases, c->cap_phrases * sizeof(ScmPhrase));
            }
            c->phrases[c->n_phrases++] = ph;
        }
        fclose(f);
    }

    /* phrases.txt (merge; add missing with default weights) */
    snprintf(p, sizeof(p), "%s/phrases.txt", dir);
    f = fopen(p, "r");
    if (f) {
        char line[SCM_MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *t = scm_trim(line);
            if (!*t || *t == '#') continue;
            int found = -1;
            for (int i = 0; i < c->n_phrases; i++)
                if (!strcmp(c->phrases[i].text, t)) { found = i; break; }
            if (found >= 0) continue;
            ScmPhrase ph; memset(&ph, 0, sizeof(ph));
            snprintf(ph.text, sizeof(ph.text), "%s", t);
            snprintf(ph.lock, sizeof(ph.lock), "%s", "-");
            if (c->n_features > 0) ph.w = calloc(c->n_features, sizeof(double));
            if (c->n_phrases >= c->cap_phrases) {
                c->cap_phrases = c->cap_phrases ? c->cap_phrases * 2 : 64;
                c->phrases = realloc(c->phrases, c->cap_phrases * sizeof(ScmPhrase));
            }
            c->phrases[c->n_phrases++] = ph;
        }
        fclose(f);
    }

    /* slots.txt */
    snprintf(p, sizeof(p), "%s/slots.txt", dir);
    f = fopen(p, "r");
    if (f) {
        char line[SCM_MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *t = scm_trim(line);
            if (!*t || *t == '#') continue;
            char *eq = strchr(t, '=');
            if (!eq) continue;
            *eq = 0;
            char *name = scm_trim(t), *vals = scm_trim(eq + 1);
            if (c->n_slots >= c->cap_slots) {
                c->cap_slots = c->cap_slots ? c->cap_slots * 2 : 8;
                c->slots = realloc(c->slots, c->cap_slots * sizeof(ScmSlot));
            }
            snprintf(c->slots[c->n_slots].name, sizeof(c->slots[0].name), "%s", name);
            snprintf(c->slots[c->n_slots].values, sizeof(c->slots[0].values), "%s", vals);
            c->n_slots++;
        }
        fclose(f);
    }

    return c->n_phrases;
}

void scm_curriculum_free(ScmCurriculum *c) {
    for (int i = 0; i < c->n_phrases; i++) free(c->phrases[i].w);
    free(c->phrases);
    for (int i = 0; i < c->n_features; i++) free(c->features[i]);
    free(c->features);
    free(c->slots);
    memset(c, 0, sizeof(*c));
}

/* apply meta/<situation>.txt deltas to every phrase's meta_delta */
int scm_meta_load(ScmCurriculum *c, const char *situation) {
    scm_meta_clear(c);
    if (!situation || !*situation) return 0;
    char p[SCM_MAX_LINE + 32];
    snprintf(p, sizeof(p), "%s/meta/%s.txt", c->path, situation);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    char line[SCM_MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *t = scm_trim(line);
        if (!*t || *t == '#') continue;
        char *pipe = strchr(t, '|');
        if (!pipe) continue;
        *pipe = 0;
        char *phrase = scm_trim(t);
        double delta = atof(scm_trim(pipe + 1));
        for (int i = 0; i < c->n_phrases; i++) {
            if (!strcmp(phrase, "*") || !strcmp(c->phrases[i].text, phrase))
                c->phrases[i].meta_delta += delta;
        }
    }
    fclose(f);
    return 1;
}

void scm_meta_clear(ScmCurriculum *c) {
    for (int i = 0; i < c->n_phrases; i++) c->phrases[i].meta_delta = 0.0;
}

int scm_intents_load(const char *conf, ScmIntent *out, int max) {
    FILE *f = fopen(conf, "r");
    if (!f) return 0;
    int n = 0;
    char line[SCM_MAX_LINE];
    while (fgets(line, sizeof(line), f) && n < max) {
        char *t = scm_trim(line);
        if (!*t || *t == '#') continue;
        char *tok = strtok(t, "|");
        if (!tok) continue;
        ScmIntent it; memset(&it, 0, sizeof(it));
        snprintf(it.bucket, sizeof(it.bucket), "%s", scm_trim(tok));
        tok = strtok(NULL, "|"); if (tok) snprintf(it.curriculum, sizeof(it.curriculum), "%s", scm_trim(tok));
        tok = strtok(NULL, "|"); if (tok) snprintf(it.fsm, sizeof(it.fsm), "%s", scm_trim(tok));
        tok = strtok(NULL, "|"); if (tok) snprintf(it.keywords, sizeof(it.keywords), "%s", scm_trim(tok));
        out[n++] = it;
    }
    fclose(f);
    return n;
}

/* deterministic router v1: /rp <pack> command, then topic keywords, then casual */
int scm_route(const char *message, ScmIntent *intents, int n) {
    int casual = -1;
    for (int i = 0; i < n; i++) if (!strcmp(intents[i].bucket, "casual")) casual = i;
    char low[SCM_MAX_LINE];
    snprintf(low, sizeof(low), "%s", message);
    for (int i = 0; low[i]; i++) if (low[i] >= 'A' && low[i] <= 'Z') low[i] += 32;

    if (strncmp(low, "/rp ", 4) == 0) {
        char pack[SCM_MAX_LINE];
        snprintf(pack, sizeof(pack), "%s", scm_trim(low + 4));
        char *sp = strchr(pack, ' ');
        if (sp) *sp = 0;
        char bucket[SCM_MAX_LINE + 8];
        snprintf(bucket, sizeof(bucket), "rp:%s", pack);
        for (int i = 0; i < n; i++) if (!strcmp(intents[i].bucket, bucket)) return i;
    }

    for (int i = 0; i < n; i++) {
        if (!strcmp(intents[i].bucket, "casual")) continue;
        if (!*intents[i].keywords) continue;
        char kwcopy[SCM_MAX_LINE];
        snprintf(kwcopy, sizeof(kwcopy), "%s", intents[i].keywords);
        char *kw = strtok(kwcopy, ",");
        while (kw) {
            kw = scm_trim(kw);
            if (*kw && strstr(low, kw)) return i;
            kw = strtok(NULL, ",");
        }
    }
    return casual;
}

void scm_mem_push(ScmMemory *m, const char *s) {
    if (m->n < SCM_MAX_MEMORY) {
        snprintf(m->items[m->n], sizeof(m->items[0]), "%s", s);
        m->n++;
    } else {
        for (int i = 1; i < SCM_MAX_MEMORY; i++)
            memmove(m->items[i-1], m->items[i], sizeof(m->items[0]));
        snprintf(m->items[SCM_MAX_MEMORY-1], sizeof(m->items[0]), "%s", s);
    }
}

/* recency: 0 if never used, else 1 (newest) .. n (oldest) */
int scm_mem_recency(ScmMemory *m, const char *s) {
    for (int i = m->n - 1; i >= 0; i--)
        if (!strcmp(m->items[i], s)) return (m->n - 1 - i) + 1;
    return 0;
}
