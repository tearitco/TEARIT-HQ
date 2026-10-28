/* bv_gpu_raymarch.c - see bv_gpu_raymarch.h + BV-GPU-RENDER-DESIGN.md.
 *
 * Headless EGL (surfaceless) + GLES 3.0. A fullscreen triangle runs a
 * fragment-shader Amanatides-Woo DDA over a GL_R8UI 3D texture of the
 * voxel grid, plus flat-colour AABB slab tests for entities/sky
 * bodies. Renders to an RGBA8 FBO; glReadPixels -> the caller's
 * buffer (rows flipped to top-left origin).
 *
 * One-shot mode (default): build + tear down per call. Persistent mode
 * (bv_gpu_set_persistent(1), used by bv_render_3d --daemon): context,
 * program and textures stay resident; each call is uniform updates +
 * a grid re-upload + draw + readback. */

#define _GNU_SOURCE
#include "bv_gpu_raymarch.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <time.h>

static int g_dbg = 0;
#define DBG(...) do { if (g_dbg) fprintf(stderr, "bv_gpu: " __VA_ARGS__); } while (0)

/* ---- shaders ---- */
static const char *VS_SRC =
"#version 300 es\n"
"void main() {\n"
"  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
"  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
"}\n";

/* The fragment shader is its own file, ops/bv_raymarch.frag (owner 2026-10-27: the shader in a file of its own, one source, edited and relaunched without a C rebuild,
 * like arrow_config.txt). It is read when the GL context is created. Where: BV_SHADER_FILE, else the path bv_render_3d gives bv_gpu_set_shader_path (the house copy), else
 * next to the running binary (../bv_raymarch.frag, ./bv_raymarch.frag), else ./bv_raymarch.frag or ./ops/bv_raymarch.frag from the working directory. Not found = this
 * function says so on stderr and the GPU path is off (the caller falls back to the CPU raymarch). */
static char s_fs_path[4096];
void bv_gpu_set_shader_path(const char *path) { snprintf(s_fs_path, sizeof s_fs_path, "%s", path ? path : ""); }
static char *load_fs(void) {
    char cand[6][4200]; int n = 0; const char *env = getenv("BV_SHADER_FILE");
    if (env && env[0]) snprintf(cand[n++], sizeof cand[0], "%s", env);
    if (s_fs_path[0]) snprintf(cand[n++], sizeof cand[0], "%s", s_fs_path);
    { char exe[4096]; ssize_t k = readlink("/proc/self/exe", exe, sizeof exe - 1);
      if (k > 0) { exe[k] = 0; char *sl = strrchr(exe, '/'); if (sl) { *sl = 0; snprintf(cand[n++], sizeof cand[0], "%s/../bv_raymarch.frag", exe); snprintf(cand[n++], sizeof cand[0], "%s/bv_raymarch.frag", exe); } } }
    snprintf(cand[n++], sizeof cand[0], "bv_raymarch.frag"); snprintf(cand[n++], sizeof cand[0], "ops/bv_raymarch.frag");
    for (int i = 0; i < n; i++) {
        FILE *f = fopen(cand[i], "rb"); if (!f) continue;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz < 100 || sz > (1 << 20)) { fclose(f); continue; }
        char *t = malloc((size_t)sz + 1); if (!t) { fclose(f); return NULL; }
        size_t got = fread(t, 1, (size_t)sz, f); fclose(f); t[got] = 0; return t;
    }
    fprintf(stderr, "bv_gpu: bv_raymarch.frag not found (BV_SHADER_FILE, bv_gpu_set_shader_path, next to the binary, ./ or ./ops/): GPU path off\n");
    return NULL;
}

