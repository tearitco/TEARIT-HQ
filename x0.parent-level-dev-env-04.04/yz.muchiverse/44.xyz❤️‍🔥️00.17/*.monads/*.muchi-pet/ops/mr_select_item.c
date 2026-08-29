/* mr_select_item - real "Select Item" event command op, 2026-08-29
 * (Task 1, gap #0 design: blocking message op - pauses the EXISTING
 * game clock around the pick window per CURSWORD-SOUL-VISION.md §4,
 * resumes on pick/timeout).
 *
 * Usage: mr_select_item.+x <entity_dir> <house_root> <var_name>
 *   entity_dir:  the real playing entity's dir ($ENT from cmd_N.sh)
 *   house_root:  the real house root ($D from cmd_N.sh - for the clock
 *                pause/resume)
 *   var_name:    variable the picked item's index is stored into
 *
 * The choices come from the entity's REAL state file inventory.txt
 * (key=value lines - today only qolq=, future items just add lines and
 * appear automatically). Each item becomes one nav-accepting SHOW_PAGE
 * row; the pick blocks the event exactly like RPG Maker's item-select
 * window, then stores the picked INDEX into the real variables.txt
 * state (mr_state_root mirrors the compiler's resolve_session_root) and
 * the picked KEY into .select_item_result.txt. Empty inventory stores
 * -1 without popping a window (no rows to show). Same real popup +
 * result-file machinery mr_show_choices.c drives.
 */
#include "mr_clock_common.h"

#define MAX_ITEMS 64

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: mr_select_item.+x <entity_dir> <house_root> <var_name>\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *house_root = argv[2];
    const char *var_name = argv[3];

    char inv_path[MR_PATH_BUF];
    snprintf(inv_path, sizeof(inv_path), "%s/inventory.txt", entity_dir);
    char rows[MAX_ITEMS][256];
    int n_rows = 0;
    FILE *ifp = fopen(inv_path, "r");
    if (ifp) {
        char line[256];
        while (n_rows < MAX_ITEMS && fgets(line, sizeof(line), ifp)) {
            char key[128] = "";
            if (sscanf(line, "%127[^=]=", key) == 1 && key[0]) {
                key[strcspn(key, "\r\n")] = '\0';
                snprintf(rows[n_rows], sizeof(rows[0]), "%s", key);
                n_rows++;
            }
        }
        fclose(ifp);
    }

    int picked = -1;
    if (n_rows <= 0) {
        mr_log(entity_dir, "SELECT_ITEM var=%s inventory empty (no items to show)", var_name);
        printf("SELECT_ITEM var=%s NO_ITEMS\n", var_name);
        return 0;
    }

    char clock_ids[MR_MAX_CLOCKS][128];
    int n_clocks = mr_clock_ids(house_root, clock_ids, MR_MAX_CLOCKS);
    int paused[MR_MAX_CLOCKS];
    mr_clock_pause(house_root, clock_ids, n_clocks, paused);

    mr_log(entity_dir, "SELECT_ITEM var=%s popup opened, items=%d", var_name, n_rows);
    picked = mr_popup_pick(entity_dir, rows, n_rows);

    char state_root[MR_PATH_BUF];
    mr_state_root(entity_dir, state_root, sizeof(state_root));
    char valbuf[32];
    int idx = (picked >= 0 && picked < n_rows) ? picked : 0;
    snprintf(valbuf, sizeof(valbuf), "%d", idx);

    char vars_path[MR_PATH_BUF];
    snprintf(vars_path, sizeof(vars_path), "%s/variables.txt", state_root);
    mr_kv_set(vars_path, var_name, valbuf);

    char result_path[MR_PATH_BUF];
    snprintf(result_path, sizeof(result_path), "%s/.select_item_result.txt", entity_dir);
    FILE *wf = fopen(result_path, "w");
    if (wf) {
        fprintf(wf, "select_item_value=%d\n", idx);
        fprintf(wf, "select_item_key=%s\n", rows[idx]);
        fprintf(wf, "select_item_var=%s\n", var_name);
        fclose(wf);
    }

    const char *timed_out = (picked < 0) ? " (TIMED OUT, used first item)" : "";
    mr_log(entity_dir, "SELECT_ITEM var=%s item_idx=%d item_key=%s%s", var_name, idx, rows[idx], timed_out);
    mr_clock_resume(house_root, clock_ids, n_clocks, paused);

    printf("SELECT_ITEM var=%s item_idx=%d item_key=%s%s\n", var_name, idx, rows[idx], timed_out);
    return 0;
}