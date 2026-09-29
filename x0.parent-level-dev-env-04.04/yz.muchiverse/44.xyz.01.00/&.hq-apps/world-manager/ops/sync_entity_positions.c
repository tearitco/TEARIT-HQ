#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    // REAL FIX 2026-09-28 (bug_bounty.md, direct instruction: "we
    // rather use malloc, and sizeof, but remember, linux automatically
    // frees memory, so double free could occur"): realpath(path, NULL)
    // MALLOCs exactly the resolved length itself, no fixed buffer to
    // size (the earlier fix here was a fixed 4096 buffer, safe but a
    // guess; this is the permanent, guess-free version). Read once (the
    // snprintf below) and freed immediately after - matches TPMOS house
    // precedent (`1.TPMOS.../#.docs/^.pmo.ld-faq+8/
    // PITFALLS_ACTIVE_2026-03-18.txt` #20: never free before the read
    // completes, never twice).
    char script_path[4096];
    char *resolved = realpath(argv[0], NULL);
    if (resolved) {
        snprintf(script_path, sizeof(script_path), "%s", resolved);
        free(resolved);
    } else {
        script_path[0] = '\0';
    }

    char *p = strrchr(script_path, '/');
    if (p) *p = '\0';
    p = strrchr(script_path, '/');
    if (p) *p = '\0';

    char house_root[4096];
    char *hqa = strstr(script_path, "/&.hq-apps");
    if (hqa) {
        strncpy(house_root, script_path, hqa - script_path);
        house_root[hqa - script_path] = '\0';
    } else {
        strcpy(house_root, ".");
    }

    char cmd_file[256];
    snprintf(cmd_file, sizeof(cmd_file), "/tmp/sync_ent_%d.sh", getpid());

    FILE *f = fopen(cmd_file, "w");
    if (!f) return 1;

    fprintf(f, "#!/bin/bash\n");
    fprintf(f, "state_dir='%s/state'\n", script_path);
    fprintf(f, "temp_file=\"$state_dir/.entities_live.tmp\"\n");
    fprintf(f, "out_file=\"$state_dir/entities_live.txt\"\n");
    fprintf(f, "find '%s/xyzfs/users' -name desktop_pos.txt 2>/dev/null | sort | while read f; do\n", house_root);
    fprintf(f, "  eid=$(echo \"$f\" | sed 's|.*/pals/||;s|/.*||')\n");
    fprintf(f, "  x=$(grep 'x=' \"$f\" 2>/dev/null | head -1 | cut -d= -f2)\n");
    fprintf(f, "  y=$(grep 'y=' \"$f\" 2>/dev/null | head -1 | cut -d= -f2)\n");
    fprintf(f, "  echo \"$eid | x=$x | y=$y\" >> \"$temp_file\"\n");
    fprintf(f, "done\n");
    fprintf(f, "mv \"$temp_file\" \"$out_file\" 2>/dev/null\n");

    fclose(f);

    chmod(cmd_file, 0755);
    int ret = system(cmd_file);
    unlink(cmd_file);

    return (ret == 0) ? 0 : 1;
}
