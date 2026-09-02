/* gov_decide - the "deciding" state's op for governments: reads
 * decision_mode and picks raise_tax(1)/cut_spending(2)/hold(0) into
 * pending_action, then advances current_state to "trading" (policy
 * execution reuses the same 3rd-state slot corp pieces use, named
 * generically). Same 5-tier chassis as corp_decide.c, applied to
 * fiscal policy instead of stock trades.
 *
 * decision_mode=1 (weighted) is REAL logic, not a stub: deficit_ratio =
 * net_operating / gdp. If deficit_ratio < -0.02 (running a meaningful
 * deficit) AND debt_to_gdp > 50 (already heavily indebted), cut
 * spending (avoid raising taxes into an already-high debt burden). If
 * deficit_ratio < -0.02 and debt_to_gdp <= 50, raise taxes. Otherwise
 * hold. A real, if simplified, fiscal heuristic using this government's
 * own actual balance-sheet fields - not arbitrary.
 *
 * Self-contained, no shared headers.
 * Usage: gov_decide.+x <piece_id> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";
static const char *GEMMA_LAN_URL = "http://10.0.0.144:11434";
static const char *GEMMA_LAN_MODEL = "gemma3:270m";

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

static char *read_full_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (buf) {
        size_t n = fread(buf, 1, size, f);
        buf[n] = '\0';
    }
    fclose(f);
    return buf;
}

static void json_escaped(FILE *out, const char *s) {
    for (const char *p = s; *p; p++) {
        if (*p == '"') fputs("\\\"", out);
        else if (*p == '\\') fputs("\\\\", out);
        else if (*p == '\n') fputs("\\n", out);
        else fputc(*p, out);
    }
}

static char *run_capture(const char *cmd) {
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return NULL;
    char *buf = malloc(4096);
    size_t total = 0, n;
    while ((n = fread(buf + total, 1, 4095 - total, pipe)) > 0) {
        total += n;
        if (total >= 4095) break;
    }
    buf[total] = '\0';
    pclose(pipe);
    return buf;
}

/* action codes: 0=hold 1=raise_tax 2=cut_spending */
static int decide_from_fiscals(float net_operating, float gdp, float debt_to_gdp) {
    float deficit_ratio = (gdp > 0) ? net_operating / gdp : 0.0f;
    if (deficit_ratio < -0.02f) {
        return (debt_to_gdp > 50.0f) ? 2 : 1;
    }
    return 0;
}

static int preset_choice(float net_operating) {
    return (net_operating < 0) ? 1 : 0; /* naive: any deficit -> raise tax */
}

static int llm_choice(float revenue, float spending, float net_operating, float gdp, float debt_to_gdp, int fallback) {
    char persona_path[PATH_BUF];
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/decide_gov_policy.txt", project_root);
    char *persona = read_full_file(persona_path);
    if (!persona) return fallback;

    char user_turn[MAX_FIELD];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(user_turn, sizeof(user_turn), "revenue=%.2f spending=%.2f net_operating=%.2f gdp=%.2f debt_to_gdp=%.2f",
             revenue, spending, net_operating, gdp, debt_to_gdp);
#pragma GCC diagnostic pop

    char request_path[PATH_BUF], response_path[PATH_BUF];
#ifdef _WIN32
    {
        char tmpdir[MAX_PATH];
        DWORD n = GetTempPathA(sizeof(tmpdir), tmpdir);
        if (n == 0 || n >= sizeof(tmpdir)) snprintf(tmpdir, sizeof(tmpdir), ".");
        snprintf(request_path, sizeof(request_path), "%sgov_decide_request_%d.json", tmpdir, (int)getpid());
        snprintf(response_path, sizeof(response_path), "%sgov_decide_response_%d.json", tmpdir, (int)getpid());
    }
#else
    snprintf(request_path, sizeof(request_path), "/tmp/gov_decide_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/gov_decide_response_%d.json", getpid());
#endif

    FILE *pf = fopen(request_path, "w");
    if (!pf) { free(persona); return fallback; }
    fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":[{\"role\":\"system\",\"content\":\"", GEMMA_LAN_MODEL);
    json_escaped(pf, persona);
    fputs("\"},{\"role\":\"user\",\"content\":\"", pf);
    json_escaped(pf, user_turn);
    fputs("\"}]}", pf);
    fclose(pf);
    free(persona);

    char full_url[MAX_FIELD];
    snprintf(full_url, sizeof(full_url), "%s/api/chat", GEMMA_LAN_URL);

    char connect_cmd[PATH_BUF * 3];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(connect_cmd, sizeof(connect_cmd), "'%s/ops/+x/connect_op.+x' '%s' '%s' '%s'",
             project_root, full_url, request_path, response_path);
