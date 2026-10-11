/* phrase_lint - the JUDGE of pilot quest q019 (Eden talk phrases). Deterministic; a model never decides.
 * usage: phrase_lint <original.pdl> <candidate.pdl> <min_total> <min_farm> <min_questions>
 * Rules (each prints LINT|FAIL|<rule>|<detail>; exit 0 only when none failed; last line LINT|VERDICT|PASS|FAIL|rules=N|failed=M):
 *   prefix    the candidate starts with the original's exact bytes (original rows untouched, same order)
 *   format    every appended non-blank line is exactly `PHRASE | <text>` (one space each side of the bar)
 *   chars     text uses only letters, digits, space and ' , . ? !  (no bar, tab, quotes, non-ASCII); starts with an uppercase letter
 *   length    text is 15..80 characters
 *   dup       no two phrases equal ignoring case (original rows included)
 *   total     PHRASE rows in the candidate >= min_total and <= 60
 *   farm      >= min_farm appended phrases contain a farm word (whole word, case-insensitive)
 *   questions >= min_questions appended phrases contain a '?'
 *   banned    no banned word (whole word, case-insensitive)
 *   variety   no more than 6 appended phrases start with the same first word
 * Self-contained C; no system(), no popen(). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <strings.h>

#define MAXR 256
static char *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)l + 1); if (!b) { fclose(f); return NULL; } size_t r = fread(b, 1, (size_t)l, f); fclose(f); b[r] = 0; *n = r; return b; }
static int g_rules = 0, g_failed = 0;
static void ok(const char *r) { g_rules++; printf("LINT|PASS|%s|\n", r); }
static void bad(const char *r, const char *fmt, ...) { g_rules++; g_failed++; printf("LINT|FAIL|%s|", r); va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); printf("\n"); }
static const char *FARM[] = { "grain", "seed", "seeds", "rain", "plot", "jug", "hut", "chicken", "chickens", "egg", "eggs", "field", "water", "hoe", "spade", "barn", "cow", "horse", "harvest", "plant", "dig", "gold", "silver", "stone", "clay", "garden", "milk", NULL };
static const char *BAN[] = { "damn", "hell", "shit", "fuck", "ass", "bitch", "crap", NULL };
static int has_word(const char *t, const char *w) {
    size_t wl = strlen(w);
    for (const char *p = t; *p; p++) {
        if (strncasecmp(p, w, wl) == 0 && (p == t || !isalpha((unsigned char)p[-1])) && !isalpha((unsigned char)p[wl])) return 1; }
    return 0; }
typedef struct { char t[160]; int appended; } Ph;
static Ph PH[MAXR]; static int nPH = 0;
static void add_phrase(const char *t, int appended) { if (nPH < MAXR) { snprintf(PH[nPH].t, sizeof PH[nPH].t, "%s", t); PH[nPH].appended = appended; nPH++; } }

int main(int argc, char **argv) {
    if (argc != 6) { fprintf(stderr, "usage: phrase_lint <original> <candidate> <min_total> <min_farm> <min_questions>\n"); return 2; }
    size_t no, nc; char *o = slurp(argv[1], &no), *c = slurp(argv[2], &nc);
    if (!o || !c) { printf("LINT|FAIL|input|cannot read a file\nLINT|VERDICT|FAIL|rules=1|failed=1\n"); return 1; }
    int min_total = atoi(argv[3]), min_farm = atoi(argv[4]), min_q = atoi(argv[5]);
    /* original phrases */
    { char *s = strdup(o), *save = NULL; for (char *ln = strtok_r(s, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) if (!strncmp(ln, "PHRASE | ", 9)) add_phrase(ln + 9, 0); free(s); }
    /* prefix */
    if (nc >= no && memcmp(o, c, no) == 0) ok("prefix"); else { bad("prefix", "candidate does not start with the original bytes"); printf("LINT|VERDICT|FAIL|rules=%d|failed=%d\n", g_rules, g_failed); return 1; }
    const char *tail = c + no; if (no && o[no - 1] != '\n' && *tail == '\n') tail++;   /* tolerate the newline that finishes the original's last line */
    int fmt_bad = 0, chars_bad = 0, len_bad = 0, nApp = 0;
    { char *s = strdup(tail), *save = NULL;
      for (char *ln = strtok_r(s, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
          char *e = ln + strlen(ln); while (e > ln && (e[-1] == '\r' || e[-1] == ' ')) e--; if (e == ln) continue;
          if (strncmp(ln, "PHRASE | ", 9) != 0 || ln[9] == ' ') { fmt_bad++; printf("LINT|NOTE|format|bad line: %.60s\n", ln); continue; }
          char txt[160]; snprintf(txt, sizeof txt, "%.*s", (int)(e - (ln + 9)), ln + 9);
          size_t L = strlen(txt); if (L < 15 || L > 80) { len_bad++; printf("LINT|NOTE|length|%zu chars: %.40s\n", L, txt); }
          int cb = !isupper((unsigned char)txt[0]);
          for (size_t i = 0; i < L; i++) { unsigned char ch = (unsigned char)txt[i]; if (!(isalnum(ch) || ch == ' ' || ch == '\'' || ch == ',' || ch == '.' || ch == '?' || ch == '!')) cb = 1; }
          if (cb) { chars_bad++; printf("LINT|NOTE|chars|%.60s\n", txt); }
          add_phrase(txt, 1); nApp++; } free(s); }
    if (fmt_bad) bad("format", "%d bad line(s)", fmt_bad); else ok("format");
    if (chars_bad) bad("chars", "%d phrase(s)", chars_bad); else ok("chars");
    if (len_bad) bad("length", "%d phrase(s) outside 15..80", len_bad); else ok("length");
    int dup = 0; for (int i = 0; i < nPH; i++) for (int j = i + 1; j < nPH; j++) if (strcasecmp(PH[i].t, PH[j].t) == 0) { dup++; printf("LINT|NOTE|dup|%.60s\n", PH[j].t); }
    if (dup) bad("dup", "%d duplicate(s)", dup); else ok("dup");
    if (nPH >= min_total && nPH <= 60) ok("total"); else bad("total", "%d rows (need %d..60)", nPH, min_total);
    int farm = 0, q = 0, ban = 0;
    for (int i = 0; i < nPH; i++) { if (!PH[i].appended) continue;
        for (int k = 0; FARM[k]; k++) if (has_word(PH[i].t, FARM[k])) { farm++; break; }
        if (strchr(PH[i].t, '?')) q++;
        for (int k = 0; BAN[k]; k++) if (has_word(PH[i].t, BAN[k])) { ban++; printf("LINT|NOTE|banned|%.60s\n", PH[i].t); break; } }
    if (farm >= min_farm) ok("farm"); else bad("farm", "%d with a farm word (need %d)", farm, min_farm);
    if (q >= min_q) ok("questions"); else bad("questions", "%d questions (need %d)", q, min_q);
    if (ban) bad("banned", "%d phrase(s)", ban); else ok("banned");
    int worst = 0; for (int i = 0; i < nPH; i++) { if (!PH[i].appended) continue; char fw[40]; size_t k = 0; while (PH[i].t[k] && PH[i].t[k] != ' ' && k < 39) { fw[k] = (char)tolower((unsigned char)PH[i].t[k]); k++; } fw[k] = 0;
        int cnt = 0; for (int j = 0; j < nPH; j++) { if (!PH[j].appended) continue; size_t m = 0; char w2[40]; while (PH[j].t[m] && PH[j].t[m] != ' ' && m < 39) { w2[m] = (char)tolower((unsigned char)PH[j].t[m]); m++; } w2[m] = 0; if (!strcmp(fw, w2)) cnt++; }
        if (cnt > worst) worst = cnt; }
    if (worst <= 6) ok("variety"); else bad("variety", "%d phrases share a first word (max 6)", worst);
    printf("LINT|VERDICT|%s|rules=%d|failed=%d\n", g_failed ? "FAIL" : "PASS", g_rules, g_failed);
    free(o); free(c); return g_failed ? 1 : 0;
}
