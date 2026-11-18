/* nb_worker_tick_loop_state.c - tick loop state and notes (mechanical extract, single-TU include) */

/* ---- the loop: lifecycle -> job/timer drain until quiescent or budget ---- */
/* returns nonzero if an event-loop callback threw (caller -> STATUS err) */
/* ---- G1-b: the tick loop (2026-10-13) ----------------------------------
 *
 * A page's setInterval has to keep running AFTER LOAD returns. Until this,
 * it did not: run_page() drained the timer queue once (bounded by
 * MAX_DRAIN_MS = 800ms, which is why the G1 fixture came back saying
 * "tick 2" and then stopped dead), emitted one RENDER, and the worker went
 * back to blocking on stdin forever. Measured 2026-10-13: 0 frames in
 * 10.1s of doing nothing at all.
 *
 * Deliberately NOT reusing run_event_loop() here. That drains to
 * QUIESCENT and sleeps until the next timer, so calling it per tick would
 * hold stdin hostage for up to MAX_DRAIN_MS and the page could not be
 * clicked. The tick slice runs only what is DUE NOW and returns, which is
 * the same pair run_event_loop() calls inside its own loop body:
 * run_due_timers() + drain_jobs(). Same semantics, no waiting.
 *
 * Change detection is a string compare against the last pushed rows
 * rather than a dirty flag in the DOM: one memcmp is exact, cannot be
 * forgotten, and needs no new bookkeeping on every mutation site - which
 * is where a dirty flag would rot.
 *
 * OFF by default (g_tick_cap_ms == 0). The resident manager worker must
 * not start emitting spontaneous frames until the manager side of the
 * push protocol is ready to consume them; the G1 probe opts in with an
 * explicit TICK frame.
 */
static int g_tick_cap_ms;             /* 0 = off; else the max stdin wait */
static char *g_last_push;             /* rows of the last frame we pushed */
