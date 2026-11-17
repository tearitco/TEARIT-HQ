/* nb_worker_node_outer_html.c - outer HTML serialization helpers (mechanical extract, single-TU include) */
static void node_outer_html_m(const NbNode *n, SB *b, int xml) {
    if (!n) return;
    if (!n->tag || !n->tag[0]) {   /* #text node */
        if (!xml && n->parent && nb_ser_raw_tag(n->parent->tag)) sb_put(b, n->text);
        else sb_put_escaped(b, n->text, xml);
        return;
    }
    if (!strcmp(n->tag, "#comment")) {
        sb_put(b, "<!--");
        sb_put_comment_data_safe(b, n->text ? n->text : "", xml);
        sb_put(b, "-->");
        return;
    }
    sb_put(b, "<");
    sb_put(b, n->tag);
    sb_put_attrs_canonical(b, n);
    sb_put(b, ">");
    if (n->text) {
        if (!xml && nb_ser_raw_tag(n->tag)) sb_put(b, n->text);
        else sb_put_escaped(b, n->text, xml);
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        node_outer_html_m(c, b, xml);
    if (xml || !nb_ser_void_tag(n->tag)) {
        sb_put(b, "</");
        sb_put(b, n->tag);
        sb_put(b, ">");
    }
}
static void node_outer_html(const NbNode *n, SB *b) { node_outer_html_m(n, b, 0); }
