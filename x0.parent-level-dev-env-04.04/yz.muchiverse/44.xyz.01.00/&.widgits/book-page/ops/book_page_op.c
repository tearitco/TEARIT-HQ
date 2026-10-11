/* book_page_op - create / list / check / rename BOOKS and PAGES under a livedesk sessions root, with the same on-disk shape the taskbar writes.
 *
 * A BOOK is <root>/<id>/ (id = s<N>) with session.pdl ("STATE | name | <book>", later "STATE | active_desk | <page>") and desks/.
 * A PAGE is <root>/<id>/desks/<page>.pdl.  Shapes mirror khtpm_taskbar_manager.c: livedesk_next_id, livedesk_ensure_session
 * (name row + empty desk_01.pdl), livedesk_new_desk (empty page file), livedesk_write_active_desk, livedesk_rename_desk.
 *
 * Usage: book_page_op --root <sessions_root> <verb> ...        (--root is REQUIRED, so a test can never hit live data by accident)
 *   new-page <book> <page> [--activate]    create the page (creates the book first if missing); active_desk changes only with --activate
 *   new-page <book>:<page> [--activate]    same, one argument
 *   new-book <book>                        create the book (session.pdl + desks/desk_01.pdl), nothing else
 *   list [<book>]                          no arg: book names (by id), one per line; with a book: its page names, sorted
 *   exists <book>[:<page>]                 prints "yes"/"no"
 *   rename-page <book> <old> <new>         (also <book>:<old> <new>); updates active_desk when it pointed at <old>
 * Names: letters, digits, '-' '_'; 1..48 chars (no '/', '..', leading dot, empty).
 * Exit codes: 0 ok / yes; 1 not found / no; 2 usage, bad name or missing --root; 3 already exists (nothing overwritten); 4 I/O error.
 * Writes are temp+rename (pages: temp+link, so an existing page is never replaced). Each mutating call appends one row to
 * <root>/book_page_ledger.txt:  <epoch_s> | <verb> | <book> | <page> | ok  or  exit=<n>.  No mtime is ever read. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define PB 4096
static char ROOT[PB];

static int valid_name(const char *s) {
    size_t n = strlen(s);
    if (n < 1 || n > 48) return 0;
    for (; *s; s++) if (!isalnum((unsigned char)*s) && *s != '-' && *s != '_') return 0;
    return 1;
}

static int write_file_atomic(const char *path, const char *text) {
    char tmp[PB + 16];
    snprintf(tmp, sizeof tmp, "%s.tmp%d", path, (int)getpid());
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    if (text[0]) fputs(text, f);
    if (fclose(f) != 0) { unlink(tmp); return -1; }
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

/* create an empty file that must not exist yet: 0 ok, 3 exists, 4 error */
static int create_new_empty(const char *path) {
    char tmp[PB + 16];
    snprintf(tmp, sizeof tmp, "%s.tmp%d", path, (int)getpid());
    FILE *f = fopen(tmp, "w");
    if (!f) return 4;
    fclose(f);
    int rc = 0;
    if (link(tmp, path) != 0) rc = (errno == EEXIST) ? 3 : 4;
    unlink(tmp);
    return rc;
}

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ')) s[--n] = 0;
}

/* value of "STATE | <key> | value" in a session.pdl */
static int read_key(const char *path, const char *key, char *out, size_t sz) {
    out[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512], want[128];
    snprintf(want, sizeof want, "STATE | %s | ", key);
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, want, strlen(want)) == 0) {
            snprintf(out, sz, "%s", line + strlen(want));
            trim(out);
            break;
        }
    }
    fclose(f);
    return out[0] != 0;
}

/* same rule as livedesk_next_id */
static int next_id(char *out, size_t sz) {
    int best = 0;
    DIR *d = opendir(ROOT);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)))
            if (e->d_name[0] == 's' && isdigit((unsigned char)e->d_name[1])) {
                int n = atoi(e->d_name + 1);
                if (n > best) best = n;
            }
        closedir(d);
    }
    snprintf(out, sz, "s%d", best + 1);
    return best + 1;
}

typedef struct { int n; char id[64]; char name[128]; } Book;
static int cmp_book(const void *a, const void *b) { return ((const Book *)a)->n - ((const Book *)b)->n; }

static int load_books(Book **out) {
    Book *v = NULL; int cnt = 0, cap = 0;
    DIR *d = opendir(ROOT);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!(e->d_name[0] == 's' && isdigit((unsigned char)e->d_name[1]))) continue;
            char p[PB];
            snprintf(p, sizeof p, "%s/%s/session.pdl", ROOT, e->d_name);
            if (cnt == cap) { cap = cap ? cap * 2 : 16; v = realloc(v, cap * sizeof *v); }
            Book *b = &v[cnt];
            snprintf(b->id, sizeof b->id, "%s", e->d_name);
            b->n = atoi(e->d_name + 1);
            read_key(p, "name", b->name, sizeof b->name);
            cnt++;
        }
        closedir(d);
    }
    if (cnt) qsort(v, cnt, sizeof *v, cmp_book);
    *out = v;
    return cnt;
}

