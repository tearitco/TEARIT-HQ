/* khtpm_wordbank.c - give every entity a word / synonym bank (zz.wordbank/) and seed it from on-disk sources.
 *
 * Text-included canonical helper (same family as khtpm_phone.c / khtpm_inventory.c): pure file I/O, no drawing.
 * Prefix: wb_.  Design: 08-roadmap/design-docs/ENTITY-WORD-BANK-DESIGN.md.
 * Quest: wordbank v1 (seed, idempotent, dry-run by default, never overwrites SOURCE=user).
 */

#ifndef KHTPM_WORDBANK_C
#define KHTPM_WORDBANK_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <glob.h>

#define WB_UNUSED __attribute__((unused))
#define WB_PATH 4096
#define WB_BUF (WB_PATH + 256)
#define WB_WORDBANK_DIR "zz.wordbank"
#define WB_MAX_LABELS 128
#define WB_MAX_METHODS 128
#define WB_MAX_ALIASES 4096

/* ---- context ---- */
typedef struct {
    int apply;
    FILE *report;
    int n_entities, n_nested, n_have_bank, n_new_bank, n_seeds_added, n_errors;
} WbCtx;

/* rebuild score counts */
typedef struct { char canon[256]; char alias[256]; int reward, punish; } WbScoreCount;

/* ---- small file helpers ---- */
static WB_UNUSED int wb_join(char *out, size_t n, const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la + 1 + lb + 1 > n) { out[0] = '\0'; return 0; }
    memcpy(out, a, la); out[la] = '/'; memcpy(out + la + 1, b, lb + 1); return 1;
}
static WB_UNUSED int wb_exists(const char *p) { return access(p, F_OK) == 0; }
static WB_UNUSED int wb_is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static WB_UNUSED int wb_read_line(const char *path, char *out, size_t n) {
    FILE *f = fopen(path, "r"); out[0] = '\0'; if (!f) return 0;
    if (fgets(out, (int)n, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f); return out[0] != '\0';
}
static WB_UNUSED int wb_write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "w"); if (!f) return 0; fputs(text, f); fclose(f); return 1;
}
static WB_UNUSED int wb_append_file(const char *path, const char *text) {
    FILE *f = fopen(path, "a"); if (!f) return 0; fputs(text, f); fclose(f); return 1;
}
static WB_UNUSED const char *wb_base(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }

/* PAL | <key> | <value> from <dir>/pal.pdl */
static WB_UNUSED int wb_pal_field(const char *dir, const char *key, char *out, size_t n) {
    char p[WB_BUF], line[1024], want[96]; FILE *f;
    out[0] = '\0'; wb_join(p, sizeof(p), dir, "pal.pdl"); snprintf(want, sizeof(want), "PAL | %s | ", key);
    if (!(f = fopen(p, "r"))) return 0;
    while (fgets(line, sizeof(line), f)) if (!strncmp(line, want, strlen(want))) {
        snprintf(out, n, "%s", line + strlen(want)); out[strcspn(out, "\r\n")] = '\0'; break; }
    fclose(f); return out[0] != '\0';
}

/* ---- seed sources ---- */
/* STATE | kind | <kind> from meta.pdl */
static WB_UNUSED int wb_meta_kind(const char *dir, char *out, size_t n) {
    char p[WB_BUF], line[2048]; FILE *f;
    out[0] = '\0'; wb_join(p, sizeof(p), dir, "meta.pdl");
    if (!(f = fopen(p, "r"))) return 0;
    char want[96]; snprintf(want, sizeof(want), "STATE        | kind                 | ");
    while (fgets(line, sizeof(line), f)) if (!strncmp(line, want, strlen(want))) {
        snprintf(out, n, "%s", line + strlen(want)); out[strcspn(out, "\r\n")] = '\0'; break; }
    fclose(f); return out[0] != '\0';
}

