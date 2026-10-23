/* dust_pdl.c - pure text-include shared by dust_check and dust_seed (two consumers, no state): pipe-row splitting and the safe helpers. Needs stdio/string/ctype. */
#ifndef DUST_PDL_C
#define DUST_PDL_C
static __attribute__((unused)) void cp(char *d, size_t n, const char *s) { size_t l = strlen(s); if (l >= n) l = n - 1; memcpy(d, s, l); d[l] = 0; }      /* explicit truncation: ids longer than the buffer are reported by the length checks below, never overflowed */
static __attribute__((unused)) int split_bar(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) { while (*f[i] == ' ') f[i]++; char *e = f[i] + strlen(f[i]); while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }
static __attribute__((unused)) int ident(const char *s) { if (!*s || strlen(s) > 40) return 0; for (; *s; s++) if (!(islower((unsigned char)*s) || isdigit((unsigned char)*s) || *s == '_')) return 0; return 1; }
#endif
