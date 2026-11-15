/* plug_ref.c - "event pages read plug parameters" (decision D74/D75, step 1 of the plugs spec, 2026-11-14). Pure, text-included by the programs that RUN event commands
 * (the editor core petslike_core.c for verbs, events-hq's mr_world.c for Tint Screen and the like) and by the harness probe. Needs only stdio, stdlib, string.
 *
 * A node argument in an event page (event.ir.pdl) may be a PLUG REFERENCE instead of a literal:   @plug:<plug>:<key>
 *   <plug>  the plug file <app>/plugs/<plug>.plug.pdl (letters, digits, - and _)      <key>  a PARAM row of that file (letters, digits and _)
 * The file row:   PARAM | key | value | type | min | max | help
 *   type  int    one integer, min and max bound it (blank = no bound)
 *         tone   four integers r,g,b,gray (RPG Maker Tint Screen), each -255..255
 *         word   letters, digits, - and _ only (for example on or off)
 *         text   plain text, at most 160 characters, no quote, backtick, dollar sign, backslash or newline
 * Rules: the plug must say `ENABLED | 1` (a disabled plug resolves nothing: the command is refused, not run with a stale value); a missing plug, key, or a value that fails its type or bounds is a
 * REFUSAL with a reason, never a silent default. The plug file is config: a game never writes it. The reference is resolved when the command RUNS, so editing the plug changes the next run with no recompile.
 * plug_ref(plugdir, arg, out, cap):  0 = arg is not a reference (use it as it is, out untouched)   1 = resolved into out   -1 = refused, out holds the reason */
#ifndef PLUG_REF_C
#define PLUG_REF_C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int plug_ref_word(const char *s, int dash) { if (!*s) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || (dash && *s == '-'))) return 0; return 1; }
static char *plug_ref_trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int plug_ref_int(const char *s, long *v) { char *e; if (!*s) return 0; *v = strtol(s, &e, 10); return *e == 0; }
/* split a row on `|` KEEPING empty fields (strtok collapses them: `PARAM | k | 5 | int |  | 200 | help` would have read 200 as the MIN, found 2026-11-14), each field trimmed; returns the field count */
static int plug_ref_split(char *p, char **fl, int max) { int n = 0; while (n < max) { char *bar = (n == max - 1) ? NULL : strchr(p, '|'); if (bar) *bar = 0; fl[n++] = plug_ref_trim(p); if (!bar) break; p = bar + 1; } return n; }

/* the VALUE rules of one PARAM row, shared by the engine path (plug_ref below) and by the plug check in harness_menu_op (ONE authority, 2026-11-14): 0 = the value passes its type and bounds, -1 = refused with the reason in out.
 * An int row whose min is above its max is refused too (a bound nobody can meet). */
static int plug_ref_value_ok(const char *plug, const char *key, const char *type, const char *value, const char *mn, const char *mx, char *out, size_t cap) {
    if (!strcmp(type, "int")) {
        long v, lo = 0, hi = 0; if (!plug_ref_int(value, &v)) { snprintf(out, cap, "%s.%s is not an integer", plug, key); return -1; }
        int has_lo = mn[0] && plug_ref_int(mn, &lo), has_hi = mx[0] && plug_ref_int(mx, &hi);
        if (has_lo && has_hi && lo > hi) { snprintf(out, cap, "%s.%s has a minimum %ld above its maximum %ld", plug, key, lo, hi); return -1; }
        if (has_lo && v < lo) { snprintf(out, cap, "%s.%s is %ld, below the minimum %ld", plug, key, v, lo); return -1; }
        if (has_hi && v > hi) { snprintf(out, cap, "%s.%s is %ld, above the maximum %ld", plug, key, v, hi); return -1; }
    } else if (!strcmp(type, "tone")) {
        char tmp[200]; snprintf(tmp, sizeof tmp, "%s", value); int n = 0; for (char *t = strtok(tmp, ","); t; t = strtok(NULL, ",")) { long v; char *tt = plug_ref_trim(t); if (!plug_ref_int(tt, &v) || v < -255 || v > 255) { snprintf(out, cap, "%s.%s: each tone number must be an integer from -255 to 255", plug, key); return -1; } n++; }
        if (n != 4) { snprintf(out, cap, "%s.%s: a tone is four numbers r,g,b,gray", plug, key); return -1; }
    } else if (!strcmp(type, "word")) {
        if (!plug_ref_word(value, 1)) { snprintf(out, cap, "%s.%s is not a plain word", plug, key); return -1; }
    } else if (!strcmp(type, "text")) {
        if (!value[0] || strlen(value) > 160) { snprintf(out, cap, "%s.%s is empty or longer than 160", plug, key); return -1; }
        for (const char *c = value; *c; c++) if (*c == '\'' || *c == '`' || *c == '$' || *c == '\\' || (unsigned char)*c < 32) { snprintf(out, cap, "%s.%s has a character that is not allowed in text", plug, key); return -1; }
    } else { snprintf(out, cap, "%s.%s has the unknown type %s", plug, key, type); return -1; }
    return 0;
}

static int plug_ref(const char *plugdir, const char *arg, char *out, size_t cap) {
    if (strncmp(arg, "@plug:", 6)) { return 0; }
    char spec[200]; snprintf(spec, sizeof spec, "%s", arg + 6); char *colon = strchr(spec, ':');
    if (!colon) { snprintf(out, cap, "plug reference %s has no key (want @plug:<plug>:<key>)", arg); return -1; }
    *colon = 0; const char *plug = spec, *key = colon + 1;
    if (!plug_ref_word(plug, 1) || !plug_ref_word(key, 0) || strlen(plug) > 40 || strlen(key) > 40) { snprintf(out, cap, "plug reference %s has a bad name", arg); return -1; }
    char path[4400]; snprintf(path, sizeof path, "%s/%s.plug.pdl", plugdir, plug); FILE *f = fopen(path, "r");
    if (!f) { snprintf(out, cap, "plug %s is not installed (no %s.plug.pdl)", plug, plug); return -1; }
    int on = 0, found = 0; char ln[600], value[200] = "", type[16] = "", mn[24] = "", mx[24] = "";
    while (fgets(ln, sizeof ln, f)) {
        char *p = plug_ref_trim(ln);
        if (!strncmp(p, "ENABLED", 7)) { char *bar = strchr(p, '|'); if (bar) on = atoi(bar + 1) == 1; continue; }
        if (strncmp(p, "PARAM", 5) || found) { continue; }
        char *fl[7]; int nf = plug_ref_split(p, fl, 7);
        if (nf < 4 || strcmp(fl[1], key)) { continue; }
        found = 1; snprintf(value, sizeof value, "%s", fl[2]); snprintf(type, sizeof type, "%s", fl[3]); if (nf > 4) { snprintf(mn, sizeof mn, "%s", fl[4]); } if (nf > 5) { snprintf(mx, sizeof mx, "%s", fl[5]); }
    }
    fclose(f);
    if (!on) { snprintf(out, cap, "plug %s is off (ENABLED is not 1)", plug); return -1; }
    if (!found) { snprintf(out, cap, "plug %s has no parameter %s", plug, key); return -1; }
    if (plug_ref_value_ok(plug, key, type, value, mn, mx, out, cap)) { return -1; }
    snprintf(out, cap, "%s", value); return 1;
}
#endif
