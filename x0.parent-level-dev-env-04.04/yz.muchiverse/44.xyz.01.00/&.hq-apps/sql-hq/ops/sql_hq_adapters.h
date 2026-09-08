/* sql_hq_adapters.h — see sql_hq_adapters.c. */
#ifndef SQL_HQ_ADAPTERS_H
#define SQL_HQ_ADAPTERS_H

#define SH_MAX_COLS  256
#define SH_MAX_LINE  (64 * 1024)
#define SH_MAX_CELL  4096

typedef struct {
    char    name[64];        /* sqlite table name (basename, sanitised) */
    char    src_path[1024];  /* backing file */
    char    src_kind;        /* 'c' csv | 'f' pdl-flat | 'r' pdl-record */
    int     ncols;
    int     nrows;
    char  **cols;            /* [ncols]  column names */
    char ***cells;           /* [nrows][ncols]  string cells (never NULL after load) */
    int     dirty;           /* set by the engine when a query wrote to this table */
} ShTable;

/* auto-detects by extension (.csv / .pdl) then, for .pdl, sniffs
 * flat-config vs record shape. Returns 1 on success. */
int  sh_load(const char *path, ShTable *t);

/* re-emit `t` to `path` in t->src_kind's shape, atomically (tmp+rename).
 * Returns 1 on success. */
int  sh_dump(const ShTable *t, const char *path);

void sh_free(ShTable *t);

#endif
