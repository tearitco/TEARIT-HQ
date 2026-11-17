/* nb_worker_layout_is_block_tag.c - block tag classifier (mechanical extract, single-TU include) */
static int layout_is_block_tag(const char *t) {
    if (!t) return 1;
    static const char *inline_tags[] = {
        "a", "b", "em", "i", "strong", "span", "code", "small", "u",
        "label", "sub", "sup", "abbr", "cite", "q", "kbd", "samp", NULL
    };
    for (int i = 0; inline_tags[i]; i++)
        if (!strcasecmp(t, inline_tags[i])) return 0;
    return 1;
}
