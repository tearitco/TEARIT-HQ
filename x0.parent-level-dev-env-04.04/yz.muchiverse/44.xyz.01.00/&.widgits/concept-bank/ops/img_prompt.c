/* img_prompt - build one text-to-image prompt for a brixel part from the image word bank (W0, BOOK 16.game/WORD-BANK-TO-IMAGE.md).
 * Usage: img_prompt <words.pdl> <parts.pdl> <brixel> [min_weight]   (default cutoff 0.30)
 * Prints PROMPT / NEGATIVE / USED / DROPPED. Order inside the prompt: subject, material, colour, angle, style, size; within a kind by weight, high first, ties by file order.
 * Exit 0 ok, 1 invalid bank or unknown brixel/word, 2 cannot open. Pure: reads two files, writes stdout, never a model call. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXW 256
typedef struct { char id[40], text[96], kind[12]; double w; int ord; } Word;
static Word W[MAXW]; static int nW;
static const char *KINDS[] = { "subject", "material", "colour", "angle", "style", "size", "negative" };
static int kidx(const char *k) { for (int i = 0; i < 7; i++) if (!strcmp(KINDS[i], k)) return i; return -1; }
static int split(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) { while (*f[i] == ' ') f[i]++; char *e = f[i] + strlen(f[i]); while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }
static Word *find(const char *id) { for (int i = 0; i < nW; i++) if (!strcmp(W[i].id, id)) return &W[i]; return 0; }
static int cmp(const void *a, const void *b) { const Word *x = *(Word *const *)a, *y = *(Word *const *)b; int kx = kidx(x->kind), ky = kidx(y->kind); if (kx != ky) return kx - ky; if (x->w != y->w) return x->w < y->w ? 1 : -1; return x->ord - y->ord; }
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: img_prompt words.pdl parts.pdl brixel [min_weight]\n"); return 2; }
    double cut = argc > 4 ? atof(argv[4]) : 0.30; char line[2048], *f[8]; int ln = 0, bad = 0;
    FILE *fp = fopen(argv[1], "r"); if (!fp) { perror(argv[1]); return 2; }
    while (fgets(line, sizeof line, fp)) { ln++; if (line[0] == '#' || strncmp(line, "WORD", 4)) continue; int n = split(line, f, 8); if (n < 5 || strcmp(f[0], "WORD")) { printf("ERR %s:%d short WORD row\n", argv[1], ln); bad++; continue; }
        if (nW >= MAXW) break; Word *w = &W[nW]; memset(w, 0, sizeof *w); snprintf(w->id, 40, "%s", f[1]); snprintf(w->text, 96, "%s", f[2]); snprintf(w->kind, 12, "%s", f[3]); w->w = atof(f[4]); w->ord = nW;
        if (find(w->id)) { printf("ERR %s:%d duplicate word id %s\n", argv[1], ln, w->id); bad++; continue; } if (kidx(w->kind) < 0) { printf("ERR %s:%d bad kind %s\n", argv[1], ln, w->kind); bad++; } if (w->w < 0 || w->w > 1) { printf("ERR %s:%d weight outside 0..1 for %s\n", argv[1], ln, w->id); bad++; } nW++; }
    fclose(fp);
    fp = fopen(argv[2], "r"); if (!fp) { perror(argv[2]); return 2; }
    Word *pick[MAXW]; int np = 0, found = 0; ln = 0; char dropped[1024] = "";
    while (fgets(line, sizeof line, fp)) { ln++; if (line[0] == '#' || strncmp(line, "PART", 4)) continue; int n = split(line, f, 8); if (n < 3 || strcmp(f[0], "PART")) { printf("ERR %s:%d short PART row\n", argv[2], ln); bad++; continue; } if (strcmp(f[1], argv[3])) continue; found = 1;
        for (char *t = strtok(f[2], ","); t; t = strtok(0, ",")) { while (*t == ' ') t++; Word *w = find(t); if (!w) { printf("ERR %s:%d unknown word id %s\n", argv[2], ln, t); bad++; continue; } if (w->w < cut) { strncat(dropped, w->id, sizeof dropped - strlen(dropped) - 2); strcat(dropped, " "); continue; } if (np < MAXW) pick[np++] = w; } }
    fclose(fp);
    if (!found) { printf("ERR no PART row for brixel %s\n", argv[3]); bad++; }
    if (bad) { printf("INVALID: %d errors\n", bad); return 1; }
    qsort(pick, np, sizeof *pick, cmp);
    char pos[1024] = "", neg[512] = "", used[1024] = ""; for (int i = 0; i < np; i++) { char *d = !strcmp(pick[i]->kind, "negative") ? neg : pos; if (d[0]) strcat(d, ", "); strcat(d, pick[i]->text); char u[80]; snprintf(u, sizeof u, "%s(%.2f) ", pick[i]->id, pick[i]->w); strcat(used, u); }
    printf("PROMPT: %s\nNEGATIVE: %s\nUSED: %s\nDROPPED(<%.2f): %s\n", pos, neg, used, cut, dropped[0] ? dropped : "-");
    return 0;
}
