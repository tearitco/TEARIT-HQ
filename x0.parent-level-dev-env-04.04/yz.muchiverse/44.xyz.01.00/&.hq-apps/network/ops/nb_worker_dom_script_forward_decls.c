/* nb_worker_dom_script_forward_decls.c - DOM/script forward declarations (mechanical extract, single-TU include) */

static const char *img_get_src(const NbNode *n);
static void img_set_src(struct NbNode *n, const char *s);
static int layout_hidden_anc(struct NbNode *n);
static void layout_xy(struct NbNode *n, double *out_x, double *out_y);
static void img_set_decoded(NbNode *n, int w, int h, unsigned char *data);
static int img_get_decoded(const NbNode *n, int *w, int *h);
static unsigned char *b64_decode(const char *in, size_t *out_len);

/* Forward declarations for functions used in script src setter */
static void cookie_header_for_url(const char *url, char *out, size_t olen);
static void cfg_line(FILE *f, const char *key, const char *val);
static void resolve_doc_url(const char *rel, char *out, size_t olen);
static int peval_budget(JSContext *ctx, const char *src, size_t src_n, char *errbuf, size_t errbuf_sz);
static void nb_mutation_notify_childlist(JSContext *ctx, NbNode *target, NbNode *added, NbNode *removed, NbNode *prev, NbNode *next);
static JSValue nb_dom_createDocumentFragment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static void nb_mutation_notify_attribute(JSContext *ctx, NbNode *target, const char *name, const char *old_value);
static void nb_mutation_notify_characterData(JSContext *ctx, NbNode *target, const char *old_value);
static void nb_obs_recheck(JSContext *ctx);
static NbNode *get_this(JSContext *ctx, JSValueConst this_val);
static char *attrs_set(const NbNode *n, const char *name, const char *val);
