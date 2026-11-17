/* nb_worker_recv_frame_wrappers.c - frame reader convenience wrappers (mechanical extract, single-TU include) */
static int recv_frame_budget(int budget_ms) { return recv_frame_ex(budget_ms, NULL); }
static int recv_frame(void) { return recv_frame_budget(-1); }
