/* mr_choice_switch - turn the answer of the last Show Choices into switches, so the branches of an event can be gated by the normal If command.
 * 2026-10-11. mr_show_choices writes <entity_dir>/choice_result.txt ("choice_result=<index>"); the registry If command only reads switches. This op sets
 * choice_<i> = 1 for the chosen index and 0 for every other index (0 .. n-1), in the given switches file (the same "name=value" file Control Switch writes).
 * Usage: mr_choice_switch.+x <entity_dir> <switches_file> <n_choices>
 * The importer (&.widgits/tsots-map-convert/tsots_event_ir.py) puts it right after every Show Choices, and its 402/403 branches are If blocks on choice_<n>. */
#include "mr_clock_common.h"

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "Usage: mr_choice_switch.+x <entity_dir> <switches_file> <n_choices>\n"); return 1; }
    char rp[MR_PATH_BUF], line[256]; int chosen = -1, n = atoi(argv[3]);
    snprintf(rp, sizeof(rp), "%s/choice_result.txt", argv[1]);
    FILE *f = fopen(rp, "r");
    if (f) { while (fgets(line, sizeof(line), f)) if (!strncmp(line, "choice_result=", 14)) chosen = atoi(line + 14); fclose(f); }
    if (n < 1) n = 1;
    if (n > 16) n = 16;
    for (int i = 0; i < n; i++) { char k[32]; snprintf(k, sizeof(k), "choice_%d", i); mr_kv_set(argv[2], k, i == chosen ? "1" : "0"); }
    mr_kv_set(argv[2], "choice_9", "0");          /* the cancel branch (403) key the importer uses */
    mr_log(argv[1], "CHOICE_SWITCH chosen=%d of %d", chosen, n);
    return 0;
}
