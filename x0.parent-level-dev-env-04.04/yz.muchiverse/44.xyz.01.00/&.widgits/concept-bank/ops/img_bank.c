/* img_bank.c - the image word bank as a text-include library (house convention: pure code with two consumers is text-included, never header + link).
 * Consumers: img_prompt.c (builds a prompt, optionally with an overlay of candidate edits) and img_edit.c (validates and promotes candidate edits).
 * Files it reads/writes (all under data/img/): words.pdl  WORD | id | text | kind | weight        parts.pdl  PART | brixel | word ids (comma list) | note
 * Candidate edits (data/img/candidates/<name>.cand), one per line, same style as the spoke edits in data/candidates/:
 *   EDIT | id=<unique> | type=img_word_weight_delta | target=<word id> | delta=<-0.5..+0.5> | reason="..." | proposer=<name> | status=candidate
 *   EDIT | id=<unique> | type=img_word_text         | target=<word id> | text="new words"                 | reason="..." | proposer=<name> | status=candidate
 *   EDIT | id=<unique> | type=img_part_words        | target=<brixel>  | words=id1,id2,...               | reason="..." | proposer=<name> | status=candidate
 *   EDIT | id=<unique> | type=img_word_add          | target=<new word id> | kind=<kind> | text="..." | weight=<0..1> | reason="..." | proposer=<name> | status=candidate
 * Rules: nothing here writes the bank except ib_promote(), which a human asks for by name and which refuses promoter == proposer. A model proposes; the validator checks; a human promotes.
 * Prefix ib_. No globals leak: the including file owns main(). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define IB_MAXW 256
#define IB_MAXP 32
#define IB_MAXPW 16
#define IB_MAXE 128
typedef struct { char id[40], text[96], kind[12]; double w; int ord; } IbWord;
typedef struct { char brixel[40]; char ids[IB_MAXPW][40]; int n; char note[96]; } IbPart;
typedef struct { char id[48], type[32], target[40], text[96], words[300], kind[12], reason[200], proposer[40]; double delta, weight; int has_delta, has_weight, line; } IbEdit;
static int ib_quiet;   /* 1 = ib_validate_edits prints only REJECT lines (img_prompt uses this so stdout stays PROMPT/NEGATIVE/USED/DROPPED) */
static IbWord IB_W[IB_MAXW]; static int ib_nW;
static IbPart IB_P[IB_MAXP]; static int ib_nP;
static IbEdit IB_E[IB_MAXE]; static int ib_nE;
static const char *IB_KINDS[] = { "subject", "material", "colour", "angle", "style", "size", "negative" };
static int ib_kind_index(const char *k) { for (int i = 0; i < 7; i++) if (!strcmp(IB_KINDS[i], k)) return i; return -1; }
static int ib_split(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } for (int i = 0; i < n; i++) { while (*f[i] == ' ') f[i]++; char *e = f[i] + strlen(f[i]); while (e > f[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; } return n; }
static IbWord *ib_word(const char *id) { for (int i = 0; i < ib_nW; i++) if (!strcmp(IB_W[i].id, id)) return &IB_W[i]; return NULL; }
static IbPart *ib_part(const char *b) { for (int i = 0; i < ib_nP; i++) if (!strcmp(IB_P[i].brixel, b)) return &IB_P[i]; return NULL; }
/* load words.pdl; returns the number of errors (duplicate id, bad kind, weight outside 0..1, short row); prints ERR lines */
static int ib_load_words(const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) { perror(path); return -1; } char line[2048], *f[8]; int ln = 0, bad = 0; ib_nW = 0;
    while (fgets(line, sizeof line, fp)) { ln++; if (line[0] == '#' || strncmp(line, "WORD", 4)) continue; int n = ib_split(line, f, 8);
        if (n < 5 || strcmp(f[0], "WORD")) { printf("ERR %s:%d short WORD row\n", path, ln); bad++; continue; } if (ib_nW >= IB_MAXW) break;
        IbWord *w = &IB_W[ib_nW]; memset(w, 0, sizeof *w); snprintf(w->id, 40, "%s", f[1]); snprintf(w->text, 96, "%s", f[2]); snprintf(w->kind, 12, "%s", f[3]); w->w = atof(f[4]); w->ord = ib_nW;
        if (ib_word(w->id)) { printf("ERR %s:%d duplicate word id %s\n", path, ln, w->id); bad++; continue; }
        if (ib_kind_index(w->kind) < 0) { printf("ERR %s:%d bad kind %s\n", path, ln, w->kind); bad++; }
        if (w->w < 0 || w->w > 1) { printf("ERR %s:%d weight outside 0..1 for %s\n", path, ln, w->id); bad++; }
        ib_nW++; }
    fclose(fp); return bad;
}
/* load parts.pdl (names the word ids per brixel part); unknown word ids are reported by ib_check_parts, not here */
static int ib_load_parts(const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) { perror(path); return -1; } char line[2048], *f[8]; int ln = 0, bad = 0; ib_nP = 0;
    while (fgets(line, sizeof line, fp)) { ln++; if (line[0] == '#' || strncmp(line, "PART", 4)) continue; int n = ib_split(line, f, 8); if (n < 3 || strcmp(f[0], "PART")) { printf("ERR %s:%d short PART row\n", path, ln); bad++; continue; } if (ib_nP >= IB_MAXP) break;
        IbPart *p = &IB_P[ib_nP++]; memset(p, 0, sizeof *p); snprintf(p->brixel, 40, "%s", f[1]); snprintf(p->note, 96, "%s", n >= 4 ? f[3] : "");
        for (char *t = strtok(f[2], ","); t && p->n < IB_MAXPW; t = strtok(0, ",")) { while (*t == ' ') t++; snprintf(p->ids[p->n++], 40, "%s", t); } }
    fclose(fp); return bad;
}
static int ib_check_parts(void) { int bad = 0; for (int i = 0; i < ib_nP; i++) for (int k = 0; k < IB_P[i].n; k++) if (!ib_word(IB_P[i].ids[k])) { printf("ERR part %s: unknown word id %s\n", IB_P[i].brixel, IB_P[i].ids[k]); bad++; } return bad; }
/* pull key=value (value may be "quoted") out of one EDIT line's fields; returns 1 when found */
static int ib_field(char **f, int n, const char *key, char *out, size_t cap) {
    size_t kl = strlen(key); for (int i = 1; i < n; i++) if (!strncmp(f[i], key, kl) && f[i][kl] == '=') { const char *v = f[i] + kl + 1; size_t l = strlen(v); if (l >= 2 && v[0] == '"' && v[l - 1] == '"') { v++; l -= 2; } if (l >= cap) l = cap - 1; memcpy(out, v, l); out[l] = 0; return 1; } return 0;
}
/* parse a .cand file into IB_E; returns errors (malformed line, duplicate id, missing proposer/reason/status). Semantic checks are in ib_validate_edits. */
static int ib_load_edits(const char *path) {
    FILE *fp = fopen(path, "r"); if (!fp) { perror(path); return -1; } char line[2048], *f[16], tmp[64]; int ln = 0, bad = 0; ib_nE = 0;
    while (fgets(line, sizeof line, fp)) { ln++; if (line[0] == '#' || line[0] == '\n') continue;
        { int q = 0; for (const char *c = line; *c; c++) if (*c == '"') q++; if (q & 1) { printf("ERR %s:%d unbalanced quote (a | inside a quoted value is not allowed: it splits the row)\n", path, ln); bad++; continue; } }   /* counted on the raw text, before ib_split cuts the line at every | */
        int n = ib_split(line, f, 16); if (strcmp(f[0], "EDIT")) { printf("ERR %s:%d line does not start with EDIT\n", path, ln); bad++; continue; } if (ib_nE >= IB_MAXE) break;
        { int broken = 0; for (int i = 1; i < n; i++) { const char *eq = strchr(f[i], '='); if (!eq) { broken = 1; break; } const char *v = eq + 1; size_t vl = strlen(v); if (v[0] == '"' && (vl < 2 || v[vl - 1] != '"')) { broken = 1; break; } }
          if (broken) { printf("ERR %s:%d a field is not key=value or has an unterminated quote (a | inside a value splits the row and is not allowed)\n", path, ln); bad++; continue; } }
        IbEdit *e = &IB_E[ib_nE]; memset(e, 0, sizeof *e); e->line = ln;
        if (!ib_field(f, n, "id", e->id, sizeof e->id) || !ib_field(f, n, "type", e->type, sizeof e->type) || !ib_field(f, n, "target", e->target, sizeof e->target)) { printf("ERR %s:%d needs id=, type= and target=\n", path, ln); bad++; continue; }
        if (!ib_field(f, n, "proposer", e->proposer, sizeof e->proposer) || !e->proposer[0]) { printf("ERR %s:%d (%s) needs proposer=\n", path, ln, e->id); bad++; continue; }
        if (!ib_field(f, n, "reason", e->reason, sizeof e->reason) || !e->reason[0]) { printf("ERR %s:%d (%s) needs reason=\n", path, ln, e->id); bad++; continue; }
        if (!ib_field(f, n, "status", tmp, sizeof tmp) || strcmp(tmp, "candidate")) { printf("ERR %s:%d (%s) status must be candidate\n", path, ln, e->id); bad++; continue; }
        for (int i = 0; i < ib_nE; i++) if (!strcmp(IB_E[i].id, e->id)) { printf("ERR %s:%d duplicate edit id %s\n", path, ln, e->id); bad++; e->id[0] = 0; break; } if (!e->id[0]) continue;
        if (ib_field(f, n, "delta", tmp, sizeof tmp)) { e->delta = atof(tmp); e->has_delta = 1; } if (ib_field(f, n, "weight", tmp, sizeof tmp)) { e->weight = atof(tmp); e->has_weight = 1; }
        ib_field(f, n, "text", e->text, sizeof e->text); ib_field(f, n, "words", e->words, sizeof e->words); ib_field(f, n, "kind", e->kind, sizeof e->kind); ib_nE++; }
    fclose(fp); return bad;
}
static int ib_text_ok(const char *t) { size_t l = strlen(t); if (!l || l > 95) return 0; for (const char *c = t; *c; c++) if (*c == '|' || *c == '\n' || *c == '\r') return 0; return 1; }
/* apply ONE edit to the in-memory bank; with check_only=1 it only validates. Returns 0 ok, else a message in err. Edits are applied in file order, so a later edit sees an earlier one. */
static int ib_apply_edit(const IbEdit *e, char *err, size_t cap) {
    if (!strcmp(e->type, "img_word_weight_delta")) { IbWord *w = ib_word(e->target); if (!w) { snprintf(err, cap, "no such word %s", e->target); return 1; } if (!e->has_delta) { snprintf(err, cap, "delta= missing"); return 1; } if (fabs(e->delta) > 0.5) { snprintf(err, cap, "delta %.2f outside +-0.5", e->delta); return 1; }
        double nw = w->w + e->delta; if (nw < -1e-9 || nw > 1 + 1e-9) { snprintf(err, cap, "weight would become %.2f (must stay 0..1)", nw); return 1; } w->w = nw < 0 ? 0 : nw > 1 ? 1 : nw; return 0; }
    if (!strcmp(e->type, "img_word_text")) { IbWord *w = ib_word(e->target); if (!w) { snprintf(err, cap, "no such word %s", e->target); return 1; } if (!ib_text_ok(e->text)) { snprintf(err, cap, "text empty, over 95 chars, or contains | or a newline"); return 1; } snprintf(w->text, 96, "%s", e->text); return 0; }
    if (!strcmp(e->type, "img_part_words")) { IbPart *p = ib_part(e->target); if (!p) { snprintf(err, cap, "no such part %s", e->target); return 1; } char buf[300]; snprintf(buf, sizeof buf, "%s", e->words); char ids[IB_MAXPW][40]; int n = 0, subj = 0;
        for (char *t = strtok(buf, ","); t; t = strtok(0, ",")) { while (*t == ' ') t++; if (n >= IB_MAXPW) { snprintf(err, cap, "more than %d words", IB_MAXPW); return 1; } IbWord *w = ib_word(t); if (!w) { snprintf(err, cap, "unknown word id %s", t); return 1; } for (int i = 0; i < n; i++) if (!strcmp(ids[i], t)) { snprintf(err, cap, "duplicate word id %s", t); return 1; } if (!strcmp(w->kind, "subject")) subj++; snprintf(ids[n++], 40, "%s", t); }
        if (!n || !subj) { snprintf(err, cap, "a part needs at least one subject word"); return 1; } p->n = n; for (int i = 0; i < n; i++) snprintf(p->ids[i], 40, "%s", ids[i]); return 0; }
    if (!strcmp(e->type, "img_word_add")) { if (ib_word(e->target)) { snprintf(err, cap, "word %s already exists", e->target); return 1; } if (ib_nW >= IB_MAXW) { snprintf(err, cap, "word bank full"); return 1; } if (ib_kind_index(e->kind) < 0) { snprintf(err, cap, "bad kind %s", e->kind); return 1; } if (!ib_text_ok(e->text)) { snprintf(err, cap, "text empty, over 95 chars, or contains | or a newline"); return 1; } if (!e->has_weight || e->weight < 0 || e->weight > 1) { snprintf(err, cap, "weight must be given and within 0..1"); return 1; }
        IbWord *w = &IB_W[ib_nW]; memset(w, 0, sizeof *w); snprintf(w->id, 40, "%s", e->target); snprintf(w->text, 96, "%s", e->text); snprintf(w->kind, 12, "%s", e->kind); w->w = e->weight; w->ord = ib_nW++; return 0; }
    snprintf(err, cap, "unknown edit type %s", e->type); return 1;
}
/* validate every loaded edit against the loaded bank (applied in order on the in-memory copy); returns the number of invalid edits */
static int ib_validate_edits(void) { int bad = 0; for (int i = 0; i < ib_nE; i++) { char err[200] = ""; if (ib_apply_edit(&IB_E[i], err, sizeof err)) { printf("REJECT %s (line %d): %s\n", IB_E[i].id, IB_E[i].line, err); bad++; } else if (!ib_quiet) printf("OK     %s (line %d): %s %s\n", IB_E[i].id, IB_E[i].line, IB_E[i].type, IB_E[i].target); } return bad; }
/* write the in-memory bank back over words.pdl/parts.pdl, keeping every comment/header line and replacing only WORD/PART rows (used by promote only) */
static int ib_rewrite(const char *path, int is_words) {
    FILE *in = fopen(path, "r"); if (!in) return 1; char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *out = fopen(tmp, "w"); if (!out) { fclose(in); return 1; } char line[2048]; int wrote_rows = 0;
    while (fgets(line, sizeof line, in)) { if (!strncmp(line, is_words ? "WORD" : "PART", 4)) { if (!wrote_rows) { wrote_rows = 1; if (is_words) for (int i = 0; i < ib_nW; i++) fprintf(out, "WORD | %-15s | %-26s | %-8s | %.2f\n", IB_W[i].id, IB_W[i].text, IB_W[i].kind, IB_W[i].w); else for (int i = 0; i < ib_nP; i++) { fprintf(out, "PART | %s | ", IB_P[i].brixel); for (int k = 0; k < IB_P[i].n; k++) fprintf(out, "%s%s", k ? "," : "", IB_P[i].ids[k]); fprintf(out, " | %s\n", IB_P[i].note); } } continue; } fputs(line, out); }
    fclose(in); fclose(out); return rename(tmp, path);
}
