/* nb_worker_eval_watchdog.c - eval budget/watchdog and error helpers (mechanical extract, single-TU include) */
#define EVAL_BUDGET_SEC 2   /* plan step 5: watchdog for runaway page.js */
/* NB_EVAL_BUDGET env override (row-31): a real 10.8MB single-IIFE bundle
 * legitimately needs more than 2s to parse, so receipt runs can raise the
 * watchdog (NB_EVAL_BUDGET=120); production default stays 2s. */
static int nb_budget(void) {
    static int b = -1;
    if (b < 0) {
        const char *e = getenv("NB_EVAL_BUDGET");
        b = e ? atoi(e) : EVAL_BUDGET_SEC;
        if (b < 1) b = EVAL_BUDGET_SEC;
    }
    return b;
}
#define MAX_DRAIN_MS 800    /* commit 7: bounded wait so short timers fire pre-RENDER */

/* ===================== Phase 2 (commit 7): timers + microtasks + events ================ */

#define MAX_TIMERS 2048
#define MAX_EVENTS 4096
#define MAX_ONPROPS 1024
#define MAX_TIMER_INVOCATIONS 5000   /* plan §2 CPU safety */
#define MAX_RAF_FRAMES 120           /* plan §2: ~2s of rAF */
#define RAF_MS 16

typedef struct { int id, active; long interval; uint64_t due; int slot; JSValue cb; } Timer;
typedef struct { int kind; NbNode *node; char type[48]; int slot, active; JSValue cb; } EvL;
/* el.on<type> handlers CANNOT live as data props on the wrapper objects:
 * push_node() creates a fresh JS object per wrap, so the C side must own the
 * callback (held as a JSValue, keyed by node+kind+type). */
typedef struct { int kind; NbNode *node; char type[48]; int slot, active; JSValue cb; } OnProp;

#define EVT_NODE 1
#define EVT_WIN  2
#define EVT_DOC  3

static Timer g_timers[MAX_TIMERS];
static int g_timer_count = 0;
static EvL g_evl[MAX_EVENTS];
static int g_evl_count = 0;
static OnProp g_onprop[MAX_ONPROPS];
static int g_onprop_count = 0;
static int g_next_id = 1;
static int g_invocations = 0;
static int g_raf_fires = 0;
static int g_pending_err = 0;   /* set when an event-loop callback throws */
static char g_pending_errmsg[512];
static int g_trace_cb = 0;      /* NB_TRACE_CB=1: dump full callback exceptions */

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
}

/* ---- eval watchdog (2026-10-26) ----
 * This used to be signal(SIGALRM, sigalrm) + alarm(nb_budget()), with the
 * handler calling _exit(128 + sig). Every script slower than the budget was
 * treated as hostile: a heavy-but-legitimate page slice killed the whole
 * worker mid-page, the process stayed a zombie, every later eval reported
 * "eval error", and the page was dead with no message anywhere.
 * QuickJS's runtime interrupt handler aborts the running script with a
 * CATCHABLE-BY-C exception instead, so the worker survives with its DOM
 * intact and the abort is visible in the log as "interrupted".
 * The abort is NOT catchable by page JS: a fixture wrapping three
 * `while(true)` loops in try/catch ended at the first abort with a single
 * "interrupted" line and no re-loop (2026-10-26), so one abort per armed
 * window is enough - a page cannot swallow it and keep spinning.
 * Measured on Cloudflare's orchestrate/chl_page challenge script (glassdoor,
 * 2026-10-26): it finishes in seconds here and then throws TypeError: not a
 * function from a browser API this engine lacks - that challenge needs the R4
 * fingerprint surface, not a bigger budget. */
static uint64_t g_watchdog_deadline = 0;   /* CLOCK_MONOTONIC ms; 0 = paused */

static int nb_watchdog_cb(JSRuntime *rt, void *opaque) {
    (void)rt; (void)opaque;
    if (!g_watchdog_deadline) return 0;
    if (now_ms() <= g_watchdog_deadline) return 0;
    g_watchdog_deadline = 0;               /* one abort ends the script */
    return 1;
}
static void nb_watchdog_install(JSRuntime *rt) {
    JS_SetInterruptHandler(rt, nb_watchdog_cb, NULL);
}
static void nb_watchdog_begin(void) {
    g_watchdog_deadline = now_ms() + (uint64_t)nb_budget() * 1000u;
}
static void nb_watchdog_pause(void) { g_watchdog_deadline = 0; }

/* Format + clear the current context exception into buf; returns buf. */
static const char *js_error_to_cstr(JSContext *ctx, char *buf, size_t bl) {
    JSValue e = JS_GetException(ctx);
    const char *s = JS_ToCString(ctx, e);
    if (!s) {
        JS_FreeValue(ctx, e);
        snprintf(buf, bl, "unknown error");
        return buf;
    }
    snprintf(buf, bl, "%s", s);
    JS_FreeCString(ctx, s);
    /* diagnosis aid: NB_STACK=1 appends the JS exception stack so a bare
     * "TypeError: not a function" in a real bundle is traceable. */
    if (getenv("NB_STACK")) {
        JSValue st = JS_GetPropertyStr(ctx, e, "stack");
        if (JS_IsString(st)) {
            const char *stc = JS_ToCString(ctx, st);
            if (stc) {
                snprintf(buf + strlen(buf), bl - strlen(buf), " @ %s", stc);
                JS_FreeCString(ctx, stc);
            }
        }
        JS_FreeValue(ctx, st);
    }
    JS_FreeValue(ctx, e);
    return buf;
}

