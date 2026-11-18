/* nb_worker_content_inset_notes.c - content inset/chrome notes (mechanical extract, single-TU include) */

/* The chrome the renderer puts ABOVE the first content row inside the
 * scrolled column. Both terms are measured off the renderer, not guessed:
 *
 *   - GENERIC_SBAR_ARROW_H (14, khtpm_core_render.c:5428). layout_scroll_region
 *     sets `content_y = y + (have_arrows ? GENERIC_SBAR_ARROW_H : 0)`, so a
 *     column that scrolls starts its first row 14px down, under the scrollbar's
 *     up-arrow. Only when it scrolls - have_arrows requires max_scroll > 0.
 *   - NB_ROW_PX (30) for the page-title row. The manager projects <head><title>
 *     into a `page-title` row as the FIRST child of the content scrolllist
 *     (network_browser_manager.c:5570), so it occupies a real row before any
 *     <p>. A DOM-only model cannot see it: <title> lives in <head> and takes no
 *     space in a browser's body, which is exactly why this offset was 44px
 *     (14 + 30) and looked like a mystery.
 *
 * This is the worker mirroring the house renderer on purpose. It is NOT
 * claiming <title> takes body space in some other browser - it is saying the
 * box tree must describe the pixels this renderer actually draws, which is
 * what G2.2 measures. */