/* ---- resident state (persistent mode) ---- */
static int        s_persist = 0;
static EGLDisplay s_dpy = EGL_NO_DISPLAY;
static EGLContext s_ctx = EGL_NO_CONTEXT;
static GLuint     s_prog = 0, s_vao = 0, s_fbo = 0, s_rbo = 0, s_tex_grid = 0, s_tex_leg = 0, s_tex_terr = 0, s_tex_mdl = 0, s_tex_atlas = 0, s_tex_cells = 0, s_tex_para = 0;
static int        s_maker_id = -1;            /* maker_id currently uploaded to s_tex_atlas / s_tex_cells */
static int        s_para_id = -1, s_para_ver = -1;
static int        s_fw = 0, s_fh = 0;          /* current FBO size */
static int        s_gw = 0, s_gh = 0, s_gd = 0; /* current grid-tex dims */
static int        s_leg_alloc = 0;            /* legend tex storage created */
static int        s_terr_alloc = 0;           /* terrain-array storage created */
static int        s_mdl_alloc = 0;
/* cached uniform locations (glGetUniformLocation is a string lookup) */
static struct {
    GLint under, nl, amb_on, amb, lpos, lcol, zoff, rock, maker, mk, atlas, cells, para, para_on, para_size, para_loop, eye, fwd, right, up, focal, res, wext, grid, leg, terr, lbbox, light, sky, clear, fog_start, fog_end, nbox, bmin, bmax, bcol, bmdl, mdl, mdim, wire_edge, wire_thin, cut, cuty, cutp, cutr, cuta, cutt, cutb;
} s_u;

static GLuint compile(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]; log[0] = '\0';
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "bv_gpu: %s shader compile failed: %s\n",
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

