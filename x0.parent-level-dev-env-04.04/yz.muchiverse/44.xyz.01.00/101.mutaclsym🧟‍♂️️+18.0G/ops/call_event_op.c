/* call_event_op - "Call Common Event" handler for the event system.
 * 2026-08-26, Task 2 of COMMON-EVENTS-MANAGER-HANDOFF.md.
 *
 * Registered as a custom op in prisc+x's default_op.txt.  The "Call
 * Common Event" command in the event registry compiles to a PAL line:
 *   OP call_event "<target_event_name>" "<trigger>"
 *
 * This op locates the house root by walking up from its own binary
 * location, finds common_events/<target>/event_pkg/pages/page_N/event.pal
 * whose condition.pdl trigger matches, and runs it via prisc+x.  The
 * MUCHI_CALLER_PKG environment variable (set by play_event.sh or the
 * calling entity's play path) is inherited by the child prisc+x process,
 * so the target common event can route relay writes back to the original
 * player entity.
 *
 * Usage (called by prisc+x exec_custom_op):
 *   call_event_op "<target_event_name>" ["<trigger>"]
 *
 * Semantics mirror play_event.sh's common-event block: ALL matching
 * common events run (not just highest-numbered — that's for page
 * variants within ONE named event).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH 1024

/* Walk up from 'from' looking for a directory named 'target_name'.
 * Returns absolute path via 'out' (including trailing '/'), or empty
 * string on failure. */
