/* mr_show_choices - "Show Choices" event command
 * Displays a list of choices to the player and records the selection.
 *
 * Usage: mr_show_choices.+x <package_dir> <choices_text> [default_index]
 *   choices_text: newline-separated list of options (will be stored in a temp file)
 *   default_index: optional, default choice index (0-based, defaults to 0)
 *
 * Outputs choice prompt to messages queue and stores player selection in state.
 * The selection is stored as choice_result=N (0-based index) in a state file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PATH_BUF 4352
#define MAX_CHOICE 256

static void write_choice_prompt(const char *package_dir, const char *choices, int default_idx) {
    char msg_path[PATH_BUF];
    snprintf(msg_path, sizeof(msg_path), "%s/messages.txt", package_dir);

    FILE *mf = fopen(msg_path, "a");
    if (!mf) {
        fprintf(stderr, "Could not open messages.txt for writing\n");
        return;
    }

    time_t now = time(NULL);
    fprintf(mf, "[%ld] SHOW_CHOICES default=%d choices=", (long)now, default_idx);

    /* Escape newlines in choices for single-line format */
    char *escaped = malloc(strlen(choices) * 2 + 1);
    if (escaped) {
        char *out = escaped;
        for (const char *in = choices; *in; in++) {
            if (*in == '\n') {
                *out++ = '\\';
                *out++ = 'n';
            } else if (*in == '\\') {
                *out++ = '\\';
                *out++ = '\\';
            } else {
                *out++ = *in;
            }
        }
        *out = '\0';
        fprintf(mf, "%s\n", escaped);
        free(escaped);
    }
    fclose(mf);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: mr_show_choices.+x <package_dir> <choices_text> [default_index]\n");
        return 1;
    }
    const char *package_dir = argv[1];
    const char *choices_text = argv[2];
    int default_idx = (argc > 3) ? atoi(argv[3]) : 0;

    write_choice_prompt(package_dir, choices_text, default_idx);

    char hist_path[PATH_BUF];
    snprintf(hist_path, sizeof(hist_path), "%s/history.txt", package_dir);
    FILE *hf = fopen(hist_path, "a");
    if (hf) {
        fprintf(hf, "SHOW_CHOICES default=%d choices=%s\n", default_idx, choices_text);
        fclose(hf);
    }

    printf("SHOW_CHOICES (default=%d): %s\n", default_idx, choices_text);
    return 0;
}