/* <item label="..."> from menu.chtpm */
static WB_UNUSED int wb_menu_labels(const char *dir, char labels[][128], size_t max, size_t *out_n) {
    char p[WB_BUF]; FILE *f; char line[4096];
    *out_n = 0; wb_join(p, sizeof(p), dir, "menu.chtpm");
    if (!(f = fopen(p, "r"))) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *item = line;
        while ((item = strstr(item, "<item")) != NULL && *out_n < max) {
            char *gt = strchr(item, '>'); if (!gt) break;
            char *l = strstr(item, "label=\""); if (!l || l > gt) break;
            l += 7; char *end = strchr(l, '"'); if (!end) break;
            size_t len = (size_t)(end - l);
            while (len > 0 && (l[len - 1] == ' ' || l[len - 1] == '\t')) len--;
            if (len >= 128) len = 127;
            memcpy(labels[*out_n], l, len); labels[*out_n][len] = '\0';
            (*out_n)++;
            item = end + 1;
        }
    }
    fclose(f); return 1;
}

/* METHOD | <name> | ... from meta.pdl */
static WB_UNUSED int wb_method_names(const char *dir, char names[][128], size_t max, size_t *out_n) {
    char p[WB_BUF]; FILE *f; char line[2048];
    *out_n = 0; wb_join(p, sizeof(p), dir, "meta.pdl");
    if (!(f = fopen(p, "r"))) return 0;
    while (fgets(line, sizeof(line), f) && *out_n < max) {
        const char *m = strstr(line, "METHOD       | "); if (!m) continue;
        m += 15; const char *end = strchr(m, '|'); if (!end) continue;
        size_t len = (size_t)(end - m);
        while (len > 0 && (m[len - 1] == ' ' || m[len - 1] == '\t')) len--;
        if (len >= 128) len = 127;
        memcpy(names[*out_n], m, len); names[*out_n][len] = '\0';
        (*out_n)++;
    }
    fclose(f); return 1;
}

/* ---- words.txt mirror helpers ---- */
/* check if CANON=<canon>|ALIAS=<alias>| already in words.txt */
static WB_UNUSED int wb_row_exists(const char *words_path, const char *canon, const char *alias) {
    char line[WB_BUF], want[512];
    FILE *f = fopen(words_path, "r"); if (!f) return 0;
    snprintf(want, sizeof(want), "CANON=%s|ALIAS=%s|", canon, alias);
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, want, strlen(want))) { fclose(f); return 1; }
    }
    fclose(f); return 0;
}

/* append one seed row if not present */
static WB_UNUSED void wb_add_seed(WbCtx *c, const char *dir, const char *canon, const char *alias) {
    char wbdir[WB_BUF], words[WB_BUF];
    wb_join(wbdir, sizeof(wbdir), dir, "inventory");
    wb_join(wbdir, sizeof(wbdir), wbdir, WB_WORDBANK_DIR);
    wb_join(words, sizeof(words), wbdir, "words.txt");
    if (wb_row_exists(words, canon, alias)) return;
    if (c->apply) {
        char row[WB_BUF];
        snprintf(row, sizeof(row), "CANON=%s|ALIAS=%s|WEIGHT=0.5|SOURCE=seed\n", canon, alias);
        wb_append_file(words, row);
    }
    c->n_seeds_added++;
}