static void walk_up_find(const char *from, const char *target_name, char *out) {
    out[0] = '\0';
    char cur[MAX_PATH];
    snprintf(cur, sizeof(cur), "%s", from);
    while (cur[0] != '\0' && cur[0] != '/') {
        /* strip trailing slash */
        size_t sl = strlen(cur);
        if (sl > 1 && cur[sl - 1] == '/') cur[sl - 1] = '\0';
        /* try: cur/target_name */
        char probe[MAX_PATH];
        snprintf(probe, sizeof(probe), "%s/%s", cur, target_name);
        struct stat st;
        if (stat(probe, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(out, MAX_PATH, "%s/", probe);
            return;
        }
        /* go up */
        char *sl2 = strrchr(cur, '/');
        if (sl2) { *sl2 = '\0'; } else { break; }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: call_event_op <target_event_name> [<trigger>]\n");
        return 1;
    }

    const char *target_event = argv[1];
    const char *trigger = (argc >= 3 && argv[2][0]) ? argv[2] : "on-click";

    /* Resolve our own binary's real path so we can walk up to house root */
    char self_path[MAX_PATH];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len < 0) {
        fprintf(stderr, "call_event_op: cannot resolve /proc/self/exe\n");
        return 1;
    }
    self_path[len] = '\0';

    /* From our binary's dir, walk up looking for common_events/ */
    char *last_slash = strrchr(self_path, '/');
    if (last_slash) *last_slash = '\0'; /* strip binary name, now = ops/+x/ */

    char ce_root[MAX_PATH];
    walk_up_find(self_path, "common_events", ce_root);
    if (ce_root[0] == '\0') {
        fprintf(stderr, "call_event_op: could not locate common_events/ above %s\n", self_path);
        return 1;
    }

    /* Target event dir: common_events/<target_event>/ */
    char target_dir[MAX_PATH];
    snprintf(target_dir, sizeof(target_dir), "%s%s", ce_root, target_event);
    struct stat st;
    if (stat(target_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "call_event_op: common event '%s' not found at %s\n", target_event, target_dir);
        return 1;
    }

    /* Find prisc+x relative to common_events/ (same layout as play_event.sh) */
    char prisc_path[MAX_PATH];
    {
        /* ce_root = "<house_root>/common_events/" — strip to house_root */
        char house_root[MAX_PATH];
        snprintf(house_root, sizeof(house_root), "%s", ce_root);
        size_t hl = strlen(house_root);
        if (hl > 1 && house_root[hl - 1] == '/') house_root[hl - 1] = '\0';
        /* house_root is now <house_root>, find 101.mutaclsym* subdir */
        DIR *hd = opendir(house_root);
        if (!hd) {
            fprintf(stderr, "call_event_op: cannot open house root %s\n", house_root);
            return 1;
        }
        struct dirent *de;
        int found = 0;
        while ((de = readdir(hd)) != NULL) {
            if (strncmp(de->d_name, "101.mutaclsym", 13) == 0) {
                snprintf(prisc_path, sizeof(prisc_path), "%s/%s/system/prisc+x",
                         house_root, de->d_name);
                if (stat(prisc_path, &st) == 0) { found = 1; break; }
            }
        }
        closedir(hd);
        if (!found) {
            fprintf(stderr, "call_event_op: prisc+x not found under %s\n", house_root);
            return 1;
        }
    }

    /* Scan pages for matching trigger, keep highest-numbered match
     * (same semantics as play_event.sh). */
    char pages_dir[MAX_PATH];
    snprintf(pages_dir, sizeof(pages_dir), "%s/event_pkg/pages", target_dir);

    char best_page[MAX_PATH] = "";
    int best_num = -1;

    DIR *pd = opendir(pages_dir);
    if (pd) {
        struct dirent *de;
        while ((de = readdir(pd)) != NULL) {
            if (strncmp(de->d_name, "page_", 5) != 0) continue;
            int pnum = atoi(de->d_name + 5);
            if (pnum <= 0) continue;

            char cond_path[MAX_PATH];
            snprintf(cond_path, sizeof(cond_path), "%s/%s/condition.pdl",
                     pages_dir, de->d_name);
            FILE *cf = fopen(cond_path, "r");
            if (!cf) continue;

            char line[256];
            while (fgets(line, sizeof(line), cf)) {
                /* Look for "COND | trigger | <value>" */
                char *p = strstr(line, "trigger");
                if (p) {
                    /* Skip past "trigger" and the next '|' */
                    p += 7;
                    while (*p == ' ') p++;
                    if (*p == '|') p++;
                    while (*p == ' ') p++;
                    /* Read the trigger value */
                    char found_trig[64] = "";
                    int i = 0;
                    while (*p && *p != '\n' && *p != '\r' && i < 63) {
                        found_trig[i++] = *p++;
                    }
                    found_trig[i] = '\0';
                    /* Trim trailing spaces */
                    while (i > 0 && found_trig[i-1] == ' ') found_trig[--i] = '\0';

                    if (strcmp(found_trig, trigger) == 0 && pnum > best_num) {
                        best_num = pnum;
                        snprintf(best_page, sizeof(best_page), "%s/%s",
                                 pages_dir, de->d_name);
                    }
                }
            }
            fclose(cf);
        }
        closedir(pd);
    }

    if (best_page[0] == '\0') {
        fprintf(stderr, "call_event_op: no page matches trigger '%s' for %s\n",
                trigger, target_event);
        return 1;
    }

    /* Run event.pal via prisc+x.  MUCHI_CALLER_PKG is already in our
     * environment (inherited from the calling chain) and propagates to
     * the child process. */
    char pal_path[MAX_PATH];
    snprintf(pal_path, sizeof(pal_path), "%s/event.pal", best_page);

    if (stat(pal_path, &st) != 0) {
        fprintf(stderr, "call_event_op: no event.pal at %s\n", pal_path);
        return 1;
    }

    fprintf(stderr, "call_event_op: running %s (trigger=%s) via %s\n",
            target_event, trigger, best_page);

    /* Run from muta system dir (same convention as play_event.sh) */
    char muta_dir[MAX_PATH];
    snprintf(muta_dir, sizeof(muta_dir), "%s", prisc_path);
    char *msl = strrchr(muta_dir, '/');
    if (msl) *msl = '\0'; /* strip prisc+x binary name */

    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "cd '%s' && '%s' '%s'", muta_dir, prisc_path, pal_path);
    int rc = system(cmd);

    fprintf(stderr, "call_event_op: %s finished (rc=%d)\n", target_event, rc);
    return 0;
}
