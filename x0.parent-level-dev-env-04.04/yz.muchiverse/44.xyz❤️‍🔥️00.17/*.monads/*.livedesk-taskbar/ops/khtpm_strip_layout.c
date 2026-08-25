/* khtpm_strip_layout.c — see khtpm_strip_layout.h for the full design
 * rationale. Tokenizer/parse_attributes/substitute_vars/is_navigable/
 * is_descendant are ported directly from chtpm_parser.c (read in full
 * before writing each of these, per khtpm-strip-parser-SCOPE.md's own
 * closing necessitation) — reduced to this strip's locked 5-tag vocabulary
 * (<panel> <text> <button> <row> <cli_io>), with CHTPM's <module>/<br/>/
 * <interact>/visibility-expr/fold handling intentionally dropped (out of
 * scope, see SCOPE.md's tag vocabulary table).
 */
#ifndef _WIN32
#define _DEFAULT_SOURCE /* strdup() under -std=c11 strict mode, matches
                          * khtpm_taskbar_manager.c's own convention for
                          * the same issue */
#endif

#include "khtpm_strip_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------------------------------------------------
 * Tokenizer — ported directly from chtpm_parser.c's tokenize() (read in
 * full at ~line 1793-1825): walk for '<', emit a TEXT token for anything
 * before it, split the tag body on the first space into tag_name/attrs,
 * detect a trailing '/' as self-close, detect a leading '/' as close-tag.
 * Same edge-case handling as the reference: a stray '<' with no matching
 * '>' is treated as literal text (not an error), matching the real
 * function's own fallback branch exactly.
 * ------------------------------------------------------------------- */
typedef enum { LTOK_TEXT, LTOK_OPEN, LTOK_CLOSE, LTOK_SELFCLOSE } LayTokenType;

typedef struct {
    LayTokenType type;
    char content[LAY_LABEL_LEN];   /* TEXT token body */
    char tag_name[LAY_TAGNAME_LEN];
    char attributes[LAY_ATTRSTR_LEN];
} LayToken;

