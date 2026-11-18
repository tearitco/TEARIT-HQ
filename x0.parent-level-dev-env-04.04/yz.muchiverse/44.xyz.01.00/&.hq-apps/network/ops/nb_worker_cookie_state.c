/* nb_worker_cookie_state.c - cookie jar state/types (mechanical extract, single-TU include) */
/* ============================= rung 6: file-backed document.cookie jar =====
 * document.cookie getter/setter as C natives (the prelude in nb_host.h leaves
 * a configurable stub; install_dom redefines it with these). The jar lives on
 * disk at $NB_COOKIES_FILE (fallback $HOME/.config/nbjs/nb_cookies.txt), so
 * cookies survive across LOADs — each LOAD runs in a fresh engine instance, so
 * the file is the only persistence. Jar line format (TAB-separated legend):
 *   host<TAB>path<TAB>name<TAB>value<TAB>expires_epoch<TAB>secure
 * host "*" = set from a URI with no host. expires 0 = session cookie.
 * RFC 6265 subset: name=value + Domain/Path/Expires/Max-Age/Secure.
 * Reads tolerate damage: junk lines are skipped, not fatal. */
#define COOKIE_MAX_ENT 512

typedef struct {
    char host[128];
    char path[256];
    char name[128];
    char value[1024];
    time_t expires;
    int secure;
} CookieEnt;

static char g_cookie_path[PATH_MAX];
static int  g_cookie_path_set = 0;
