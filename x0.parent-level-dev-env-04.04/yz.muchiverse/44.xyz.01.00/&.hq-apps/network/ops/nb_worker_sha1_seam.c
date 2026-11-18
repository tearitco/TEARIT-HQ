/* nb_worker_sha1_seam.c - generic SHA-1 seam notes and header include (mechanical extract, single-TU include) */

/* ==================== rung 6 seam: generic SHA-1 primitive =============
 * Row-34 login: Google's page JS signs SAPISIDHASH =
 *   <ts>_<base64(sha1(<ts> " " <SAPISID> " " <origin>))>
 * by itself (Chromium parity — browsers have no LOGIN op; the site's own
 * script does the crypto). But the page needs a SHA-1 primitive to do so.
 * This is the engine-generic one: `__nb_sha1(utf8)->base64(digest)`, exposed
 * to any page. Not youtube-specific; the page JS handles the SAPISIDHASH
 * composition purely in JS. Raw digest bytes can't round-trip through a JS
 * string, so the digest is pre-encoded ASCII base64 here — verified against openssl
 * in worker_sapisid_test).  Shared impl in nb_sha1.h so fixture servers
 * recompute the SAME digest when verifying a received signature. */
#include "nb_sha1.h"
