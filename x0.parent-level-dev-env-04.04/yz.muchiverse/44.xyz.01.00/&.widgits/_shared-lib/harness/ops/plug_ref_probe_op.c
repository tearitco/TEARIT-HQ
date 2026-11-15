/* plug_ref_probe_op - harness probe for &.widgits/_shared-lib/plug_ref.c (decision D74/D75, step 1: event pages read plug parameters). It #includes the real file.
 *   plug_ref_probe_op <plugdir> <arg>      prints  OK|<value>  (a reference, resolved)   LIT|<arg>  (not a reference: used as it is)   REFUSED|<reason>  (exit 3)
 * Build: gcc -std=gnu11 -O2 -Wall -o plug_ref_probe_op plug_ref_probe_op.c */
#include "../../plug_ref.c"
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: plug_ref_probe_op <plugdir> <arg>\n"); return 2; }
    char out[300]; int rc = plug_ref(argv[1], argv[2], out, sizeof out);
    if (rc == 0) { printf("LIT|%s\n", argv[2]); return 0; }
    if (rc > 0) { printf("OK|%s\n", out); return 0; }
    printf("REFUSED|%s\n", out); return 3;
}
