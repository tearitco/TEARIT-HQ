/* train_pet - one verb, one binary, no shared headers.
 * Moke-Pet's train.c reward-scoring pattern (mutaclsym/dox/
 * 01-cdda-architecture.md sec.4, recapped in egg-pals.txt sec.5): the
 * reward is deliberately trivial - doing the action always grants XP
 * as long as the pet has enough energy, no complex RL. Leveling up
 * raises HP/MP caps (full heal/restore) and, at
 * pieces/registry/skills/skill_pool.txt's unlock_level thresholds,
 * grants one new skill the pet doesn't already know.
 *
 * Usage: train_pet.+x <pet_piece_id>
 * Prints a one-line result message to stdout. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_LINES 48
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define TRAIN_ENERGY_COST 20
#define TRAIN_XP_GAIN 10
#define MAX_SKILLS_IN_POOL 64

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void append_ledger(const char *piece_id, const char *key, const char *value, const char *trigger) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/master_ledger.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] StateChange: %s %s %s | Trigger: %s\n", ts, piece_id, key, value, trigger);
    fclose(f);
}

typedef struct {
    char id[32];
    int unlock_level;
} SkillEntry;

static int load_skill_pool(SkillEntry *out, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/skills/skill_pool.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    int count = 0;
    while (count < max && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = '\0'; /* CRLF-safe */
        char *id = line;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        *p1 = '\0';
        int unlock = atoi(p1 + 1);
        /* Registry ids are genuinely short words despite gcc only being
         * able to statically prove the source no shorter than the line
         * buffer - same class of warning already suppressed narrowly in
         * generate_egg.c rather than widening out[].id to match. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(out[count].id, sizeof(out[count].id), "%s", id);
#pragma GCC diagnostic pop
        out[count].unlock_level = unlock;
        count++;
    }
    fclose(f);
    return count;
}

/* Exact-token membership check against a comma-separated skills field -
 * avoids a naive substring match wrongly matching "peck" inside a
 * longer future skill id. */
static int has_skill(const char *skills, const char *id) {
    char copy[MAX_LINE];
    snprintf(copy, sizeof(copy), "%s", skills);
    char *tok = strtok(copy, ",");
    while (tok) {
        if (strcmp(tok, id) == 0) return 1;
        tok = strtok(NULL, ",");
    }
    return 0;
}

/* Lowest-unlock_level skill the pet doesn't already know, gated by
 * current level - empty string if none available. */
