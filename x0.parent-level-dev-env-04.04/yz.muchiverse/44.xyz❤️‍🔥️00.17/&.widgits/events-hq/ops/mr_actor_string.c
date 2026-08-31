/* mr_actor_string - the three STRING-typed ACTOR event commands as one
 * real op, 2026-08-30 (Task 1/2b: Change Name, Change Nickname, Change
 * Profile).
 *
 * Usage: mr_actor_string.+x <entity_dir> <actor_id> <field> <value>
 *   field: name | nickname | profile
 *   value: the literal new string (spaces allowed; the registry TEMPLATE
 *          single-quotes it, so a value containing a ' must be avoided -
 *          same documented house-template limitation Change Gold etc.
 *          already have).
 *
 * Why a real op and not a PAL line: prisc+x's real ecall system
 * (SYS_GET_KV_INT/SYS_SET_KV_INT) is integer-only by name and
 * implementation (atoi/"%d\n") - there is NO string-set primitive, so a
 * string command genuinely needs a small C op (the "Shape 2" case the
 * registry's own header documents). This op lives entirely in
 * events-hq's ops dir - no shared-engine/prisc edits, no renderer.
 *
 * Effect: writes one real `key=value` STRING line into
 * <entity_dir>/actor_<actor_id>_stats.txt (the same flat-kv actor state
 * file Change HP/MP/EXP/etc. already use - mr_kv_set preserves every
 * other line, so a name/nickname/profile sits beside hp/level/class_id
 * losslessly), plus the usual messages.txt/history.txt audit.
 * Non-blocking: no popup, no clock coupling (matches SetImage on the
 * database side - an actor name is data, never a player-wait).
 */
#include "mr_clock_common.h"

static const char *normalize_field(const char *field) {
    if (strcmp(field, "name") == 0) return "name";
    if (strcmp(field, "nickname") == 0) return "nickname";
    if (strcmp(field, "profile") == 0) return "profile";
    return field;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: mr_actor_string.+x <entity_dir> <actor_id> <field> <value>\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *actor_id = argv[2];
    const char *field = normalize_field(argv[3]);
    const char *value = argv[4];

    char state_path[MR_PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/actor_%s_stats.txt", entity_dir, actor_id);
    mr_kv_set(state_path, field, value);

    mr_log(entity_dir, "ACTOR actor=%s %s=%s", actor_id, field, value);
    printf("ACTOR actor=%s %s=%s (state in %s)\n", actor_id, field, value, state_path);
    return 0;
}