#ifndef SCM_CORE_H
#define SCM_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define SCM_MAX_LINE     1024
#define SCM_MAX_NAME     256
#define SCM_MAX_FEATURES 64
#define SCM_MAX_MEMORY   32

typedef struct {
    char text[SCM_MAX_LINE];
    char lock[SCM_MAX_NAME];
    double bias;
    double *w;            /* [n_features] */
    double meta_delta;    /* active situation override delta */
} ScmPhrase;

typedef struct {
    char name[SCM_MAX_NAME];
    char values[SCM_MAX_LINE];   /* comma-separated */
} ScmSlot;

typedef struct {
    char name[SCM_MAX_NAME];
    char path[SCM_MAX_LINE];     /* corpus dir */
    char reply_shape[32];        /* single | sequence | template */
    int seq_len;                 /* N for sequence(N) */
    double temperature;
    char context[SCM_MAX_NAME];  /* casual | topic | roleplay | game (drives topic_roleplay feature) */
    char fallback[SCM_MAX_LINE];
    char **features;
    int n_features;
    ScmPhrase *phrases;
    int n_phrases;
    int cap_phrases;
    ScmSlot *slots;
    int n_slots;
    int cap_slots;
} ScmCurriculum;

typedef struct {
    char bucket[SCM_MAX_NAME];       /* casual | topic:<n> | rp:<n> | game:<n> */
    char curriculum[SCM_MAX_LINE];   /* corpus dir */
    char fsm[SCM_MAX_LINE];          /* fsm dir (unused v1) */
    char keywords[SCM_MAX_LINE];     /* comma-separated topic keywords */
} ScmIntent;

typedef struct {
    char items[SCM_MAX_MEMORY][SCM_MAX_LINE];
    int n;
} ScmMemory;

char *scm_trim(char *s);
int  scm_curriculum_load(ScmCurriculum *c, const char *dir);
void scm_curriculum_free(ScmCurriculum *c);
int  scm_meta_load(ScmCurriculum *c, const char *situation);
void scm_meta_clear(ScmCurriculum *c);

int  scm_intents_load(const char *conf, ScmIntent *out, int max);
int  scm_route(const char *message, ScmIntent *intents, int n);

void scm_feature_vec(const char *message, ScmCurriculum *c, double *fv, int turn, int rp_on);

double scm_score_phrase(ScmCurriculum *c, ScmPhrase *p, const double *fv);
int  scm_select(ScmCurriculum *c, const double *fv, int want, int *idx_out,
                ScmMemory *mem, unsigned *seed);
void scm_slot_fill(ScmCurriculum *c, const char *src, char *out, int outsz, unsigned *seed);

void scm_mem_push(ScmMemory *m, const char *s);
int  scm_mem_recency(ScmMemory *m, const char *s);

#endif