static LayToken *lay_tokenize(const char *content, int *token_count) {
    LayToken *tokens = (LayToken *)calloc((size_t)LAY_MAX_TOKENS, sizeof(LayToken));
    *token_count = 0;
    if (!tokens) return NULL;
    const char *cursor = content;
    while (*cursor && *token_count < LAY_MAX_TOKENS) {
        /* Real bug fix (2026-08-11, direct live report — the header cursor
         * was landing on a wrong, invisible index that turned out to be a
         * spurious element whose label/onClick were the literal ".."
         * placeholder text from this very file's own documentation
         * comment, e.g. "<button label=".." onClick="..">" inside the
         * <!-- ... --> block at the top of every .chtpm layout file):
         * this tokenizer had ZERO comment-awareness — it parsed the whole
         * file including hand-written prose/examples inside <!-- -->
         * blocks as if they were real tags, corrupting element indices
         * and inflating element_count (37 instead of the real ~23 for
         * khtpm_strip_header.chtpm). Skip <!-- ... --> spans entirely,
         * emitting no token for them, checked BEFORE tag_start is treated
         * as a real tag (not just when cursor happens to sit exactly at
         * one, so a comment appearing after some real markup is caught
         * too, not just one at the very start of the file). */
        if (strncmp(cursor, "<!--", 4) == 0) {
            const char *comment_end = strstr(cursor + 4, "-->");
            cursor = comment_end ? comment_end + 3 : cursor + strlen(cursor);
            continue;
        }
        const char *tag_start = strchr(cursor, '<');
        if (tag_start && strncmp(tag_start, "<!--", 4) == 0) {
            if (tag_start > cursor) {
                LayToken *t = &tokens[(*token_count)++];
                t->type = LTOK_TEXT;
                int len = (int)(tag_start - cursor);
                if (len > LAY_LABEL_LEN - 1) len = LAY_LABEL_LEN - 1;
                strncpy(t->content, cursor, (size_t)len);
                t->content[len] = '\0';
            }
            const char *comment_end = strstr(tag_start + 4, "-->");
            cursor = comment_end ? comment_end + 3 : tag_start + strlen(tag_start);
            continue;
        }
        if (!tag_start) {
            LayToken *t = &tokens[(*token_count)++];
            t->type = LTOK_TEXT;
            strncpy(t->content, cursor, LAY_LABEL_LEN - 1);
            t->content[LAY_LABEL_LEN - 1] = '\0';
            break;
        }
        if (tag_start > cursor) {
            LayToken *t = &tokens[(*token_count)++];
            t->type = LTOK_TEXT;
            int len = (int)(tag_start - cursor);
            if (len > LAY_LABEL_LEN - 1) len = LAY_LABEL_LEN - 1;
            strncpy(t->content, cursor, (size_t)len);
            t->content[len] = '\0';
        }
        const char *tag_end = strchr(tag_start, '>');
        if (!tag_end) {
            /* Stray '<' with no matching '>' — treat as literal text and continue,
             * exactly matching chtpm_parser.c's tokenize() fallback. */
            LayToken *t = &tokens[(*token_count)++];
            t->type = LTOK_TEXT;
            t->content[0] = '<';
            t->content[1] = '\0';
            cursor = tag_start + 1;
            continue;
        }
        LayToken *t = &tokens[(*token_count)++];
        char tag_body[LAY_ATTRSTR_LEN];
        int body_len = (int)(tag_end - tag_start - 1);
        if (body_len > (int)sizeof(tag_body) - 1) body_len = (int)sizeof(tag_body) - 1;
        if (body_len < 0) body_len = 0;
        strncpy(tag_body, tag_start + 1, (size_t)body_len);
        tag_body[body_len] = '\0';
        if (tag_body[0] == '/') {
            t->type = LTOK_CLOSE;
            strncpy(t->tag_name, tag_body + 1, LAY_TAGNAME_LEN - 1);
        } else {
            int self_closing = 0;
            if (body_len > 0 && tag_body[body_len - 1] == '/') {
                self_closing = 1;
                tag_body[body_len - 1] = '\0';
            }
            t->type = self_closing ? LTOK_SELFCLOSE : LTOK_OPEN;
            char *space = strchr(tag_body, ' ');
            if (space) {
                *space = '\0';
                strncpy(t->tag_name, tag_body, LAY_TAGNAME_LEN - 1);
                strncpy(t->attributes, space + 1, LAY_ATTRSTR_LEN - 1);
            } else {
                strncpy(t->tag_name, tag_body, LAY_TAGNAME_LEN - 1);
                t->attributes[0] = '\0';
            }
        }
        cursor = tag_end + 1;
    }
    return tokens;
}

/* ---------------------------------------------------------------------
 * Generic attribute parser — ported directly from chtpm_parser.c's
 * parse_attributes() (read in full at ~line 1718-1748): walk name[=value]
 * pairs, quoted or bare values, generic (not per-tag-hardcoded) — reduced
 * to only the attribute names the strip's 5-tag vocabulary actually uses
 * (label, onClick, target_id, sprite — the last added 2026-08-11 for tab
 * sprite rendering, see khtpm_strip_layout.h's own LAY_SPRITE_LEN comment)
 * since visibility/fg/bg/source/prefix/suffix/href/id/input_mode/
 * time_reactive are all CHTPM attributes this strip's locked vocabulary
 * explicitly drops (see SCOPE.md's tag table).
 * ------------------------------------------------------------------- */
