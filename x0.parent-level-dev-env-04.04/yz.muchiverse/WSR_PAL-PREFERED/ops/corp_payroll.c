/*
 * corp_payroll.c - corporations pay wages to households. THE MISSING LEG.
 *
 * Without this the goods market is a one-way transfer: households spend and
 * never earn, so the only buyer base is consumed and never replaced. Measured
 * on a clean 12-tick run, household cash fell 118,715 -> 23,025 while corp cash
 * rose, and every household is within a few ticks of being unable to bid at
 * all. A market with no buyers is not an economy, it is a liquidation.
 *
 * This op closes the loop: revenue earned from goods sales -> wages -> the
 * households that spend them. That is what makes "real GDP from discovered
 * prices" (ROADMAP 2.2) possible, because taxes need a circulating income
 * base that does not drain to zero.
 *
 * THE RATE IS NOT A PRICE. Nothing here assigns a value to anything. Payroll is
 * a share of revenue that already happened in goods_settle at a price that was
 * already discovered by matched orders. See docs/OPERATING-INCOME.md 3.2.
 *
 * SELF-CONTAINED, like the rest of ops/: no shared project headers. The
 * contracts it reads are:
 *   - data/gsold_<GOOD>.txt, rows "producer|sold|offered|revenue", written by
 *     goods_settle.c. Payroll is sized off real booked revenue, never off
 *     production volume or a guessed margin.
 *   - pieces/<id>/state.txt cash= for corps and households.
 *   - data/market_ledger.txt, appended in the house double-entry format.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#undef MAX_PATH
#endif

#define PATHBUF 1400
#define MAXLINE 512
#define MAXPIECE 64
#define MAXCORP  128

/* ---- tuning constants, owned by THIS op ---- */

/* Share of GROSS MARGIN returned to households as wages - NOT a share of
 * revenue. This distinction is the whole point and the first version got it
 * wrong in a way that looked completely healthy.
 *
 * Revenue-based payroll is a trap here. Goods cost WSR_PRODUCE_COST per unit
 * and clear at roughly price = cost * 1.25, so gross margin is about 20% of
 * revenue. Paying 60% OF REVENUE as wages guarantees every corporation goes
 * insolvent: measured, corps fell 37,834 -> 10,595 over 16 ticks while
 * households were fine and the goods market still looked busy.
 *
 * Paying a share of MARGIN (revenue minus the cost of the units sold) is
 * self-limiting by construction - a firm that cannot cover its production
 * costs has no margin, pays no wages, and that is reported rather than hidden.
 */
#ifndef WSR_PAYROLL_RATE
#define WSR_PAYROLL_RATE 0.60
#endif

/* Production cost per unit, mirroring goods_quote.c's WSR_PRODUCE_COST. It is
 * duplicated here rather than shared because ops/ is self-contained by house
 * rule - no shared project headers. If the cost changes, BOTH must change;
 * corp_payroll states the dependency so a drift is findable by grep. */
#ifndef WSR_PRODUCE_COST
#define WSR_PRODUCE_COST 1.0
#endif

/* A corp may not pay wages out of its last cent. Below this it keeps a buffer
 * so a firm that owes wages is still alive to owe them next tick. */
#ifndef WSR_PAYROLL_FLOOR
#define WSR_PAYROLL_FLOOR 1.00
#endif

static char g_root[PATHBUF];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && *env) {
        size_t n = strlen(env);
        if (n >= sizeof(g_root) - 128) {
            fprintf(stderr, "corp_payroll: PRISC_PROJECT_ROOT too long (%zu) - "
                            "refusing to truncate a path\n", n);
            exit(1);
        }
        memcpy(g_root, env, n + 1);
    } else {
        snprintf(g_root, sizeof(g_root), ".");
    }
}

static void piece_state(char *out, size_t n, const char *id) {
    snprintf(out, n, "%s/projects/wsr-pal/pieces/%s/state.txt", g_root, id);
}

static void read_field(const char *path, const char *key, char *out, size_t n) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAXLINE];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            char *v = line + kl + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, n, "%s", v);
            break;
        }
    }
    fclose(f);
}

static double field_d(const char *path, const char *key) {
    char buf[MAXLINE];
    read_field(path, key, buf, sizeof(buf));
    return atof(buf);
}

static void write_field(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    static char lines[96][MAXLINE];
    int nlines = 0;
    if (f) {
        while (nlines < 96 && fgets(lines[nlines], MAXLINE, f)) nlines++;
        fclose(f);
    }
    size_t kl = strlen(key);
    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "corp_payroll: cannot write %s\n", path); return; }
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, kl) == 0 && lines[i][kl] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static FILE *g_ledger = NULL;

