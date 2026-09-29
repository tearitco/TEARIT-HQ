/* tk_focus_item - test-harness op. Finds a numbered menu item's CURRENT
 * number by its label text in a rendered current_frame.txt, then
 * focuses it by injecting one KEY_PRESSED digit-keystroke PER DIGIT of
 * that number (e.g. item 10 = keys '1' then '0'), matching the
 * chtpm_parser_pal.c digit-accumulation nav-jump (see
 * !.local-ux-testing-ai.txt Part 1, step 1, for the full why - this
 * encodes the exact rule that was gotten wrong twice while building the
 * bash-only version of this test: never assume a fixed item number,
 * and never send a combined "ASCII '0'+n" code for n>=10, always send
 * one real digit keystroke per character of the number).
 *
 * Prints the discovered item number to stdout on success (so a caller
 * can log/verify it), or nothing + exit 1 if the label isn't found in
 * the frame at all.
 *
 * Self-contained, no shared headers, no regex library dependency
 * (manual line-scan instead of PCRE, per this project's own convention
 * of small dependency-free ops).
 * Usage: tk_focus_item.+x <session_dir> <frame_file> <label_substring> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define PATH_BUF 4352
#define MAX_LINE 512

/* On a line containing `label`, look for the "] <digits> ." item-number
 * prefix that precedes it (format: "[ ] N. Label" or "[>] N. [Label]").
 * Returns the parsed number, or -1 if this line doesn't have that
 * shape. */
static int parse_item_number(const char *line) {
    const char *bracket = strchr(line, ']');
    if (!bracket) return -1;
    const char *p = bracket + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (!isdigit((unsigned char)*p)) return -1;
    int n = atoi(p);
    while (isdigit((unsigned char)*p)) p++;
    if (*p != '.') return -1;
    return n;
}

static void inject_key(const char *session_dir, int code) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/keyboard/history.txt", session_dir);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "[2026-07-26 00:00:00] KEY_PRESSED: %d\n", code);
        fclose(f);
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: tk_focus_item.+x <session_dir> <frame_file> <label_substring>\n");
        return 1;
    }
    const char *session_dir = argv[1];
    const char *frame_file = argv[2];
    const char *label = argv[3];

    FILE *f = fopen(frame_file, "r");
    if (!f) {
        fprintf(stderr, "tk_focus_item: cannot open %s\n", frame_file);
        return 1;
    }

    char line[MAX_LINE];
    int found = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, label)) {
            int n = parse_item_number(line);
            if (n > 0) { found = n; break; }
        }
    }
    fclose(f);

    if (found < 0) {
        fprintf(stderr, "tk_focus_item: label '%s' not found (or no valid item-number prefix) in %s\n", label, frame_file);
        return 1;
    }

    /* Inject one digit keystroke per character of the number - see this
     * file's own header comment for why this must NOT be a single
     * combined code for multi-digit items. */
    char numbuf[16];
    snprintf(numbuf, sizeof(numbuf), "%d", found);
    for (char *d = numbuf; *d; d++) {
        inject_key(session_dir, (int)(unsigned char)*d);
        usleep(80000);
    }

    printf("%d\n", found);
    return 0;
}
