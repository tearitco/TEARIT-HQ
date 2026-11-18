/* nb_worker_html_serializer_notes.c - HTML serializer behavior notes (mechanical extract, single-TU include) */
/* HTML fragment serialization (innerHTML / outerHTML / XMLSerializer).
 * The old emitter wrote text verbatim, which is wrong for the three things
 * a page actually does with innerHTML:
 *   - ESCAPE text: '&' -> &amp;, '<' -> &lt;, '>' -> &gt;, U+00A0 -> &nbsp;.
 *     Verbatim text re-parses as markup or as an entity, so a round-trip
 *     (`el.innerHTML = el.innerHTML`) silently changed the tree.
 *   - Raw-text elements (script/style/xmp/iframe/noembed/noframes/
 *     plaintext/noscript) keep their data literally: the parser never
 *     treated it as markup, so escaping it would corrupt JS/CSS.
 *   - VOID elements have no end tag. Emitting `</br>` makes a round-trip
 *     add a second break, because the HTML parser reads `</br>` as `<br>`.
 * XML mode escapes the same three bytes but not U+00A0 (XML has no &nbsp;
 * entity) and keeps closing tags for void elements (XML is not HTML).
 * Attribute blobs are still emitted as stored: parser/load blobs are
 * already source markup, and setAttribute values are not re-escaped yet. */
