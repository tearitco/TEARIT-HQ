/* corp_decide - the "deciding" state's op: reads decision_mode and
 * picks buy(1)/sell(2)/hold(0) into pending_action, then advances
 * current_state to "trading" (1 -> 2) - except decision_mode=4
 * (human), which parks in "deciding" until a human queues a choice,
 * same design as muchi-evo-pal's bot_choose.c.
 *
 * decision_mode=1 (weighted) is a REAL PORT, not a stub - see
 * fundamental_value() below, ported directly from wsr-pal's own
 * Mar$.$treetRace.wsr]Q]k32/analysis_loop.c's compute_new_stock_price()
 * (book value per share x market cap multiplier x leverage factor x
 * risk-bias factor, blended 70/30 with current price momentum) - same
 * formula, same risk_bias weights.txt convention wsr-pal already uses
 * for its 50 real corporations, just repurposed here as a genuine
 * BUY/SELL/HOLD trigger (fundamental > price -> buy, fundamental
 * meaningfully < price -> sell, else hold) instead of only a price-
 * movement estimate. This is the honest opposite of muchi-evo-pal's
 * weighted stub: real logic existed in wsr-pal to port, so it was
 * ported, not faked.
 *
 * decision_mode=3 (llm) reuses the exact same LAN gemma3:270m pattern
 * as muchi-evo-pal's bot_choose.c (connect_op.+x/json_parser.+x, a
 * one-shot ollama /api/chat call, no history/tools[]).
 *
 * Self-contained, no shared headers.
 * Usage: corp_decide.+x <piece_id> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
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

/* Ported verbatim in spirit from analysis_loop.c's compute_new_stock_
 * price() - book value/share x market-cap multiplier x leverage factor
 * x risk-bias factor, blended 70% fundamentals / 30% price momentum. */
static float fundamental_value(float book_value, float shares_outstanding,
                                float market_cap, float debt_to_equity,
                                int risk_bias, float current_price) {
    float book_value_per_share = (shares_outstanding > 0) ? book_value / shares_outstanding : 0.0f;

    float market_cap_multiplier = 1.0f;
    if (market_cap > 0) market_cap_multiplier = 1.0f + (log10f(market_cap) - 3) * 0.05f;

    float leverage_factor;
    if (debt_to_equity > 1.0f) leverage_factor = 1.0f - (debt_to_equity - 1.0f) * 0.1f;
    else leverage_factor = 1.0f + (0.5f - debt_to_equity) * 0.05f;

    float bias_factor = 0.5f + (risk_bias / 100.0f) * 1.0f;

    float value = book_value_per_share * market_cap_multiplier * leverage_factor * bias_factor;
    float momentum_factor = 0.7f;
    float new_price = (value * momentum_factor) + (current_price * (1.0f - momentum_factor));
    if (new_price < 0.1f) new_price = 0.1f;
    return new_price;
}

/* action codes: 0=hold 1=buy 2=sell */
static int decide_from_value(float fv, float price, float cash, int shares_held) {
    if (fv > price * 1.02f && cash >= price) return 1; /* undervalued enough, can afford -> buy */
    if (fv < price * 0.98f && shares_held > 0) return 2; /* overvalued, holding some -> sell */
    return 0;
}

static int preset_choice(float cash, float price, int shares_held) {
    if (cash >= price && shares_held < 5) return 1; /* buy */
    return 0; /* hold */
}

