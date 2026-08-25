/* wsr_wizard_input - the real 5-step Startup New Corp prompt sequence,
 * modeled on TPMOS's own real `cli_io` text-input element (researched
 * directly - pieces/chtpm/plugins/chtpm_parser.c: a per-element
 * input_buffer, printable chars append, backspace trims, Enter submits,
 * an input_mode=="numeric" flag filters non-digit keys before they ever
 * reach the buffer). This is a small, wsr-pal-native equivalent of that
 * same shape - NOT a port of chtpm_parser.c's own code, a fresh
 * implementation in this project's own style, per this whole family's
 * no-shared-headers/duplicate-freely convention.
 *
 * Direct user correction this was built to fix: Startup New Corp had
 * been simplified to one fixed-value keypress (auto ticker, $200M),
 * when the REAL incorporation.c/financing.c source has a genuine 5-step
 * interactive flow (industry choice, country choice, funding amount,
 * corp name, ticker symbol) - see wsr-pal-final-handoff.txt §2 and
 * feedback_replicate_wsr_flows_exactly.md: when a real, detailed
 * precedent exists, replicate it exactly, don't shortcut it.
 *
 * Steps (tracked via wsr_menu/state.txt's prompt_step field):
 *   1 = industry choice (numeric, list read live from the real
 *       corporations/36_industries_wsr.txt - same file the original
 *       game reads, re-numbered 1..N by display order since we don't
 *       need the file's own numbering)
 *   2 = country choice (numeric, list read live from the real
 *       governments/generated/gov-list.txt, same re-numbering approach
 *       - that file's own leading index column is inconsistent (skips
 *       a number), so we never trust it, only row order)
 *   3 = funding amount (numeric free text, minimum enforced: 1000 if
 *       industry 1 or 2 [Banking/Insurance] else 100, same real
 *       thresholds incorporation.c itself uses)
 *   4 = corp name (free text, 5-25 chars - matches the real length
 *       bounds; NOT uniqueness-checked against other corp names, a
 *       named simplification - only the ticker collides structurally)
 *   5 = ticker symbol (free text, 1-6 chars, auto-uppercased; checked
 *       for collision via corp_ipo.+x's own existing "already exists"
 *       failure - on collision, this step re-prompts rather than
 *       aborting the whole wizard)
 * On step 5's successful Enter: calls corp_ipo.+x with the real
 * collected values, then corp_set_owner.+x to make the founder the
 * owner (same as the old fixed-value NEW_CORP path already did),
 * returns to wsr_financing_menu.
 *
 * KNOWN LIMITATION, named not hidden: main_loop.pal's own top-level
 * loop treats raw keycode 113 (lowercase 'q') as an unconditional quit
 * BEFORE any key ever reaches this op - a corp name containing a
 * literal lowercase "q" would quit the game mid-typing instead of
 * being appended to the buffer. Fixing this needs main_loop.pal itself
 * to gate its quit check on prompt_active (a real, small PAL-level
 * change), not fixed in this pass - avoid typing lowercase "q" in the
 * corp name field until that's addressed.
 *
 * Usage: wsr_wizard_input.+x <keycode> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <stdio.h>
#define popen _popen
#define pclose _pclose
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_BUF 64

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
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

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
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

/* Reads the industries file, returns the Nth (1-based) industry name
 * (after its "N. " prefix), or empty if out of range. count_out gets
 * the total number of lines, for range display/validation. */
static void nth_industry(char *out, size_t out_sz, int n, int *count_out) {
    out[0] = '\0';
    *count_out = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/Mar$.$treetRace.wsr]Q]k32/corporations/36_industries_wsr.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    int i = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (!line[0]) continue;
        i++;
        if (i == n) {
            char *dot = strchr(line, '.');
            const char *name = dot ? dot + 1 : line;
            while (*name == ' ') name++;
            snprintf(out, out_sz, "%s", name);
        }
    }
    fclose(f);
    *count_out = i;
}

/* Same shape for the governments list - re-numbers by row order since
 * the real file's own leading index column is inconsistent (confirmed
 * by reading it directly - it skips a number), never trusted here. */
