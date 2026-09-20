/* cc -o /tmp/t tests/test_ui_scale.c && /tmp/t   (from _shared-lib/) */
#include "../khtpm_ui_scale.c"
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main(void) {
    const int base = 80;
    /* auto for the screens we care about */
    int a_ref = kps_auto_pct(0, 2496, 1664, 2496, 1664);
    int a_small = kps_auto_pct(0, 1366, 768, 2496, 1664);
    int a_1080 = kps_auto_pct(0, 1920, 1080, 2496, 1664);
    int a_4k = kps_auto_pct(0, 3840, 2160, 2496, 1664);
    CHECK(a_ref == 100, "ref auto %d", a_ref);
    printf("auto: ref=%d 1366x768=%d 1920x1080=%d 3840x2160=%d\n", a_ref, a_small, a_1080, a_4k);
    /* identity at reference, for any value incl. negatives/off-grid */
    for (int v = -50; v < 5000; v += 7) {
        CHECK(kps_ref_to_screen(v, base, 100) == v, "identity r2s %d", v);
        CHECK(kps_screen_to_ref(v, base, 100) == v, "identity s2r %d", v);
    }
    /* grid alignment: every ref multiple of the base cell lands on a multiple of the scaled cell, and comes back exactly */
    int autos[] = { a_small, a_1080, a_4k, 50, 73, 133, 300 };
    for (unsigned i = 0; i < sizeof(autos) / sizeof(autos[0]); i++) {
        int a = autos[i], sc = kps_scaled_cell(base, a);
        for (int k = 0; k < 64; k++) {
            int ref = k * base, scr = kps_ref_to_screen(ref, base, a);
            CHECK(scr == k * sc, "auto %d k %d: %d != %d*%d", a, k, scr, k, sc);
            CHECK(kps_screen_to_ref(scr, base, a) == ref, "auto %d round trip k %d", a, k);
        }
        /* off-grid values: ref -> screen -> ref -> screen is stable (idempotent after the first conversion) */
        for (int ref = 0; ref < 4000; ref += 13) {
            int s1 = kps_ref_to_screen(ref, base, a);
            int r1 = kps_screen_to_ref(s1, base, a);
            int s2 = kps_ref_to_screen(r1, base, a);
            CHECK(s1 == s2, "auto %d ref %d not stable: s1=%d r1=%d s2=%d", a, ref, s1, r1, s2);
            int r2 = kps_screen_to_ref(s2, base, a);
            CHECK(r1 == r2, "auto %d ref %d ref drifts: r1=%d r2=%d", a, ref, r1, r2);
        }
    }
    /* proportional: ref (1200,800) on 1366x768 lands at ~ 54% */
    printf("ref(1200,800) -> 1366x768 screen (%d,%d); -> 3840x2160 (%d,%d)\n",
           kps_ref_to_screen(1200, base, a_small), kps_ref_to_screen(800, base, a_small),
           kps_ref_to_screen(1200, base, a_4k), kps_ref_to_screen(800, base, a_4k));
    printf(fails ? "FAILED (%d)\n" : "ALL OK\n", fails);
    return fails != 0;
}
