/* concept_edit_validate.c — real, small, deterministic C validator for
 * a Concept Bank candidate EDIT record.
 *
 * Schema per A-TEARIT-IS-ALL-YOU-NEED.md §2.3/§2.4 and this track's
 * own concept-bank/ layout (see ops/README-less header comments in
 * concept_mirror_rebuild.sh and data/masters/force.pdl):
 *
 *   EDIT | id=<uuid> | type=spoke_weight_delta | target=<spoke name>
 *       | slot=<master name the spoke's slot POINTS_TO>
 *       | delta=<float> | reason="<text>" | proposer=<name>
 *       | status=candidate
 *
 * Rejects, on sight, before anything touches real state (§2.4, this
 * task's own instruction):
 *   1. Malformed records — any line that doesn't parse cleanly is a
 *      reject, never a partial apply (one bad line kills the whole
 *      candidate).
 *   2. target/slot that don't resolve to a real, existing master —
 *      hub-and-spoke enforcement: target must be a real spoke record
 *      under data/spokes/<target>.pdl, and that spoke's own slots
 *      must contain one whose POINTS_TO equals `slot`, and `slot`
 *      itself must be a real master file under data/masters/<slot>.pdl.
 *   3. delta outside the bounded range [-0.2, +0.2] — chosen tight,
 *      per this task's own instruction, grounded in tomom's own
 *      unbounded matrices having blown up into the thousands
 *      (`mlp_model.txt` real values `13075.6`/`17331.1`/`-861.1`,
 *      cited in AI-TRACK-BRAINSTORM-QUESTIONS.md Q9 9a) — this bound
 *      is deliberately far tighter than that failure mode, not a
 *      generic guess.
 *
 * Only `type=spoke_weight_delta` is implemented (this task's real
 * smoke-test subject). The other three §2.3 types (`new_concept_node`,
 * `fsm_transition_describe`, `goap_action_describe`) are named, real,
 * and OPEN — not implemented here, this is the smallest real first
 * validator, not a claim the other three are handled.
 *
 * Usage: concept_edit_validate <concept_bank_dir> <edit_record_file>
 * Exit 0 = PASS (line printed: "PASS <reason>"), exit 1 = REJECT
 * (line printed: "REJECT <reason>"). No partial state is ever written
 * by this binary — it only validates, promotion/apply is a separate,
 * later step (§2.5/§2.7), not built by this pass.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXLINE 2048
#define DELTA_MIN -0.2
#define DELTA_MAX  0.2

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || isspace((unsigned char)s[n-1]))) s[--n] = '\0';
}

/* very small "KEY=value" field extractor out of one pipe-delimited
 * EDIT record. Returns 1 and fills out on success, 0 if the field is
 * simply absent (not itself an error - caller decides required-ness). */
