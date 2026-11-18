/* nb_worker_storage_state.c - localStorage/sessionStorage state (mechanical extract, single-TU include) */
/* ===================== rung 6: localStorage (disk jar) + sessionStorage (per-LOAD) =====
 * install_host leaves getItem/setItem/removeItem as no-op stubs; install_dom replaces
 * BOTH globals with real C-backed objects here. localStorage persists across LOADs via
 * a jar on disk at $NB_LOCALSTORAGE_FILE (fallback ~/.config/nbjs/nb_localstorage.txt);
 * sessionStorage lives in process memory and is cleared at the top of every run_page,
 * so each LOAD gets a fresh session (a fresh runtime/context could not carry JS state
 * anyway). Jar line format: <pct-encoded key>\t<pct-encoded value>\n — keys/values are
 * percent-encoded (RFC 3986 unreserved pass through, everything else %XX) so tabs,
 * newlines and control chars are safe inside a line. Reads tolerate damage. */
#define ST_MAX_ENT 256

typedef struct { char key[256]; char value[4096]; } StEnt;

static char g_ls_path[PATH_MAX];
static int  g_ls_path_set = 0;
static StEnt g_ls[ST_MAX_ENT];      /* reused load/save buffer (single-threaded) */
static StEnt g_ss[ST_MAX_ENT];      /* in-memory session map, cleared per LOAD */
static int  g_ss_count = 0;
