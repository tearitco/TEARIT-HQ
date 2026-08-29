/* mr_input_number - real "Input Number" event command op, 2026-08-29
 * (Task 1, gap #0 design: gap #0 design: blocking message op - pauses
 * the EXISTING game clock around the entry window per
 * CURSWORD-SOUL-VISION.md §4, resumes on pick/timeout).
 *
 * Usage: mr_input_number.+x <entity_dir> <house_root> <var_name> [digits]
 *   entity_dir:  the real playing entity's dir ($ENT from cmd_N.sh)
 *   house_root:  the real house root ($D from cmd_N.sh; needed only so
 *                the op can pause/resume the real game clock)
 *   var_name:    variable the entered number is stored into
 *   digits:      number of digits to enter, 1-8 (default 1)
 *
 * Entry is REAL digit-by-digit: each of `digits` rounds shows a
 * nav-accepting SHOW_PAGE popup of digit rows 0-9 (the same real
 * choice machinery mr_show_choices.c drives - no renderer change),
 * and the result file poll blocks the event like RPG Maker's entry
 * window does. The accumulated value is written to the real
 * variables.txt state (mr_state_root mirrors the compiler's
 * resolve_session_root, so a later Conditional Branch on this variable
 * reads the SAME file), plus an input_number_result.txt meta file and
 * the usual messages.txt/history.txt audit. Missing/0 digits fall back
 * to 1; a timeout on any round enters 0 for that round.
 */
#include "mr_clock_common.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: mr_input_number.+x <entity_dir> <house_root> <var_name> [digits]\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *house_root = argv[2];
    const char *var_name = argv[3];
    int digits = (argc > 4) ? atoi(argv[4]) : 1;
    if (digits < 1) digits = 1;
    if (digits > 8) digits = 8;

    char clock_ids[MR_MAX_CLOCKS][128];
    int n_clocks = mr_clock_ids(house_root, clock_ids, MR_MAX_CLOCKS);
    int paused[MR_MAX_CLOCKS];
    mr_clock_pause(house_root, clock_ids, n_clocks, paused);

    long value = 0;
    char rows[10][256] = { "0","1","2","3","4","5","6","7","8","9" };
    mr_log(entity_dir, "INPUT_NUMBER var=%s digits=%d popup opened", var_name, digits);
    for (int d = 0; d < digits; d++) {
        int picked = mr_popup_pick(entity_dir, rows, 10);
        int digit = (picked >= 0 && picked <= 9) ? picked : 0;
        value = value * 10 + digit;
    }
    mr_log(entity_dir, "INPUT_NUMBER var=%s digits=%d value=%ld", var_name, digits, value);

    char state_root[MR_PATH_BUF];
    mr_state_root(entity_dir, state_root, sizeof(state_root));
    char vars_path[MR_PATH_BUF];
    snprintf(vars_path, sizeof(vars_path), "%s/variables.txt", state_root);
    char valbuf[32];
    snprintf(valbuf, sizeof(valbuf), "%ld", value);
    mr_kv_set(vars_path, var_name, valbuf);

    char result_path[MR_PATH_BUF];
    snprintf(result_path, sizeof(result_path), "%s/input_number_result.txt", entity_dir);
    FILE *wf = fopen(result_path, "w");
    if (wf) {
        fprintf(wf, "input_number_value=%ld\n", value);
        fprintf(wf, "input_number_var=%s\n", var_name);
        fprintf(wf, "input_number_digits=%d\n", digits);
        fclose(wf);
    }

    mr_clock_resume(house_root, clock_ids, n_clocks, paused);
    printf("INPUT_NUMBER var=%s value=%ld (stored in %s)\n", var_name, value, vars_path);
    return 0;
}