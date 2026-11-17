/* nb_worker_cookie_datetime.c - cookie datetime helpers (mechanical extract, single-TU include) */
static const char *const COOKIE_MONTHS[12] =
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* IMF-fixdate ("Sun, 06 Nov 1994 08:49:37 GMT"); (time_t)-1 = unparseable.
 * Note a real "01 Jan 1970" parses to epoch 0 (a valid instant, NOT the
 * "no expiry" sentinel — that distinction is handled in the setter). */
static time_t cookie_datetime(const char *s) {
    if (!s || !*s) return (time_t)-1;
    int d = 0, y = 0, hh = 0, mi = 0, ss = 0, mon = -1;
    char monname[8] = {0};
    if (sscanf(s, "%*[^,], %d %7s %d %d:%d:%d",
               &d, monname, &y, &hh, &mi, &ss) == 6) {
        for (int i = 0; i < 12 && mon < 0; i++)
            if (!strncasecmp(COOKIE_MONTHS[i], monname, 3)) mon = i;
        if (mon >= 0 && y >= 1970 && y <= 9999)
            return epoch_from_ymd(y, mon + 1, d, hh, mi, ss);
    }
    return (time_t)-1;
}
