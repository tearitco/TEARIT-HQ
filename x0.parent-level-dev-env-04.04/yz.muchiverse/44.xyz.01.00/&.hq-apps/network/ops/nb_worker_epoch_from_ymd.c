/* nb_worker_epoch_from_ymd.c - epoch from ymd helper (mechanical extract, single-TU include) */
static time_t epoch_from_ymd(int y, int m, int d, int hh, int mi, int ss) {
    if (m < 3) { m += 12; y--; }
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (unsigned)(m > 2 ? m - 3 : m + 9) + 2) / 5 + (unsigned)d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    long days = (long)(era * 146097) + (long)doe - 719468L;
    return (time_t)days * 86400L + hh * 3600L + mi * 60L + ss;
}
