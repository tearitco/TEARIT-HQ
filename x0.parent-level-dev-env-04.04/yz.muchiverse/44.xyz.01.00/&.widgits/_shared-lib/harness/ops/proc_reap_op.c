/* proc_reap_op - runs the house's REAL reaper (kh_proc_reap_all from kh_proc_registry.h) against one house root and prints "signalled=<n>". For pal harness cases of the process ledger.
 * Usage: proc_reap_op <house_root>    Build (from this ops dir): gcc -std=gnu11 -Wall -I "../.." -o +x/proc_reap_op.+x proc_reap_op.c */
#define KH_PROC_REGISTRY_IMPL
#include "kh_proc_registry.h"
#include <stdio.h>
int main(int argc, char **argv) { if (argc < 2) return 2; printf("signalled=%d\n", kh_proc_reap_all(argv[1], 300, 0)); return 0; }
