/* exchange_chart - draws the Exchange HQ chart canvas (candles + volume + bid/ask/average lines, or a depth chart) into an RGBA file the
 * shared renderer blits (<canvas sprite="${chart_raw}">). Pure drawing: it reads ONE input file the manager wrote and knows nothing about
 * ledgers. Output is atomic (tmp + rename): <out>.raw then <out>.receipt.txt (frame_w/frame_h, the pc-hq board convention).
 * Input (text, one record per line):
 *   MODE price|depth      UNIT <id>      SIZE <w> <h>      BID <bp>  ASK <bp>  AVG <bp>   (0 = none)
 *   T <price_bp> <amount>        chronological fills (price mode)
 *   B <price_bp> <cumulative>    bid levels best first      A <price_bp> <cumulative>   ask levels best first   (depth mode)
 * Usage: exchange_chart.+x <input.txt> <out_base>      (writes <out_base>.raw and <out_base>.receipt.txt)
 * Self-contained, no shared headers. Build: gcc -O2 -o exchange_chart.+x exchange_chart.c -lm */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#define MAXP 400
static int W = 560, H = 320, PADB = 0; static unsigned char *fb;
static void px(int x, int y, unsigned c) { if (x < 0 || y < 0 || x >= W || y >= H) return; unsigned char *p = fb + ((size_t)y * W + x) * 4; p[0] = (c >> 16) & 255; p[1] = (c >> 8) & 255; p[2] = c & 255; p[3] = 255; }
static void rect(int x, int y, int w, int h, unsigned c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, c); }
static void vline(int x, int y0, int y1, unsigned c) { if (y0 > y1) { int t = y0; y0 = y1; y1 = t; } for (int y = y0; y <= y1; y++) px(x, y, c); }
static void hline(int x0, int x1, int y, unsigned c, int dash) { if (x0 > x1) { int t = x0; x0 = x1; x1 = t; } for (int x = x0; x <= x1; x++) if (!dash || (x / dash) % 2 == 0) px(x, y, c); }
/* 3x5 digit font, '.' and '-' ; one row per byte (3 bits) */
static const unsigned char GL[12][5] = { {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},{0,0,0,0,2},{0,0,7,0,0} };
static void text(int x, int y, const char *s, unsigned c, int sc) { for (; *s; s++) { int g = (*s >= '0' && *s <= '9') ? *s - '0' : *s == '.' ? 10 : *s == '-' ? 11 : -1; if (g >= 0) for (int r = 0; r < 5; r++) for (int b = 0; b < 3; b++) if (GL[g][r] & (4 >> b)) rect(x + b * sc, y + r * sc, sc, sc, c); x += 4 * sc; } }
static void price_txt(char *o, size_t n, double bp) { snprintf(o, n, "%.4f", bp / 10000.0); }
typedef struct { double p, a; } Pt; static Pt T[MAXP], B[64], A[64]; static int nT = 0, nB = 0, nA = 0; static double bid = 0, ask = 0, avg = 0; static char mode[16] = "price";
static void frame(void) { rect(0, 0, W, H, 0x12151c); }
static void draw_price(void) {
    int pr = 56, top = 8, volh = 52, bot = H - PADB - volh - 8; /* plot area: x 6..W-pr, y top..bot ; volume under it */
    double lo = 1e18, hi = -1e18; int bucket = nT > 48 ? (nT + 47) / 48 : 1, nc = (nT + bucket - 1) / bucket; double O[64], Hh[64], L[64], C[64], V[64];
    for (int c = 0; c < nc; c++) { int a = c * bucket, b = a + bucket; if (b > nT) b = nT; O[c] = T[a].p; C[c] = T[b - 1].p; Hh[c] = L[c] = T[a].p; V[c] = 0; for (int i = a; i < b; i++) { if (T[i].p > Hh[c]) Hh[c] = T[i].p; if (T[i].p < L[c]) L[c] = T[i].p; V[c] += T[i].a; } if (L[c] < lo) lo = L[c]; if (Hh[c] > hi) hi = Hh[c]; }
    double ex[3] = { bid, ask, avg }; for (int i = 0; i < 3; i++) if (ex[i] > 0) { if (ex[i] < lo) lo = ex[i]; if (ex[i] > hi) hi = ex[i]; }
    if (lo > hi) { lo = 0; hi = 1; } if (hi - lo < 1) { hi = lo + 1; } double pad = (hi - lo) * 0.08; lo -= pad; hi += pad; if (lo < 0) lo = 0;
    for (int g = 0; g <= 4; g++) { int y = top + (bot - top) * g / 4; hline(6, W - pr, y, 0x2a3040, 3); char s[24]; price_txt(s, 24, hi - (hi - lo) * g / 4); text(W - pr + 4, y - 2, s, 0x8a93a8, 1); }
    #define Y(v) (top + (int)((hi - (v)) / (hi - lo) * (bot - top)))
    int plotw = W - pr - 6, cw = nc > 0 ? plotw / (nc > 20 ? nc : 20) : 10; if (cw < 3) cw = 3; int vmax = 1; double vm = 1; for (int c = 0; c < nc; c++) if (V[c] > vm) vm = V[c]; (void)vmax;
    for (int c = 0; c < nc; c++) { int x = 6 + c * (plotw / (nc > 20 ? nc : 20)) + cw / 2; unsigned col = C[c] >= O[c] ? 0x3ddc97 : 0xff6b6b; vline(x, Y(Hh[c]), Y(L[c]), col); int y1 = Y(O[c]), y2 = Y(C[c]); if (y1 == y2) y2++; rect(x - cw / 2 + 1, y1 < y2 ? y1 : y2, cw - 2 > 1 ? cw - 2 : 1, abs(y2 - y1), col); int vh = (int)(V[c] / vm * (volh - 4)); rect(x - cw / 2 + 1, H - PADB - 6 - vh, cw - 2 > 1 ? cw - 2 : 1, vh, col == 0x3ddc97 ? 0x1f6b4d : 0x7a3535); }
    if (avg > 0) hline(6, W - pr, Y(avg), 0xffc857, 5); if (bid > 0) hline(6, W - pr, Y(bid), 0x3ddc97, 2); if (ask > 0) hline(6, W - pr, Y(ask), 0xff6b6b, 2);
    if (bid > 0) { char s[24]; price_txt(s, 24, bid); text(W - pr + 4, Y(bid) + 3, s, 0x3ddc97, 1); } if (ask > 0) { char s[24]; price_txt(s, 24, ask); text(W - pr + 4, Y(ask) - 8, s, 0xff6b6b, 1); }
    hline(6, W - pr, bot + 4, 0x2a3040, 0);
}
static void draw_depth(void) {
    int pr = 56, top = 8, bot = H - PADB - 24; double lo = 1e18, hi = -1e18, cmax = 1; for (int i = 0; i < nB; i++) { if (B[i].p < lo) lo = B[i].p; if (B[i].a > cmax) cmax = B[i].a; } for (int i = 0; i < nA; i++) { if (A[i].p > hi) hi = A[i].p; if (A[i].a > cmax) cmax = A[i].a; } if (nB && !nA) hi = bid * 1.02; if (nA && !nB) lo = ask * 0.98; if (lo > hi) { lo = 0; hi = 1; } if (hi - lo < 1) hi = lo + 1;
    double pad = (hi - lo) * 0.05; lo -= pad; hi += pad; int pl = 6, pw = W - pr - 6;
    #define X(v) (pl + (int)(((v) - lo) / (hi - lo) * pw))
    #define YC(v) (bot - (int)((v) / cmax * (bot - top - 4)))
    for (int g = 0; g <= 3; g++) { int y = top + (bot - top) * g / 3; hline(pl, pl + pw, y, 0x2a3040, 3); }
    for (int i = 0; i < nB; i++) { int x0 = X(B[i].p), x1 = i ? X(B[i - 1].p) : X(bid > 0 ? bid : B[i].p); rect(x0, YC(B[i].a), (x1 > x0 ? x1 - x0 : 1), bot - YC(B[i].a), 0x1f6b4d); vline(x0, YC(B[i].a), bot, 0x3ddc97); hline(x0, x1, YC(B[i].a), 0x3ddc97, 0); }
    for (int i = 0; i < nA; i++) { int x0 = i ? X(A[i - 1].p) : X(ask > 0 ? ask : A[i].p), x1 = X(A[i].p); rect(x0, YC(A[i].a), (x1 > x0 ? x1 - x0 : 1), bot - YC(A[i].a), 0x7a3535); vline(x1, YC(A[i].a), bot, 0xff6b6b); hline(x0, x1, YC(A[i].a), 0xff6b6b, 0); }
    if (bid > 0 && ask > 0) { double mid = (bid + ask) / 2; vline(X(mid), top, bot, 0xffc857); char s[24]; price_txt(s, 24, mid); text(X(mid) - 12, bot + 6, s, 0xffc857, 1); }
    char s[24]; price_txt(s, 24, lo); text(pl, bot + 6, s, 0x8a93a8, 1); price_txt(s, 24, hi); text(pl + pw - 22, bot + 6, s, 0x8a93a8, 1); snprintf(s, 24, "%.0f", cmax); text(W - pr + 4, top, s, 0x8a93a8, 1);
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "Usage: %s <input.txt> <out_base>\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "r"); if (!f) return 1; char line[256]; char key[16];
    while (fgets(line, sizeof line, f)) { double a = 0, b = 0; if (sscanf(line, "%15s", key) < 1) continue;
        if (!strcmp(key, "MODE")) sscanf(line, "%*s %15s", mode); else if (!strcmp(key, "SIZE")) { sscanf(line, "%*s %d %d", &W, &H); }
        else if (!strcmp(key, "PADB")) sscanf(line, "%*s %d", &PADB); else if (!strcmp(key, "BID")) sscanf(line, "%*s %lf", &bid); else if (!strcmp(key, "ASK")) sscanf(line, "%*s %lf", &ask); else if (!strcmp(key, "AVG")) sscanf(line, "%*s %lf", &avg);
        else if (!strcmp(key, "T") && sscanf(line, "%*s %lf %lf", &a, &b) == 2 && nT < MAXP) { T[nT].p = a; T[nT].a = b; nT++; }
        else if (!strcmp(key, "B") && sscanf(line, "%*s %lf %lf", &a, &b) == 2 && nB < 64) { B[nB].p = a; B[nB].a = b; nB++; }
        else if (!strcmp(key, "A") && sscanf(line, "%*s %lf %lf", &a, &b) == 2 && nA < 64) { A[nA].p = a; A[nA].a = b; nA++; } }
    fclose(f); if (W < 80 || H < 60 || W > 2000 || H > 1200) return 1; fb = calloc((size_t)W * H, 4); if (!fb) return 1; frame();
    if (!strcmp(mode, "depth")) draw_depth(); else draw_price();
    char rawp[1100], tmp[1120], rcp[1100]; snprintf(rawp, sizeof rawp, "%s.raw", argv[2]); snprintf(tmp, sizeof tmp, "%s.tmp%d", rawp, (int)getpid());
    FILE *o = fopen(tmp, "wb"); if (!o) return 1; fwrite(fb, 1, (size_t)W * H * 4, o); fclose(o); if (rename(tmp, rawp) != 0) { unlink(tmp); return 1; }
    snprintf(rcp, sizeof rcp, "%s.receipt.txt", argv[2]); snprintf(tmp, sizeof tmp, "%s.tmp%d", rcp, (int)getpid());
    o = fopen(tmp, "w"); if (!o) return 1; fprintf(o, "frame_w=%d\nframe_h=%d\n", W, H); fclose(o); if (rename(tmp, rcp) != 0) { unlink(tmp); return 1; }
    free(fb); return 0; }