/* ---- rebuild: derive weights from scores.txt and rewrite words.txt ---- */
static WB_UNUSED void wb_rebuild_mirror(WbCtx *c, const char *dir) {
    char wbdir[WB_BUF], words[WB_BUF], scores[WB_BUF];
    wb_join(wbdir, sizeof(wbdir), dir, "inventory");
    wb_join(wbdir, sizeof(wbdir), wbdir, WB_WORDBANK_DIR);
    wb_join(words, sizeof(words), wbdir, "words.txt");
    wb_join(scores, sizeof(scores), wbdir, "scores.txt");
    if (!wb_exists(words) || !wb_exists(scores)) return;

    WbScoreCount counts[WB_MAX_ALIASES]; size_t nc = 0;
    FILE *f = fopen(scores, "r"); if (!f) return;
    char line[WB_BUF];
    while (fgets(line, sizeof(line), f) && nc < WB_MAX_ALIASES) {
        if (strncmp(line, "SCORE|", 6) != 0) continue;
        char *p = line + 6;
        char *bar1 = strchr(p, '|'); if (!bar1) continue; *bar1 = '\0';
        char *bar2 = strchr(bar1 + 1, '|'); if (!bar2) continue; *bar2 = '\0';
        char *bar3 = strchr(bar2 + 1, '|'); if (!bar3) continue; *bar3 = '\0';
        char *valp = strstr(bar3 + 1, "valence="); if (!valp) continue; valp += 8;
        int valence = 0; if (!strncmp(valp, "+1", 2)) valence = 1; else if (!strncmp(valp, "-1", 2)) valence = -1;
        size_t i;
        for (i = 0; i < nc; i++) if (!strcmp(counts[i].canon, p) && !strcmp(counts[i].alias, bar1 + 1)) break;
        if (i == nc) {
            if (nc >= WB_MAX_ALIASES) continue;
            snprintf(counts[nc].canon, sizeof(counts[nc].canon), "%s", p);
            snprintf(counts[nc].alias, sizeof(counts[nc].alias), "%s", bar1 + 1);
            counts[nc].reward = 0; counts[nc].punish = 0; nc++;
        }
        if (valence > 0) counts[i].reward++; else if (valence < 0) counts[i].punish++;
    }
    fclose(f);

    char new_words[WB_BUF * 8]; size_t nw = 0; new_words[0] = '\0';
    f = fopen(words, "r"); if (!f) return;
    while (fgets(line, sizeof(line), f) && nw < sizeof(new_words) - 1) {
        if (strncmp(line, "CANON=", 6) != 0) { nw += snprintf(new_words + nw, sizeof(new_words) - nw, "%s", line); continue; }
        char canon[256], alias[256];
        char *cbar = strchr(line + 6, '|'); if (!cbar) { nw += snprintf(new_words + nw, sizeof(new_words) - nw, "%s", line); continue; }
        size_t clen = (size_t)(cbar - (line + 6)); if (clen >= 256) clen = 255; memcpy(canon, line + 6, clen); canon[clen] = '\0';
        char *abar = strstr(cbar + 1, "ALIAS="); if (!abar) { nw += snprintf(new_words + nw, sizeof(new_words) - nw, "%s", line); continue; }
        abar += 6; char *aend = strchr(abar, '|'); if (!aend) { nw += snprintf(new_words + nw, sizeof(new_words) - nw, "%s", line); continue; }
        size_t alen = (size_t)(aend - abar); if (alen >= 256) alen = 255; memcpy(alias, abar, alen); alias[alen] = '\0';

        double weight = 0.5;
        for (size_t i = 0; i < nc; i++) if (!strcmp(counts[i].canon, canon) && !strcmp(counts[i].alias, alias)) {
            weight = (counts[i].reward + 1.0) / (counts[i].reward + counts[i].punish + 2.0); break; }
        nw += snprintf(new_words + nw, sizeof(new_words) - nw, "CANON=%s|ALIAS=%s|WEIGHT=%.4f|SOURCE=seed\n", canon, alias, weight);
    }
    fclose(f);
    if (c->apply) wb_write_file(words, new_words);
}