static void lay_parse_attributes(LayElement *el, const char *attr_str) {
    if (!attr_str || attr_str[0] == '\0') return;
    char *attrs = strdup(attr_str);
    if (!attrs) return;
    char *pos = attrs;
    while (*pos) {
        while (*pos && isspace((unsigned char)*pos)) pos++;
        if (!*pos) break;
        char *name_start = pos;
        while (*pos && *pos != '=' && !isspace((unsigned char)*pos)) pos++;
        char saved = *pos;
        *pos = '\0';
        while (*(++pos) && isspace((unsigned char)*pos)) { /* skip */ }
        if (*pos == '=') {
            pos++;
            while (*pos && isspace((unsigned char)*pos)) pos++;
        }
        char *val_start = pos;
        if (*pos == '"' || *pos == '\'') {
            char quote = *pos++;
            val_start = pos;
            while (*pos && *pos != quote) pos++;
            if (*pos) *pos++ = '\0';
        } else {
            while (*pos && !isspace((unsigned char)*pos) && *pos != '/') pos++;
            if (*pos) *pos++ = '\0';
        }

        if (strcmp(name_start, "label") == 0) {
            strncpy(el->label, val_start, LAY_LABEL_LEN - 1);
        } else if (strcmp(name_start, "onClick") == 0) {
            strncpy(el->onClick, val_start, LAY_ONCLICK_LEN - 1);
        } else if (strcmp(name_start, "target_id") == 0) {
            strncpy(el->target_id, val_start, LAY_ATTR_LEN - 1);
        } else if (strcmp(name_start, "sprite") == 0) {
            strncpy(el->sprite, val_start, LAY_SPRITE_LEN - 1);
        } else if (strcmp(name_start, "id") == 0) {
            strncpy(el->id, val_start, LAY_ID_LEN - 1);
        }
        /* saved is only meaningful if we didn't already null-terminate past
         * it via the quote/bare-value walk above; chtpm_parser.c restores it
         * unconditionally too (harmless no-op in the already-terminated
         * case since pos has moved on), kept for byte-for-byte parity. */
        if (pos > attrs) *(pos - 1) = saved;
    }
    free(attrs);
}

/* ---------------------------------------------------------------------
 * ${var} substitution — ported directly from chtpm_parser.c's
 * substitute_vars_naked() (whole-document pass, skips content inside '<'
 * '>' tags — used BEFORE tokenizing so a var containing a markup fragment,
 * e.g. a pre-rendered <button> list, expands into real tags before the
 * tokenizer ever sees it) and substitute_vars() (used on a single already-
 * parsed attribute value, e.g. a button's label, at render time — no
 * in-tag skip needed since the input here is never a whole tagged
 * document). Both honor the same "\n" literal escape unescaping inside a
 * var's own value that the reference does.
 * ------------------------------------------------------------------- */
static void lay_substitute_vars_naked(const char *src, char *dst, size_t max_len,
                                       LayVarLookupFn get_var, void *ctx) {
    const char *p_src = src;
    char *p_dst = dst;
    int in_tag = 0;
    while (*p_src && (size_t)(p_dst - dst) < max_len - 1) {
        if (*p_src == '<') in_tag = 1;
        else if (*p_src == '>') in_tag = 0;

        if (!in_tag && *p_src == '$' && *(p_src + 1) == '{') {
            const char *end = strchr(p_src, '}');
            if (end) {
                char var_name[64];
                int len = (int)(end - (p_src + 2));
                if (len > 63) len = 63;
                if (len < 0) len = 0;
                strncpy(var_name, p_src + 2, (size_t)len);
                var_name[len] = '\0';
                const char *val = get_var ? get_var(var_name, ctx) : "";
                if (!val) val = "";
                while (*val && (size_t)(p_dst - dst) < max_len - 1) {
                    if (*val == '\\' && *(val + 1) == 'n') { *p_dst++ = '\n'; val += 2; }
                    else *p_dst++ = *val++;
                }
                p_src = end + 1;
                continue;
            }
        }
        *p_dst++ = *p_src++;
    }
    *p_dst = '\0';
}

