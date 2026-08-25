/* khtpm_core.h — SHARED livedesk/KHTPM design logic (OS-independent).
 *
 * House rule (WIN-COMPAT-RULE.md): develop on Linux; Windows is
 * compatibility via thin shims. Put menus, registry, sprites, history
 * HERE — never re-copy into plat_win / plat_x11 when design changes.
 *
 * Platform code only: create window, GL context, events, popup shell.
 */
#ifndef KHTPM_CORE_H
#define KHTPM_CORE_H

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KHTPM_PATH_BUF 4352
#define KHTPM_MAX_METHODS 32
#define KHTPM_MAX_PAGES 8
#define KHTPM_GRID_CELL_PX 80
#define KHTPM_DEFAULT_WIN_PX 64

typedef struct {
    char label[128];
    char action[KHTPM_PATH_BUF];
} KhtpmMethod;

typedef struct {
    char name[32];
    KhtpmMethod items[KHTPM_MAX_METHODS];
    int n_items;
} KhtpmPage;

typedef struct {
    char package_dir[KHTPM_PATH_BUF];
    char house_root[KHTPM_PATH_BUF]; /* usually "." when CWD=house */
    char entity[128];
    char glyph[256];
    char history_path[KHTPM_PATH_BUF];
    char relay_path[KHTPM_PATH_BUF];
    char full_id[96];
    int livedesk_index;
    int win_px; /* DEFAULT_WIN_PX * footprint_tiles */
    int footprint_tiles;

    /* sprite pixels (RGBA), owned by core until khtpm_sprite_free */
    unsigned char *sprite_pixels;
    int sprite_res;

    /* multi-page objects.pdl */
    KhtpmPage pages[KHTPM_MAX_PAGES];
    int n_pages;
    int page_stack[KHTPM_MAX_PAGES];
    int page_stack_n;
    int cur_page; /* index into pages[], or -1 if methods-only */

    /* flat method list for current view (objects page or meta methods) */
    KhtpmMethod view[KHTPM_MAX_METHODS];
    int n_view;
} KhtpmEntity;

/* ---- paths / package ---- */
void khtpm_path_join(char *out, size_t n, const char *a, const char *b);
void khtpm_path_norm(char *s); /* / -> \ on Win only if needed by caller */

/* package_dir may be relative; house_root usually "." */
int  khtpm_entity_init(KhtpmEntity *e, const char *package_dir, const char *house_root);
void khtpm_entity_shutdown(KhtpmEntity *e);

/* ---- sprite.csv ---- */
int  khtpm_sprite_load(KhtpmEntity *e); /* package_dir/sprite.csv */
void khtpm_sprite_free(KhtpmEntity *e);

/* ---- history / relay ---- */
void khtpm_history(const KhtpmEntity *e, const char *fmt, ...);
void khtpm_relay_clear(const KhtpmEntity *e);
/* returns 1 if CLOSE requested.
 * *raise_out=1 ACTIVATE; *open_menu_out=1 OPEN_CONTEXT (toolbar parity) */
int  khtpm_relay_poll(const KhtpmEntity *e, int *raise_out, int *open_menu_out);

/* ---- desktop position ---- */
void khtpm_pos_read(const KhtpmEntity *e, int *x, int *y);
void khtpm_pos_write(const KhtpmEntity *e, int x, int y);
void khtpm_pos_clamp(int *x, int *y, int win_px, int screen_w, int screen_h);

/* ---- livedesk registry ---- */
int  khtpm_registry_next_index(const char *house_root);
void khtpm_registry_add(const KhtpmEntity *e, int pid);
void khtpm_registry_remove(const char *house_root, int pid);

/* ---- menus (objects.pdl + meta.pdl) — THE design surface ---- */
/* load pages + methods; build initial view (main page or methods) */
int  khtpm_menu_load(KhtpmEntity *e);
/* rebuild e->view for current page */
void khtpm_menu_build_view(KhtpmEntity *e);
/* apply action; returns:
 *   KHTPM_ACT_NONE, CLOSE, RAISE_MENU (GOTO/BACK), RUN_SHELL (shell in out_cmd) */
#define KHTPM_ACT_NONE       0
#define KHTPM_ACT_CLOSE      1
#define KHTPM_ACT_RAISE_MENU 2
#define KHTPM_ACT_RUN        3
#define KHTPM_ACT_OPEN_DIR   4
#define KHTPM_ACT_SKIP       5 /* linux-only / void */

int khtpm_menu_apply(KhtpmEntity *e, const char *action,
                     char *out_cmd, size_t out_cmd_sz);

/* strip /home/.../house/ prefix to house-relative if present */
void khtpm_action_make_portable(const char *in, char *out, size_t out_sz);

/* package still present? */
int khtpm_package_exists(const KhtpmEntity *e);

/* taskbar ensure path helper (relative) */
void khtpm_taskbar_exe_rel(char *out, size_t n);

#ifdef __cplusplus
}
#endif
#endif /* KHTPM_CORE_H */
