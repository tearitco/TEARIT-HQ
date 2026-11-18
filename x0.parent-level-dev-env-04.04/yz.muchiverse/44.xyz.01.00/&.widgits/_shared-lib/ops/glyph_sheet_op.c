/* glyph_sheet_op - which Chinese character does a sprite sheet get in the ASCII view? (harness front of ../glyph_sheet.c). Usage: glyph_sheet_op <house> <sheet name>   prints the character, exit 0; exit 1 when nothing is defined. */
#include "../glyph_sheet.c"
int main(int argc, char **argv) { if (argc < 3) { fprintf(stderr, "usage: glyph_sheet_op <house> <sheet>\n"); return 2; } char o[16]; gs_load(argv[1]); if (!gs_find(argv[2], o, sizeof o)) return 1; puts(o); return 0; }