static void lay_substitute_vars(const char *src, char *dst, size_t max_len,
                                 LayVarLookupFn get_var, void *ctx) {
    const char *p_src = src;
    char *p_dst = dst;
    while (*p_src && (size_t)(p_dst - dst) < max_len - 1) {
        if (*p_src == '\\' && (*(p_src + 1) == '$' || *(p_src + 1) == '{' ||
                                *(p_src + 1) == '<' || *(p_src + 1) == '\\')) {
            *p_dst++ = *(p_src + 1);
            p_src += 2;
            continue;
        }
        if (*p_src == '$' && *(p_src + 1) == '{') {
            const char *end = strchr(p_src, '}');
            if (end) {
                char var_name[64];
                int len = (int)(end - (p_src + 2));
                if (len > 63) len = 63;
                if (len < 0) len = 0;
                strncpy(var_name, p_src + 2, (size_t)len);
                var_name[len] = '\0';
                const char *val = get_var ? get_var(var_name, ctx) : "";
                if (!val) val = "";
                while (*val && (size_t)(p_dst - dst) < max_len - 1) {
                    if (*val == '\\' && *(val + 1) == 'n') { *p_dst++ = '\n'; val += 2; }
                    else *p_dst++ = *val++;
                }
                p_src = end + 1;
                continue;
            }
        }
        *p_dst++ = *p_src++;
    }
    *p_dst = '\0';
}

static char *lay_read_file_to_string(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length < 0) { fclose(f); return NULL; }
    if (length > LAY_FILE_MAX - 1) length = LAY_FILE_MAX - 1;
    char *buffer = (char *)malloc((size_t)length + 1);
    if (!buffer) { fclose(f); return NULL; }
    size_t n = fread(buffer, 1, (size_t)length, f);
    buffer[n] = '\0';
    fclose(f);
    return buffer;
}

/* ---------------------------------------------------------------------
 * Tag tree builder — token stream -> LayElement flat array with
 * parent_index, matching CHTPM's own parse_chtm() shape (a token walk with
 * an explicit stack[]/top for nesting), reduced to this strip's 5 tags: a
 * root <panel> pushes an empty (-1-parented) scope and is otherwise a
 * no-op element (matches chtpm_parser.c's own panel/layout handling, which
 * also never allocates a UIElement for <panel> itself); text tokens
 * outside any tag become "text" elements (trimmed, empty ones dropped,
 * exactly like the reference).
 * ------------------------------------------------------------------- */
static void lay_build_tree(LayDoc *doc, LayToken *tokens, int tc) {
    doc->element_count = 0;
    int stack[64];
    int top = -1;
    for (int i = 0; i < tc && doc->element_count < LAY_MAX_ELEMENTS; i++) {
        LayToken *t = &tokens[i];
        if (t->type == LTOK_TEXT) {
            char *trim = strdup(t->content);
            if (!trim) continue;
            char *p = trim;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p) {
                char *end = p + strlen(p) - 1;
                while (end > p && isspace((unsigned char)*end)) *end-- = '\0';
            }
            if (!*p) { free(trim); continue; }
            LayElement *el = &doc->elements[doc->element_count++];
            memset(el, 0, sizeof(*el));
            strcpy(el->type, "text");
            strncpy(el->label, p, LAY_LABEL_LEN - 1);
            el->parent_index = (top >= 0) ? stack[top] : -1;
            if (el->parent_index >= 0) {
                LayElement *pa = &doc->elements[el->parent_index];
                if (pa->num_children < LAY_MAX_CHILDREN)
                    pa->children[pa->num_children++] = doc->element_count - 1;
            }
            free(trim);
        } else if (t->type == LTOK_OPEN || t->type == LTOK_SELFCLOSE) {
            if (strcmp(t->tag_name, "panel") == 0) {
                if (t->type == LTOK_OPEN && top < 63) stack[++top] = -1;
                continue;
            }
            /* Unknown tag names outside the locked 5-tag vocabulary are
             * skipped defensively (their children still get attached to
             * OUR nearest real parent if the tag is self-closing == no
             * children possible; if it's an open tag we still push -1 so
             * a stray close doesn't underflow the stack) rather than
             * crashing the tree builder on a typo in a hand-authored
             * layout file. */
            int known = (strcmp(t->tag_name, "text") == 0 ||
                         strcmp(t->tag_name, "button") == 0 ||
                         strcmp(t->tag_name, "row") == 0 ||
                         strcmp(t->tag_name, "cli_io") == 0);
            if (!known) {
                if (t->type == LTOK_OPEN && top < 63) {
                    int parent_of_unknown = (top >= 0) ? stack[top] : -1;
                    stack[++top] = parent_of_unknown;
                }
                continue;
            }
            LayElement *el = &doc->elements[doc->element_count++];
            memset(el, 0, sizeof(*el));
            strncpy(el->type, t->tag_name, LAY_TYPE_LEN - 1);
            lay_parse_attributes(el, t->attributes);
            el->parent_index = (top >= 0) ? stack[top] : -1;
            int my_index = doc->element_count - 1;
            if (el->parent_index >= 0) {
                LayElement *pa = &doc->elements[el->parent_index];
                if (pa->num_children < LAY_MAX_CHILDREN)
                    pa->children[pa->num_children++] = my_index;
            }
            if (t->type == LTOK_OPEN && top < 63) stack[++top] = my_index;
        } else if (t->type == LTOK_CLOSE) {
            if (strcmp(t->tag_name, "panel") == 0) {
                if (top >= 0) top--;
                continue;
            }
            if (top >= 0) top--;
        }
    }
}

