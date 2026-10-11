/* ghost_chk - deterministic judge double for ghost_run.pal: ghost_chk <file> <needle> <verdict_file>. Writes "PASS ok" when <file> contains <needle>, else "FAIL missing". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc != 4) return 2;
    FILE *f = fopen(argv[1], "rb"); char b[65536]; size_t n = 0;
    if (f) { n = fread(b, 1, sizeof b - 1, f); fclose(f); }
    b[n] = 0;
    FILE *v = fopen(argv[3], "wb"); if (!v) return 3;
    fputs((f && strstr(b, argv[2])) ? "harness note\nPASS ok\n" : "harness note\nFAIL missing\n", v); fclose(v);
    return 0;
}