/* create EGL display+context+program+VAO if not already present */
static int gl_ensure_context(void) {
    if (s_ctx != EGL_NO_CONTEXT) return 0;
    if (getenv("BV_GPU_DEBUG")) g_dbg = 1;

    EGLDisplay dpy = EGL_NO_DISPLAY;
    PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (getPlatformDisplay)
        dpy = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    if (dpy == EGL_NO_DISPLAY) dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) { DBG("no EGL display\n"); return 1; }

    EGLint emaj = 0, emin = 0;
    if (!eglInitialize(dpy, &emaj, &emin)) { DBG("eglInitialize 0x%x\n", eglGetError()); return 1; }
    DBG("EGL %d.%d %s\n", emaj, emin, eglQueryString(dpy, EGL_VENDOR));
    if (!eglBindAPI(EGL_OPENGL_ES_API)) { DBG("bindAPI failed\n"); eglTerminate(dpy); return 1; }

    EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg; EGLint ncfg = 0;
    if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &ncfg) || ncfg < 1) {
        DBG("chooseConfig failed\n"); eglTerminate(dpy); return 1;
    }
    EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ctx == EGL_NO_CONTEXT) { DBG("createContext 0x%x\n", eglGetError()); eglTerminate(dpy); return 1; }
    if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        DBG("makeCurrent 0x%x\n", eglGetError());
        eglDestroyContext(dpy, ctx); eglTerminate(dpy); return 1;
    }

    GLuint vs = compile(GL_VERTEX_SHADER, VS_SRC);
    char *fs_text = load_fs();
    GLuint fs = fs_text ? compile(GL_FRAGMENT_SHADER, fs_text) : 0;
    free(fs_text);
    if (!vs || !fs) { eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                      eglDestroyContext(dpy, ctx); eglTerminate(dpy); return 1; }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; GLsizei n=0; glGetProgramInfoLog(prog, sizeof(log), &n, log);
               fprintf(stderr, "bv_gpu: link failed:\n%.*s\n", (int)n, log);
               glDeleteProgram(prog);
               eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
               eglDestroyContext(dpy, ctx); eglTerminate(dpy); return 1; }

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);

    /* legend texture (contents re-uploaded per frame) */
    GLuint leg = 0;
    glGenTextures(1, &leg);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, leg);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* terrain emoji atlas: 16x16 x up-to-64 layers, one per legend entry */
    GLuint terr = 0;
    glGenTextures(1, &terr);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, terr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint mdl = 0;
    glGenTextures(1, &mdl);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_3D, mdl);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    /* maker_view atlas + cell map: 1x1 placeholders until a maker frame uploads the real ones (a sampler must never be left on a
     * unit another sampler type uses, or the draw fails with INVALID_OPERATION). */
    GLuint atl = 0, cel = 0;
    glGenTextures(1, &atl);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, atl);
    { unsigned char px[4] = {0,0,0,0}; glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px); }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenTextures(1, &cel);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, cel);
    { unsigned short px[2] = {0,0}; glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16UI, 1, 1, 0, GL_RG_INTEGER, GL_UNSIGNED_SHORT, px); }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    s_maker_id = -1;
    GLuint para = 0;
    glGenTextures(1, &para);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, para);
    { unsigned char px[4] = {0,0,0,0}; glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px); }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    s_para_id = -1; s_para_ver = -1;

    s_dpy = dpy; s_ctx = ctx; s_prog = prog; s_vao = vao; s_tex_leg = leg; s_tex_terr = terr; s_tex_mdl = mdl; s_tex_atlas = atl; s_tex_cells = cel; s_tex_para = para;
    s_leg_alloc = 0; s_terr_alloc = 0; s_mdl_alloc = 0;

    #define UL(n) glGetUniformLocation(prog, n)
    s_u.eye=UL("u_eye"); s_u.fwd=UL("u_fwd"); s_u.right=UL("u_right"); s_u.up=UL("u_up");
    s_u.focal=UL("u_focal"); s_u.res=UL("u_res"); s_u.wext=UL("u_wext");
    s_u.grid=UL("u_grid"); s_u.leg=UL("u_leg"); s_u.terr=UL("u_terr"); s_u.lbbox=UL("u_lbbox");
    s_u.light=UL("u_light"); s_u.nl=UL("u_nl"); s_u.amb_on=UL("u_amb_on"); s_u.amb=UL("u_amb"); s_u.lpos=UL("u_lpos"); s_u.lcol=UL("u_lcol"); s_u.sky=UL("u_sky"); s_u.clear=UL("u_clear"); s_u.fog_start=UL("u_fog_start"); s_u.fog_end=UL("u_fog_end");
    s_u.maker=UL("u_maker"); s_u.zoff=UL("u_zoff"); s_u.rock=UL("u_rock"); s_u.mk=UL("u_mk"); s_u.atlas=UL("u_atlas"); s_u.cells=UL("u_cells"); s_u.under=UL("u_under");
    s_u.para=UL("u_para"); s_u.para_on=UL("u_para_on"); s_u.para_size=UL("u_para_size"); s_u.para_loop=UL("u_para_loop");
    s_u.wire_edge=UL("u_wire_edge"); s_u.wire_thin=UL("u_wire_thin"); s_u.nbox=UL("u_nbox"); s_u.bmin=UL("u_bmin"); s_u.bmax=UL("u_bmax"); s_u.bcol=UL("u_bcol");
    s_u.cut=UL("u_cut"); s_u.cuty=UL("u_cuty"); s_u.cutp=UL("u_cutp"); s_u.cutr=UL("u_cutr"); s_u.cuta=UL("u_cuta"); s_u.cutt=UL("u_cutt"); s_u.cutb=UL("u_cutb");
    s_u.bmdl=UL("u_bmdl"); s_u.mdl=UL("u_mdl"); s_u.mdim=UL("u_mdim");
    #undef UL
    return 0;
}

static int gl_ensure_targets(int w, int h) {
    if (s_fbo && s_fw == w && s_fh == h) return 0;
    if (s_rbo) { glDeleteRenderbuffers(1, &s_rbo); s_rbo = 0; }
    if (s_fbo) { glDeleteFramebuffers(1, &s_fbo); s_fbo = 0; }
    glGenFramebuffers(1, &s_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glGenRenderbuffers(1, &s_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, s_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, s_rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "bv_gpu: FBO incomplete %dx%d\n", w, h);
        return 1;
    }
    s_fw = w; s_fh = h;
    return 0;
}