/* ---- main ensure ---- */
static WB_UNUSED void wb_ensure(const char *dir, WbCtx *c, int depth) {
    char wbdir[WB_BUF], label_path[WB_BUF], words[WB_BUF], scores[WB_BUF], vars[WB_BUF];
    char label[256], kind[128], canon[512];
    int have_bank;
    c->n_entities++; if (depth > 0) c->n_nested++;
    snprintf(label_path, sizeof(label_path), "%s/instance_id.txt", dir);
    if (!wb_read_line(label_path, label, sizeof(label)))
        snprintf(label, sizeof(label), "%s", wb_base(dir));

    wb_join(wbdir, sizeof(wbdir), dir, "inventory");
    wb_join(wbdir, sizeof(wbdir), wbdir, WB_WORDBANK_DIR);
    have_bank = wb_is_dir(wbdir);
    if (have_bank) c->n_have_bank++;
    else if (c->apply) {
        char inv[WB_BUF]; wb_join(inv, sizeof(inv), dir, "inventory");
        mkdir(inv, 0755);
        if (mkdir(wbdir, 0755) == 0) c->n_new_bank++;
    }

    wb_join(words, sizeof(words), wbdir, "words.txt");
    wb_join(scores, sizeof(scores), wbdir, "scores.txt");
    wb_join(vars, sizeof(vars), wbdir, "vars.txt");
    if (!have_bank && c->apply) {
        wb_write_file(words, "");
        wb_write_file(scores, "");
        wb_write_file(vars, "");
    }

    if (!have_bank || !c->apply) {
        /* seed name CANON */
        char name[256]; snprintf(name, sizeof(name), "%s", wb_base(dir));
        snprintf(canon, sizeof(canon), "name:%s", name);
        wb_add_seed(c, dir, canon, name);

        /* seed kind CANON */
        if (wb_meta_kind(dir, kind, sizeof(kind))) {
            snprintf(canon, sizeof(canon), "kind:%s", kind);
            wb_add_seed(c, dir, canon, kind);
        }

        /* seed action CANONs from menu labels */
        char labels[WB_MAX_LABELS][128]; size_t nl = 0;
        wb_menu_labels(dir, labels, WB_MAX_LABELS, &nl);
        for (size_t i = 0; i < nl; i++) {
            snprintf(canon, sizeof(canon), "action:%s", labels[i]);
            wb_add_seed(c, dir, canon, labels[i]);
        }

        /* seed action CANONs from meta.pdl METHOD names */
        char methods[WB_MAX_METHODS][128]; size_t nm = 0;
        wb_method_names(dir, methods, WB_MAX_METHODS, &nm);
        for (size_t i = 0; i < nm; i++) {
            if (strlen(methods[i]) == 0) continue;
            snprintf(canon, sizeof(canon), "action:%s", methods[i]);
            wb_add_seed(c, dir, canon, methods[i]);
        }
    }

    /* rebuild mirror from scores */
    if (have_bank && c->apply) wb_rebuild_mirror(c, dir);

    /* report */
    char note[64] = "";
    if (c->apply && !have_bank) snprintf(note, sizeof(note), "bank created");
    else if (!have_bank) snprintf(note, sizeof(note), "would create bank");
    else snprintf(note, sizeof(note), "bank exists");
    if (c->report)
        fprintf(c->report, "%-3s %-28s seeds:%d  %s\n", depth ? "  >" : "", label, c->n_seeds_added, note);

    /* recurse into inventory items that are entities */
    char inv[WB_BUF]; DIR *d; struct dirent *e;
    wb_join(inv, sizeof(inv), dir, "inventory");
    if ((d = opendir(inv))) {
        while ((e = readdir(d))) {
            char child[WB_BUF], cp[WB_BUF];
            if (e->d_name[0] == '.' || !strcmp(e->d_name, WB_WORDBANK_DIR)) continue;
            if (!wb_join(child, sizeof(child), inv, e->d_name)) continue;
            wb_join(cp, sizeof(cp), child, "pal.pdl");
            if (wb_is_dir(child) && wb_exists(cp)) wb_ensure(child, c, depth + 1);
        }
        closedir(d);
    }
}

/* ---- parser path: alias lookup + use-scoring ---- */

/* Case-insensitive whole-word search, same semantics as message_has_word()
   in 045.muchi-pal-agent's send_message.c. Returns 1 if <word> appears as
   a whole word in <text> (bounded by non-alphanumeric on both sides). */
static WB_UNUSED int wb_word_present(const char *text, const char *word) {
    size_t wlen = strlen(word);
    if (wlen == 0) return 0;
    size_t tlen = strlen(text);
    char *lower_t = malloc(tlen + 1);
    char *lower_w = malloc(wlen + 1);
    if (!lower_t || !lower_w) { free(lower_t); free(lower_w); return 0; }
    for (size_t i = 0; i < tlen; i++) lower_t[i] = (char)tolower((unsigned char)text[i]);
    lower_t[tlen] = '\0';
    for (size_t i = 0; i < wlen; i++) lower_w[i] = (char)tolower((unsigned char)word[i]);
    lower_w[wlen] = '\0';
    int found = 0;
    char *p = lower_t;
    while ((p = strstr(p, lower_w)) != NULL) {
        char before = (p == lower_t) ? ' ' : *(p - 1);
        char after = *(p + wlen);
        if (!isalnum((unsigned char)before) && !isalnum((unsigned char)after)) { found = 1; break; }
        p++;
    }
    free(lower_t); free(lower_w);
    return found;
}

