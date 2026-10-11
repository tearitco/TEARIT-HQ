/* ladder_pick.c - tiny pet-training provider selector */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define MAX_PROVIDERS 64
#define LINE_MAX 1024

typedef struct {
    char name[256];
    long long order;
    long long daily_budget;
    long long used_today;
    long long cooldown_until;
    int valid;
} Provider;

static void trim(char *s) {
    char *p = s;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\t')) s[--len] = '\0';
}

static int is_blank_or_comment(const char *s) {
    while (*s && (*s == ' ' || *s == '\t')) s++;
    return (*s == '\0' || *s == '#');
}

static int parse_ll(const char *s, long long *out) {
    char *end;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno || *end != '\0') return 0;
    *out = v;
    return 1;
}

static Provider *find_provider(Provider providers[], int count, const char *name) {
    for (int i = 0; i < count; ++i) {
        if (providers[i].valid && strcmp(providers[i].name, name) == 0)
            return &providers[i];
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) exit(2);
    const char *ladder_path = argv[1];
    const char *state_path = argv[2];
    const char *now_str = argv[3];

    /* validate now */
    long long now;
    {
        char *end;
        errno = 0;
        now = strtoll(now_str, &end, 10);
        if (errno || *end != '\0') exit(2);
    }
    long long cur_day = now / 86400LL;

    Provider providers[MAX_PROVIDERS];
    int prov_cnt = 0;
    memset(providers, 0, sizeof(providers));

    /* read ladder file */
    FILE *lf = fopen(ladder_path, "r");
    if (!lf) exit(2);
    char line[LINE_MAX];
    while (fgets(line, sizeof line, lf)) {
        line[strcspn(line, "\n")] = '\0';
        if (is_blank_or_comment(line)) continue;

        /* split by '|' */
        char *fields[8];
        int fcnt = 0;
        char *p = line;
        while (fcnt < 8) {
            char *sep = strchr(p, '|');
            if (sep) *sep = '\0';
            trim(p);
            fields[fcnt++] = p;
            if (!sep) break;
            p = sep + 1;
        }
        if (fcnt < 4) continue;
        if (strcmp(fields[0], "PROVIDER") != 0) continue;

        if (prov_cnt >= MAX_PROVIDERS) continue;
        Provider *pr = &providers[prov_cnt];
        strncpy(pr->name, fields[1], sizeof pr->name - 1);
        pr->name[sizeof pr->name - 1] = '\0';
        if (!parse_ll(fields[2], &pr->order)) continue;
        if (!parse_ll(fields[3], &pr->daily_budget)) continue;
        pr->used_today = 0;
        pr->cooldown_until = 0;
        pr->valid = 1;
        ++prov_cnt;
    }
    fclose(lf);

    /* read state file (ignore if missing) */
    FILE *sf = fopen(state_path, "r");
    if (sf) {
        while (fgets(line, sizeof line, sf)) {
            line[strcspn(line, "\n")] = '\0';
            if (is_blank_or_comment(line)) continue;
            char *fields[8];
            int fcnt = 0;
            char *p = line;
            while (fcnt < 8) {
                char *sep = strchr(p, '|');
                if (sep) *sep = '\0';
                trim(p);
                fields[fcnt++] = p;
                if (!sep) break;
                p = sep + 1;
            }
            if (fcnt < 3) continue;
            if (strcmp(fields[0], "USED") == 0 && fcnt >= 4) {
                const char *name = fields[1];
                long long day, cnt;
                if (!parse_ll(fields[2], &day)) continue;
                if (!parse_ll(fields[3], &cnt)) continue;
                if (day != cur_day) continue;
                Provider *pr = find_provider(providers, prov_cnt, name);
                if (pr) pr->used_today += cnt;
            } else if (strcmp(fields[0], "COOLDOWN") == 0 && fcnt >= 3) {
                const char *name = fields[1];
                long long until;
                if (!parse_ll(fields[2], &until)) continue;
                Provider *pr = find_provider(providers, prov_cnt, name);
                if (pr && until > pr->cooldown_until) pr->cooldown_until = until;
            }
        }
        fclose(sf);
    }

    /* select provider */
    Provider *best = NULL;
    for (int i = 0; i < prov_cnt; ++i) {
        Provider *pr = &providers[i];
        if (!pr->valid) continue;
        if (pr->daily_budget <= 0) continue;
        if (pr->used_today >= pr->daily_budget) continue;
        if (now < pr->cooldown_until) continue;
        if (!best || pr->order < best->order) {
            best = pr;
        }
    }

    if (best) {
        printf("PICK|%s\n", best->name);
        return 0;
    } else {
        printf("NONE|exhausted\n");
        return 1;
    }
}