static int llm_choice(float cash, float price, int shares_held, float fv, int fallback) {
    char persona_path[PATH_BUF];
    snprintf(persona_path, sizeof(persona_path), "%s/pieces/registry/personas/decide_trade.txt", project_root);
    char *persona = read_full_file(persona_path);
    if (!persona) return fallback;

    char user_turn[MAX_FIELD];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(user_turn, sizeof(user_turn), "cash=%.2f stock_price=%.2f shares_held=%d fundamental_value_estimate=%.2f",
             cash, price, shares_held, fv);
#pragma GCC diagnostic pop

    char request_path[PATH_BUF], response_path[PATH_BUF];
#ifdef _WIN32
    {
        char tmpdir[MAX_PATH];
        DWORD n = GetTempPathA(sizeof(tmpdir), tmpdir);
        if (n == 0 || n >= sizeof(tmpdir)) snprintf(tmpdir, sizeof(tmpdir), ".");
        snprintf(request_path, sizeof(request_path), "%scorp_decide_request_%d.json", tmpdir, (int)getpid());
        snprintf(response_path, sizeof(response_path), "%scorp_decide_response_%d.json", tmpdir, (int)getpid());
    }
#else
    snprintf(request_path, sizeof(request_path), "/tmp/corp_decide_request_%d.json", getpid());
    snprintf(response_path, sizeof(response_path), "/tmp/corp_decide_response_%d.json", getpid());
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
        if (strstr(content, "buy") && cash >= price) result = 1;
        else if (strstr(content, "sell") && shares_held > 0) result = 2;
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

    char mode_str[MAX_FIELD], cash_str[MAX_FIELD], price_str[MAX_FIELD], shares_str[MAX_FIELD];
    char book_str[MAX_FIELD], out_str[MAX_FIELD], mcap_str[MAX_FIELD], dte_str[MAX_FIELD], risk_str[MAX_FIELD];
    char human_decision[MAX_FIELD];
    read_state_field(state_path, "decision_mode", mode_str, sizeof(mode_str));
    read_state_field(state_path, "cash", cash_str, sizeof(cash_str));
    read_state_field(state_path, "stock_price", price_str, sizeof(price_str));
    read_state_field(state_path, "shares_held", shares_str, sizeof(shares_str));
    read_state_field(state_path, "book_value", book_str, sizeof(book_str));
    read_state_field(state_path, "shares_outstanding", out_str, sizeof(out_str));
    read_state_field(state_path, "market_cap", mcap_str, sizeof(mcap_str));
    read_state_field(state_path, "debt_to_equity", dte_str, sizeof(dte_str));
    read_state_field(state_path, "risk_bias", risk_str, sizeof(risk_str));
    read_state_field(state_path, "human_decision", human_decision, sizeof(human_decision));

    int mode = mode_str[0] ? atoi(mode_str) : 0;
    float cash = cash_str[0] ? atof(cash_str) : 0;
    float price = price_str[0] ? atof(price_str) : 0;
    int shares_held = shares_str[0] ? atoi(shares_str) : 0;
    float book_value = book_str[0] ? atof(book_str) : 0;
    float shares_outstanding = out_str[0] ? atof(out_str) : 0;
    float market_cap = mcap_str[0] ? atof(mcap_str) : 0;
    float debt_to_equity = dte_str[0] ? atof(dte_str) : 0;
    int risk_bias = risk_str[0] ? atoi(risk_str) : 50;

    float fv = fundamental_value(book_value, shares_outstanding, market_cap, debt_to_equity, risk_bias, price);

    if (mode == 4) {
        if (!human_decision[0]) {
            printf("[human] waiting for input (run: button.sh choose buy|sell|hold) - fundamental_value=%.2f vs price=%.2f\n", fv, price);
            return 0;
        }
        int action = 0;
        if (strcmp(human_decision, "buy") == 0 && cash >= price) action = 1;
        else if (strcmp(human_decision, "sell") == 0 && shares_held > 0) action = 2;
        write_state_field(state_path, "human_decision", "");
        char action_str[8];
        snprintf(action_str, sizeof(action_str), "%d", action);
        write_state_field(state_path, "pending_action", action_str);
        write_state_field(state_path, "current_state", "2");
        printf("[human] chose %s\n", action == 1 ? "buy" : action == 2 ? "sell" : "hold");
        return 0;
    }

    int action;
    const char *mode_name;
    if (mode == 3) {
        action = llm_choice(cash, price, shares_held, fv, decide_from_value(fv, price, cash, shares_held));
        mode_name = "llm";
    } else if (mode == 2) {
        action = decide_from_value(fv, price, cash, shares_held); /* rl: STUB, falls back to weighted's real logic for now */
        mode_name = "rl(stub->weighted)";
    } else if (mode == 1) {
        action = decide_from_value(fv, price, cash, shares_held);
        mode_name = "weighted(real)";
    } else {
        action = preset_choice(cash, price, shares_held);
        mode_name = "preset";
    }

    char action_str[8];
    snprintf(action_str, sizeof(action_str), "%d", action);
    write_state_field(state_path, "pending_action", action_str);
    write_state_field(state_path, "current_state", "2");
    printf("[%s] chose %s (fundamental_value=%.2f price=%.2f cash=%.2f shares_held=%d)\n",
           mode_name, action == 1 ? "buy" : action == 2 ? "sell" : "hold", fv, price, cash, shares_held);
    return 0;
}
