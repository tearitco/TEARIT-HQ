/* wsr_chart — Generate HTML charts from XOD performance data.
 *
 * Reads fitness data from tournament dirs or event bus and produces
 * an HTML file with bar charts, line charts, and metric tables.
 *
 * Usage: wsr_chart.+x <tournament_dir> [output.html]
 *   tournament_dir: directory containing agent*_fitness.txt files
 *   output.html:    output path (default: tournament_dir/chart.html)
 *
 * Also supports: wsr_chart.+x --events <interact_relay.txt> [output.html]
 *   Parses FITNESS|score|... lines for time-series data.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

#define MAX_AGENTS 16
#define MAX_LINE 4096
#define PATH_BUF (4096 + 512)
#define MAX_EVENTS 1024

static char project_root[PATH_BUF] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

struct agent_data {
    char id[32];
    char fitness[64];
    char portfolio[64];
    char cash[64];
    char price[64];
    char held[64];
    char chain[64];
    int used;
};

static char *read_file(const char *path, size_t *sz_out) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, sz, f);
    fclose(f);
    buf[n] = '\0';
    if (sz_out) *sz_out = n;
    return buf;
}

static void read_kv(const char *buf, const char *key, char *out, size_t out_sz) {
    char line[MAX_LINE];
    const char *p = buf;
    size_t key_len = strlen(key);
    while (p && *p) {
        const char *nl = strchr(p, '\n');
        size_t linelen = nl ? (size_t)(nl - p) : strlen(p);
        if (linelen < MAX_LINE - 1) {
            memcpy(line, p, linelen);
            line[linelen] = '\0';
        }
        char *eq = strchr(line, '=');
        if (eq && strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            strncpy(out, eq + 1, out_sz - 1);
            out[out_sz - 1] = '\0';
            return;
        }
        if (!nl) break;
        p = nl + 1;
    }
    out[0] = '\0';
}

static double kv_to_double(const char *val) {
    if (!val || !*val) return 0.0;
    return atof(val);
}

static void html_escape(char *out, size_t out_sz, const char *in) {
    const char *end = out + out_sz - 1;
    while (*in && out < end) {
        switch (*in) {
            case '<':  out += snprintf(out, end - out, "&lt;"); break;
            case '>':  out += snprintf(out, end - out, "&gt;"); break;
            case '&':  out += snprintf(out, end - out, "&amp;"); break;
            case '"':  out += snprintf(out, end - out, "&quot;"); break;
            default: *out++ = *in; break;
        }
        in++;
    }
    *out = '\0';
}

static int parse_agent_file(const char *path, struct agent_data *agent) {
    size_t sz;
    char *buf = read_file(path, &sz);
    if (!buf) return 0;
    read_kv(buf, "fitness", agent->fitness, sizeof(agent->fitness));
    read_kv(buf, "portfolio_value", agent->portfolio, sizeof(agent->portfolio));
    read_kv(buf, "cash", agent->cash, sizeof(agent->cash));
    read_kv(buf, "stock_price", agent->price, sizeof(agent->price));
    read_kv(buf, "shares_held", agent->held, sizeof(agent->held));
    free(buf);

    /* Extract agent ID from filename: agent1_fitness.txt -> 1 */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *end = strstr(base, "_fitness.txt");
    if (end) {
        ptrdiff_t len = end - base - 5; /* skip "agent" */
        if (len > 0 && len < 31) {
            memcpy(agent->id, base + 5, len);
            agent->id[len] = '\0';
        }
    }
    agent->used = 1;
    return 1;
}

