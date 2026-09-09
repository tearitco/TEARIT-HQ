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

static const char *FS_SRC =
"#version 300 es\n"
"precision highp float;\n"
"precision highp int;\n"
"precision highp usampler3D;\n"
"precision highp sampler3D;\n"
"precision highp sampler2D;\n"
"precision highp sampler2DArray;\n"
"out vec4 o_col;\n"
"uniform vec3  u_eye, u_fwd, u_right, u_up;\n"
"uniform float u_focal;\n"
"uniform vec2  u_res;\n"
"uniform vec3  u_wext;\n"          /* world extents: (board_w, z_count, board_h) */
"uniform highp usampler3D u_grid;\n"
"uniform sampler2D u_leg;\n"       /* 256x1 RGBA8: .rgb flat colour, .a*255 = 0 air / layer+1 solid */
"uniform sampler2DArray u_terr;\n" /* 16x16xN terrain emoji slices, layer per legend entry */
"uniform vec4  u_lbbox[64];\n"     /* per-layer opaque bbox (u0,v0,u1,v1) in 0..1 */
"uniform float u_light;\n"
"uniform vec3  u_sky;\n"
"uniform int   u_nbox;\n"
"uniform vec3  u_bmin[128];\n"
"uniform vec3  u_bmax[128];\n"
"uniform vec4  u_bcol[128];\n"     /* .rgb colour, .a: 1 = apply light, 0 = self-lit */
"\n"
"bool slab(vec3 ro, vec3 rd, vec3 bn, vec3 bx, out float t, out int face) {\n"
"  float tmin = -1e30, tmax = 1e30; face = -1;\n"
"  for (int i = 0; i < 3; i++) {\n"
"    float o = ro[i], d = rd[i];\n"
"    if (abs(d) < 1e-9) { if (o < bn[i] || o > bx[i]) return false; }\n"
"    else {\n"
"      float t0 = (bn[i]-o)/d, t1 = (bx[i]-o)/d; int f0 = i*2;\n"
"      if (t0 > t1) { float tt=t0; t0=t1; t1=tt; f0 = i*2+1; }\n"
"      if (t0 > tmin) { tmin = t0; face = f0; }\n"
"      if (t1 < tmax) tmax = t1;\n"
"      if (tmin > tmax) return false;\n"
"    }\n"
"  }\n"
"  if (tmax < 0.0) return false;\n"
"  t = tmin < 0.0 ? 0.0 : tmin;\n"
"  return true;\n"
"}\n"
"\n"
"void main() {\n"
"  float a = (gl_FragCoord.x - u_res.x * 0.5) / u_focal;\n"
"  float b = (u_res.y * 0.5 - gl_FragCoord.y) / u_focal;\n"   /* flipped: GL row 0 = image top, so glReadPixels needs no row-flip */
"  vec3 rd = normalize(u_fwd + a * u_right + b * u_up);\n"
"  vec3 ro = u_eye;\n"
"\n"
"  float bestT = 1e30;\n"
"  vec3  col   = u_sky;\n"
"  bool  hit = false, self_lit = false;\n"
"  int   face = -1;\n"
"\n"
"  for (int i = 0; i < u_nbox; i++) {\n"
"    float t; int f;\n"
"    if (slab(ro, rd, u_bmin[i], u_bmax[i], t, f) && t < bestT) {\n"
"      bestT = t; col = u_bcol[i].rgb; hit = true;\n"
"      self_lit = (u_bcol[i].a < 0.5); face = f;\n"
"    }\n"
"  }\n"
"\n"
"  float bt; int bf;\n"
"  if (slab(ro, rd, vec3(-1e-3), u_wext + vec3(1e-3), bt, bf)) {\n"
"    float tcur = max(bt, 0.0);\n"
"    vec3  p = ro + rd * (tcur + 1e-4);\n"
"    ivec3 c = clamp(ivec3(floor(p)), ivec3(0), ivec3(u_wext) - 1);\n"
"    ivec3 st = ivec3(sign(rd));\n"
"    vec3  tdelta = abs(1.0 / rd);\n"
"    vec3  tmax;\n"
"    for (int i = 0; i < 3; i++) {\n"
"      float nb = (st[i] > 0) ? float(c[i] + 1) : float(c[i]);\n"
"      tmax[i] = (abs(rd[i]) < 1e-9) ? 1e30 : (nb - ro[i]) / rd[i];\n"
"    }\n"
"    int ax = bf / 2;\n"
"    for (int it = 0; it < 320; it++) {\n"
"      if (c.x < 0 || c.y < 0 || c.z < 0 ||\n"
"          c.x >= int(u_wext.x) || c.y >= int(u_wext.y) || c.z >= int(u_wext.z)) break;\n"
"      uint g = texelFetch(u_grid, ivec3(c.x, c.z, c.y), 0).r;\n"   /* (col,row,lvl) */
"      vec4 L = texelFetch(u_leg, ivec2(int(g), 0), 0);\n"
"      int lay = int(L.a * 255.0 + 0.5) - 1;\n"                     /* -1 = air */
"      if (lay >= 0) {\n"
"        /* real per-cell slab test for the true hit face + t (the DDA\n"
"         * step axis is wrong for the first cell - it's the board-bbox\n"
"         * entry face, not this voxel's). Matches the CPU DDA. */\n"
"        float ct; int cf;\n"
"        if (slab(ro, rd, vec3(c), vec3(c) + 1.0, ct, cf) && ct < bestT) {\n"
"          bestT = ct; hit = true; self_lit = false; face = cf;\n"
"          vec3 wp = ro + rd * ct;\n"
"          vec3 fr = wp - vec3(c);\n"
"          vec2 uv;\n"
"          if (cf == 2 || cf == 3)      uv = vec2(fr.x, fr.z);\n"
"          else if (cf == 4 || cf == 5) uv = vec2(fr.x, 1.0 - fr.y);\n"
"          else                         uv = vec2(fr.z, 1.0 - fr.y);\n"
"          vec4 bb = u_lbbox[lay];\n"
"          if (bb.z <= bb.x) bb = vec4(0.0, 0.0, 1.0, 1.0);\n"
"          vec2 tuv = bb.xy + clamp(uv, 0.0, 0.999) * (bb.zw - bb.xy);\n"
"          ivec2 ti = clamp(ivec2(tuv * 16.0), ivec2(0), ivec2(15));\n"
"          vec4 tx = texelFetch(u_terr, ivec3(ti, lay), 0);\n"
"          col = (tx.a > 0.04) ? tx.rgb : L.rgb;\n"
"        }\n"
"        break;\n"
"      }\n"
"      if (tmax.x < tmax.y) { if (tmax.x < tmax.z) ax = 0; else ax = 2; }\n"
"      else               { if (tmax.y < tmax.z) ax = 1; else ax = 2; }\n"
"      tcur = tmax[ax];\n"
"      c[ax]   += st[ax];\n"
"      tmax[ax] += tdelta[ax];\n"
"    }\n"
"  }\n"
"\n"
"  if (hit && !self_lit) {\n"
"    if (face != 3) col *= 0.75;\n"   /* top = face 3 (Y slab, swapped case) - matches bv_render_3d CPU */
"    col *= u_light;\n"
"  }\n"
"  o_col = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
"}\n";