static void gl_ensure_grid_tex(int gw, int gh, int gd) {
    glActiveTexture(GL_TEXTURE0);
    if (!s_tex_grid) {
        glGenTextures(1, &s_tex_grid);
        glBindTexture(GL_TEXTURE_3D, s_tex_grid);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        s_gw = s_gh = s_gd = 0;
    } else {
        glBindTexture(GL_TEXTURE_3D, s_tex_grid);
    }
    if (s_gw != gw || s_gh != gh || s_gd != gd) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, gw, gh, gd, 0,
                     GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
        s_gw = gw; s_gh = gh; s_gd = gd;
    }
}

void bv_gpu_shutdown(void) {
    if (s_dpy == EGL_NO_DISPLAY) { s_persist = 0; return; }
    eglMakeCurrent(s_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, s_ctx);
    if (s_vao) glDeleteVertexArrays(1, &s_vao);
    if (s_prog) glDeleteProgram(s_prog);
    if (s_tex_grid) glDeleteTextures(1, &s_tex_grid);
    if (s_tex_leg) glDeleteTextures(1, &s_tex_leg);
    if (s_tex_terr) glDeleteTextures(1, &s_tex_terr);
    if (s_tex_atlas) glDeleteTextures(1, &s_tex_atlas);
    if (s_tex_para) glDeleteTextures(1, &s_tex_para);
    if (s_tex_cells) glDeleteTextures(1, &s_tex_cells);
    if (s_tex_mdl) glDeleteTextures(1, &s_tex_mdl);
    if (s_rbo) glDeleteRenderbuffers(1, &s_rbo);
    if (s_fbo) glDeleteFramebuffers(1, &s_fbo);
    eglMakeCurrent(s_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s_ctx != EGL_NO_CONTEXT) eglDestroyContext(s_dpy, s_ctx);
    eglTerminate(s_dpy);
    s_dpy = EGL_NO_DISPLAY; s_ctx = EGL_NO_CONTEXT;
    s_prog = s_vao = s_fbo = s_rbo = s_tex_grid = s_tex_leg = s_tex_terr = s_tex_mdl = s_tex_atlas = s_tex_cells = s_tex_para = 0;
    s_fw = s_fh = s_gw = s_gh = s_gd = 0;
    s_leg_alloc = 0;
    s_terr_alloc = 0;
    s_mdl_alloc = 0;
    s_persist = 0;
}

void bv_gpu_set_persistent(int on) {
    if (on) { s_persist = 1; return; }
    bv_gpu_shutdown();   /* clears s_persist */
}

