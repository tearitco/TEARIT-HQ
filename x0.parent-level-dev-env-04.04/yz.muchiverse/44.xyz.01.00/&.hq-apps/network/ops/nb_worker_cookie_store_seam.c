/* nb_worker_cookie_store_seam.c - unified cookie store seam comments (mechanical extract, single-TU include) */

/* ======================== rung 6 seam: unified cookie store =============
 * ONE authoritative jar shared by document.cookie AND the network layer
 * (nb_fetch_sync).  Chromium-parity: the old two-store split (NB_COOKIES_FILE
 * for document.cookie, NB_CURL_COOKIES_FILE for curl -b/-c) is eliminated;
 * all Set-Cookie ingress and Cookie egress goes through our jar.  The manager
 * may still set NB_CURL_COOKIES_FILE for its own curls; the worker ignores it
 * from here forward.
 *
 * cookie_header_for_url()  — scope-match + emit Cookie: for outgoing request
 * cookie_set_from_wire()   — parse response Set-Cookie into the jar
 */

/* Extract host and path from an arbitrary URL (scheme://host[:port]/path).
 * Port is stripped for cookie scoping (RFC 6265 section 1). */