/* ---- resident state (persistent mode) ---- */
static int        s_persist = 0;
static EGLDisplay s_dpy = EGL_NO_DISPLAY;
static EGLContext s_ctx = EGL_NO_CONTEXT;
static GLuint     s_prog = 0, s_vao = 0, s_fbo = 0, s_rbo = 0, s_tex_grid = 0, s_tex_leg = 0, s_tex_terr = 0;
static int        s_fw = 0, s_fh = 0;          /* current FBO size */
static int        s_gw = 0, s_gh = 0, s_gd = 0; /* current grid-tex dims */
static int        s_leg_alloc = 0;            /* legend tex storage created */
static int        s_terr_alloc = 0;           /* terrain-array storage created */
/* cached uniform locations (glGetUniformLocation is a string lookup) */
static struct {
    GLint eye, fwd, right, up, focal, res, wext, grid, leg, terr, lbbox, light, sky, nbox, bmin, bmax, bcol;
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
    GLuint fs = compile(GL_FRAGMENT_SHADER, FS_SRC);
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

    s_dpy = dpy; s_ctx = ctx; s_prog = prog; s_vao = vao; s_tex_leg = leg; s_tex_terr = terr;
    s_leg_alloc = 0; s_terr_alloc = 0;

    #define UL(n) glGetUniformLocation(prog, n)
    s_u.eye=UL("u_eye"); s_u.fwd=UL("u_fwd"); s_u.right=UL("u_right"); s_u.up=UL("u_up");
    s_u.focal=UL("u_focal"); s_u.res=UL("u_res"); s_u.wext=UL("u_wext");
    s_u.grid=UL("u_grid"); s_u.leg=UL("u_leg"); s_u.terr=UL("u_terr"); s_u.lbbox=UL("u_lbbox");
    s_u.light=UL("u_light"); s_u.sky=UL("u_sky");
    s_u.nbox=UL("u_nbox"); s_u.bmin=UL("u_bmin"); s_u.bmax=UL("u_bmax"); s_u.bcol=UL("u_bcol");
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
    if (s_rbo) glDeleteRenderbuffers(1, &s_rbo);
    if (s_fbo) glDeleteFramebuffers(1, &s_fbo);
    eglMakeCurrent(s_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s_ctx != EGL_NO_CONTEXT) eglDestroyContext(s_dpy, s_ctx);
    eglTerminate(s_dpy);
    s_dpy = EGL_NO_DISPLAY; s_ctx = EGL_NO_CONTEXT;
    s_prog = s_vao = s_fbo = s_rbo = s_tex_grid = s_tex_leg = s_tex_terr = 0;
    s_fw = s_fh = s_gw = s_gh = s_gd = 0;
    s_leg_alloc = 0;
    s_terr_alloc = 0;
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
    glUniform1f (s_u.light, s->light_level);
    glUniform3fv(s_u.sky,   1, s->sky);
    {
        int nb = s->box_n; if (nb > BV_GPU_MAX_BOX) nb = BV_GPU_MAX_BOX; if (nb > 128) nb = 128;
        glUniform1i(s_u.nbox, nb);
        if (nb > 0) {
            float bmin[128*3], bmax[128*3], bcol[128*4];
            for (int i = 0; i < nb; i++) {
                bmin[i*3+0]=s->box[i].min_x; bmin[i*3+1]=s->box[i].min_y; bmin[i*3+2]=s->box[i].min_z;
                bmax[i*3+0]=s->box[i].max_x; bmax[i*3+1]=s->box[i].max_y; bmax[i*3+2]=s->box[i].max_z;
                bcol[i*4+0]=s->box[i].r; bcol[i*4+1]=s->box[i].g; bcol[i*4+2]=s->box[i].b;
                bcol[i*4+3]=s->box[i].self_lit ? 0.0f : 1.0f;
            }
            glUniform3fv(s_u.bmin, nb, bmin);
            glUniform3fv(s_u.bmax, nb, bmax);
            glUniform4fv(s_u.bcol, nb, bcol);
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
        /* read the reduced frame, then nearest-upscale into the full out */
        static unsigned char scratch[1280 * 960 * 4];
        if ((size_t)rw * rh * 4 > sizeof(scratch)) { fprintf(stderr, "bv_gpu: LOD scratch too small\n"); goto done; }
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
