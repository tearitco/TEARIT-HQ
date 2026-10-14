/* g1_make_dom.c - build a fetch.dom from HTML with the project's OWN
 * parser and serializer.
 *
 *   cc -std=gnu11 -I<pkg> -o /tmp/g1_make_dom tests/g1_make_dom.c nb_dom.c
 *   /tmp/g1_make_dom <in.html> <out.dom>
 *
 * Hand-writing the nb_serialize wire format is a TRAP, and it cost this
 * harness two full wrong theories. Two separate mistakes, both invisible
 * to a tree dump because the SHAPE was right:
 *
 *   1. A stray "U" after a childless <p>. A node with children is written
 *      as N-line, D, children, U; a childless node gets NEITHER. The
 *      extra U closed body's child block and re-parented the paragraph.
 *   2. THE ONE THAT ACTUALLY BROKE THE PAGE: the id in the id FIELD is
 *      not the id the runtime reads. nb_attr_get() parses the raw ATTRS
 *      BLOB, so an element with an empty blob is invisible to
 *      document.getElementById() no matter what its id field says. The
 *      serializer writes  N|p|t||6|tick 0|8|p id="t"  - the blob repeats
 *      the tag name and carries the attributes. A hand-written
 *      N|p|t||6|tick 0|0|  loads fine, prints fine, and silently does
 *      nothing at runtime.
 *
 * Only nb_serialize can be trusted to get the blob right.
 */
#include "nb_dom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s in.html out.dom\n", argv[0]); return 2; }
    FILE *in = fopen(argv[1], "rb");
    if (!in) { perror(argv[1]); return 2; }
    fseek(in, 0, SEEK_END);
    long n = ftell(in);
    fseek(in, 0, SEEK_SET);
    char *html = malloc((size_t)n + 1);
    if (!html) { fclose(in); return 2; }
    size_t got = fread(html, 1, (size_t)n, in);
    html[got] = 0;
    fclose(in);

    NbNode *root = nb_parse_html(html, got);
    free(html);
    if (!root) { fprintf(stderr, "parse failed: %s\n", argv[1]); return 1; }
    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return 2; }
    long wrote = nb_serialize(out, root);
    fclose(out);
    /* nb_serialize returns a NODE COUNT, not a byte count. Labelling it
     * "bytes" made a perfectly good 370-byte fixture read as "7 bytes"
     * and briefly looked like a total failure of the generator. */
    printf("wrote %s (%ld nodes)\n", argv[2], wrote);
    return 0;
}
