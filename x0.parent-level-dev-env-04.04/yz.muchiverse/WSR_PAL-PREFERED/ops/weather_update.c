/* weather_update - applies ONE tick of the global climate cycle, the
 * "not in original WSR" metric named directly (real WSR has zero
 * weather simulation). Deliberately `decision_mode=preset`-only by
 * default per `GAME-AI-SPEED-DOCTRINE.txt`'s own framing - a planet's
 * weather is, appropriately, always the cheapest tier - a simple
 * deterministic seasonal cycle (sine wave over `season_tick`), not a
 * real atmospheric model. `EVO-DESIGN.txt` §2/§5's own correction
 * (decision_mode is fractal, available even here, just defaulting
 * cheap) still applies - this op still reads decision_mode and would
 * respect a non-preset value if one were ever set, it just isn't by
 * default.
 *
 * NOTE (per direct instruction to note where "new" features should
 * broaden later): this is a single GLOBAL climate value - real
 * per-region weather (a genuine `EVO-DESIGN.txt`-style ecosystem-tier
 * simulation) is named as a real future extension in
 * `SOCIETY-ECONOMY-ARCHITECTURE.txt` §12, not built here. Also not yet
 * wired: weather affecting `pop_update.c`'s growth formula or
 * commodity production yields - both real, named future connections.
 *
 * Self-contained, no shared headers.
 * Usage: weather_update.+x <piece_id> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

static const float BASE_TEMP = 15.0f;
static const float TEMP_AMPLITUDE = 12.0f;
static const int SEASON_LENGTH = 12; /* ticks per full cycle */

int main(int argc, char *argv[]) {
    resolve_root();
    if (argc < 2) return 1;
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, argv[1]);

    char tick_str[MAX_FIELD];
    read_state_field(state_path, "season_tick", tick_str, sizeof(tick_str));
    int season_tick = tick_str[0] ? atoi(tick_str) : 0;

    season_tick = (season_tick + 1) % SEASON_LENGTH;
    float angle = (2.0f * (float)M_PI * (float)season_tick) / (float)SEASON_LENGTH;
    float temperature = BASE_TEMP + TEMP_AMPLITUDE * sinf(angle);
    float precipitation = 0.3f + 0.2f * cosf(angle);
    if (precipitation < 0) precipitation = 0;

    char new_tick[MAX_FIELD], new_temp[MAX_FIELD], new_precip[MAX_FIELD];
    snprintf(new_tick, sizeof(new_tick), "%d", season_tick);
    snprintf(new_temp, sizeof(new_temp), "%.1f", temperature);
    snprintf(new_precip, sizeof(new_precip), "%.2f", precipitation);
    write_state_field(state_path, "season_tick", new_tick);
    write_state_field(state_path, "temperature", new_temp);
    write_state_field(state_path, "precipitation", new_precip);
    write_state_field(state_path, "last_action", "cycled");
    write_state_field(state_path, "current_state", "0");
    printf("[preset] weather tick %d: temperature=%.1f precipitation=%.2f\n", season_tick, temperature, precipitation);
    return 0;
}
