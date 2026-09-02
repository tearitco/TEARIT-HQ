/* realestate_act - executes pending_action (1=collect_rent,
 * 2=lower_rent). Collecting rent pays into the OWNING company's real
 * `cash` field (per SOCIETY-ECONOMY-ARCHITECTURE.txt §12's own "route
 * through existing real financial fields, never a parallel shadow
 * system" principle, and the direct instruction that everything owned
 * by a company should track to it) - this is the concrete mechanism
 * behind the `owned_by` ownership chain actually meaning something,
 * not just a label.
 *
 * Self-contained, no shared headers.
 * Usage: realestate_act.+x <piece_id> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_state_field(const char *state_path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(state_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static void write_state_field(const char *state_path, const char *key, const char *value) {
    FILE *f = fopen(state_path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(state_path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

int main(int argc, char *argv[]) {
    resolve_root();
    if (argc < 2) return 1;
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, argv[1]);

    char action_str[MAX_FIELD], owned_by[MAX_FIELD], rent_str[MAX_FIELD];
    read_state_field(state_path, "pending_action", action_str, sizeof(action_str));
    read_state_field(state_path, "owned_by", owned_by, sizeof(owned_by));
    read_state_field(state_path, "rent_price", rent_str, sizeof(rent_str));
    int action = action_str[0] ? atoi(action_str) : 0;
    float rent_price = rent_str[0] ? atof(rent_str) : 0;
    const char *label;

    if (action == 1) {
        if (owned_by[0]) {
            char owner_state_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(owner_state_path, sizeof(owner_state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, owned_by);
#pragma GCC diagnostic pop
            char owner_cash_str[MAX_FIELD];
            read_state_field(owner_state_path, "cash", owner_cash_str, sizeof(owner_cash_str));
            float owner_cash = owner_cash_str[0] ? atof(owner_cash_str) : 0;
            owner_cash += rent_price;
            char new_owner_cash[MAX_FIELD];
            snprintf(new_owner_cash, sizeof(new_owner_cash), "%.2f", owner_cash);
            write_state_field(owner_state_path, "cash", new_owner_cash);
        }
        label = "collected_rent";
    } else if (action == 2) {
        rent_price *= 0.95f;
        char new_rent[MAX_FIELD];
        snprintf(new_rent, sizeof(new_rent), "%.2f", rent_price);
        write_state_field(state_path, "rent_price", new_rent);
        label = "lowered_rent";
    } else {
        label = "none";
    }

    write_state_field(state_path, "pending_action", "");
    write_state_field(state_path, "last_action", label);
    write_state_field(state_path, "current_state", "0");
    printf("realestate: %s (rent_price now %.2f, owned_by=%s)\n", label, rent_price, owned_by[0] ? owned_by : "(none)");
    return 0;
}
