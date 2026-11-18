/* nb_worker_insert_adjacent_notes.c - insertAdjacent behavior notes (mechanical extract, single-TU include) */
/* ---- insertAdjacentHTML / Text / Element (2026-11-09) ----
 * Real DOM insertion at the four spec positions, built on the same parser
 * and tree helpers that innerHTML/appendChild already use. Positions are
 * ASCII case-insensitive; anything else is a SyntaxError. beforebegin and
 * afterend on a node with no parent are a no-op, exactly as in a browser
 * (#document is the top; there is no place beside it).
 *
 * ChildList MutationObserver records are emitted for each inserted node.
 * A parsed fragment contributes one record per node (the C record model
 * carries one addedNode); a browser would batch them into one record. Both
 * describe the same real insertions, which is what an observer reacts to.
 *
 * insertAdjacentElement moves an existing node and returns it, or null when
 * the insertion is a no-op. Script-execution on insertion is not claimed
 * here: appendChild's dynamic <script> path is separate and this slice does
 * not duplicate it. */