/* lowest-id book called <name>; fills id; returns 1 if found */
static int find_book(const char *name, char *id, size_t sz) {
    Book *v; int n = load_books(&v), hit = 0;
    for (int i = 0; i < n; i++)
        if (strcmp(v[i].name, name) == 0) { snprintf(id, sz, "%s", v[i].id); hit = 1; break; }
    free(v);
    return hit;
}

static int mkdir_p1(const char *p) { return (mkdir(p, 0775) == 0 || errno == EEXIST) ? 0 : -1; }

/* livedesk_ensure_session shape for a NEW id: name row + desks/desk_01.pdl (empty) */
static int make_book(const char *name, char *id, size_t sz) {
    next_id(id, sz);
    char sdir[PB], desks[PB], sp[PB], d1[PB], row[256];
    snprintf(sdir, sizeof sdir, "%s/%s", ROOT, id);
    snprintf(desks, sizeof desks, "%s/desks", sdir);
    snprintf(sp, sizeof sp, "%s/session.pdl", sdir);
    snprintf(d1, sizeof d1, "%s/desk_01.pdl", desks);
    if (mkdir_p1(sdir) || mkdir_p1(desks)) return 4;
    snprintf(row, sizeof row, "STATE | name | %s\n", name);
    if (write_file_atomic(sp, row) != 0) return 4;
    int rc = create_new_empty(d1);
    return (rc == 4) ? 4 : 0;
}

/* livedesk_write_active_desk shape */
static int write_active(const char *id, const char *page) {
    char sp[PB], name[256], body[600];
    snprintf(sp, sizeof sp, "%s/%s/session.pdl", ROOT, id);
    read_key(sp, "name", name, sizeof name);
    body[0] = 0;
    if (name[0]) snprintf(body, sizeof body, "STATE | name | %s\n", name);
    size_t l = strlen(body);
    snprintf(body + l, sizeof body - l, "STATE | active_desk | %s\n", page);
    return write_file_atomic(sp, body) == 0 ? 0 : 4;
}

static void ledger(const char *verb, const char *book, const char *page, int rc) {
    char p[PB];
    snprintf(p, sizeof p, "%s/book_page_ledger.txt", ROOT);
    FILE *f = fopen(p, "a");
    if (!f) return;
    char res[32];
    if (rc == 0) snprintf(res, sizeof res, "ok"); else snprintf(res, sizeof res, "exit=%d", rc);
    fprintf(f, "%ld | %s | %s | %s | %s\n", (long)time(NULL), verb, book, page, res);
    fclose(f);
}

static int split_bp(const char *arg, char *book, char *page, size_t sz) {
    const char *c = strchr(arg, ':');
    if (!c) { snprintf(book, sz, "%s", arg); page[0] = 0; return 0; }
    if ((size_t)(c - arg) >= sz) return -1;
    memcpy(book, arg, c - arg); book[c - arg] = 0;
    snprintf(page, sz, "%s", c + 1);
    return 1;
}

static int do_new_page(const char *book, const char *page, int activate) {
    if (!valid_name(book) || !valid_name(page)) return 2;
    char id[64];
    int rc;
    if (!find_book(book, id, sizeof id)) {
        if ((rc = make_book(book, id, sizeof id)) != 0) return rc;
        if (strcmp(page, "desk_01") == 0) goto done;   /* the book's default page was just made */
    } else {
        char desks[PB];
        snprintf(desks, sizeof desks, "%s/%s/desks", ROOT, id);
        if (mkdir_p1(desks)) return 4;
    }
    char pp[PB];
    snprintf(pp, sizeof pp, "%s/%s/desks/%s.pdl", ROOT, id, page);
    if ((rc = create_new_empty(pp)) != 0) return rc;
done:
    if (activate && write_active(id, page) != 0) return 4;
    printf("created %s:%s (%s)\n", book, page, id);
    return 0;
}

static int do_new_book(const char *book) {
    if (!valid_name(book)) return 2;
    char id[64];
    if (find_book(book, id, sizeof id)) return 3;
    int rc = make_book(book, id, sizeof id);
    if (rc == 0) printf("created %s (%s)\n", book, id);
    return rc;
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int do_list(const char *book) {
    if (!book) {
        Book *v; int n = load_books(&v);
        for (int i = 0; i < n; i++) puts(v[i].name[0] ? v[i].name : v[i].id);
        free(v);
        return 0;
    }
    if (!valid_name(book)) return 2;
    char id[64], dp[PB];
    if (!find_book(book, id, sizeof id)) return 1;
    snprintf(dp, sizeof dp, "%s/%s/desks", ROOT, id);
    char *names[4096]; int n = 0;
    DIR *d = opendir(dp);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && n < 4096) {
            size_t l = strlen(e->d_name);
            if (l > 4 && strcmp(e->d_name + l - 4, ".pdl") == 0 && e->d_name[0] != '.') {
                names[n] = strndup(e->d_name, l - 4); n++;
            }
        }
        closedir(d);
    }
    qsort(names, n, sizeof *names, cmp_str);
    for (int i = 0; i < n; i++) { puts(names[i]); free(names[i]); }
    return 0;
}

