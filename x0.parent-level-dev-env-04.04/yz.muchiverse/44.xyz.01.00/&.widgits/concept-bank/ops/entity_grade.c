/* entity_grade - the ENTITY is the learner (RPG-Maker style): a grade (tier, the soul/brain's school year), a level, EXP, MP, and skills that cost MP.
 *
 *   entity_grade check   <ent_dir> <curriculum.pdl>              report card -> variables.txt (grade_tier, grade_ready, grade_needs_person, grade_exp_total); prints SUBJECT/GRADE rows
 *   entity_grade advance <ent_dir> <curriculum.pdl> <by>         graduate one grade if the report card says ready; a GRADE row with approval=person refuses by=auto (exit 4)
 *   entity_grade use     <ent_dir> <skillbook.pdl> <skill>       use a skill: needs grade >= tier=, level >= level=, MP >= mp=; costs MP, gives EXP, may level up
 *   entity_grade rest    <ent_dir> <skillbook.pdl>               refill MP to the level's maximum (call it from the day-tick event)
 *   entity_grade tick    <school_dir> <participants_dir>         the day-tick event: for EVERY entity folder under participants_dir: rest, report card, and an automatic graduation
 *                                                                (by=auto; grades with approval=person stay put). school_dir holds curriculum.pdl and skillbook.pdl (one copy per game, like Eden's conductor/).
 *
 * GRADE = graduated by EXAMS (graded evidence), LEVEL = grown by EXPERIENCE (using skills). Evidence for the report card, all inside <ent_dir>:
 *   obs_feedback_log.txt   FEEDBACK rows (valence=+1/-1, concept=<subject>, layer=curriculum|extracurricular, intensity=1): one subject per layer/concept, Laplace score (reward+1)/(reward+punish+2)
 *   actor_skills.txt       lines `skill_1:<name>=1` (Eden's unlocked skills): rewards of the subject extracurricular/eden-skills
 *   learning_limits.pdl    `strength_of_training.max_tier: <preschool|elementary_hs|associate_bachelor|master_phd>` = the current grade (missing = preschool); advance rewrites it
 *   transcript.txt         append-only: one GRADUATED row per graduation
 *   variables.txt          the event mechanism's `key=int` file: rpg_level, rpg_exp, rpg_mp, rpg_mp_max, grade_* (so event pages can branch on them with variable_math lt/ge/eq)
 * curriculum.pdl rows:  GRADE | from=<tier> | to=<tier> | min_subjects=N | min_score=0.NN | min_n=N | min_extra=N | approval=auto|person
 *     a subject PASSES with n >= min_n and score >= min_score; ready = passing >= min_subjects AND passing extracurricular >= min_extra.
 * skillbook.pdl rows:   LEVEL | exp_per_level=N | base_mp=N | mp_per_level=N | max_level=N        SKILL | <name> | tier=N | level=N | mp=N | exp=N
 * Exit 0 ok | 2 usage / unreadable | 3 not ready / unknown skill | 4 refused (approval, grade, level or MP) | 5 already at the last grade.
 * Every path comes from argv. Build: gcc -std=gnu11 -Wall -Wextra -Werror -O2 -o entity_grade.+x entity_grade.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#define L 2048
static const char *TIER[4] = { "preschool", "elementary_hs", "associate_bachelor", "master_phd" };
static char ENT[2048];
static void path(char *out, size_t n, const char *f) { snprintf(out, n, "%.1900s/%s", ENT, f); }
static long fnum(const char *row, const char *key, long dflt) {                   /* ` key=123` inside a pipe row */
    char pat[64]; snprintf(pat, sizeof pat, "%s=", key); const char *p = row;
    while ((p = strstr(p, pat))) { if (p == row || p[-1] == ' ' || p[-1] == '|') return strtol(p + strlen(pat), NULL, 10); p++; }
    return dflt;
}
static double fdbl(const char *row, const char *key, double dflt) {
    char pat[64]; snprintf(pat, sizeof pat, "%s=", key); const char *p = row;
    while ((p = strstr(p, pat))) { if (p == row || p[-1] == ' ' || p[-1] == '|') return strtod(p + strlen(pat), NULL); p++; }
    return dflt;
}
static int fstr(const char *row, const char *key, char *out, size_t sz) {
    char pat[64]; snprintf(pat, sizeof pat, "%s=", key); const char *p = row;
    while ((p = strstr(p, pat))) { if (p == row || p[-1] == ' ' || p[-1] == '|') { p += strlen(pat); size_t k = 0; while (p[k] && p[k] != ' ' && p[k] != '|' && p[k] != '\n' && p[k] != '\r' && k + 1 < sz) { out[k] = p[k]; k++; } out[k] = 0; return 1; } p++; }
    return 0;
}
static int col(const char *row, int k, char *out, size_t sz) {
    const char *p = row; for (int i = 0; i < k; i++) { p = strchr(p, '|'); if (!p) return 0; p++; }
    while (*p == ' ') p++;
    size_t n = 0; while (p[n] && p[n] != '|' && p[n] != '\n' && p[n] != '\r') n++;
    while (n && p[n - 1] == ' ') n--;
    if (n >= sz) n = sz - 1;
    memcpy(out, p, n); out[n] = 0; return 1;
}
static long var_get(const char *key, long dflt) {
    char p[2100], ln[L]; path(p, sizeof p, "variables.txt"); FILE *f = fopen(p, "r"); size_t kl = strlen(key);
    if (!f) return dflt;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { fclose(f); return strtol(ln + kl + 1, NULL, 10); }
    fclose(f); return dflt;
}
static int var_set(const char *key, long v) {                                      /* replace in place else append; temp file + rename */
    char p[2100], tp[2110], ln[L]; size_t kl = strlen(key); int done = 0;
    path(p, sizeof p, "variables.txt"); snprintf(tp, sizeof tp, "%s.tmp", p);
    FILE *f = fopen(p, "r"), *o = fopen(tp, "w"); if (!o) { if (f) fclose(f); return 0; }
    while (f && fgets(ln, sizeof ln, f)) {
        if (!done && !strncmp(ln, key, kl) && ln[kl] == '=') { fprintf(o, "%s=%ld\n", key, v); done = 1; }
        else { fputs(ln, o); if (!strchr(ln, '\n')) fputc('\n', o); }
    }
    if (f) fclose(f);
    if (!done) fprintf(o, "%s=%ld\n", key, v);
    fclose(o); return rename(tp, p) == 0;
}
static int tier_of(const char *name) { for (int i = 0; i < 4; i++) if (!strcmp(name, TIER[i])) return i; return -1; }
static int read_tier(void) {
    char p[2100], ln[L], v[64]; path(p, sizeof p, "learning_limits.pdl"); FILE *f = fopen(p, "r"); if (!f) return 0;
    while (fgets(ln, sizeof ln, f)) if (strstr(ln, "max_tier") && ln[0] != '#') { char *c = strchr(ln, ':'); if (c) { c++; while (*c == ' ') c++; size_t k = 0; while (c[k] && !isspace((unsigned char)c[k]) && k < 63) { v[k] = c[k]; k++; } v[k] = 0; int t = tier_of(v); fclose(f); return t < 0 ? 0 : t; } }
    fclose(f); return 0;
}
static int write_tier(int t) {                                                    /* keep every other line; replace the max_tier line or append one */
    char p[2100], tp[2110], ln[L]; int done = 0; path(p, sizeof p, "learning_limits.pdl"); snprintf(tp, sizeof tp, "%s.tmp", p);
    FILE *f = fopen(p, "r"), *o = fopen(tp, "w"); if (!o) { if (f) fclose(f); return 0; }
    while (f && fgets(ln, sizeof ln, f)) { if (!done && ln[0] != '#' && strstr(ln, "max_tier")) { fprintf(o, "strength_of_training.max_tier: %s\n", TIER[t]); done = 1; } else { fputs(ln, o); if (!strchr(ln, '\n')) fputc('\n', o); } }
    if (f) fclose(f);
    if (!done) fprintf(o, "strength_of_training.max_tier: %s\n", TIER[t]);
    fclose(o); return rename(tp, p) == 0;
}
typedef struct { char key[96]; long reward, punish; int extra; } Subj;
static Subj S[128]; static int nS = 0;
static Subj *subj(const char *layer, const char *concept) {
    char k[96]; snprintf(k, sizeof k, "%.30s/%.60s", layer, concept);
    for (int i = 0; i < nS; i++) if (!strcmp(S[i].key, k)) return &S[i];
    if (nS >= 128) return NULL;
    snprintf(S[nS].key, sizeof S[nS].key, "%s", k); S[nS].extra = !strcmp(layer, "extracurricular"); return &S[nS++];
}
static void load_evidence(void) {
    char p[2100], ln[L]; FILE *f; path(p, sizeof p, "obs_feedback_log.txt"); nS = 0; memset(S, 0, sizeof S);
    if ((f = fopen(p, "r"))) {
        while (fgets(ln, sizeof ln, f)) {
            char *q = strstr(ln, "FEEDBACK |"); char con[64] = "", lay[32] = "curriculum"; if (!q) continue;
            if (!fstr(q, "concept", con, sizeof con)) continue;
            fstr(q, "layer", lay, sizeof lay);
            long v = fnum(q, "valence", 0); long inten = (long)(fdbl(q, "intensity", 1.0) + 0.5); Subj *s = subj(lay, con); if (!s) continue;
            if (v > 0) s->reward += inten; else if (v < 0) s->punish += inten;
        }
        fclose(f);
    }
    path(p, sizeof p, "actor_skills.txt");
    if ((f = fopen(p, "r"))) {
        while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "skill_1:", 8)) { const char *e = strchr(ln, '='); if (e && atol(e + 1) >= 1) { Subj *s = subj("extracurricular", "eden-skills"); if (s) s->reward++; } }
        fclose(f);
    }
}
static double score(const Subj *s) { return (double)(s->reward + 1) / (double)(s->reward + s->punish + 2); }
/* report card for the current grade; returns 1 ready, 0 not ready, -1 no GRADE row / last grade. Fills the next tier and approval. */
static int report(const char *curr, int *next, int *person) {
    int cur = read_tier(); char ln[L]; FILE *f = fopen(curr, "r"); long total = 0; int found = 0, ready = 0; *next = -1; *person = 0;
    load_evidence();
    for (int i = 0; i < nS; i++) total += S[i].reward;
    if (!f) return -2;
    while (fgets(ln, sizeof ln, f)) {
        char fr[64] = "", to[64] = "", ap[16] = "auto"; if (strncmp(ln, "GRADE |", 7)) continue;
        if (!fstr(ln, "from", fr, sizeof fr) || !fstr(ln, "to", to, sizeof to) || tier_of(fr) != cur || tier_of(to) < 0) continue;
        fstr(ln, "approval", ap, sizeof ap);
        long min_s = fnum(ln, "min_subjects", 1), min_n = fnum(ln, "min_n", 5), min_x = fnum(ln, "min_extra", 0); double min_sc = fdbl(ln, "min_score", 0.70); int pass = 0, passx = 0;
        for (int i = 0; i < nS; i++) {
            int ok = (S[i].reward + S[i].punish) >= min_n && score(&S[i]) >= min_sc;
            printf("SUBJECT | %s | n=%ld | reward=%ld | punish=%ld | score=%.2f | pass=%d\n", S[i].key, S[i].reward + S[i].punish, S[i].reward, S[i].punish, score(&S[i]), ok);
            if (ok) { pass++; if (S[i].extra) passx++; }
        }
        ready = pass >= min_s && passx >= min_x; found = 1; *next = tier_of(to); *person = !strcmp(ap, "person");
        printf("GRADE | %s -> %s | passing=%d/%ld | extra=%d/%ld | ready=%d | approval=%s\n", fr, to, pass, min_s, passx, min_x, ready, ap);
        break;
    }
    fclose(f);
    var_set("grade_tier", cur); var_set("grade_ready", ready); var_set("grade_needs_person", *person); var_set("grade_exp_total", total);
    if (!found) { printf("GRADE | %s | no further grade in the curriculum | ready=0\n", TIER[cur]); return -1; }
    return ready;
}
/* ---- RPG part ---- */
typedef struct { long exp_per_level, base_mp, mp_per_level, max_level; } Lv;
static Lv read_levels(const char *book) {
    Lv v = { 10, 6, 2, 20 }; char ln[L]; FILE *f = fopen(book, "r"); if (!f) return v;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "LEVEL |", 7)) { v.exp_per_level = fnum(ln, "exp_per_level", v.exp_per_level); v.base_mp = fnum(ln, "base_mp", v.base_mp); v.mp_per_level = fnum(ln, "mp_per_level", v.mp_per_level); v.max_level = fnum(ln, "max_level", v.max_level); }
    fclose(f); if (v.exp_per_level < 1) v.exp_per_level = 1; return v;
}
static long level_for(long exp, const Lv *v) { long l = 1 + exp / v->exp_per_level; return l > v->max_level ? v->max_level : l; }
static long mp_max_for(long level, const Lv *v) { return v->base_mp + (level - 1) * v->mp_per_level; }
static void ensure_actor(const Lv *v) {
    if (var_get("rpg_level", 0) < 1) { var_set("rpg_level", 1); var_set("rpg_exp", 0); var_set("rpg_mp_max", mp_max_for(1, v)); var_set("rpg_mp", mp_max_for(1, v)); }
}
static int do_rest(const char *book) {
    Lv v = read_levels(book); ensure_actor(&v); long lv = var_get("rpg_level", 1), mx = mp_max_for(lv, &v); var_set("rpg_mp_max", mx); var_set("rpg_mp", mx);
    printf("RESTED level=%ld mp=%ld/%ld\n", lv, mx, mx); return 0;
}
static int do_advance(const char *curr, const char *by) {
    int nx, pe, cur = read_tier(); int r = report(curr, &nx, &pe);
    if (r == -2) return 2;
    if (r == -1) { printf("LAST_GRADE %s\n", TIER[cur]); return 5; }
    if (!r) { printf("NOT_READY %s\n", TIER[cur]); return 3; }
    if (pe && !strcmp(by, "auto")) { printf("REFUSED %s -> %s needs a person's approval (by=auto is not enough)\n", TIER[cur], TIER[nx]); return 4; }
    if (!write_tier(nx)) { fprintf(stderr, "cannot write learning_limits.pdl\n"); return 2; }
    { char tp[2100]; path(tp, sizeof tp, "transcript.txt"); FILE *t = fopen(tp, "a"); if (t) { fprintf(t, "GRADUATED | from=%s | to=%s | by=%s | exp_total=%ld\n", TIER[cur], TIER[nx], by, var_get("grade_exp_total", 0)); fclose(t); } }
    var_set("grade_tier", nx); var_set("grade_ready", 0);
    printf("GRADUATED %s -> %s by %s\n", TIER[cur], TIER[nx], by); return 0;
}
static int cmp_names(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static int do_tick(const char *school, const char *parts) {
    char curr[2100], book[2100], names[256][256]; int n = 0, grads = 0; struct stat sb;
    snprintf(curr, sizeof curr, "%.2000s/curriculum.pdl", school); snprintf(book, sizeof book, "%.2000s/skillbook.pdl", school);
    DIR *d = opendir(parts); if (!d) { fprintf(stderr, "cannot open %s\n", parts); return 2; }
    for (struct dirent *e; (e = readdir(d)) && n < 256; ) { char q[2400]; if (e->d_name[0] == '.') continue; snprintf(q, sizeof q, "%.2000s/%.250s", parts, e->d_name); if (!stat(q, &sb) && S_ISDIR(sb.st_mode)) snprintf(names[n++], 256, "%s", e->d_name); }
    closedir(d); qsort(names, (size_t)n, 256, cmp_names);
    for (int i = 0; i < n; i++) {
        snprintf(ENT, sizeof ENT, "%.1900s/%.100s", parts, names[i]);
        printf("ENTITY %s\n", names[i]); do_rest(book);
        if (do_advance(curr, "auto") == 0) grads++;
    }
    printf("TICK entities=%d graduated=%d\n", n, grads); return 0;
}
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: entity_grade check|advance|use|rest|tick <dir> <file> [by|skill]\n"); return 2; }
    const char *cmd = argv[1]; snprintf(ENT, sizeof ENT, "%s", argv[2]);
    if (!strcmp(cmd, "check") && argc == 4) { int nx, pe; int r = report(argv[3], &nx, &pe); return r == -2 ? 2 : 0; }
    if (!strcmp(cmd, "advance") && argc == 5) return do_advance(argv[3], argv[4]);
    if (!strcmp(cmd, "rest") && argc == 4) return do_rest(argv[3]);
    if (!strcmp(cmd, "tick") && argc == 4) return do_tick(argv[2], argv[3]);
    if (!strcmp(cmd, "use") && argc == 5) {
        Lv v = read_levels(argv[3]); char ln[L]; FILE *f = fopen(argv[3], "r"); long tier_need = 0, lvl_need = 1, mp_cost = 0, exp_gain = 0; int hit = 0;
        if (!f) return 2;
        while (fgets(ln, sizeof ln, f)) { char nm[64]; if (strncmp(ln, "SKILL |", 7) || !col(ln, 1, nm, sizeof nm) || strcmp(nm, argv[4])) continue;
            tier_need = fnum(ln, "tier", 0); lvl_need = fnum(ln, "level", 1); mp_cost = fnum(ln, "mp", 0); exp_gain = fnum(ln, "exp", 0); hit = 1; break; }
        fclose(f);
        if (!hit) { printf("UNKNOWN_SKILL %s\n", argv[4]); return 3; }
        ensure_actor(&v);
        long lv = var_get("rpg_level", 1), mp = var_get("rpg_mp", 0), ex = var_get("rpg_exp", 0); int cur = read_tier();
        if (cur < tier_need) { printf("REFUSED %s needs grade %s (this entity: %s)\n", argv[4], TIER[tier_need < 4 ? tier_need : 3], TIER[cur]); return 4; }
        if (lv < lvl_need) { printf("REFUSED %s needs level %ld (this entity: %ld)\n", argv[4], lvl_need, lv); return 4; }
        if (mp < mp_cost) { printf("REFUSED %s costs %ld MP (this entity has %ld)\n", argv[4], mp_cost, mp); return 4; }
        long nmp = mp - mp_cost, nex = ex + exp_gain, nlv = level_for(nex, &v);
        var_set("rpg_mp", nmp); var_set("rpg_exp", nex);
        if (nlv > lv) { var_set("rpg_level", nlv); var_set("rpg_mp_max", mp_max_for(nlv, &v)); var_set("rpg_mp", nmp + (mp_max_for(nlv, &v) - mp_max_for(lv, &v))); }
        printf("USED %s mp=%ld->%ld exp=%ld->%ld level=%ld%s\n", argv[4], mp, nmp, ex, nex, nlv > lv ? nlv : lv, nlv > lv ? " LEVEL_UP" : ""); return 0;
    }
    fprintf(stderr, "usage: entity_grade check|advance|use|rest ...\n"); return 2;
}
