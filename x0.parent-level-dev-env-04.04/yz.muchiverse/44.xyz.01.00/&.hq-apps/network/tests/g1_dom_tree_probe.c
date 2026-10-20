/* g1_dom_tree_probe.c - print the tree nb_dom_load() actually builds.
 *
 * THE FIRST THING TO RUN NEXT TIME. Everything the G1 harness learned the
 * slow way - five wip commits, four wrong theories - is answerable by
 * linking nb_dom.c and walking the tree: no worker, no browser, no X, no
 * protocol, one compile.
 *
 *   cc -std=gnu11 -I<pkg> -o /tmp/g1_dom_tree tests/g1_dom_tree_probe.c nb_dom.c
 *   /tmp/g1_dom_tree tests/fixtures/g1/timer.dom
 *
 * It is what proved the loader was innocent. The fixture is valid, the
 * field order matches serialize_rec, and load_node assigns n->text - so
 * the fault is downstream, in the worker's render path, not in the input.
 *
 * It also caught a bug in my own fixture: a stray U after each childless
 * <p> closed body's child block early and made the second paragraph a
 * SIBLING of <body>. Printing the tree is how you see that; reading the
 * fixture text is not.
 */
#include "nb_dom.h"
#include <stdio.h>
static void walk(const NbNode *n, int d) {
    if (!n) return;
    for (int i = 0; i < d; i++) printf("  ");
    printf("<%s> text=[%s]\n", n->tag ? n->tag : "(null)",
           n->text ? n->text : "(null)");
    for (const NbNode *c = n->first_child; c; c = c->next_sibling) walk(c, d + 1);
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("cannot open %s\n", argv[1]); return 2; }
    NbNode *root = nb_dom_load(f);
    fclose(f);
    if (!root) { printf("nb_dom_load -> NULL\n"); return 1; }
    printf("tree nb_dom_load built from %s:\n", argv[1]);
    walk(root, 1);
    return 0;
}