int lay_load(LayDoc *doc, const char *path, LayVarLookupFn get_var, void *ctx) {
    memset(doc, 0, sizeof(*doc));
    doc->active_index = -1;
    doc->focus_index = -1;

    char *content = lay_read_file_to_string(path);
    if (!content) return 0;

    char *substituted = (char *)malloc(LAY_FILE_MAX);
    if (!substituted) { free(content); return 0; }
    lay_substitute_vars_naked(content, substituted, LAY_FILE_MAX, get_var, ctx);
    free(content);

    int tc = 0;
    LayToken *tokens = lay_tokenize(substituted, &tc);
    free(substituted);
    if (!tokens) return 0;

    lay_build_tree(doc, tokens, tc);
    free(tokens);

    /* Seed focus to the first navigable element, matching CHTPM's own
     * initialize_focus() being called right after parse_chtm(). */
    for (int i = 0; i < doc->element_count; i++) {
        if (lay_is_navigable(doc, i)) { doc->focus_index = i; break; }
    }
    if (doc->focus_index < 0) doc->focus_index = 0;
    return 1;
}

int lay_reload_preserving_scope(LayDoc *doc, const char *path, LayVarLookupFn get_var, void *ctx) {
    char saved_onclick[LAY_ONCLICK_LEN] = "";
    char saved_focus_onclick[LAY_ONCLICK_LEN] = "";
    if (doc->active_index >= 0 && doc->active_index < doc->element_count)
        strncpy(saved_onclick, doc->elements[doc->active_index].onClick, LAY_ONCLICK_LEN - 1);
    if (doc->focus_index >= 0 && doc->focus_index < doc->element_count)
        strncpy(saved_focus_onclick, doc->elements[doc->focus_index].onClick, LAY_ONCLICK_LEN - 1);

    if (!lay_load(doc, path, get_var, ctx)) return 0;

    /* Real bug fix (2026-08-11, direct live report: header cursor landing
     * on a wrong, invisible index — "not defaulting to 13, but not 1
     * either. there is no '>' this time"): <row> and <text> elements never
     * get their onClick attribute parsed at all (only button/cli_io do,
     * via lay_parse_attributes()) — they're all left as an empty string
     * "" by the memset() in lay_build_tree(). The saved-onclick match
     * below used to compare against EVERY element regardless of type, so
     * an empty saved_focus_onclick — or, worse, a genuinely-empty real
     * onClick from a still-interactive element — could match the FIRST
     * non-interactive element in array order (e.g. a <row> or the literal
     * "${strip_hq_items}" text node right after it) instead of a real
     * button/cli_io. Restricting the match to lay_is_interactive()
     * elements closes this whole bug class, not just one instance of it. */
    if (saved_onclick[0]) {
        for (int i = 0; i < doc->element_count; i++) {
            if (lay_is_interactive(&doc->elements[i]) &&
                strcmp(doc->elements[i].onClick, saved_onclick) == 0) {
                doc->active_index = i;
                break;
            }
        }
    }
    if (saved_focus_onclick[0]) {
        for (int i = 0; i < doc->element_count; i++) {
            if (lay_is_interactive(&doc->elements[i]) &&
                strcmp(doc->elements[i].onClick, saved_focus_onclick) == 0) {
                doc->focus_index = i;
                break;
            }
        }
    }
    /* If the previously-focused element no longer resolves to something
     * navigable (e.g. its scope closed), fall back to the first navigable
     * element again, same as a fresh lay_load(). */
    if (doc->focus_index < 0 || doc->focus_index >= doc->element_count ||
        !lay_is_navigable(doc, doc->focus_index)) {
        doc->focus_index = -1;
        for (int i = 0; i < doc->element_count; i++) {
            if (lay_is_navigable(doc, i)) { doc->focus_index = i; break; }
        }
        if (doc->focus_index < 0) doc->focus_index = 0;
    }
    return 1;
}

