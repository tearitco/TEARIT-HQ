/* nb_worker_recv_frame_budget_notes.c - bounded frame read notes and result codes (mechanical extract, single-TU include) */

/* Bounded frame read. budget_ms < 0 keeps the historical blocking
 * behaviour; >= 0 gives up once the budget is spent and returns 0.
 *
 * Why: recv_frame() used a bare blocking read(), so a driver that never
 * answered parked the worker in recv() FOREVER. The worker is a long-lived
 * resident process (see the "keep the resident browser worker alive through
 * long page loads" change), so one unanswered FETCH wedged it permanently
 * and SILENTLY - `make check` printed nothing and had to be killed. Callers
 * already treat 0 as "no frame" and fall back, so a bounded wait degrades
 * into the direct-curl path instead of hanging the house.
 */
/* Why recv_frame_budget() gave up. Needed because "the caller timed out"
 * and "the peer went away" were BOTH a bare 0, and the tick loop must
 * do opposite things with them: keep ticking versus shut down. */
#define NB_RECV_SHUTDOWN 0
#define NB_RECV_TIMEOUT  1