int main(int argc, char **argv) {
    resolve_root();

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <tournament_dir> [output.html]\n", argv[0]);
        fprintf(stderr, "       %s --events <events_file> [output.html]\n", argv[0]);
        return 1;
    }

    char out_path[PATH_BUF];
    char title[256] = "XOD Agent Performance Dashboard";

    struct agent_data agents[MAX_AGENTS] = {0};
    int num_agents = 0;

    /* Time-series event data */
    struct {
        char agent_id[32];
        double fitness;
        char reason[256];
        char action[64];
        double conf;
        int has_data;
    } events[MAX_EVENTS];
    int num_events = 0;

    if (strcmp(argv[1], "--events") == 0 && argc >= 3) {
        /* Parse event bus for FITNESS and LLM_DECISION lines */
        size_t sz;
        char *buf = read_file(argv[2], &sz);
        if (!buf) {
            fprintf(stderr, "Cannot read: %s\n", argv[2]);
            return 1;
        }
        char line[MAX_LINE];
        const char *p = buf;
        snprintf(title, sizeof(title), "XOD Fitness Timeline");
        while (p && *p && num_events < MAX_EVENTS) {
            const char *nl = strchr(p, '\n');
            size_t linelen = nl ? (size_t)(nl - p) : strlen(p);
            if (linelen >= MAX_LINE - 1) linelen = MAX_LINE - 2;
            memcpy(line, p, linelen);
            line[linelen] = '\0';

            if (strncmp(line, "FITNESS|", 8) == 0) {
                char *f = line + 8;
                char *pipe = strchr(f, '|');
                if (pipe) {
                    *pipe = '\0';
                    events[num_events].fitness = atof(f);
                    events[num_events].has_data = 1;
                    num_events++;
                }
            }
            if (strncmp(line, "LLM_DECISION|", 13) == 0) {
                /* Format: LLM_DECISION|action|confidence|reason */
                if (num_events < MAX_EVENTS) {
                    char *d = line + 13;
                    char *p1 = strchr(d, '|');
                    if (p1) {
                        *p1 = '\0';
                        strncpy(events[num_events].action, d, 63);
                        events[num_events].action[63] = '\0';
                        d = p1 + 1;

                        char *p2 = strchr(d, '|');
                        if (p2) {
                            *p2 = '\0';
                            events[num_events].conf = atof(d);
                            d = p2 + 1;
                            strncpy(events[num_events].reason, d, 255);
                            events[num_events].reason[255] = '\0';
                        }
                    }
                    events[num_events].has_data = 1;
                    num_events++;
                }
            }
            if (!nl) break;
            p = nl + 1;
        }
        free(buf);
    } else {
        /* Read tournament dir for agent*_fitness.txt files */
        const char *tournament_dir = argv[1];
        snprintf(out_path, sizeof(out_path), "%s/chart.html", tournament_dir);

        DIR *dir = opendir(tournament_dir);
        if (!dir) {
            fprintf(stderr, "Cannot open directory: %s\n", tournament_dir);
            return 1;
        }

        struct dirent *de;
        while ((de = readdir(dir)) && num_agents < MAX_AGENTS) {
            if (strncmp(de->d_name, "agent", 5) == 0 &&
                strstr(de->d_name, "_fitness.txt")) {
                char full[PATH_BUF];
                snprintf(full, sizeof(full), "%s/%s", tournament_dir, de->d_name);
                parse_agent_file(full, &agents[num_agents]);
                num_agents++;
            }
        }
        closedir(dir);

        /* Also read current fitness.txt as "active" agent */
        char fitness_path[PATH_BUF];
        snprintf(fitness_path, sizeof(fitness_path), "%s/pieces/display/fitness.txt",
                 project_root[0] == '.' && project_root[1] == '\0' ? "." : project_root);
        size_t sz;
        char *buf = read_file(fitness_path, &sz);
        if (buf && num_agents < MAX_AGENTS) {
            struct agent_data *a = &agents[num_agents];
            strcpy(a->id, "current");
            read_kv(buf, "fitness", a->fitness, sizeof(a->fitness));
            read_kv(buf, "portfolio_value", a->portfolio, sizeof(a->portfolio));
            read_kv(buf, "cash", a->cash, sizeof(a->cash));
            read_kv(buf, "stock_price", a->price, sizeof(a->price));
            read_kv(buf, "shares_held", a->held, sizeof(a->held));
            a->used = 1;
            num_agents++;
        }
        if (buf) free(buf);
    }

    /* Determine output path */
    if (argc >= 3) {
        strncpy(out_path, argv[argc - 1], sizeof(out_path) - 1);
        out_path[sizeof(out_path) - 1] = '\0';
    } else if (argv[1][0] != '-' || strncmp(argv[1], "--events", 8) != 0) {
        if (strcmp(argv[1], "--events") == 0 && argc >= 3) {
            snprintf(out_path, sizeof(out_path), "%s/%s/chart.html",
                     project_root[0] == '.' && project_root[1] == '\0' ? "." : project_root,
                     "pieces/tournament_current");
        }
    }

    /* If no output path set, use a default */
    char final_out[PATH_BUF];
    if (argc >= 3 && argv[1][0] != '-') {
        strncpy(final_out, argv[2], sizeof(final_out) - 1);
    } else if (argc >= 4 && strcmp(argv[1], "--events") == 0) {
        strncpy(final_out, argv[3], sizeof(final_out) - 1);
    } else {
        snprintf(final_out, sizeof(final_out), "%s/xod_chart.html",
                 project_root[0] == '.' && project_root[1] == '\0' ? "." : project_root);
    }
    final_out[sizeof(final_out) - 1] = '\0';

    FILE *f = fopen(final_out, "w");
    if (!f) {
        fprintf(stderr, "Cannot write: %s\n", final_out);
        return 1;
    }

    /* Find max fitness for bar chart scaling */
    double max_fit = 0.001;
    for (int i = 0; i < num_agents; i++) {
        if (agents[i].used) {
            double fit = kv_to_double(agents[i].fitness);
            if (fit > max_fit) max_fit = fit;
        }
    }

    /* HTML header */
    fprintf(f, "<!DOCTYPE html>\n<html><head><meta charset='utf-8'>\n");
    fprintf(f, "<title>%s</title>\n", title);
    fprintf(f, "<style>\n");
    fprintf(f, "body { font-family: monospace; background: #1a1a2e; color: #e0e0e0; margin: 20px; }\n");
    fprintf(f, "h1 { color: #00d4ff; border-bottom: 2px solid #00d4ff; padding-bottom: 10px; }\n");
    fprintf(f, "h2 { color: #00ff88; margin-top: 30px; }\n");
    fprintf(f, "table { border-collapse: collapse; margin: 10px 0; }\n");
    fprintf(f, "th { background: #16213e; color: #00d4ff; padding: 8px 16px; text-align: left; }\n");
    fprintf(f, "td { padding: 6px 12px; border-bottom: 1px solid #0f3460; }\n");
    fprintf(f, "tr:hover { background: #0f3460; }\n");
    fprintf(f, ".bar-container { display: flex; align-items: center; margin: 4px 0; }\n");
    fprintf(f, ".bar-label { width: 80px; text-align: right; padding-right: 10px; }\n");
    fprintf(f, ".bar-track { flex: 1; background: #0f3460; height: 24px; }\n");
    fprintf(f, ".bar-fill { background: linear-gradient(90deg, #00ff88, #00d4ff); height: 100%%; }\n");
    fprintf(f, ".bar-value { width: 100px; padding-left: 10px; color: #ff6b6b; font-weight: bold; }\n");
    fprintf(f, ".sparkline { color: #00ff88; }\n");
    fprintf(f, "pre { background: #0d1b2a; padding: 10px; border-radius: 4px; overflow-x: auto; }\n");
    fprintf(f, ".grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }\n");
    fprintf(f, "</style></head><body>\n");

    fprintf(f, "<h1>%s</h1>\n", title);

    if (num_events > 0) {
        int display_count = 0;
        for (int i = 0; i < num_events; i++) {
            if (events[i].action[0]) display_count++;
        }

        fprintf(f, "<h2>Fitness Timeline</h2>\n");
        fprintf(f, "<table><tr><th>#</th><th>Action</th><th>Confidence</th><th>Fitness</th><th>Reason</th></tr>\n");
        int row = 0;
        for (int i = 0; i < num_events; i++) {
            if (!events[i].action[0]) continue;
            row++;
            char esc_reason[512];
            html_escape(esc_reason, sizeof(esc_reason), events[i].reason);
            fprintf(f, "<tr><td>%d</td><td>%s</td><td>%.2f</td><td>%.2f</td><td>%s</td></tr>\n",
                    row, events[i].action, events[i].conf, events[i].fitness, esc_reason);
        }
        fprintf(f, "</table>\n");

        /* SVG line chart for fitness over time */
        if (display_count > 1) {
            int chart_w = 600, chart_h = 200, margin = 40;
            double data_max = 0;
            int data_count = 0;
            for (int i = 0; i < num_events; i++) {
                if (events[i].action[0]) {
                    if (events[i].fitness > data_max) data_max = events[i].fitness;
                    data_count++;
                }
            }
            if (data_max == 0) data_max = 1;
            fprintf(f, "<h2>Fitness Trend</h2>\n");
            fprintf(f, "<svg width='%d' height='%d' style='background:#0d1b2a'>\n", chart_w, chart_h);
            /* Y axis */
            for (int y = 0; y <= 4; y++) {
                int yp = margin + chart_h - margin - (y * (chart_h - margin * 2) / 4);
                fprintf(f, "<line x1='%d' y1='%d' x2='%d' y2='%d' stroke='#0f3460'/>\n",
                        margin, yp, chart_w - margin, yp);
                fprintf(f, "<text x='%d' y='%d' fill='#888' font-size='10'>%.1f</text>\n",
                        margin - 5, yp + 3, data_max * (4.0 - y) / 4.0);
            }
            /* Data points */
            fprintf(f, "<polyline fill='none' stroke='#00ff88' stroke-width='2' points='");
            int idx = 0;
            for (int i = 0; i < num_events; i++) {
                if (events[i].action[0]) {
                    int xp = margin + (idx * (chart_w - margin * 2) / (data_count - 1 < 1 ? 1 : data_count - 1));
                    int yp = margin + chart_h - margin - (events[i].fitness / data_max * (chart_h - margin * 2));
                    fprintf(f, "%d,%d ", xp, yp);
                    idx++;
                }
            }
            fprintf(f, "'/>\n");
            idx = 0;
            for (int i = 0; i < num_events; i++) {
                if (events[i].action[0]) {
                    int xp = margin + (idx * (chart_w - margin * 2) / (data_count - 1 < 1 ? 1 : data_count - 1));
                    int yp = margin + chart_h - margin - (events[i].fitness / data_max * (chart_h - margin * 2));
                    fprintf(f, "<circle cx='%d' cy='%d' r='3' fill='#00d4ff'/>\n", xp, yp);
                    idx++;
                }
            }
            fprintf(f, "</svg>\n");
        }
    }

    if (num_agents > 0) {
        /* Bar chart: compare agents */
        fprintf(f, "<h2>Agent Fitness Comparison</h2>\n");
        for (int i = 0; i < num_agents; i++) {
            if (!agents[i].used) continue;
            double fit = kv_to_double(agents[i].fitness);
            int pct = (int)(fit / max_fit * 100);
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;

            char esc_id[64];
            html_escape(esc_id, sizeof(esc_id), agents[i].id);
            fprintf(f, "<div class='bar-container'>\n");
            fprintf(f, "  <div class='bar-label'>Agent %s</div>\n", esc_id);
            fprintf(f, "  <div class='bar-track'><div class='bar-fill' style='width: %d%%;'></div></div>\n", pct);
            fprintf(f, "  <div class='bar-value'>%.2f</div>\n", fit);
            fprintf(f, "</div>\n");
        }

        /* Detailed metrics table */
        fprintf(f, "<h2>Agent Metrics</h2>\n");
        fprintf(f, "<table>\n<tr><th>Agent</th><th>Fitness</th><th>Portfolio</th><th>Cash</th><th>Price</th><th>Held</th></tr>\n");
        for (int i = 0; i < num_agents; i++) {
            if (!agents[i].used) continue;
            char esc_id[64];
            html_escape(esc_id, sizeof(esc_id), agents[i].id);
            fprintf(f, "<tr><td>Agent %s</td><td>%.2f</td><td>%.2f</td><td>%.2f</td><td>%.2f</td><td>%s</td></tr>\n",
                    esc_id,
                    kv_to_double(agents[i].fitness),
                    kv_to_double(agents[i].portfolio),
                    kv_to_double(agents[i].cash),
                    kv_to_double(agents[i].price),
                    agents[i].held);
        }
        fprintf(f, "</table>\n");
    }

    if (num_agents == 0 && num_events == 0) {
        fprintf(f, "<p>No data found. Run a tournament first:</p>\n");
        fprintf(f, "<pre>  bash tournament_setup.sh\n");
        fprintf(f, "  bash run_xod_agent.sh &lt;session_dir&gt; survive gemma3:1b 10\n");
        fprintf(f, "  bash live_tournament_xod.sh</pre>\n");
    }

    fprintf(f, "</body></html>\n");
    fclose(f);

    printf("Chart written to: %s\n", final_out);
    printf("Agents: %d, Events: %d\n", num_agents, num_events);
    return 0;
}
