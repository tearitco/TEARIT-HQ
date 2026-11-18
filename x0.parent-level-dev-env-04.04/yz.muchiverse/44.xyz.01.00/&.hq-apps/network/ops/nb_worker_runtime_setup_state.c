/* nb_worker_runtime_setup_state.c - runtime includes, globals, and wire state (mechanical extract, single-TU include) */

#define STB_IMAGE_IMPLEMENTATION
#include "../js/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include <strings.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

extern char **environ;

#define MAX_MSG (1024 * 1024)

/* ---- wire framing ---- */
static char g_rbuf[MAX_MSG];      /* command payload buffer */
static size_t g_rlen = 0;

static char g_page_js[4096];
static char g_fetch_dom[4096];
static char g_style_css[4096];   /* rung 7: LOAD-delivered stylesheet file */
static NbCss *g_css = NULL;      /* parsed rule cache, per LOAD */
static char g_href[4096];
static char g_title[512];

static int g_cli = 0;            /* argv mode: plain text out, no RPC framing */
static int g_cli_status_ok = 0;  /* CLI exit-status latch set by send_status */
static int g_nav_emit = 0;       /* rung-6 slice 2: daemon only (!g_cli, set */
static int g_module_page_loaded = 0;  /* set when a module page is loaded; worker exits after */
                                 /* per run_page) - NAV frames go to manager */

/* devtools console EVAL: the last LOAD's QuickJS context + DOM tree stay live
 * (g_live_ctx / g_dom_root) between LOAD and EVAL, so an eval:<js> snippet
 * runs against the real page context (document, window, globals, timer
 * bindings) and can mutate the DOM. Tear down only at the next LOAD or on
 * QUIT/error - see run_page() and live_teardown(). */
static JSContext *g_live_ctx = NULL;
static JSRuntime *g_live_rt  = NULL;
