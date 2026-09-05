#ifndef NB_DOM_H
#define NB_DOM_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* nb_dom.c — a compact, tolerant HTML tag tree + pre-order serializer.
 * Phase 1 step 1 of the NB-JS persistent-worker plan. The manager parses
 * the fetched HTML once into a node tree and serializes it to fetch.dom;
 * the (future) worker reads that file and rebuilds the tree in its own
 * heap for lazy Duktape accessors. This file has zero JS/Duktape deps.
 *
 * Intentionally NOT a full HTML5 parser: skips script/style/title/
 * noscript/comments, handles void elements + a few common real-page
 * auto-close cases, and otherwise requires well-formed-ish nesting. If a
 * page's markup defeats the parser, it degrades gracefully (text-only,
 * no crash) — the manager falls back to its existing linear extractor.
 */

typedef struct NbNode {
    char *tag;          /* lowercase, e.g. "div" */
    char *id;           /* id attr value, or NULL */
    char *cls;          /* class attr value (space-joined as-is), or NULL */
    char *attrs;        /* raw attribute blob, or NULL (see nb_attr_get) */
    char *text;         /* concatenated decoded text of this element, or NULL */
    struct NbNode *parent;
    struct NbNode *first_child;
    struct NbNode *next_sibling;
    struct NbNode *last_child;
} NbNode;

/* Parse a whole HTML document. Returns a root NbNode* (a synthetic
 * <#document> node) or NULL on failure. Caller frees with nb_node_free. */
NbNode *nb_parse_html(const char *html, size_t len);

/* Serialize a tree to `out` in pre-order with the plan §3 wire format:
 *   N|<tag>|<id>|<cls>|<textlen>|<text>|<attrlen>|<attrs>\n   a node
 *   D\n   (first emit) ... then N... children ... U\n   close/back-up
 * Text and attr blobs are length-prefixed so they may contain newlines
 * and '|'. Returns number of nodes written, or -1 on error. */
long nb_serialize(FILE *out, const NbNode *root);

/* Deserialize a tree written by nb_serialize back into a fresh NbNode
 * tree (the worker rebuilds the manager's fetch.dom in its own heap —
 * no shared memory / ABI coupling). Returns a root NbNode* (a synthetic
 * <#document> node) or NULL on malformed input / hitting the node cap.
 * Caller frees with nb_node_free. */
NbNode *nb_dom_load(FILE *in);

/* Value (decoded) of a single named attribute, or "" if absent. */
const char *nb_attr_get(const NbNode *n, const char *name);

/* Free an entire tree. */
void nb_node_free(NbNode *root);

/* Count nodes (for the 50k cap). */
long nb_count(const NbNode *root);

#ifdef __cplusplus
}
#endif

#endif /* NB_DOM_H */