int bv_gpu_raymarch(const BvGpuScene *s, unsigned char *out) {
    if (getenv("BV_GPU_DEBUG")) g_dbg = 1;
    if (!s || !out || s->w <= 0 || s->h <= 0) return 1;
    if (s->board_w <= 0 || s->board_h <= 0 || s->z_count <= 0 || !s->grid) return 1;

    int step = (s->lod_step > 1) ? s->lod_step : 1;
    if (step > 4) step = 4;
    int rw = s->w / step, rh = s->h / step;
    if (rw < 32) rw = 32;
    if (rh < 32) rh = 32;

    if (gl_ensure_context() != 0) { if (!s_persist) bv_gpu_shutdown(); return 1; }
    /* FBO is always full output size - LOD just renders a sub-rect of
     * it (glViewport), so toggling LOD never re-allocates the FBO. */
    if (gl_ensure_targets(s->w, s->h) != 0) { if (!s_persist) bv_gpu_shutdown(); return 1; }
    gl_ensure_grid_tex(s->board_w, s->board_h, s->z_count);

    int rc = 1;

    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glBindVertexArray(s_vao);
    glUseProgram(s_prog);

    /* grid upload */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, s_tex_grid);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, s->board_w, s->board_h, s->z_count,
                    GL_RED_INTEGER, GL_UNSIGNED_BYTE, s->grid);

    int nlay = s->legend_n; if (nlay > BV_GPU_MAX_LEGEND) nlay = BV_GPU_MAX_LEGEND;
    if (nlay < 1) nlay = 1;

    /* legend LUT: .rgb = flat colour, .a*255 = 0 (air) or layer+1 (solid) */
    {
        unsigned char lut[256 * 4];
        memset(lut, 0, sizeof(lut));
        for (int i = 0; i < nlay; i++) {
            unsigned bb = s->legend_glyph[i];
            lut[bb*4+0] = (unsigned char)(s->legend_rgb[i][0] * 255.0f + 0.5f);
            lut[bb*4+1] = (unsigned char)(s->legend_rgb[i][1] * 255.0f + 0.5f);
            lut[bb*4+2] = (unsigned char)(s->legend_rgb[i][2] * 255.0f + 0.5f);
            lut[bb*4+3] = (unsigned char)(i + 1);
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s_tex_leg);
        if (!s_leg_alloc) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, lut);
            s_leg_alloc = 1;
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, lut);
        }
    }

    /* terrain emoji atlas + per-layer bbox */
    {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D_ARRAY, s_tex_terr);
        /* legend_tex[64][1024] is contiguous -> one upload of nlay layers */
        if (!s_terr_alloc) {
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 16, 16, BV_GPU_MAX_LEGEND);
            s_terr_alloc = 1;
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 16, 16, nlay,
                        GL_RGBA, GL_UNSIGNED_BYTE, s->legend_tex);
        float lbb[BV_GPU_MAX_LEGEND * 4];
        for (int i = 0; i < nlay; i++) {
            if (s->legend_has_tex[i]) {
                lbb[i*4+0]=s->legend_bbox[i][0]; lbb[i*4+1]=s->legend_bbox[i][1];
                lbb[i*4+2]=s->legend_bbox[i][2]; lbb[i*4+3]=s->legend_bbox[i][3];
            } else { lbb[i*4+0]=0; lbb[i*4+1]=0; lbb[i*4+2]=1; lbb[i*4+3]=1; }
        }
        glUniform4fv(s_u.lbbox, nlay, lbb);
    }

    /* phymoji model 3D texture: 32x32x(8*BV_GPU_MAX_MODEL), model m in
     * z-slices [m*8, m*8+8) */
    if (s->model_n > 0) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_3D, s_tex_mdl);
        if (!s_mdl_alloc) {
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, BV_GPU_MDL_DIM, BV_GPU_MDL_DIM,
                         BV_GPU_MDL_DEPTH * BV_GPU_MAX_MODEL, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            s_mdl_alloc = 1;
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        int mn = s->model_n; if (mn > BV_GPU_MAX_MODEL) mn = BV_GPU_MAX_MODEL;
        for (int m = 0; m < mn; m++)
            glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, m * BV_GPU_MDL_DEPTH,
                            BV_GPU_MDL_DIM, BV_GPU_MDL_DIM, BV_GPU_MDL_DEPTH,
                            GL_RGBA, GL_UNSIGNED_BYTE, s->model_vox[m]);
        int mdim[BV_GPU_MAX_MODEL * 3];
        for (int m = 0; m < BV_GPU_MAX_MODEL; m++) {
            mdim[m*3+0] = (m < mn && s->model_dim[m][0] > 0) ? s->model_dim[m][0] : 1;
            mdim[m*3+1] = (m < mn && s->model_dim[m][1] > 0) ? s->model_dim[m][1] : 1;
            mdim[m*3+2] = (m < mn && s->model_dim[m][2] > 0) ? s->model_dim[m][2] : 1;
        }
        glUniform3iv(s_u.mdim, BV_GPU_MAX_MODEL, mdim);
    }
    glUniform1i(s_u.mdl, 3);

    glUniform3fv(s_u.eye,   1, s->eye);
    glUniform3fv(s_u.fwd,   1, s->fwd);
    glUniform3fv(s_u.right, 1, s->right);
    glUniform3fv(s_u.up,    1, s->up);
    glUniform1f (s_u.focal, s->focal * (float)rh / (float)s->h);   /* same FOV at the reduced res */
    glUniform2f (s_u.res,   (float)rw, (float)rh);
    glUniform3f (s_u.wext,  (float)s->board_w, (float)s->z_count, (float)s->board_h);
    glUniform1i (s_u.grid,  0);
    glUniform1i (s_u.leg,   1);
    glUniform1i (s_u.terr,  2);
    /* maker_view: upload the atlas + cell map only when maker_id changed; samplers always point at their own units */
    int under_on = (s->maker && s->under) ? 1 : 0;
    if (s->maker && s->atlas && s->cells && s->maker_id != s_maker_id) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, s_tex_atlas);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, s->atlas_w, s->atlas_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->atlas);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, s_tex_cells);
        if (s->under) {      /* the cells, then a second block of rows holding the under-run slots in the red channel */
            size_t n = (size_t)s->cells_w * (size_t)s->cells_h; unsigned short *tmp = (unsigned short *)calloc(n * 4, sizeof(unsigned short));
            if (tmp) { memcpy(tmp, s->cells, n * 2 * sizeof(unsigned short)); for (size_t i = 0; i < n; i++) tmp[n * 2 + i * 2] = s->under[i];
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16UI, s->cells_w, s->cells_h * 2, 0, GL_RG_INTEGER, GL_UNSIGNED_SHORT, tmp); free(tmp); }
        } else glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16UI, s->cells_w, s->cells_h, 0, GL_RG_INTEGER, GL_UNSIGNED_SHORT, s->cells);
        s_maker_id = s->maker_id;
    }
    glUniform1i (s_u.atlas, 4);
    glUniform1i (s_u.cells, 5);
    glUniform1i (s_u.under, under_on);
    glUniform1i (s_u.zoff, s->zoff);
    glUniform1i (s_u.rock, s->rock);
    glUniform1i (s_u.maker, (s->maker && s->atlas && s->cells) ? 1 : 0);
    if (s->para && s->para_w > 0 && s->para_h > 0 && (s->maker_id != s_para_id || s->para_ver != s_para_ver)) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, s_tex_para);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, s->para_w, s->para_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->para);
        s_para_id = s->maker_id; s_para_ver = s->para_ver;
    }
    glUniform1i(s_u.para, 6);
    glUniform1i(s_u.para_on, (s->para && s->para_w > 0) ? 1 : 0);
    glUniform2f(s_u.para_size, (float)(s->para_w > 0 ? s->para_w : 1), (float)(s->para_h > 0 ? s->para_h : 1));
    glUniform2f(s_u.para_loop, s->para_loop_x ? 1.0f : 0.0f, s->para_loop_y ? 1.0f : 0.0f);
    glUniform2i (s_u.mk, s->atlas_cols > 0 ? s->atlas_cols : 1, s->tile_px > 0 ? s->tile_px : 24);
    glActiveTexture(GL_TEXTURE0);
    glUniform1f (s_u.light, s->light_level);
    { int nl = s->light_n < 0 ? 0 : s->light_n > BV_GPU_MAX_LIGHT ? BV_GPU_MAX_LIGHT : s->light_n; glUniform1i(s_u.nl, nl); glUniform1i(s_u.amb_on, s->light_amb_on ? 1 : 0); glUniform1f(s_u.amb, s->light_amb);
      if (nl) { glUniform4fv(s_u.lpos, nl, &s->light_pos[0][0]); glUniform4fv(s_u.lcol, nl, &s->light_col[0][0]); } }
    glUniform3fv(s_u.sky,   1, s->sky);
    glUniform1i(s_u.clear, s->see_through ? 1 : 0);
    glUniform1f(s_u.fog_start, s->fog_start);
    glUniform1f(s_u.fog_end,   s->fog_end);
    glUniform1i(s_u.cut, s->cut_on ? 1 : 0); glUniform1i(s_u.cuty, s->cut_y); glUniform3fv(s_u.cutp, 1, s->cut_pos);
    glUniform1f(s_u.cutr, s->cut_r > 0 ? s->cut_r : 8.0f); glUniform1f(s_u.cuta, s->cut_a > 0 ? s->cut_a : 0.3f); glUniform1f(s_u.cutt, s->cut_depth > 0 ? s->cut_depth : 12.0f); glUniform1f(s_u.cutb, (float)s->cut_base);
    {
        int nb = s->box_n; if (nb > BV_GPU_MAX_BOX) nb = BV_GPU_MAX_BOX; if (nb > 128) nb = 128;
        glUniform1i(s_u.nbox, nb);
        glUniform1f(s_u.wire_edge, s->wire_edge > 0 ? s->wire_edge : 0.10f);
        glUniform1f(s_u.wire_thin, s->wire_thin > 0 ? s->wire_thin : 0.03f);
        if (nb > 0) {
            float bmin[128*3], bmax[128*3], bcol[128*4];
            int bmdl[128];
            for (int i = 0; i < nb; i++) {
                bmin[i*3+0]=s->box[i].min_x; bmin[i*3+1]=s->box[i].min_y; bmin[i*3+2]=s->box[i].min_z;
                bmax[i*3+0]=s->box[i].max_x; bmax[i*3+1]=s->box[i].max_y; bmax[i*3+2]=s->box[i].max_z;
                bcol[i*4+0]=s->box[i].r; bcol[i*4+1]=s->box[i].g; bcol[i*4+2]=s->box[i].b;
                bcol[i*4+3]=s->box[i].wire == 2 ? 0.15f : s->box[i].wire ? 0.25f : (s->box[i].self_lit ? 0.0f : 1.0f);
                {
                    int m = s->box[i].model, sw = 0;
                    if (m >= 128) { sw = 1; m -= 128; }
                    bmdl[i] = (m >= 0 && m < BV_GPU_MAX_MODEL) ? (m + (sw ? 128 : 0)) : -1;
                }
            }
            glUniform3fv(s_u.bmin, nb, bmin);
            glUniform3fv(s_u.bmax, nb, bmax);
            glUniform4fv(s_u.bcol, nb, bcol);
            glUniform1iv(s_u.bmdl, nb, bmdl);
        }
    }

    glViewport(0, 0, rw, rh);
    glDisable(GL_DEPTH_TEST);
    glClearColor(s->sky[0], s->sky[1], s->sky[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    {
        GLenum e = glGetError();
        if (e != GL_NO_ERROR) { fprintf(stderr, "bv_gpu: GL error 0x%x after draw\n", e); goto done; }
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (step == 1) {
        glReadPixels(0, 0, s->w, s->h, GL_RGBA, GL_UNSIGNED_BYTE, out);   /* shader renders top-down -> no flip */
    } else {
        /* read the reduced frame, then nearest-upscale into the full out.
         * REAL FIX 2026-09-30, direct live report ("drag window bigger,
         * doesn't make camera lens wider, it just shows black") - this
         * used to be a fixed `scratch[1280*960*4]`, sized for whatever
         * window dimensions someone tested against at the time. Any
         * window resize big enough that rw*rh (the LOD-reduced render,
         * still s->w/h / lod_step - a live drag-resize runs through
         * THIS branch every frame, since resizing counts as motion,
         * g_lod_step>1) exceeded that fixed cap hit the guard below and
         * bailed with zero frame written - the exact black screen
         * reported. A resizable window has no real upper bound, so
         * there is no correct fixed constant here; grown on demand
         * instead, persisted across calls (same `static` lifetime the
         * fixed buffer had, just heap-backed and resizable) so a normal
         * fixed-size session pays one allocation, not one per frame. */
        static unsigned char *scratch = NULL;
        static size_t scratch_cap = 0;
        size_t need = (size_t)rw * rh * 4;
        if (need > scratch_cap) {
            unsigned char *grown = realloc(scratch, need);
            if (!grown) { fprintf(stderr, "bv_gpu: LOD scratch realloc failed (%zu bytes)\n", need); goto done; }
            scratch = grown;
            scratch_cap = need;
        }
        glReadPixels(0, 0, rw, rh, GL_RGBA, GL_UNSIGNED_BYTE, scratch);
        for (int y = 0; y < s->h; y++) {
            int sy = y * rh / s->h; if (sy >= rh) sy = rh - 1;
            const unsigned char *srow = scratch + (size_t)sy * rw * 4;
            unsigned char *drow = out + (size_t)y * s->w * 4;
            for (int x = 0; x < s->w; x++) {
                int sx = x * rw / s->w; if (sx >= rw) sx = rw - 1;
                const unsigned char *sp = srow + (size_t)sx * 4;
                unsigned char *dp = drow + (size_t)x * 4;
                dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2]; dp[3]=255;
            }
        }
    }
    rc = 0;

done:
    if (!s_persist) bv_gpu_shutdown();
    return rc;
}

#ifdef BV_GPU_STANDALONE_TEST
int main(void) {
    g_dbg = 1;
    int W = 400, H = 300;
    BvGpuScene s; memset(&s, 0, sizeof(s));
    s.w = W; s.h = H;
    s.focal = (H / 2.0f) / 0.8391f;
    s.board_w = 8; s.board_h = 8; s.z_count = 4;
    static unsigned char grid[8*8*4];
    for (int lvl = 0; lvl < 2; lvl++)
      for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
          grid[col + row*8 + lvl*64] = 's';
    s.grid = grid;
    s.legend_n = 1; s.legend_glyph[0] = 's';
    s.legend_rgb[0][0]=0.47f; s.legend_rgb[0][1]=0.47f; s.legend_rgb[0][2]=0.47f;
    s.light_level = 1.0f;
    s.sky[0]=0.53f; s.sky[1]=0.81f; s.sky[2]=0.92f;
    s.eye[0]=4; s.eye[1]=8; s.eye[2]=-6;
    float fl = 1.0f/1.118f;
    s.fwd[0]=0; s.fwd[1]=-0.5f*fl; s.fwd[2]=1.0f*fl;
    s.right[0]=1; s.right[1]=0; s.right[2]=0;
    s.up[0]=s.right[1]*s.fwd[2]-s.right[2]*s.fwd[1];
    s.up[1]=s.right[2]*s.fwd[0]-s.right[0]*s.fwd[2];
    s.up[2]=s.right[0]*s.fwd[1]-s.right[1]*s.fwd[0];
    unsigned char *out = malloc((size_t)W*H*4);
    /* exercise persistent mode: two renders, one context */
    bv_gpu_set_persistent(1);
    int rc = bv_gpu_raymarch(&s, out);
    rc |= bv_gpu_raymarch(&s, out);
    bv_gpu_shutdown();
    fprintf(stderr, "rc=%d\n", rc);
    if (rc == 0) {
        FILE *f = fopen("/tmp/bv_gpu_test.ppm", "wb");
        fprintf(f, "P6\n%d %d\n255\n", W, H);
        for (int i = 0; i < W*H; i++) fwrite(out + i*4, 1, 3, f);
        fclose(f);
        fprintf(stderr, "wrote /tmp/bv_gpu_test.ppm\n");
    }
    free(out);
    return rc;
}
#endif
