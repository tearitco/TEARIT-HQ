/* verify_cell - W1 op. Reviews a generated cell with gemma3:1b on the MAC
 * LAN node (http://10.0.0.144:11434). Per PITFALL 69 the review is
 * describe-shaped, never a judge: the 1b lists observed issues, and ALL
 * verdicts stay deterministic (apply_cell normalizes verses regardless).
 *
 * Output: canon/work/<book>/<chapter>/cells/<cell_id>.review
 * Usage: verify_cell.+x <book> <chapter> <cell_id>
 * Env:   PRISC_PROJECT_ROOT or CWD. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 512)
#define MAX_TEXT 40000
#define API_1B "http://10.0.0.144:11434/api/generate"

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
    if (size <= 0 || size > MAX_TEXT) { fclose(f); return NULL; }
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

static const char *REVIEW_PROMPT =
    "You are a copy-editor for an ancient-style scripture. Below is a passage "
    "of numbered verses. Describe ONLY what you observe, issue by issue, "
    "checking these points:\n"
    "1. Does every verse line begin with a sequential number marker **N** ? "
    "List any missing or out-of-order numbers.\n"
    "2. Does the passage sound like biblical narrative (register, openers like "
    "And/Then/Therefore/For/Yet/Thus/Behold), not a screenplay or a modern novel? "
    "Quote any line that breaks the register.\n"
    "3. Is any verse unrealistically short (a single fragment)? List those verse numbers.\n"
    "4. Does the passage mention game mechanics (XP, levels, hit points, skills, "
    "quests, inventory)? List any.\n"
    "5. Are all facts consistent with the events in the source material (no new "
    "characters or outcomes invented)? List any possible contradictions.\n"
    "Give your answer as a plain list: '1) ... 2) ...' under one heading 'OBSERVED ISSUES'. "
    "If a check passes clean, say 'clean'. Do not rewrite the passage.\n\nPASSAGE:\n";

int main(int argc, char **argv) {
    resolve_root();
    if (argc < 4) { fprintf(stderr, "usage: verify_cell.+x <book> <chapter> <cell_id>\n"); return 1; }
    const char *book = argv[1], *chapter = argv[2], *cell_id = argv[3];

    char cells_dir[PATH_BUF];
    snprintf(cells_dir, sizeof(cells_dir), "%s/canon/work/%s/%s/cells", project_root, book, chapter);

    char txt_path[PATH_BUF];
    snprintf(txt_path, sizeof(txt_path), "%s/%s.txt", cells_dir, cell_id);
    char *passage = read_full_file(txt_path);
    if (!passage) { fprintf(stderr, "verify_cell: no cell text at %s\n", txt_path); return 1; }

    char req_path[PATH_BUF];
    snprintf(req_path, sizeof(req_path), "%s/%s.review.request.json", cells_dir, cell_id);
    FILE *rf = fopen(req_path, "w");
    if (!rf) return 1;
    fputs("{\"model\":\"gemma3:1b\",\"stream\":false,", rf);
    fputs("\"prompt\":\"", rf);
    json_escaped(rf, REVIEW_PROMPT);
    fputs("\",\"options\":{\"num_predict\":600,\"temperature\":0.3}}", rf);
    fclose(rf);

    char out_path[PATH_BUF], cmd[PATH_BUF * 3];
    snprintf(out_path, sizeof(out_path), "%s/%s.review.raw", cells_dir, cell_id);
    snprintf(cmd, sizeof(cmd),
        "curl -sS --max-time 300 -H 'Content-Type: application/json' '%s' -d @'%s' -o '%s'",
        API_1B, req_path, out_path);
    int rc = system(cmd);
    if (rc != 0) { fprintf(stderr, "verify_cell: curl to 1b failed (rc=%d)\n", rc); free(passage); return 1; }

    char *raw = read_full_file(out_path);
    if (!raw) { fprintf(stderr, "verify_cell: no 1b response body\n"); free(passage); return 1; }
    const char *mk = strstr(raw, "\"response\":\"");
    if (!mk) { fprintf(stderr, "verify_cell: no response field (MAC offline?): %s\n", out_path); free(raw); free(passage); return 1; }
    mk += strlen("\"response\":\"");

    char rev_path[PATH_BUF];
    snprintf(rev_path, sizeof(rev_path), "%s/%s.review", cells_dir, cell_id);
    FILE *tf = fopen(rev_path, "w");
    if (!tf) { free(raw); free(passage); return 1; }
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
        } else fputc(*p, tf);
    }
    fclose(tf);

    printf("verify_cell: %s reviewed by 1b -> %s.review\n", cell_id, cell_id);
    free(raw); free(passage);
    return 0;
}
