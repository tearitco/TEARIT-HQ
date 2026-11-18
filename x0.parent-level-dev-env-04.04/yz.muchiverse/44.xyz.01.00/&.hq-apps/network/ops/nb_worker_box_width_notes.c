/* nb_worker_box_width_notes.c - element border-box width notes (mechanical extract, single-TU include) */

/* WIDTH of an element's border box - the half of the box tree that was
 * missing (G2.1, slice 1, 2026-10-14).
 *
 * Before this, getBoundingClientRect() reported `w = st.width`, the
 * DECLARED CSS width and nothing else, so every unstyled element on every
 * page reported 0. Measured on the G2-a fixture: box_d1=w0/h+, box_p1=w0
 * /h+, box_s1=w0/h+, box_p2=w0/h+ - heights were already right (nb_content_h
 * below), only widths were absent. A browser that reports width 0 for
 * ordinary markup is not reporting geometry, it is reporting an absence.
 *
 * Two rules, which is what block layout actually does:
 *
 *   - A BLOCK box fills its containing block. Declared width wins; else
 *     the parent's own box; the document root is the content column.
 *   - An INLINE box shrinks to fit its text - it does NOT stretch to the
 *     column, and reporting 640px for a three-word <span> would be its own
 *     kind of lie.
 *
 * Deliberately NOT here yet: padding/border/margin in the width sum, and
 * inline-vs-block detection by tag beyond the common set. Those are
 * slice-2 work and pretending otherwise is how a box tree starts lying.
 */
