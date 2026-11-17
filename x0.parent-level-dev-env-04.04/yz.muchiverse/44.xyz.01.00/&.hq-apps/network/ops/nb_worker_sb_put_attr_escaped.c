/* nb_worker_sb_put_attr_escaped.c - attribute escaping helper (mechanical extract, single-TU include) */
/* Attribute values are stored in the blob as valid markup: setAttribute
 * escapes on write and parser/load blobs already came from markup, so the
 * serializer can emit the blob verbatim. A raw '"' would terminate the
 * quoted value and a raw '&' could start an entity. Per the HTML spec,
 * attribute mode escapes '&', '"' and U+00A0 but leaves '<'/'>' literal. */
static void sb_put_attr_escaped(SB *b, const char *s) {
    if (!s) return;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (p[0] == 0xC2 && p[1] == 0xA0) { sb_put(b, "&nbsp;"); p++; continue; }
        switch (*p) {
        case '&': sb_put(b, "&amp;"); break;
        case '"': sb_put(b, "&quot;"); break;
        default: { char one[2] = { (char)*p, 0 }; sb_put(b, one); }
        }
    }
}
