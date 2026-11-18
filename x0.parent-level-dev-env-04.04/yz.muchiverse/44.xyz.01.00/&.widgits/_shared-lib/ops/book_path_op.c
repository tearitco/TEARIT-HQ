/* book_path_op - where is a book? (shell and harness front of the shared resolver pchq_book_path.c; owner 2026-11-18: no symlinks in code)
 *
 * Usage:  book_path_op <data root> <house> <book>        prints the folder of the book; exit 0 found, 1 not found (the printed path is then the instance's own would-be path)
 *         book_path_op list <data root> <house>          prints one book id per line (unsorted; "default" left out); exit 0
 * <data root> = the instance's own pc-hq root (the folder that holds pieces/); <house> = the house root (44.xyz.01.00). Rules and order: pchq_book_path.c header. */
#include "../pchq_book_path.c"
int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "list")) { static char names[128][BP_NAME]; int n = bp_list_books(argv[2], argv[3], names, 128); for (int i = 0; i < n; i++) puts(names[i]); return 0; }
    if (argc == 4) { char out[BP_PATH]; int ok = bp_book_dir(argv[1], argv[2], argv[3], out, sizeof out); puts(out); return ok ? 0 : 1; }
    fprintf(stderr, "usage: book_path_op <data root> <house> <book> | book_path_op list <data root> <house>\n"); return 2;
}
