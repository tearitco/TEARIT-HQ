/* load_governor.c - text-included (pure, no globals shared with the includer): "how long should I wait between two frames, given how busy the machine is right now".
 * Owner 2026-11-03 ("cpu is still crazy"): with the 3D board in use the renderer (chtpm_rgb_render) cost 53 % of a core and the engine daemon (bv_render_3d) 37 % for about 28
 * frames a second, and neither slowed down when the machine was already busy, which is exactly when cpu_guard ends the house. Idle cost is ~0 (event driven); this only bounds the
 * cost WHILE a frame stream runs, and only when the machine is loaded.
 *   int lg_min_interval_ms(void)   calm (<= 50 % of all CPUs in the last ~0.5 s) 40 ms (25 fps); over 50 %: 80 ms (12 fps); over 70 %: 150 ms (7 fps)
 *   long long lg_now_ms(void)      monotonic milliseconds
 * Samples /proc/stat at most every 500 ms (one fopen per half second, nothing per frame). Non-Linux: a fixed calm value. Include once per program. */
#ifndef LOAD_GOVERNOR_C
#define LOAD_GOVERNOR_C
#include <stdio.h>
#include <time.h>
static long long lg_now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (long long)t.tv_sec * 1000LL + t.tv_nsec / 1000000LL; }
#ifdef __linux__
static int lg_min_interval_ms(void) {
    static long long t_last = 0; static int cur = 40; static unsigned long long pb = 0, pa = 0;
    long long now = lg_now_ms();
    if (now - t_last >= 500) {
        t_last = now; FILE *f = fopen("/proc/stat", "r");
        if (f) { unsigned long long v[8] = {0}; int n = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]); fclose(f);
            if (n >= 4) { unsigned long long idle = v[3] + v[4], tot = 0; for (int i = 0; i < 8; i++) tot += v[i]; unsigned long long b = tot - idle;
                if (pa && tot > pa) { unsigned long long db = b - pb, dt = tot - pa; unsigned long long pct = db * 100ULL / dt; cur = pct > 70 ? 150 : pct > 50 ? 80 : 40; }
                pb = b; pa = tot; } } }
    return cur;
}
#else
static int lg_min_interval_ms(void) { return 33; }
#endif
#endif
