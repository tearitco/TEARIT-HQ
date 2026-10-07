/* obs_feedback_write.c - write OBS/FEEDBACK records per A-TEARIT §2.1
 *
 * Usage: obs_feedback_write.+x <entity_dir> <OBS|FEEDBACK> [fields...]
 * Writes to <entity_dir>/obs_feedback_log.txt in the unified format
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>

#define PATH_BUF 4352

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: obs_feedback_write.+x <entity_dir> <OBS|FEEDBACK> [fields...]\n");
        return 1;
    }
    const char *entity_dir = argv[1];
    const char *record_type = argv[2];

    if (strcmp(record_type, "OBS") != 0 && strcmp(record_type, "FEEDBACK") != 0) {
        fprintf(stderr, "Type must be OBS or FEEDBACK\n");
        return 1;
    }

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/obs_feedback_log.txt", entity_dir);
    FILE *f = fopen(log_path, "a");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", log_path);
        return 1;
    }

    time_t now = time(NULL);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    uuid_t uuid;
    uuid_generate_random(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);

    fprintf(f, "[%s] %s | id=%s", timebuf, record_type, uuid_str);

    for (int i = 3; i < argc; i++) {
        fprintf(f, " | %s", argv[i]);
    }
    fprintf(f, "\n");
    fclose(f);

    printf("Written to %s\n", log_path);
    return 0;
}