int lay_is_interactive(const LayElement *el) {
    return strcmp(el->type, "button") == 0 || strcmp(el->type, "cli_io") == 0;
}

int lay_is_activate_marker(const char *onClick) {
    return onClick && strncmp(onClick, "ACTIVATE", 8) == 0;
}

int lay_is_descendant(const LayDoc *doc, int child_idx, int parent_idx) {
    if (child_idx < 0 || parent_idx < 0) return 0;
    if (child_idx >= doc->element_count || parent_idx >= doc->element_count) return 0;
    int p = doc->elements[child_idx].parent_index;
    while (p != -1) {
        if (p == parent_idx) return 1;
        p = doc->elements[p].parent_index;
    }
    return 0;
}

/* Ported directly from chtpm_parser.c's is_navigable() (~line 1750-1785). */
int lay_is_navigable(const LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return 0;
    const LayElement *el = &doc->elements[idx];
    if (!lay_is_interactive(el)) return 0;

    /* cli_io special case (SCOPE.md: "a special-cased leaf, not a scope"):
     * it must NEVER be part of ordinary header-row browsing (there is no
     * per-element visibility expression in this vocabulary to hide it
     * otherwise) — it is only navigable while it IS the active_index
     * itself, which the host (khtpm_strip_parser.c) sets explicitly via
     * lay_activate() when the manager's cliio_active state says so. This
     * is the one place this generically-CHTPM-modeled engine knows
     * anything strip-specific; documented as a necessitation discovered
     * during implementation, not assumed up front. */
    if (strcmp(el->type, "cli_io") == 0 && doc->active_index != idx) return 0;

    if (doc->active_index != -1) {
        const LayElement *active_el = &doc->elements[doc->active_index];
        if (active_el->num_children > 0 && lay_is_activate_marker(active_el->onClick)) {
            if (idx == doc->active_index) return 1;
            return lay_is_descendant(doc, idx, doc->active_index);
        }
        /* Active element is not an ACTIVATE scope root (shouldn't normally
         * happen for this strip's vocabulary since only ACTIVATE buttons
         * ever get lay_activate()'d) — only it is navigable. */
        return idx == doc->active_index;
    }

    /* Global mode: nothing is navigable underneath an ACTIVATE menu that
     * isn't itself the open one (matches the reference's own walk-up-
     * ancestors check, minus the is_folded check this vocabulary doesn't
     * have). */
    int p = el->parent_index;
    while (p != -1) {
        if (lay_is_activate_marker(doc->elements[p].onClick)) return 0;
        p = doc->elements[p].parent_index;
    }
    return 1;
}

