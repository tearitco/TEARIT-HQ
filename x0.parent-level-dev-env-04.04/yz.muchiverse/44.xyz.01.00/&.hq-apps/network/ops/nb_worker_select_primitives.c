/* nb_worker_select_primitives.c - select/option selection primitives (mechanical extract, single-TU include) */
/* ---- <select> / <option> (2026-10-05) ----------------------------------
 * A select's .value is the SELECTED OPTION's value, not the select's own
 * `value` attribute. Real pages almost never put one on the <select>
 * itself, so the generic getter below answered "" for every dropdown on a
 * real page — page code then read an empty string, took a "no selection"
 * branch, and never fired the request the dropdown was there to make.
 *
 * Selection state lives in a hidden \xffsel shadow on each option wrapper.
 * push_node() caches exactly one wrapper per node in the ID map, so the
 * shadow survives repeated getElementById/childNodes lookups exactly the
 * way \xffvalue already does. Writing a `selected` ATTRIBUTE instead would
 * also work, but that blob is what nb_serialize() ships to the renderer, so
 * a script-side selection would come back out as markup.
 */
#define SELKEY "\xffsel"

static int node_is_tag(const NbNode *n, const char *tag) {
    return n && n->tag && !strcmp(n->tag, tag);
}
