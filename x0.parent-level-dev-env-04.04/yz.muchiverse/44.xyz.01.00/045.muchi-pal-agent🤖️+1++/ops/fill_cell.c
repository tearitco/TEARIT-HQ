/* fill_cell - W1 op. Generates one cell's verses with gemma3:270m on the
 * LINUX LAN node (http://10.0.0.187:11434, plain HTTP, no SSH), writing the
 * raw response to canon/work/<book>/<chapter>/cells/cell_NN.raw and the
 * extracted text to cell_NN.txt.
 *
 * Prompt shape (270m-friendly): system = condensed LT-PROMPT style rules;
 * user = SOURCE MATERIAL (referenced scene excerpts, truncated) + BEAT +
 * verse range + previous-cell last verse (continuity, S2) + output rule.
 *
 * No tools, no JSON-chat shape: /api/generate with system+prompt+options.
 * Uses curl -d @file (no shell interpolation of content). Self-contained.
 * Usage: fill_cell.+x <book> <chapter> <cell_id>
 * Env:   PRISC_PROJECT_ROOT or CWD. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 512)
#define MAX_LINE 65536
#define MAX_PROMPT 120000
#define MAX_SCENE_CHARS 3500
#define MAX_SCENES 4
#define API_270M "http://10.0.0.187:11434/api/generate"

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static char *read_full_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > MAX_PROMPT) { fclose(f); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)size, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void json_escaped(FILE *out, const char *s) {
    for (const char *p = s; *p; p++) {
        if (*p == '"') fputs("\\\"", out);
        else if (*p == '\\') fputs("\\\\", out);
        else if (*p == '\n') fputs("\\n", out);
        else if (*p == '\r') fputs("\\r", out);
        else if (*p == '\t') fputs("\\t", out);
        else if ((unsigned char)*p < 32) fprintf(out, "\\u%04x", *p);
        else fputc(*p, out);
    }
}

/* Next verse number after the given one, as a string. */
static const char *next_verse_num(const char *v) {
    static char buf[16];
    int n = atoi(v) + 1;
    if (n < 100) snprintf(buf, sizeof(buf), "%02d", n);
    else          snprintf(buf, sizeof(buf), "%d", n);
    return buf;
}

/* Find the last verse line of a cell txt: last line matching **N** ... */
static char *last_verse_of(const char *path) {    char *txt = read_full_file(path);
    if (!txt) return NULL;
    char *last = NULL;
    char *save = NULL;
    char *line = strtok_r(txt, "\n", &save);
    while (line) {
        const char *t = line;
        while (*t == ' ' || *t == '\t') t++;
        if (t[0] == '*' && t[1] == '*' && t[2] >= '0' && t[2] <= '9') last = line;
        line = strtok_r(NULL, "\n", &save);
    }
    if (last) return strdup(last);
    free(txt);
    return NULL;
}

static const char *LT_RULES =
    "You are writing a chapter of the Living Testament: a sacred historical "
    "account of a futuristic age, recorded as true history. Rules:\n"
    "1. Every line is a numbered verse: start each with **N** followed by the "
    "verse text. Numbers increase by one, continuous from the previous cell.\n"
    "2. Register: biblical narrative. Use openers like 'And it came to pass', "
    "'Now', 'Then', 'Therefore', 'For', 'Yet', 'Thus', 'After these things', "
    "'Behold'. Do not overuse thee/thou/thy/thine.\n"
    "3. Verse length varies: many verses are long, containing several actions, "
    "observations, dialogue and consequences; short verses are acceptable. "
    "Do not write one sentence per verse.\n"
    "4. Modern terminology is allowed (station, colony, spaceship, clone, "
    "laboratory) so the technology reads as magic and the magic as technology.\n"
    "5. Never mention game mechanics (XP, levels, stats, skills).\n"
    "6. The source material is canon. Expand, add atmosphere and dialogue, but "
    "never contradict major events.\n"
    "7. Dialogue resembles scripture: 'Then Astra said unto Lucky, ...'.\n"
    "8. Write only the verses requested. No headers, no notes, no commentary.\n";

static char *field(const char *spec, const char *key) {
    char needle[64];
    snprintf(needle, sizeof(needle), "%s|", key);
    const char *hit = strstr(spec, needle);
    if (!hit) return NULL;
    const char *start = hit + strlen(needle);
    const char *end = strchr(start, '\n');
    if (!end) end = start + strlen(start);
    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1);
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

