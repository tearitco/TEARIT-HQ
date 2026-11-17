/* nb_worker_sb_put_comment_data_safe.c - safe comment serialization helper (mechanical extract, single-TU include) */
static void sb_put_comment_data_safe(SB *b, const char *s, int xml) {
    if (!s) return;
    char last = 0;
    for (size_t i = 0; s[i]; i++) {
        /* Prevent premature comment end: "-->" or "--!>" */
        if (s[i] == '-' && s[i+1] == '-' && s[i+2] == '>') {
            if (xml) {
                /* XML comments must not contain "--"; keep it well-formed. */
                sb_put(b, "- -&gt;");
                last = ';';
            } else {
                sb_put(b, "--&gt;");
                last = ';';
            }
            i += 2;
            continue;
        }
        if (s[i] == '-' && s[i+1] == '-' && s[i+2] == '!' && s[i+3] == '>') {
            if (xml) {
                sb_put(b, "- -!&gt;");
                last = ';';
            } else {
                sb_put(b, "--!&gt;");
                last = ';';
            }
            i += 3;
            continue;
        }
        if (xml && s[i] == '-' && s[i+1] == '-') {
            /* XML: comment data must not contain "--". Insert a space to
             * break the pair while preserving readable content. */
            sb_put(b, "- ");
            last = ' ';
            /* do not consume s[i+1]; reprocess it to catch runs/trailing '-' */
            continue;
        }
        char one[2] = { s[i], 0 };
        sb_put(b, one);
        last = s[i];
    }
    if (xml && last == '-') {
        /* XML: comment must not end with '-' */
        sb_put(b, " ");
    }
}
