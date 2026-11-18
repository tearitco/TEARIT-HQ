/* nb_worker_crash_handler_notes.c - fatal-signal backtrace notes (mechanical extract, single-TU include) */

/* ---- fatal-signal backtrace (2026-10-26) ----
 * A hostile or oversized page script can take the worker down in ways that
 * leave NOTHING in the log: no WERR, no STATUS, just a zombie the manager
 * never hears from again. Measured on Cloudflare's orchestrate/chl_page
 * script: the last line in the worker log is "resolved script URL", and the
 * process is then gone. Without gdb on the box, the only way to tell a
 * SIGSEGV from an abort() is to have the worker report it itself. Gated by
 * NB_CRASH_BT so normal runs stay quiet. backtrace_symbols_fd() is the
 * async-signal-safe half of execinfo, and stderr is already the per-house
 * worker log, so the trace lands next to the page errors. */