#pragma GCC diagnostic pop
    int rc = system(connect_cmd);
    remove(request_path);
    if (rc != 0) { remove(response_path); return fallback; }

    char json_parser_cmd[PATH_BUF * 2];
    snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'message.content'", project_root, response_path);
    char *content = run_capture(json_parser_cmd);
    remove(response_path);

    int result = fallback;
    if (content) {
        for (char *p = content; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strstr(content, "raise")) result = 1;
        else if (strstr(content, "cut")) result = 2;
        else if (strstr(content, "hold")) result = 0;
        free(content);
    }
    return result;
}

int main(int argc, char *argv[]) {
    resolve_root();
    if (argc < 2) return 1;
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, argv[1]);

    char mode_str[MAX_FIELD], rev_str[MAX_FIELD], spend_str[MAX_FIELD], net_str[MAX_FIELD];
    char gdp_str[MAX_FIELD], dtg_str[MAX_FIELD], human_decision[MAX_FIELD];
    read_state_field(state_path, "decision_mode", mode_str, sizeof(mode_str));
    read_state_field(state_path, "revenue", rev_str, sizeof(rev_str));
    read_state_field(state_path, "spending", spend_str, sizeof(spend_str));
    read_state_field(state_path, "net_operating", net_str, sizeof(net_str));
    read_state_field(state_path, "gdp", gdp_str, sizeof(gdp_str));
    read_state_field(state_path, "debt_to_gdp", dtg_str, sizeof(dtg_str));
    read_state_field(state_path, "human_decision", human_decision, sizeof(human_decision));

    int mode = mode_str[0] ? atoi(mode_str) : 0;
    float revenue = rev_str[0] ? atof(rev_str) : 0;
    float spending = spend_str[0] ? atof(spend_str) : 0;
    float net_operating = net_str[0] ? atof(net_str) : 0;
    float gdp = gdp_str[0] ? atof(gdp_str) : 0;
    float debt_to_gdp = dtg_str[0] ? atof(dtg_str) : 0;

    if (mode == 4) {
        if (!human_decision[0]) {
            printf("[human] waiting for input (run: button.sh choose raise|cut|hold) - net_operating=%.2f debt_to_gdp=%.2f\n", net_operating, debt_to_gdp);
            return 0;
        }
        int action = 0;
        if (strcmp(human_decision, "raise") == 0) action = 1;
        else if (strcmp(human_decision, "cut") == 0) action = 2;
        write_state_field(state_path, "human_decision", "");
        char action_str[8];
        snprintf(action_str, sizeof(action_str), "%d", action);
        write_state_field(state_path, "pending_action", action_str);
        write_state_field(state_path, "current_state", "2");
        printf("[human] chose %s\n", action == 1 ? "raise" : action == 2 ? "cut" : "hold");
        return 0;
    }

    int action;
    const char *mode_name;
    if (mode == 3) {
        action = llm_choice(revenue, spending, net_operating, gdp, debt_to_gdp, decide_from_fiscals(net_operating, gdp, debt_to_gdp));
        mode_name = "llm";
    } else if (mode == 2) {
        action = decide_from_fiscals(net_operating, gdp, debt_to_gdp); /* rl: STUB, falls back to weighted's real logic */
        mode_name = "rl(stub->weighted)";
    } else if (mode == 1) {
        action = decide_from_fiscals(net_operating, gdp, debt_to_gdp);
        mode_name = "weighted(real)";
    } else {
        action = preset_choice(net_operating);
        mode_name = "preset";
    }

    char action_str[8];
    snprintf(action_str, sizeof(action_str), "%d", action);
    write_state_field(state_path, "pending_action", action_str);
    write_state_field(state_path, "current_state", "2");
    printf("[%s] chose %s (net_operating=%.2f gdp=%.2f debt_to_gdp=%.2f)\n",
           mode_name, action == 1 ? "raise" : action == 2 ? "cut" : "hold", net_operating, gdp, debt_to_gdp);
    return 0;
}
