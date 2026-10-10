/* auction_manager - Auction HQ (X11-HQ window module), same three-part shape as chain-hq/exchange-hq.
 * ONE append-only ledger auction/ledger.txt; the state of every auction (open, high bid, closed, sold, cancelled) is DERIVED by replaying it, never stored twice:
 *   LIST|id|ts|seller|kind|ref|unit|min|buyout|ends_at
 *   BID|id|ts|bidder|amount        BUY|id|ts|buyer        CANCEL|id|ts|seller
 * Rejected commands go to auction/rejects.txt (not the ledger). Ownership/NFT checks and wallet settlement are NOT built yet (paper ledger).
 * Humans and AI entities use the same command: cmd=AUC:<actor>|<verb>|args  (the window's cli_io shim uses the login name as actor).
 *   LIST kind ref unit min buyout minutes     BID id amount     BUY id     CANCEL id
 * Cmds: TAB:<open|sell|bid|history>  REFRESH  AUC:<actor>|<line>.   Self-contained, no shared headers. Usage: auction_manager.+x <house_root> <package_dir> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#define PL 4096
#define MAXA 600
#define SHOW 12
static char pkg[PL], house[PL], cur_tab[16] = "open", msg[300] = "";
static long min_min = 1, max_min = 1440, min_inc = 1, snipe = 0;
typedef struct { long id, ts, ends, min, buyout, hi, hits; char seller[48], kind[16], ref[48], unit[16], hibidder[48], buyer[48]; int cancelled, bought; } Auc;
static Auc A[MAXA]; static int nA = 0; static long next_id = 1;

static void trim(char *s) { char *a = s; while (*a == ' ' || *a == '\t') a++; memmove(s, a, strlen(a) + 1); size_t n = strlen(s); while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0; }
static void path_of(char *o, const char *rel) { snprintf(o, PL, "%s/%s", pkg, rel); }
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) trim(f[i]); return n; }
static void load_rules(void) { char p[PL], line[400]; path_of(p, "auction.pdl"); FILE *f = fopen(p, "r"); if (!f) return; while (fgets(line, sizeof line, f)) { if (line[0] == '#') continue; char *c = strstr(line, "   #"); if (c) *c = 0; char *fl[5]; int n = split(line, fl, 5); if (n < 3 || strcmp(fl[0], "RULE")) continue;
        if (!strcmp(fl[1], "min_minutes")) min_min = atol(fl[2]); else if (!strcmp(fl[1], "max_minutes")) max_min = atol(fl[2]); else if (!strcmp(fl[1], "min_increment")) min_inc = atol(fl[2]); else if (!strcmp(fl[1], "snipe_minutes")) snipe = atol(fl[2]); } fclose(f); }
static Auc *find(long id) { for (int i = 0; i < nA; i++) if (A[i].id == id) return &A[i]; return NULL; }
static void replay(void) { char p[PL], line[600]; path_of(p, "auction/ledger.txt"); FILE *f = fopen(p, "r"); nA = 0; next_id = 1; if (!f) return;
    while (fgets(line, sizeof line, f)) { char *fl[12]; int n = split(line, fl, 12); if (n < 4) continue;
        if (!strcmp(fl[0], "LIST") && n >= 10 && nA < MAXA) { Auc *a = &A[nA++]; memset(a, 0, sizeof *a); a->id = atol(fl[1]); a->ts = atol(fl[2]); snprintf(a->seller, 48, "%s", fl[3]); snprintf(a->kind, 16, "%s", fl[4]); snprintf(a->ref, 48, "%s", fl[5]); snprintf(a->unit, 16, "%s", fl[6]); a->min = atol(fl[7]); a->buyout = atol(fl[8]); a->ends = atol(fl[9]); if (a->id >= next_id) next_id = a->id + 1; }
        else if (!strcmp(fl[0], "BID") && n >= 5) { Auc *a = find(atol(fl[1])); if (a) { a->hi = atol(fl[4]); snprintf(a->hibidder, 48, "%s", fl[3]); a->hits++; if (snipe > 0 && a->ends - atol(fl[2]) < snipe * 60) a->ends = atol(fl[2]) + snipe * 60; } }
        else if (!strcmp(fl[0], "BUY")) { Auc *a = find(atol(fl[1])); if (a) { a->bought = 1; snprintf(a->buyer, 48, "%s", fl[3]); } }
        else if (!strcmp(fl[0], "CANCEL")) { Auc *a = find(atol(fl[1])); if (a) a->cancelled = 1; } } fclose(f); }
static const char *state_of(Auc *a, time_t now) { if (a->cancelled) return "cancelled"; if (a->bought) return "sold (buyout)"; if (now < a->ends) return "open"; return a->hits ? "sold" : "ended unsold"; }
static void append(const char *rel, const char *s) { char p[PL]; path_of(p, rel); FILE *f = fopen(p, "a"); if (f) { fputs(s, f); fclose(f); } }
static void reject(const char *actor, const char *why, const char *line) { char b[500]; snprintf(b, sizeof b, "%ld|%s|%s|%s\n", (long)time(NULL), actor, why, line); append("auction/rejects.txt", b); snprintf(msg, sizeof msg, "refused: %s", why); }
static int known(const char *section, const char *id) { char p[PL], line[300]; path_of(p, "auction.pdl"); FILE *f = fopen(p, "r"); if (!f) return 0; int ok = 0; while (fgets(line, sizeof line, f)) { if (line[0] == '#') continue; char *fl[5]; int n = split(line, fl, 5); if (n >= 2 && !strcmp(fl[0], section) && !strcmp(fl[1], id)) ok = 1; } fclose(f); return ok; }
static void do_auc(const char *arg) { char b[400]; snprintf(b, sizeof b, "%s", arg); char *bar = strchr(b, '|'); const char *actor = "human"; char *line = b; char ab[48]; if (bar) { *bar = 0; snprintf(ab, 48, "%s", b); actor = ab; line = bar + 1; } trim(line);
    char verb[16] = ""; sscanf(line, "%15s", verb); char *rest = line + strlen(verb); time_t now = time(NULL); replay(); char out[500];
    if (!strcasecmp(verb, "LIST")) { char kind[16] = "", ref[48] = "", unit[16] = ""; long mn = 0, bo = 0, mins = 0; if (sscanf(rest, "%15s %47s %15s %ld %ld %ld", kind, ref, unit, &mn, &bo, &mins) < 6) { reject(actor, "usage: LIST kind ref unit min buyout minutes", line); return; }
        if (!known("KIND", kind)) { reject(actor, "unknown kind", line); return; } if (!known("UNIT", unit)) { reject(actor, "unknown unit", line); return; } if (mn <= 0) { reject(actor, "min must be above 0", line); return; } if (bo < 0 || (bo && bo < mn)) { reject(actor, "buyout must be 0 or at least min", line); return; } if (mins < min_min || mins > max_min) { reject(actor, "minutes outside the allowed range", line); return; }
        snprintf(out, sizeof out, "LIST|%ld|%ld|%s|%s|%s|%s|%ld|%ld|%ld\n", next_id, (long)now, actor, kind, ref, unit, mn, bo, (long)now + mins * 60); append("auction/ledger.txt", out); snprintf(msg, sizeof msg, "#%ld listed by %s: %s %s, min %ld %s, %ld min", next_id, actor, kind, ref, mn, unit, mins); }
    else if (!strcasecmp(verb, "BID")) { long id = 0, amt = 0; if (sscanf(rest, "%ld %ld", &id, &amt) < 2) { reject(actor, "usage: BID id amount", line); return; } Auc *a = find(id); if (!a) { reject(actor, "no such auction", line); return; } const char *st = state_of(a, now); if (strcmp(st, "open")) { reject(actor, "auction is not open", line); return; }
        if (!strcmp(a->seller, actor)) { reject(actor, "seller cannot bid on their own auction", line); return; } if (amt < a->min) { reject(actor, "below the minimum", line); return; } if (a->hits && amt < a->hi + min_inc) { reject(actor, "must beat the high bid by the minimum increment", line); return; }
        snprintf(out, sizeof out, "BID|%ld|%ld|%s|%ld\n", id, (long)now, actor, amt); append("auction/ledger.txt", out); snprintf(msg, sizeof msg, "%s bid %ld %s on #%ld", actor, amt, a->unit, id); }
    else if (!strcasecmp(verb, "BUY")) { long id = 0; if (sscanf(rest, "%ld", &id) < 1) { reject(actor, "usage: BUY id", line); return; } Auc *a = find(id); if (!a) { reject(actor, "no such auction", line); return; } if (strcmp(state_of(a, now), "open")) { reject(actor, "auction is not open", line); return; } if (!a->buyout) { reject(actor, "no buyout price on this auction", line); return; } if (!strcmp(a->seller, actor)) { reject(actor, "seller cannot buy their own item", line); return; }
        snprintf(out, sizeof out, "BUY|%ld|%ld|%s\n", id, (long)now, actor); append("auction/ledger.txt", out); snprintf(msg, sizeof msg, "%s bought #%ld at the buyout price %ld %s", actor, id, a->buyout, a->unit); }
    else if (!strcasecmp(verb, "CANCEL")) { long id = 0; if (sscanf(rest, "%ld", &id) < 1) { reject(actor, "usage: CANCEL id", line); return; } Auc *a = find(id); if (!a) { reject(actor, "no such auction", line); return; } if (strcmp(a->seller, actor)) { reject(actor, "only the seller can cancel", line); return; } if (strcmp(state_of(a, now), "open") || a->hits) { reject(actor, "cannot cancel once bidding has started or it has ended", line); return; }
        snprintf(out, sizeof out, "CANCEL|%ld|%ld|%s\n", id, (long)now, actor); append("auction/ledger.txt", out); snprintf(msg, sizeof msg, "#%ld cancelled", id); }
    else reject(actor, "verbs: LIST BID BUY CANCEL", line); }
static void hhmm(char *o, size_t n, long secs) { if (secs < 0) secs = 0; if (secs >= 3600) snprintf(o, n, "%ldh%02ldm", secs / 3600, (secs % 3600) / 60); else snprintf(o, n, "%ldm%02lds", secs / 60, secs % 60); }
static void write_ui(void) { char tmp[PL], dst[PL]; path_of(dst, "auction_ui.txt"); snprintf(tmp, PL, "%s.tmp", dst); FILE *f = fopen(tmp, "w"); if (!f) return; time_t now = time(NULL);
    const char *tabs[] = { "open", "sell", "bid", "history" }; for (int i = 0; i < 4; i++) { int a = !strcmp(cur_tab, tabs[i]); fprintf(f, "tab_%s=%s\ncls_%s=%s\n", tabs[i], a ? "1" : "", tabs[i], a ? "tab-active" : ""); }
    int no = 0, nh = 0; for (int i = 0; i < nA; i++) { if (!strcmp(state_of(&A[i], now), "open")) no++; else nh++; }
    fprintf(f, "status=%s\ntitle=Auction  ·  %d open  ·  %d done\n", msg[0] ? msg : "paper ledger (wallet/NFT settlement not built yet); state is replayed from auction/ledger.txt", no, nh);
    fprintf(f, "open_hdr=%-3s %-5s %-10s %-5s %7s %6s %-8s %7s\n", "id", "kind", "item", "unit", "hi/min", "buyout", "seller", "ends");
    int k = 0; for (int i = nA - 1; i >= 0 && k < SHOW; i--) { Auc *a = &A[i]; if (strcmp(state_of(a, now), "open")) continue; char e[16]; hhmm(e, 16, a->ends - now); char hb[40]; if (a->hits) snprintf(hb, 40, "%ld*", a->hi); else snprintf(hb, 40, "%ld", a->min); char bo[24]; if (a->buyout) snprintf(bo, 24, "%ld", a->buyout); else snprintf(bo, 24, "-");
        fprintf(f, "o_%d_text=%-3ld %-5.5s %-10.10s %-5.5s %7s %6s %-8.8s %7s\n", k, a->id, a->kind, a->ref, a->unit, hb, bo, a->seller, e); k++; }
    fprintf(f, "n_o=%d\nopen_empty=%s\n", k, k ? "" : "No open auctions. Use the Sell tab: LIST food apple cones 5 20 60");
    k = 0; for (int i = nA - 1; i >= 0 && k < SHOW; i--) { Auc *a = &A[i]; const char *st = state_of(a, now); if (!strcmp(st, "open")) continue; char who[60] = "-"; if (a->bought) snprintf(who, 60, "%s", a->buyer); else if (!a->cancelled && a->hits) snprintf(who, 60, "%s @%ld", a->hibidder, a->hi);
        fprintf(f, "h_%d_text=#%-3ld %-5.5s %-10.10s %-13s %-8.8s -> %s\n", k, a->id, a->kind, a->ref, st, a->seller, who); k++; } fprintf(f, "n_h=%d\nhist_empty=%s\n", k, k ? "" : "Nothing has finished yet.");
    fclose(f); rename(tmp, dst); }
static void refresh(void) { load_rules(); replay(); write_ui(); }
static void do_cmd(const char *cmd) { if (!strncmp(cmd, "TAB:", 4)) { snprintf(cur_tab, sizeof cur_tab, "%s", cmd + 4); msg[0] = 0; } else if (!strncmp(cmd, "AUC:", 4)) do_auc(cmd + 4); refresh(); }
static void poll(int *last) { char p[PL], buf[1500]; path_of(p, "auction_action.txt"); FILE *f = fopen(p, "r"); if (!f) return; size_t nr = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[nr] = 0; int seq = 0; char cmd[1024] = "";
    for (char *ls = buf; *ls;) { char *le = strchr(ls, '\n'); size_t ll = le ? (size_t)(le - ls) : strlen(ls); if (!strncmp(ls, "seq=", 4)) seq = atoi(ls + 4); else if (!strncmp(ls, "cmd=", 4)) { size_t cl = ll - 4; if (cl >= sizeof cmd) cl = sizeof cmd - 1; memcpy(cmd, ls + 4, cl); cmd[cl] = 0; } if (!le) break; ls = le + 1; }
    if (seq > *last && cmd[0]) { *last = seq; do_cmd(cmd); } }
static void bye(int s) { (void)s; _exit(0); }
int main(int argc, char **argv) { if (argc < 3) { fprintf(stderr, "Usage: %s <house_root> <package_dir>\n", argv[0]); return 1; } snprintf(house, PL, "%s", argv[1]); snprintf(pkg, PL, "%s", argv[2]); signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye);
    { char p[PL]; path_of(p, "auction"); mkdir(p, 0755); path_of(p, "auction_action.txt"); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); } }
    refresh(); int last = 0, slow = 0; for (;;) { usleep(50000); poll(&last); if (++slow >= 40) { slow = 0; refresh(); } } }
