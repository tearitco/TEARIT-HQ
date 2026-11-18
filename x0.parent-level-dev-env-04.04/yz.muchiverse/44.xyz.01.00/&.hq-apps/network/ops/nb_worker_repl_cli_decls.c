/* nb_worker_repl_cli_decls.c - REPL/CLI forward declarations and notes (mechanical extract, single-TU include) */

/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon. */
/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon, but
 * `duk -i` forces the REPL even when stdin is piped. */
/* CLI-2/CLI-3 native hook used by both the REPL and node mode. */
static JSValue nb_cjs_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static void      nb_install_fs(JSContext *ctx);
static const char g_cjs_prelude[];