/* Look up the user's <input> text against an entity's words.txt.
   Returns 1 and fills <out_canon>/<out_alias>/<out_weight> on the best match
   (highest WEIGHT among matching SOURCE=seed or SOURCE=user rows).
   The entity_dir is the entity's root folder (containing inventory/zz.wordbank/). */
static WB_UNUSED int wb_alias_lookup(const char *entity_dir, const char *input,
        char *out_canon, size_t canon_sz, char *out_alias, size_t alias_sz, double *out_weight) {
    char words[WB_BUF];
    wb_join(words, sizeof(words), entity_dir, "inventory");
    wb_join(words, sizeof(words), words, WB_WORDBANK_DIR);
    wb_join(words, sizeof(words), words, "words.txt");
    FILE *f = fopen(words, "r");
    if (!f) return 0;
    char line[WB_BUF];
    double best_weight = -1.0;
    int best_canon = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "CANON=", 6) != 0) continue;
        char canon[512], alias[256];
        char *cbar = strchr(line + 6, '|'); if (!cbar) continue;
        size_t clen = (size_t)(cbar - (line + 6));
        if (clen >= sizeof(canon)) clen = sizeof(canon) - 1;
        memcpy(canon, line + 6, clen); canon[clen] = '\0';
        char *abar = strstr(cbar, "ALIAS="); if (!abar) continue;
        abar += 6; char *aend = strchr(abar, '|');
        if (!aend) continue;
        size_t alen = (size_t)(aend - abar);
        if (alen >= sizeof(alias)) alen = sizeof(alias) - 1;
        memcpy(alias, abar, alen); alias[alen] = '\0';
        char *wbar = strstr(aend, "WEIGHT=");
        double w = 0.5;
        if (wbar) w = atof(wbar + 7);
        char *sbar = strstr(aend, "SOURCE=");
        if (!sbar) continue;
        /* check whole-word match of alias in input */
        if (wb_word_present(input, alias)) {
            if (w >= best_weight) {  /* first match wins on ties */
                best_weight = w;
                if (!best_canon) {  /* copy on first match, update only if higher */
                    snprintf(out_canon, canon_sz, "%s", canon);
                    snprintf(out_alias, alias_sz, "%s", alias);
                } else if (w > best_weight - 1) {  /* already set, update on strictly better */
                    snprintf(out_canon, canon_sz, "%s", canon);
                    snprintf(out_alias, alias_sz, "%s", alias);
                }
                best_canon = 1;
            }
        }
    }
    fclose(f);
    if (best_canon) { *out_weight = best_weight; return 1; }
    return 0;
}

/* Append a USE score: append a SCORE row with source=use to the entity's
   scores.txt. The pal_hash is the entity_hash for chain verification (empty
   for local-use row only). valence is +1 (match used) / -1 (user corrected) / 0 (neutral). */
static WB_UNUSED void wb_use_score(const char *entity_dir, const char *canon, const char *alias,
        int valence, const char *pal_hash) {
    char scores[WB_BUF];
    wb_join(scores, sizeof(scores), entity_dir, "inventory");
    wb_join(scores, sizeof(scores), scores, WB_WORDBANK_DIR);
    wb_join(scores, sizeof(scores), scores, "scores.txt");
    char row[WB_BUF];
    snprintf(row, sizeof(row), "SCORE|%s|%s|valence=%+d|source=use|pal_hash=%s|ts=%ld|id=%s\n",
             canon, alias, valence, pal_hash ? pal_hash : "", (long)time(NULL), alias);
    wb_append_file(scores, row);
}

#endif