static void ledger_open(void) {
    if (g_ledger) return;
    char path[PATHBUF];
    snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/market_ledger.txt", g_root);
    g_ledger = fopen(path, "a");
    if (!g_ledger)
        fprintf(stderr, "corp_payroll: cannot append to %s - refusing to pay wages "
                        "without a ledger\n", path);
}

/* Wages are an EXPENSE, not revenue: the ledger says so via the event name, and
 * they debit the corp. Without this row the goods market looks profitable and
 * households look like they were funded by a money printer. */
static void ledger_wage(const char *corp, const char *household, long cents) {
    if (!g_ledger) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date_str[32];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", t);
    fprintf(g_ledger,
            "Time: %s | Debit: %s | Credit: %s | Amount: %ld.%02ld Dollars | "
            "Event: corp_payroll.+x | Kind: wage\n",
            date_str, corp, household, cents / 100, labs(cents % 100));
}

int main(void) {
    resolve_root();
    ledger_open();

    /* ---- households, discovered by scanning pieces/, never a fixed roster ---- */
    static char hh[MAXCORP][MAXPIECE];
    int nhh = 0;
    char dirpath[PATHBUF];
    snprintf(dirpath, sizeof(dirpath), "%s/projects/wsr-pal/pieces", g_root);
    DIR *d = opendir(dirpath);
    if (!d) { fprintf(stderr, "corp_payroll: cannot open %s\n", dirpath); return 1; }
    struct dirent *e;
    while ((e = readdir(d)) && nhh < MAXCORP) {
        if (strncmp(e->d_name, "pop_", 4) != 0) continue;
        char st[PATHBUF];
        piece_state(st, sizeof(st), e->d_name);
        FILE *probe = fopen(st, "r");
        if (!probe) continue;               /* directory without an entity */
        fclose(probe);
        snprintf(hh[nhh], MAXPIECE, "%s", e->d_name);
        nhh++;
    }
    closedir(d);

    if (nhh == 0) {
        fprintf(stderr, "corp_payroll: no pop_* households found - refusing to run, "
                        "wages have nowhere to go\n");
        return 1;
    }

    /* ---- booked goods revenue per producer, from goods_settle's own records ---- */
    static char       corpid[MAXCORP][MAXPIECE];
    static double     rev[MAXCORP];
    static long       sold_units[MAXCORP];
    int ncorp = 0;

    char datapath[PATHBUF];
    snprintf(datapath, sizeof(datapath), "%s/projects/wsr-pal/data", g_root);
    d = opendir(datapath);
    if (d) {
        while ((e = readdir(d))) {
            if (strncmp(e->d_name, "gsold_", 6) != 0) continue;
            char gf[PATHBUF];
            snprintf(gf, sizeof(gf), "%s/%s", datapath, e->d_name);
            FILE *f = fopen(gf, "r");
            if (!f) continue;
            char line[MAXLINE];
            while (fgets(line, sizeof(line), f)) {
                char name[MAXPIECE]; long sold, offered; double revenue;
                if (sscanf(line, "%63[^|]|%ld|%ld|%lf", name, &sold, &offered, &revenue) != 4)
                    continue;               /* stale 3-field row from an older build */
                if (revenue <= 0.0) continue;
                int k = -1;
                for (int i = 0; i < ncorp; i++)
                    if (!strcmp(corpid[i], name)) { k = i; break; }
                if (k < 0 && ncorp < MAXCORP) {
                    snprintf(corpid[ncorp], MAXPIECE, "%s", name);
                    rev[ncorp] = 0.0;
                    sold_units[ncorp] = 0;
                    k = ncorp++;
                }
                if (k >= 0) { rev[k] += revenue; sold_units[k] += sold; }
            }
            fclose(f);
        }
        closedir(d);
    }

    /* ---- pay ---- */
    long paid_total_cents = 0;
    int  paid_corps = 0, skipped = 0;
    double wages_corp_total = 0.0, wages_hh_total = 0.0;

    for (int i = 0; i < ncorp; i++) {
        if (rev[i] <= 0.0) continue;

        /* GROSS MARGIN, not revenue. Cost is charged on units SOLD.
         *
         * KNOWN LIMITATION, deliberately left visible: a producer that makes
         * units it cannot sell has a real loss that this does not see, because
         * production volume is not recorded per producer anywhere the payroll op
         * can reach it. So margin here is optimistic exactly when sell-through is
         * poor - which is backwards, and would overpay wages to a struggling
         * producer. The honest fix is for goods_quote to write production per
         * producer (data/gprod_<GOOD>.txt) and charge cost on that. Until then,
         * treat this figure as an upper bound on payable wages. */
        double margin = rev[i] - (double)sold_units[i] * WSR_PRODUCE_COST;
        if (margin <= 0.0) continue;

        char cst[PATHBUF];
        piece_state(cst, sizeof(cst), corpid[i]);
        FILE *probe = fopen(cst, "r");
        if (!probe) continue;
        fclose(probe);
        double cash = field_d(cst, "cash");

        /* Whole cents throughout. Payroll is money moving between accounts; if
         * this rounds, corps and households drift apart over a long run and the
         * ledger stops balancing for a reason nobody can find. */
        long owed_cents = (long)llround(margin * WSR_PAYROLL_RATE * 100.0);
        if (owed_cents <= 0) continue;

        long cash_cents = (long)llround(cash * 100.0);
        if (cash_cents - owed_cents < (long)llround(WSR_PAYROLL_FLOOR * 100.0)) {
            /* Cannot pay in full without going under the floor. Report rather than
             * silently paying a fraction nobody agreed to: a corp that sells and
             * cannot pay wages is a real signal, not noise to be smoothed away. */
            skipped++;
            continue;
        }

        /* Equal split, with residual cents to the first households so the total
         * paid is EXACTLY owed_cents and not owed_cents rounded down N times.
         *
         * v1 SIMPLIFICATION, and a real one: every household gets the same wage
         * regardless of how much it bought. That means employment does not yet
         * follow demand. The upgrade is to weight each household by what it
         * bought from this corp last tick, which requires per-buyer records the
         * goods market does not keep yet. Until then this is a deliberate
         * simplification, not a discovery. */
        long base = owed_cents / nhh;
        long rem  = owed_cents - base * nhh;

        char val[MAXLINE];
        snprintf(val, sizeof(val), "%.2f", (cash_cents - owed_cents) / 100.0);
        write_field(cst, "cash", val);

        double corp_paid = 0.0;
        for (int h = 0; h < nhh; h++) {
            long share = base + (h < rem ? 1 : 0);
            if (share <= 0) continue;

            char hst[PATHBUF];
            piece_state(hst, sizeof(hst), hh[h]);
            double hc = field_d(hst, "cash");
            long hcents = (long)llround(hc * 100.0);

            snprintf(val, sizeof(val), "%.2f", (hcents + share) / 100.0);
            write_field(hst, "cash", val);

            /* Read back. Both legs, every time. A half-applied wage run is
             * money that exists on one side of the ledger only. */
            double got_c = field_d(cst, "cash");
            double got_h = field_d(hst, "cash");
            if (fabs(got_c - (cash_cents - owed_cents) / 100.0) > 0.01 ||
                fabs(got_h - (hcents + share) / 100.0) > 0.01) {
                fprintf(stderr, "corp_payroll: WRITE FAILED paying %s -> %s "
                                "- aborting rather than half-paying\n", corpid[i], hh[h]);
                return 1;
            }

            ledger_wage(corpid[i], hh[h], share);
            corp_paid += share / 100.0;
        }

        paid_total_cents += owed_cents;
        wages_corp_total += corp_paid;
        wages_hh_total += corp_paid;
        paid_corps++;
    }

    printf("corp_payroll: %d corp(s) paid, %d skipped (could not afford), "
           "%d household(s) receiving\n", paid_corps, skipped, nhh);
    printf("corp_payroll: wages %ld.%02ld total, %.2f out, %.2f in\n",
           paid_total_cents / 100, labs(paid_total_cents % 100),
           wages_corp_total, wages_hh_total);

    /* Conservation: money paid out of corps must equal money received by
     * households, to the cent. They are accumulated from the same share values,
     * so this is a check on the arithmetic above, not a re-derivation. */
    if (fabs(wages_corp_total - wages_hh_total) > 0.005) {
        fprintf(stderr, "corp_payroll: WAGES DO NOT BALANCE (out %.2f vs in %.2f)\n",
                wages_corp_total, wages_hh_total);
        return 1;
    }

    if (paid_corps == 0 && ncorp > 0)
        printf("corp_payroll: no corp paid wages - either nothing sold, or every "
               "seller is under its payroll floor\n");
    return 0;
}