static void next_unlocked_skill(const SkillEntry *pool, int count, const char *skills, int level, char *out, size_t out_sz) {
    out[0] = '\0';
    int best_unlock = -1;
    for (int i = 0; i < count; i++) {
        if (pool[i].unlock_level > level) continue;
        if (has_skill(skills, pool[i].id)) continue;
        if (best_unlock == -1 || pool[i].unlock_level < best_unlock) {
            best_unlock = pool[i].unlock_level;
            /* pool[i].id is bounded to sizeof(SkillEntry.id) (32) by
             * load_skill_pool's own snprintf above; gcc can't see that
             * invariant across the function boundary, so the same
             * narrow suppression applies here as there. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", pool[i].id);
#pragma GCC diagnostic pop
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pet_piece_id>\n", argv[0]);
        return 1;
    }
    const char *pet_id = argv[1];
    resolve_root();

    char state_path[PATH_BUF + 32];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, pet_id);

    FILE *f = fopen(state_path, "r");
    if (!f) { printf("Train failed: unknown pet %s.\n", pet_id); return 1; }

    char lines[MAX_LINES][MAX_LINE];
    int nlines = 0;
    int energy = 0, xp = 0, level = 1, hp_max = 0, mp_max = 0;
    char skills[MAX_LINE] = "";
    /* Pets hatched before this session's hatch_egg.c added level/xp
     * default to level=1/xp=0 above and, without this tracking, would
     * silently reset every training call since the missing keys never
     * get appended back - same class of migration tick_pets.c already
     * handles for its own fields. */
    int seen_energy = 0, seen_xp = 0, seen_level = 0, seen_hp_max = 0, seen_mp_max = 0, seen_skills = 0;
    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            const char *key = lines[nlines];
            if (strcmp(key, "energy") == 0) { energy = atoi(eq + 1); seen_energy = 1; }
            else if (strcmp(key, "xp") == 0) { xp = atoi(eq + 1); seen_xp = 1; }
            else if (strcmp(key, "level") == 0) { level = atoi(eq + 1); seen_level = 1; }
            else if (strcmp(key, "hp_max") == 0) { hp_max = atoi(eq + 1); seen_hp_max = 1; }
            else if (strcmp(key, "mp_max") == 0) { mp_max = atoi(eq + 1); seen_mp_max = 1; }
            else if (strcmp(key, "skills") == 0) {
                seen_skills = 1;
                char *val = eq + 1;
                val[strcspn(val, "\r\n")] = '\0'; /* CRLF-safe */
                snprintf(skills, sizeof(skills), "%s", val);
            }
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    if (energy < TRAIN_ENERGY_COST) {
        printf("%s is too tired to train (needs %d energy, has %d).\n", pet_id, TRAIN_ENERGY_COST, energy);
        return 1;
    }

    energy -= TRAIN_ENERGY_COST;
    xp += TRAIN_XP_GAIN;

    int leveled = 0;
    while (xp >= level * 20) {
        xp -= level * 20;
        level += 1;
        hp_max += 5;
        mp_max += 3;
        leveled = 1;
    }

    char new_skill[32] = "";
    if (leveled) {
        SkillEntry pool[MAX_SKILLS_IN_POOL];
        int pool_count = load_skill_pool(pool, MAX_SKILLS_IN_POOL);
        next_unlocked_skill(pool, pool_count, skills, level, new_skill, sizeof(new_skill));
        if (new_skill[0]) {
            char updated_skills[MAX_LINE];
            if (skills[0]) snprintf(updated_skills, sizeof(updated_skills), "%s,%s", skills, new_skill);
            else snprintf(updated_skills, sizeof(updated_skills), "%s", new_skill);
            snprintf(skills, sizeof(skills), "%s", updated_skills);
        }
    }

    f = fopen(state_path, "w");
    if (!f) { printf("Train failed: could not write state.\n"); return 1; }
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            const char *key = lines[i];
            int handled = 1;
            if (strcmp(key, "energy") == 0) fprintf(f, "energy=%d\n", energy);
            else if (strcmp(key, "xp") == 0) fprintf(f, "xp=%d\n", xp);
            else if (strcmp(key, "level") == 0) fprintf(f, "level=%d\n", level);
            else if (strcmp(key, "hp_max") == 0) fprintf(f, "hp_max=%d\n", hp_max);
            else if (strcmp(key, "mp_max") == 0) fprintf(f, "mp_max=%d\n", mp_max);
            else if (strcmp(key, "hp") == 0) fprintf(f, "hp=%d\n", leveled ? hp_max : atoi(eq + 1));
            else if (strcmp(key, "mp") == 0) fprintf(f, "mp=%d\n", leveled ? mp_max : atoi(eq + 1));
            else if (strcmp(key, "skills") == 0) fprintf(f, "skills=%s\n", skills);
            else handled = 0;
            *eq = '=';
            if (!handled) fputs(lines[i], f);
        } else {
            fputs(lines[i], f);
        }
    }
    if (!seen_energy) fprintf(f, "energy=%d\n", energy);
    if (!seen_xp) fprintf(f, "xp=%d\n", xp);
    if (!seen_level) fprintf(f, "level=%d\n", level);
    if (!seen_hp_max) fprintf(f, "hp_max=%d\n", hp_max);
    if (!seen_mp_max) fprintf(f, "mp_max=%d\n", mp_max);
    if (!seen_skills) fprintf(f, "skills=%s\n", skills);
    fclose(f);

    char valbuf[16];
    snprintf(valbuf, sizeof(valbuf), "%d", energy);
    append_ledger(pet_id, "energy", valbuf, "train_pet");
    snprintf(valbuf, sizeof(valbuf), "%d", xp);
    append_ledger(pet_id, "xp", valbuf, "train_pet");
    if (leveled) {
        snprintf(valbuf, sizeof(valbuf), "%d", level);
        append_ledger(pet_id, "level", valbuf, "train_pet");
        if (new_skill[0]) append_ledger(pet_id, "skills", skills, "train_pet");
    }

    if (leveled && new_skill[0]) printf("%s trained hard! Leveled up to %d and learned %s!\n", pet_id, level, new_skill);
    else if (leveled) printf("%s trained hard! Leveled up to %d!\n", pet_id, level);
    else printf("%s trained. XP: %d/%d Energy: %d\n", pet_id, xp, level * 20, energy);
    return 0;
}