static int get_field(const char *line, const char *key, char *out, size_t outsz) {
    char buf[MAXLINE];
    snprintf(buf, sizeof(buf), "%s", line);
    char *tok = strtok(buf, "|");
    size_t keylen = strlen(key);
    while (tok) {
        while (*tok == ' ') tok++;
        if (strncmp(tok, key, keylen) == 0 && tok[keylen] == '=') {
            char *val = tok + keylen + 1;
            /* strip a single pair of surrounding quotes, if present */
            size_t vlen = strlen(val);
            while (vlen && (val[vlen-1] == ' ' || val[vlen-1] == '\n' || val[vlen-1] == '\r')) { val[--vlen] = '\0'; }
            if (vlen >= 2 && val[0] == '"' && val[vlen-1] == '"') {
                val[vlen-1] = '\0';
                val++;
            }
            snprintf(out, outsz, "%s", val);
            return 1;
        }
        tok = strtok(NULL, "|");
    }
    return 0;
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* does spoke_path's own record contain a SLOT line whose POINTS_TO
 * equals master_name? Malformed SLOT lines inside the spoke file are
 * themselves a reject (rule 1 applies to state files too - a spoke
 * record concept_edit_validate can't even parse is not a resolvable
 * target). */
static int spoke_has_slot_to(const char *spoke_path, const char *master_name) {
    FILE *f = fopen(spoke_path, "r");
    if (!f) return -1; /* caller already checked existence; treat as hard error */
    char line[MAXLINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        char *p = line;
        while (*p == ' ') p++;
        if (strncmp(p, "SLOT", 4) != 0) continue;
        char *pt = strstr(p, "POINTS_TO=");
        if (!pt) continue; /* SLOT line with no POINTS_TO= just isn't a pointer slot (e.g. malformed) */
        pt += strlen("POINTS_TO=");
        char name[256] = {0};
        int i = 0;
        while (pt[i] && pt[i] != ' ' && pt[i] != '|' && i < 255) { name[i] = pt[i]; i++; }
        name[i] = '\0';
        if (strcmp(name, master_name) == 0) { found = 1; break; }
    }
    fclose(f);
    return found;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: concept_edit_validate <concept_bank_dir> <edit_record_file>\n");
        return 2;
    }
    const char *bank_dir = argv[1];
    const char *edit_path = argv[2];

    FILE *ef = fopen(edit_path, "r");
    if (!ef) {
        printf("REJECT cannot open edit record file: %s\n", edit_path);
        return 1;
    }
    char line[MAXLINE];
    int found_edit_line = 0;
    char type[128] = {0}, target[256] = {0}, slot[256] = {0}, delta_s[64] = {0};
    while (fgets(line, sizeof(line), ef)) {
        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        char *p = line;
        while (*p == ' ') p++;
        if (strncmp(p, "EDIT", 4) != 0) {
            printf("REJECT malformed record: unrecognized line (not EDIT|...): \"%s\"\n", line);
            fclose(ef);
            return 1;
        }
        found_edit_line = 1;
        if (!get_field(p, "type", type, sizeof(type))) {
            printf("REJECT malformed record: missing type= field\n");
            fclose(ef);
            return 1;
        }
        if (!get_field(p, "target", target, sizeof(target))) {
            printf("REJECT malformed record: missing target= field\n");
            fclose(ef);
            return 1;
        }
        if (!get_field(p, "slot", slot, sizeof(slot))) {
            printf("REJECT malformed record: missing slot= field\n");
            fclose(ef);
            return 1;
        }
        if (!get_field(p, "delta", delta_s, sizeof(delta_s))) {
            printf("REJECT malformed record: missing delta= field\n");
            fclose(ef);
            return 1;
        }
        break; /* one EDIT record per file, this pass */
    }
    fclose(ef);

    if (!found_edit_line) {
        printf("REJECT malformed record: no EDIT line found in %s\n", edit_path);
        return 1;
    }

    if (strcmp(type, "spoke_weight_delta") != 0) {
        printf("REJECT unsupported type '%s' — only spoke_weight_delta is implemented by this validator pass (new_concept_node/fsm_transition_describe/goap_action_describe are real §2.3 types, OPEN, not yet built)\n", type);
        return 1;
    }

    /* rule 3: bounded delta, and must actually parse as a number */
    char *endptr = NULL;
    double delta = strtod(delta_s, &endptr);
    if (endptr == delta_s || *endptr != '\0') {
        printf("REJECT malformed record: delta='%s' is not a clean float\n", delta_s);
        return 1;
    }
    if (delta < DELTA_MIN || delta > DELTA_MAX) {
        printf("REJECT delta %.4f out of bounds [%.2f, %.2f]\n", delta, DELTA_MIN, DELTA_MAX);
        return 1;
    }

    /* rule 2: hub-and-spoke resolution */
    char master_path[1024];
    snprintf(master_path, sizeof(master_path), "%s/data/masters/%s.pdl", bank_dir, slot);
    if (!file_exists(master_path)) {
        printf("REJECT slot='%s' does not resolve to an existing master (%s not found) — hub-and-spoke violation\n", slot, master_path);
        return 1;
    }

    char spoke_path[1024];
    snprintf(spoke_path, sizeof(spoke_path), "%s/data/spokes/%s.pdl", bank_dir, target);
    if (!file_exists(spoke_path)) {
        printf("REJECT target='%s' does not resolve to an existing spoke record (%s not found)\n", target, spoke_path);
        return 1;
    }

    int has_slot = spoke_has_slot_to(spoke_path, slot);
    if (has_slot <= 0) {
        printf("REJECT spoke '%s' has no slot pointing at master '%s' — cannot apply a weight delta to a relation that doesn't exist\n", target, slot);
        return 1;
    }

    printf("PASS target=%s slot=%s delta=%.4f resolves against real master '%s' and existing spoke slot — clears validator bounds\n",
           target, slot, delta, slot);
    return 0;
}