int main(int argc, char **argv) {
    resolve_root();
    if (argc < 4) { fprintf(stderr, "usage: fill_cell.+x <book> <chapter> <cell_id>\n"); return 1; }
    const char *book = argv[1], *chapter = argv[2], *cell_id = argv[3];

    char cells_dir[PATH_BUF];
    snprintf(cells_dir, sizeof(cells_dir), "%s/canon/work/%s/%s/cells", project_root, book, chapter);

    char spec_path[PATH_BUF];
    snprintf(spec_path, sizeof(spec_path), "%s/%s.pdl", cells_dir, cell_id);
    char *spec = read_full_file(spec_path);
    if (!spec) { fprintf(stderr, "cell spec not found: %s\n", spec_path); return 1; }

    char *title = field(spec, "title");
    char *v_from = field(spec, "v_from");
    char *v_to = field(spec, "v_to");
    char *scenes = field(spec, "scenes");
    char *beat = field(spec, "beat");
    if (!title || !v_from || !v_to || !scenes || !beat) {
        fprintf(stderr, "malformed cell spec\n"); return 1;
    }

    /* Build user prompt: source excerpts + beat + range + continuity */
    char *user = malloc(MAX_PROMPT);
    size_t up = 0;
    up += (size_t)snprintf(user + up, MAX_PROMPT - up,
        "SOURCE MATERIAL (canon - you may expand but not contradict):\n\n");

    char *scenefree = strdup(scenes);
    char *save = NULL;
    char *sc = strtok_r(scenefree, ";", &save);
    int scount = 0;
    while (sc && scount < MAX_SCENES) {
        char sp[PATH_BUF];
        snprintf(sp, sizeof(sp), "%s/canon/source/%s/%s", project_root, book, sc);
        char *body = read_full_file(sp);
        if (body) {
            size_t blen = strlen(body);
            if (blen > MAX_SCENE_CHARS) blen = MAX_SCENE_CHARS;
            if (up + blen + 64 < MAX_PROMPT) {
                up += (size_t)snprintf(user + up, MAX_PROMPT - up, "-- %s --\n", sc);
                memcpy(user + up, body, blen);
                up += blen;
                user[up++] = '\n';
            }
            free(body);
        }
        sc = strtok_r(NULL, ";", &save);
        scount++;
    }
    free(scenefree);

    up += (size_t)snprintf(user + up, MAX_PROMPT - up, "\nBEAT (what this passage must cover):\n%s\n", beat);

    /* Continuity: last verse of previous cell (S2 rolling memory). */
    char *cont = NULL;
    if (strcmp(cell_id, "cell_01") != 0) {
        int idx = atoi(cell_id + 5);
        char prev[64];
        snprintf(prev, sizeof(prev), "cell_%02d", idx - 1);
        char pv_path[PATH_BUF];
        snprintf(pv_path, sizeof(pv_path), "%s/%s.txt", cells_dir, prev);
        char *last = last_verse_of(pv_path);
        if (last) {
            cont = last;
            up += (size_t)snprintf(user + up, MAX_PROMPT - up,
                "\nPREVIOUS CELL'S LAST VERSE (continue from here in tone and fact):\n%s\n", cont);
        }
    }

    up += (size_t)snprintf(user + up, MAX_PROMPT - up,
        "\nNow write verses %s through %s of this chapter. Format every verse as a "
        "bold verse number followed by the verse text, exactly like this:\n"
        "**%s** And it came to pass that the city slept beneath the dust.\n"
        "**%s** Now the man of great wealth walked its streets, and none knew him.\n"
        "Write ONLY those verse lines, in order, starting at verse %s. "
        "No headers, no notes, no commentary.\n",
        v_from, v_to, v_from, next_verse_num(v_from), v_from);

    /* Build ollama /api/generate request file */
    char req_path[PATH_BUF];
    snprintf(req_path, sizeof(req_path), "%s/%s.request.json", cells_dir, cell_id);
    FILE *rf = fopen(req_path, "w");
    if (!rf) { fprintf(stderr, "cannot write %s\n", req_path); return 1; }
    fputs("{\"model\":\"gemma3:270m\",\"stream\":false,", rf);
    fputs("\"system\":\"", rf); json_escaped(rf, LT_RULES); fputs("\",", rf);
    fputs("\"prompt\":\"", rf); json_escaped(rf, user); fputs("\",", rf);
    fputs("\"options\":{\"num_predict\":1400,\"temperature\":0.7}}", rf);
    fclose(rf);

    /* curl (no shell interpolation of content - @file) */
    char out_path[PATH_BUF], cmd[PATH_BUF * 3];
    snprintf(out_path, sizeof(out_path), "%s/%s.raw", cells_dir, cell_id);
    snprintf(cmd, sizeof(cmd),
        "curl -sS --max-time 300 -H 'Content-Type: application/json' '%s' -d @'%s' -o '%s'",
        API_270M, req_path, out_path);
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "fill_cell: curl to 270m failed (rc=%d)\n", rc);
        return 1;
    }

    /* Extract "response":"..." from raw JSON, unescape */
    char *raw = read_full_file(out_path);
    if (!raw) { fprintf(stderr, "fill_cell: no response body\n"); return 1; }
    const char *mk = strstr(raw, "\"response\":\"");
    if (!mk) { fprintf(stderr, "fill_cell: no response field in raw (node may be offline?): %s\n", out_path); free(raw); return 1; }
    mk += strlen("\"response\":\"");
    char txt_path[PATH_BUF];
    snprintf(txt_path, sizeof(txt_path), "%s/%s.txt", cells_dir, cell_id);
    FILE *tf = fopen(txt_path, "w");
    if (!tf) { fprintf(stderr, "cannot write %s\n", txt_path); return 1; }
    /* simple unescape */
    for (const char *p = mk; *p && *p != '"'; p++) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': fputc('\n', tf); break;
                case 't': fputc('\t', tf); break;
                case 'r': fputc('\r', tf); break;
                case '"': fputc('"', tf); break;
                case '\\': fputc('\\', tf); break;
                default: fputc('\\', tf); fputc(*p, tf);
            }
        } else {
            fputc(*p, tf);
        }
    }
    fclose(tf);

    printf("fill_cell: %s v%s..%s generated -> %s.txt (%d bytes raw)\n",
           cell_id, v_from, v_to, cell_id, (int)strlen(raw));

    free(title); free(v_from); free(v_to); free(scenes); free(beat);
    free(cont); free(user); free(spec); free(raw);
    return 0;
}
