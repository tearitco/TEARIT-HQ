/* nb_worker_preamble.c - file preamble, initial includes, and minimal shims (mechanical extract, single-TU include) */
#define _POSIX_C_SOURCE 200809L
/* nb_js_worker.c — resident JavaScript worker for network-browser-hq.
 * Owned by network_browser_manager (its direct child, spawned lazily on
 * first <script> presence), talking line-RPC over a socketpair dup2'd to
 * stdin/stdout. NB-JS worker plan §1/§2/§4.
 *
 * This is the step-2 SKELETON: it creates one QuickJS runtime/context, reuses the
 * shared rung-1/6 host (nb_host.h), reads LOAD-delivered files, runs the
 * page's JS, and reports STATUS ok|err. It writes nothing to the page yet
 * (the DOM-tree accessors + RENDER merge land in steps 3-4). It stays
 * resident across LOADs and never bare-spins (blocks on stdin read).
 *
 * RPC framing (plan §4): length-prefixed lines.
 *   send:  "%.6d\n" + payload + "\n"
 *   recv:  read a %06d length line, then that many payload bytes.
 *
 * manager -> worker: LOAD\n<path page.js>\n<path fetch.dom>\n<href>\n<title>
 * worker  -> manager: RENDER\n<len>\n<page.state rows>   (post-JS DOM, step 4)
 * worker  -> manager: STATUS ok|err:<message>
 * manager -> worker: QUIT            (shut down cleanly)
 */
#include "nb_host.h"
#include <execinfo.h>   /* NB_CRASH_BT: fatal-signal backtrace into the worker log */
#include <time.h>
#include <sys/random.h>  /* getrandom(2) for nb_rand_bytes (crypto entropy) */
#include <errno.h>       /* EINTR in nb_rand_bytes' getrandom loop */
#include "../nb_css.h"
#include "../nb_dom.h"

/* R4 minimal shims */
static unsigned char _nbbd(char c){ if(c>='A'&&c<='Z')return c-'A'; if(c>='a'&&c<='z')return c-'a'+26; if(c>='0'&&c<='9')return c-'0'+52; if(c=='+')return 62; if(c=='/')return 63; if(c=='=')return 64; return 255; }