static int do_exists(const char *book, const char *page) {
    char id[64];
    if (!valid_name(book) || (page[0] && !valid_name(page))) return 2;
    int yes = find_book(book, id, sizeof id);
    if (yes && page[0]) {
        char pp[PB];
        snprintf(pp, sizeof pp, "%s/%s/desks/%s.pdl", ROOT, id, page);
        yes = access(pp, F_OK) == 0;
    }
    puts(yes ? "yes" : "no");
    return yes ? 0 : 1;
}

static int do_rename(const char *book, const char *oldp, const char *newp) {
    if (!valid_name(book) || !valid_name(oldp) || !valid_name(newp)) return 2;
    char id[64], a[PB], b[PB], sp[PB], active[256];
    if (!find_book(book, id, sizeof id)) return 1;
    snprintf(a, sizeof a, "%s/%s/desks/%s.pdl", ROOT, id, oldp);
    snprintf(b, sizeof b, "%s/%s/desks/%s.pdl", ROOT, id, newp);
    if (access(a, F_OK) != 0) return 1;
    if (strcmp(oldp, newp) == 0) return 3;
    if (link(a, b) != 0) return errno == EEXIST ? 3 : 4;   /* never clobbers */
    if (unlink(a) != 0) return 4;
    snprintf(sp, sizeof sp, "%s/%s/session.pdl", ROOT, id);
    read_key(sp, "active_desk", active, sizeof active);
    if (strcmp(active, oldp) == 0 && write_active(id, newp) != 0) return 4;
    printf("renamed %s:%s -> %s\n", book, oldp, newp);
    return 0;
}

int main(int argc, char **argv) {
    const char *pos[16]; int np = 0, activate = 0, have_root = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) { snprintf(ROOT, sizeof ROOT, "%s", argv[++i]); have_root = 1; }
        else if (strcmp(argv[i], "--activate") == 0) activate = 1;
        else if (np < 16) pos[np++] = argv[i];
    }
    if (!have_root || !ROOT[0]) { fprintf(stderr, "book_page_op: --root <sessions_root> is required\n"); return 2; }
    size_t rl = strlen(ROOT);
    while (rl > 1 && ROOT[rl-1] == '/') ROOT[--rl] = 0;
    struct stat st;
    if (stat(ROOT, &st) != 0 || !S_ISDIR(st.st_mode)) { fprintf(stderr, "book_page_op: root is not a directory\n"); return 2; }
    if (np < 1) { fprintf(stderr, "usage: book_page_op --root <root> new-page|new-book|list|exists|rename-page ...\n"); return 2; }

    const char *verb = pos[0];
    char book[128] = "", page[128] = "", extra[128] = "";
    int rc = 2;
    if (!strcmp(verb, "new-page") && np >= 2) {
        int colon = split_bp(pos[1], book, page, sizeof book);
        if (colon == 0) { if (np < 3) goto usage; snprintf(page, sizeof page, "%s", pos[2]); }
        else if (colon < 0) goto usage;
        rc = do_new_page(book, page, activate);
        ledger(verb, book, page, rc);
    } else if (!strcmp(verb, "new-book") && np >= 2) {
        snprintf(book, sizeof book, "%s", pos[1]);
        rc = do_new_book(book);
        ledger(verb, book, "-", rc);
    } else if (!strcmp(verb, "list")) {
        rc = do_list(np >= 2 ? pos[1] : NULL);
    } else if (!strcmp(verb, "exists") && np >= 2) {
        if (split_bp(pos[1], book, page, sizeof book) < 0) goto usage;
        rc = do_exists(book, page);
    } else if (!strcmp(verb, "rename-page") && np >= 3) {
        int colon = split_bp(pos[1], book, page, sizeof book);
        if (colon < 0) goto usage;
        if (colon == 1) snprintf(extra, sizeof extra, "%s", pos[2]);
        else { if (np < 4) goto usage; snprintf(page, sizeof page, "%s", pos[2]); snprintf(extra, sizeof extra, "%s", pos[3]); }
        rc = do_rename(book, page, extra);
        ledger(verb, book, page, rc);
    } else {
usage:
        fprintf(stderr, "usage: book_page_op --root <root> new-page <book> <page> [--activate] | new-book <book> | list [<book>] | exists <book>[:<page>] | rename-page <book> <old> <new>\n");
        return 2;
    }
    if (rc == 2) fprintf(stderr, "book_page_op: bad name (letters, digits, - _ ; 1..48 chars)\n");
    else if (rc == 3) fprintf(stderr, "book_page_op: already exists, nothing overwritten\n");
    else if (rc == 4) fprintf(stderr, "book_page_op: I/O error\n");
    return rc;
}
