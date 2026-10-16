/* wer - word error rate: wer <reference text> <hypothesis text>        (BOOK 19.audio/LISTENER-STT-DESIGN.md section 5)
 * Both texts are lower-cased and stripped of everything except letters, digits and apostrophes (hyphens split words), then compared word by word with edit distance (substitution, insertion, deletion = 1 each).
 * Prints  WER | rate | errors | reference words | S | I | D  and exits 0. An empty reference exits 2 (no rate). Deterministic. Tests: identical = 0, empty hypothesis = 1, one wrong word in four = 0.25. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define MW 2048
static int words(const char *t, char w[][48]) { int n = 0, l = 0; char cur[48]; for (const char *p = t;; p++) { int c = (unsigned char)*p; if (isalnum(c) || c == '\'') { if (l < 47) cur[l++] = (char)tolower(c); } else { if (l && n < MW) { cur[l] = 0; strcpy(w[n++], cur); } l = 0; if (!c) break; } } return n; }
static char R[MW][48], H[MW][48]; static int D[MW + 1][MW + 1];
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: wer \"reference\" \"hypothesis\"\n"); return 2; } int nr = words(argv[1], R), nh = words(argv[2], H); if (!nr) { fprintf(stderr, "empty reference\n"); return 2; }
    for (int i = 0; i <= nr; i++) D[i][0] = i; for (int j = 0; j <= nh; j++) D[0][j] = j;
    for (int i = 1; i <= nr; i++) for (int j = 1; j <= nh; j++) { int c = strcmp(R[i - 1], H[j - 1]) ? 1 : 0, a = D[i - 1][j - 1] + c, b = D[i - 1][j] + 1, d = D[i][j - 1] + 1; D[i][j] = a < b ? (a < d ? a : d) : (b < d ? b : d); }
    int i = nr, j = nh, S = 0, I = 0, De = 0; while (i > 0 || j > 0) { if (i > 0 && j > 0 && D[i][j] == D[i - 1][j - 1] + (strcmp(R[i - 1], H[j - 1]) ? 1 : 0)) { if (strcmp(R[i - 1], H[j - 1])) S++; i--; j--; } else if (i > 0 && D[i][j] == D[i - 1][j] + 1) { De++; i--; } else { I++; j--; } }
    printf("WER | %.4f | %d | %d | %d | %d | %d\n", (double)D[nr][nh] / nr, D[nr][nh], nr, S, I, De); return 0;
}