void lay_activate(LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return;
    doc->active_index = idx;
    doc->focus_index = idx;
    for (int i = 0; i < doc->element_count; i++) {
        if (i != idx && lay_is_descendant(doc, i, idx) && lay_is_navigable(doc, i)) {
            doc->focus_index = i;
            break;
        }
    }
}

/* Ported directly from send_command()'s "BACK" branch (chtpm_parser.c
 * ~line 1667-1681): pop to the nearest ACTIVATE ancestor of active_index. */
void lay_back(LayDoc *doc) {
    if (doc->active_index == -1) return;
    int old_active = doc->active_index;
    int p = doc->elements[doc->active_index].parent_index;
    while (p != -1 && !lay_is_activate_marker(doc->elements[p].onClick))
        p = doc->elements[p].parent_index;
    doc->active_index = p;
    doc->focus_index = old_active;
    if (doc->active_index != -1 && !lay_is_navigable(doc, doc->focus_index)) {
        doc->focus_index = -1;
        for (int i = 0; i < doc->element_count; i++) {
            if (lay_is_navigable(doc, i)) { doc->focus_index = i; break; }
        }
    }
}

void lay_focus_delta(LayDoc *doc, int delta) {
    if (doc->element_count == 0) return;
    int nav_count = 0;
    int nav_list[LAY_MAX_ELEMENTS];
    int cur_pos = -1;
    for (int i = 0; i < doc->element_count; i++) {
        if (lay_is_navigable(doc, i)) {
            if (i == doc->focus_index) cur_pos = nav_count;
            nav_list[nav_count++] = i;
        }
    }
    if (nav_count == 0) return;
    if (cur_pos < 0) { doc->focus_index = nav_list[0]; return; }
    int next = (cur_pos + delta) % nav_count;
    if (next < 0) next += nav_count;
    doc->focus_index = nav_list[next];
}

const char *lay_cursor_prefix(const LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return "[ ]";
    int is_active = (idx == doc->active_index);
    int is_focused = (idx == doc->focus_index);
    int navigable = lay_is_navigable(doc, idx);
    if (is_active) return "[^]";
    if (is_focused && (doc->active_index == -1 || navigable)) return "[>]";
    return "[ ]";
}

void lay_get_label(const LayDoc *doc, int idx, LayVarLookupFn get_var, void *ctx,
                    char *out, size_t outsz) {
    if (idx < 0 || idx >= doc->element_count) { if (outsz) out[0] = '\0'; return; }
    lay_substitute_vars(doc->elements[idx].label, out, outsz, get_var, ctx);
}

/* Real gap fix (2026-08-11, "user should have a sprite at least"): the
 * "sprite" attribute needs the SAME ${var} substitution label already
 * gets — unlike bottom-bar tabs, whose sprite="<concrete path>" is
 * already resolved by the manager before the markup fragment is even
 * tokenized, the header's static khtpm_strip_header.chtpm uses a literal
 * sprite="${avatar_dir}" placeholder that must be substituted at draw
 * time, same as label. Mirrors lay_get_label() exactly. */
void lay_get_sprite(const LayDoc *doc, int idx, LayVarLookupFn get_var, void *ctx,
                     char *out, size_t outsz) {
    if (idx < 0 || idx >= doc->element_count) { if (outsz) out[0] = '\0'; return; }
    lay_substitute_vars(doc->elements[idx].sprite, out, outsz, get_var, ctx);
}

/* REAL, NEW 2026-08-16 (see LayElement's own `id` field comment) - a
 * button's real stable identity, no ${var} substitution needed (it's
 * a literal name, not display text, unlike label/sprite). */
const char *lay_get_id(const LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return "";
    return doc->elements[idx].id;
}