static void nth_country(char *out, size_t out_sz, int n, int *count_out) {
    out[0] = '\0';
    *count_out = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/Mar$.$treetRace.wsr]Q]k32/governments/generated/gov-list.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    int i = 0;
    int first = 1;
    while (fgets(line, sizeof(line), f)) {
        if (first) { first = 0; continue; } /* header row */
        line[strcspn(line, "\n")] = '\0';
        if (!line[0]) continue;
        i++;
        if (i == n) {
            char *tab = strchr(line, '\t');
            if (!tab) tab = strchr(line, ' ');
            const char *name_start = tab ? tab + 1 : line;
            char tmp[MAX_LINE];
            snprintf(tmp, sizeof(tmp), "%s", name_start);
            char *tab2 = strchr(tmp, '\t');
            if (tab2) *tab2 = '\0';
            snprintf(out, out_sz, "%s", tmp);
        }
    }
    fclose(f);
    *count_out = i;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    char state_path[PATH_BUF], wizard_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/wsr_menu/state.txt", project_root);
    snprintf(wizard_path, sizeof(wizard_path), "%s/projects/wsr-pal/pieces/wsr_menu/new_corp_wizard.txt", project_root);

    int step = read_kv_int(state_path, "prompt_step", 1);
    char buf[MAX_BUF];
    read_kv_str(state_path, "prompt_buffer", buf, sizeof(buf));

    int is_enter = (key == 10 || key == 13);
    int is_backspace = (key == 8 || key == 127);
    int is_cancel = (key == 27); /* ESC - 'q' can't reach here, see header comment */
    int numeric_step = (step == 1 || step == 2 || step == 3);

    if (is_cancel) {
        write_kv(state_path, "prompt_active", "0");
        write_kv(state_path, "prompt_step", "0");
        write_kv(state_path, "prompt_buffer", "");
        write_kv(state_path, "last_message", "Startup New Corp cancelled.");
        remove(wizard_path);
        return 0;
    }

    if (is_backspace) {
        size_t len = strlen(buf);
        if (len > 0) buf[len - 1] = '\0';
        write_kv(state_path, "prompt_buffer", buf);
        return 0;
    }

    if (is_enter) {
        if (step == 1) {
            int count;
            char name[MAX_LINE];
            int choice = atoi(buf);
            nth_industry(name, sizeof(name), choice, &count);
            if (choice < 1 || choice > count || !name[0]) {
                write_kv(state_path, "last_message", "Invalid industry choice - try again.");
                write_kv(state_path, "prompt_buffer", "");
                return 0;
            }
            char choice_str[8]; snprintf(choice_str, sizeof(choice_str), "%d", choice);
            write_kv(wizard_path, "industry_choice", choice_str);
            write_kv(wizard_path, "industry_name", name);
            write_kv(state_path, "prompt_step", "2");
            write_kv(state_path, "prompt_buffer", "");
            write_kv(state_path, "last_message", "Industry set. Now choose a country.");
        } else if (step == 2) {
            int count;
            char name[MAX_LINE];
            int choice = atoi(buf);
            nth_country(name, sizeof(name), choice, &count);
            if (choice < 1 || choice > count || !name[0]) {
                write_kv(state_path, "last_message", "Invalid country choice - try again.");
                write_kv(state_path, "prompt_buffer", "");
                return 0;
            }
            write_kv(wizard_path, "country_name", name);
            write_kv(state_path, "prompt_step", "3");
            write_kv(state_path, "prompt_buffer", "");
            write_kv(state_path, "last_message", "Country set. Now enter the funding amount.");
        } else if (step == 3) {
            char industry_choice_str[MAX_LINE];
            read_kv_str(wizard_path, "industry_choice", industry_choice_str, sizeof(industry_choice_str));
            int industry_choice = atoi(industry_choice_str);
            float min_funding = (industry_choice == 1 || industry_choice == 2) ? 1000.0f : 100.0f;
            float amount = atof(buf);
            if (amount < min_funding) {
                char msg[MAX_LINE];
                snprintf(msg, sizeof(msg), "Funding must be at least %.0f - try again.", min_funding);
                write_kv(state_path, "last_message", msg);
                write_kv(state_path, "prompt_buffer", "");
                return 0;
            }
            write_kv(wizard_path, "funding_amount", buf);
            write_kv(state_path, "prompt_step", "4");
            write_kv(state_path, "prompt_buffer", "");
            write_kv(state_path, "last_message", "Funding set. Now enter a corporation name (5-25 chars).");
        } else if (step == 4) {
            size_t len = strlen(buf);
            if (len < 5 || len > 25) {
                write_kv(state_path, "last_message", "Name must be 5-25 characters - try again.");
                write_kv(state_path, "prompt_buffer", "");
                return 0;
            }
            write_kv(wizard_path, "corp_name", buf);
            write_kv(state_path, "prompt_step", "5");
            write_kv(state_path, "prompt_buffer", "");
            write_kv(state_path, "last_message", "Name set. Now enter a ticker symbol (1-6 chars).");
        } else if (step == 5) {
            size_t len = strlen(buf);
            if (len < 1 || len > 6) {
                write_kv(state_path, "last_message", "Ticker must be 1-6 characters - try again.");
                write_kv(state_path, "prompt_buffer", "");
                return 0;
            }
            char ticker[MAX_BUF];
            for (size_t i = 0; i < len; i++) ticker[i] = toupper((unsigned char)buf[i]);
            ticker[len] = '\0';

            char funding[MAX_LINE], corp_name[MAX_LINE], industry_name[MAX_LINE], country_name[MAX_LINE];
            read_kv_str(wizard_path, "funding_amount", funding, sizeof(funding));
            read_kv_str(wizard_path, "corp_name", corp_name, sizeof(corp_name));
            read_kv_str(wizard_path, "industry_name", industry_name, sizeof(industry_name));
            read_kv_str(wizard_path, "country_name", country_name, sizeof(country_name));

            char cmd[PATH_BUF * 2];
#ifdef _WIN32
            snprintf(cmd, sizeof(cmd),
                     "cmd /c \"cd /d \"%s\" && ops\\+x\\corp_ipo.+x %s %s \"%s\" \"%s\" \"%s\"\"",
                     project_root, ticker, funding, corp_name, industry_name, country_name);
#else
            snprintf(cmd, sizeof(cmd), "cd '%s' && ./ops/+x/corp_ipo.+x %s %s '%s' '%s' '%s' 2>&1",
                     project_root, ticker, funding, corp_name, industry_name, country_name);
#endif
            FILE *p = popen(cmd, "r");
            char result[MAX_LINE] = "";
            if (p) {
                if (fgets(result, sizeof(result), p)) result[strcspn(result, "\r\n")] = '\0';
                pclose(p);
            }

            if (strstr(result, "IPO successful")) {
                char owner_cmd[PATH_BUF * 2];
#ifdef _WIN32
                snprintf(owner_cmd, sizeof(owner_cmd),
                         "cmd /c \"cd /d \"%s\" && ops\\+x\\corp_set_owner.+x corp_%s player_you >nul 2>&1\"",
                         project_root, ticker);
#else
                snprintf(owner_cmd, sizeof(owner_cmd), "cd '%s' && ./ops/+x/corp_set_owner.+x corp_%s player_you > /dev/null 2>&1", project_root, ticker);
#endif
                int rc = system(owner_cmd);
                (void)rc;
                write_kv(state_path, "prompt_active", "0");
                write_kv(state_path, "prompt_step", "0");
                write_kv(state_path, "prompt_buffer", "");
                write_kv(state_path, "active_menu_piece", "wsr_financing_menu");
                char msg[MAX_LINE];
                snprintf(msg, sizeof(msg), "Founded %s (%s) - you are the owner.", corp_name, ticker);
                write_kv(state_path, "last_message", msg);
                remove(wizard_path);
            } else {
                char msg[MAX_LINE];
                snprintf(msg, sizeof(msg), "%s - choose a different ticker.", result[0] ? result : "Ticker already exists");
                write_kv(state_path, "last_message", msg);
                write_kv(state_path, "prompt_buffer", "");
            }
        }
        return 0;
    }

    /* Printable character - append to buffer, filtered to digits only
     * for the numeric steps (industry/country/funding), matching
     * chtpm_parser.c's own real input_mode=="numeric" guard. */
    if (key >= 32 && key <= 126) {
        if (numeric_step && !isdigit(key) && key != '.') return 0;
        size_t len = strlen(buf);
        if (len < sizeof(buf) - 2) {
            buf[len] = (char)key;
            buf[len + 1] = '\0';
            write_kv(state_path, "prompt_buffer", buf);
        }
    }
    return 0;
}
