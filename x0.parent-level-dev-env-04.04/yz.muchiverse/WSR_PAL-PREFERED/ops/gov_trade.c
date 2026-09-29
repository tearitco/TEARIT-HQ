/* gov_trade - executes pending_action (0=hold 1=raise_tax 2=cut_
 * spending), updates revenue/spending/net_operating/tax_rate_adj,
 * returns to idle. Real, simplified fiscal effects (not real
 * economics, same toy-but-real honesty standard as corp_trade.c and
 * this whole document family):
 *   raise_tax: tax_rate_adj += 1.0 (percentage point), revenue
 *              increases by 1% of current revenue per point raised
 *              (a simple, named-as-simplified elasticity assumption).
 *   cut_spending: spending -= 2% of current spending.
 * Self-contained, no shared headers.
 * Usage: gov_trade.+x <piece_id> */
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

    char action_str[MAX_FIELD], rev_str[MAX_FIELD], spend_str[MAX_FIELD], tax_str[MAX_FIELD];
    read_state_field(state_path, "pending_action", action_str, sizeof(action_str));
    read_state_field(state_path, "revenue", rev_str, sizeof(rev_str));
    read_state_field(state_path, "spending", spend_str, sizeof(spend_str));
    read_state_field(state_path, "tax_rate_adj", tax_str, sizeof(tax_str));

    int action = action_str[0] ? atoi(action_str) : 0;
    float revenue = rev_str[0] ? atof(rev_str) : 0;
    float spending = spend_str[0] ? atof(spend_str) : 0;
    float tax_rate_adj = tax_str[0] ? atof(tax_str) : 0;
    const char *label;

    if (action == 1) {
        tax_rate_adj += 1.0f;
        revenue += revenue * 0.01f;
        label = "raise";
    } else if (action == 2) {
        spending -= spending * 0.02f;
        label = "cut";
    } else {
        label = "hold";
    }

    float net_operating = revenue - spending;

    char new_rev[MAX_FIELD], new_spend[MAX_FIELD], new_net[MAX_FIELD], new_tax[MAX_FIELD];
    snprintf(new_rev, sizeof(new_rev), "%.2f", revenue);
    snprintf(new_spend, sizeof(new_spend), "%.2f", spending);
    snprintf(new_net, sizeof(new_net), "%.2f", net_operating);
    snprintf(new_tax, sizeof(new_tax), "%.1f", tax_rate_adj);
    write_state_field(state_path, "revenue", new_rev);
    write_state_field(state_path, "spending", new_spend);
    write_state_field(state_path, "net_operating", new_net);
    write_state_field(state_path, "tax_rate_adj", new_tax);
    write_state_field(state_path, "pending_action", "");
    write_state_field(state_path, "last_action", label);
    write_state_field(state_path, "current_state", "0");
    printf("policy: %s - revenue now %.2f, spending now %.2f, net_operating now %.2f\n", label, revenue, spending, net_operating);
    return 0;
}
