/* args_echo_op - writes its arguments to a file, for the prisc_exec_args pal harness: proves how many arguments a pal `exec` really delivers.
 * Usage: args_echo_op <out file> <arg>...   -> the file gets one line  ARGS|<arg1>|<arg2>|...  (argc is also recorded: N=<count>)  Build: gcc -Wall -O2 -o +x/args_echo_op.+x args_echo_op.c */
#include <stdio.h>
int main(int argc, char **argv) {
    FILE *f; if (argc < 2 || !(f = fopen(argv[1], "w"))) return 2;
    fputs("ARGS", f); for (int i = 2; i < argc; i++) fprintf(f, "|%s", argv[i]); fprintf(f, "\nN=%d\n", argc - 2); fclose(f); return 0;
}
