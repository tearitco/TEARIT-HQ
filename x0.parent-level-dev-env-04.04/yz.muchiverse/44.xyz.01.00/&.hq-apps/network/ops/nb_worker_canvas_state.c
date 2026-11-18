/* nb_worker_canvas_state.c - canvas rasterizer state/types (mechanical extract, single-TU include) */
/* REAL, NEW 2026-10-07 (maximal-scope directive: canvas fast) - software
 * rasterizer backing every canvas 2D context. Before this, every draw op
 * was a noop and pages rendered blank canvases; now fill/clear/blit ops
 * write real pixels readable back via getImageData. Deliberately partial:
 * full affine image/rect sampling, source-over alpha, save/restore;
 * text drawing stays noop, gradients/patterns are flat colors, shadow/
 * filter/composite ops ignored. Display plumbing (surface publish) is
 * the next slice - this one proves pixel truth through getImageData. */
#define NB_CANVAS_MAX 64
#define NB_CANVAS_PX_MAX (2048 * 2048)
typedef struct {
    int used, w, h;
    unsigned char *px;      /* RGBA, w*h*4, straight alpha */
    double m[6];            /* current transform a,b,c,d,e,f */
    double alpha;           /* snapshot cache; live prop re-read per draw */
    int dirty;              /* set by any mutating op, cleared on publish */
    int save_depth;
    double save_stack[8][7]; /* m[6] + alpha */
    char fillstyle[128];
} NbCanvas;
static NbCanvas g_canvases[NB_CANVAS_MAX];
