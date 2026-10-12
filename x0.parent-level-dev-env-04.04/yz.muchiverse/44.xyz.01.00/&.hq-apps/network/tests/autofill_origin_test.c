/* autofill_origin_test.c - pins autofill_origin(), the manager's own
 * origin-derivation function, by compiling the REAL implementation
 * rather than a copy of it. A test that re-derives the rule it is
 * testing proves nothing, so this #includes the manager source and calls
 * the static function directly - the same trick the house already uses
 * for header-only units.
 *
 * The rule under test (an autofill jar keyed by origin is only as private
 * as this function):
 *   - http and https on one host are DIFFERENT jars
 *   - a non-default port is part of the origin; the default port is not
 *   - userinfo (http://user@host/) is not part of the origin
 *   - anything that is not http/https (file://, bare paths, mailto:)
 *     has NO origin at all, so autofill stays off for it - a local test
 *     page must never inherit a value typed on a real site
 *
 * Build+run: make -C <network dir> afot && ./afot
 */

static int fails = 0;

/* The manager ships its own main(); including the source would collide.
 * Renaming it here means the real translation unit - every static, every
 * real autofill_origin body - still compiles and links, and only the
 * entry point is replaced. */
#define main nb_manager_real_main
#include "../network_browser_manager.c"
#undef main

static void expect_origin(const char *url, const char *want, const char *what) {
    char got[512];
    autofill_origin(url, got, sizeof(got));
    if (strcmp(got, want) != 0) {
        printf("FAIL %s: %s -> '%s', wanted '%s'\n", what, url, got, want);
        fails++;
    }
}

int main(void) {
    /* the basic shape */
    expect_origin("https://example.com/form", "https://example.com", "scheme+host");
    expect_origin("https://example.com", "https://example.com", "no path");
    expect_origin("https://example.com/a/b/c?q=1#f", "https://example.com", "path/query/frag dropped");

    /* http and https must not share a jar */
    expect_origin("http://example.com/", "http://example.com", "http kept");
    expect_origin("https://example.com/", "https://example.com", "https kept");

    /* default ports are not part of the origin */
    expect_origin("https://example.com:443/x", "https://example.com", "default https port dropped");
    expect_origin("http://example.com:80/x", "http://example.com", "default http port dropped");
    /* a non-default port IS a different site and must be kept */
    expect_origin("http://example.com:8080/x", "http://example.com:8080", "non-default port kept");
    expect_origin("https://example.com:8443/", "https://example.com:8443", "non-default https port kept");

    /* userinfo is not part of the origin (RFC 6454) */
    expect_origin("http://user:pw@example.com/x", "http://example.com", "userinfo stripped");
    expect_origin("http://user@example.com/x", "http://example.com", "user only stripped");

    /* NO origin for anything that is not http/https - this is the rule
     * that keeps file:// pages (including every local fixture) from
     * inheriting a real site's saved values. */
    expect_origin("file:///home/x/page.html", "", "file:// has no origin");
    expect_origin("/plain/path.html", "", "bare path has no origin");
    expect_origin("mailto:a@b.com", "", "mailto has no origin");
    expect_origin("yt:watch?v=abc", "", "yt: shortcut has no origin");
    expect_origin("", "", "empty url has no origin");

    /* Case is normalised on BOTH halves. Found by this test, not by
     * inspection: without it https://Example.COM and https://example.com
     * were two different jars, so following a link that capitalises the
     * host differently silently lost the user's saved value. */
    expect_origin("HTTPS://Example.COM/x", "https://example.com", "scheme+host lowercased");
    expect_origin("HTTP://EXAMPLE.com/", "http://example.com", "all-caps host lowercased");

    if (fails) { printf("autofill_origin_test: %d FAILED\n", fails); return 1; }
    printf("autofill_origin_test: all cases pass\n");
    return 0;
}