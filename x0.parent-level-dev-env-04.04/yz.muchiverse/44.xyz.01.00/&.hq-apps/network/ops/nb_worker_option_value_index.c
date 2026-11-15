/* nb_worker_option_value_index.c - option value/index helpers (mechanical extract, single-TU include) */
/* HTML: an option's value is its `value` attribute, else its text. */
static void option_value(const NbNode *opt, char *out, size_t cap) {
    out[0] = 0;
    if (!opt) return;
    const char *v = nb_attr_get(opt, "value");
    if (v && *v) { snprintf(out, cap, "%s", v); return; }
    if (opt->text) snprintf(out, cap, "%s", opt->text);
}
static int sel_option_index(const NbNode *sel, const NbNode *opt) {
    int i = 0;
    for (NbNode *c = sel->first_child; c; c = c->next_sibling) {
        if (!node_is_tag(c, "option")) continue;
        if (c == opt) return i;
        i++;
    }
    return -1;
}
