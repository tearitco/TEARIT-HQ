/* nb_worker_webgl.c - canvas WebGL (GLES2/EGL) implementation (mechanical extract, single-TU include) */
/* ---- WebGL (GLES2 via EGL pbuffer, headless) ----
 * REAL, NEW 2026-10-07 (maximal scope: canvas/webgl fast): EGL probe
 * rendered+readback with no X server. getContext('webgl') returns a
 * working context: real shader compile/link, buffers, draws execute,
 * and every drawing call readbacks into the shared NbCanvas px so the
 * existing PNG publish + sprite tile path displays it with zero new
 * plumbing. Slice 1: no textures, no stencil/queries; one EGL
 * surface+context per canvas. GL objects are raw GLuints as JS numbers.
 * Builds without EGL headers too (NB_HAVE_GLES off -> undefined). */
#ifdef NB_HAVE_GLES
#include <EGL/egl.h>
#include <GLES2/gl2.h>
typedef struct { int used; EGLSurface surf; EGLContext ctx; int w, h; } NbGL;
static NbGL g_nbgl[NB_CANVAS_MAX];
static EGLDisplay g_egl_dpy = EGL_NO_DISPLAY;
static EGLConfig g_egl_cfg = NULL;
static int nb_egl_ready(void) {
    if (g_egl_dpy != EGL_NO_DISPLAY) return 1;
    EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (d == EGL_NO_DISPLAY || !eglInitialize(d, NULL, NULL)) return 0;
    EGLint ca[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                    EGL_DEPTH_SIZE, 16, EGL_NONE };
    EGLConfig cfg = NULL; EGLint n = 0;
    if (!eglChooseConfig(d, ca, &cfg, 1, &n) || !n) return 0;
    g_egl_dpy = d; g_egl_cfg = cfg;
    return 1;
}
static void nb_gl_end_all(void) {
    if (g_egl_dpy == EGL_NO_DISPLAY) return;
    for (int i = 0; i < NB_CANVAS_MAX; i++) {
        if (!g_nbgl[i].used) continue;
        eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_nbgl[i].ctx != EGL_NO_CONTEXT) eglDestroyContext(g_egl_dpy, g_nbgl[i].ctx);
        if (g_nbgl[i].surf != EGL_NO_SURFACE) eglDestroySurface(g_egl_dpy, g_nbgl[i].surf);
        g_nbgl[i].used = 0;
    }
}
static NbGL *nb_gl_for(JSContext *ctx, JSValueConst o) {
    JSValue v = JS_GetPropertyStr(ctx, o, "__nb_glslot");
    int slot = -1;
    if (JS_IsNumber(v)) JS_ToInt32(ctx, &slot, v);
    JS_FreeValue(ctx, v);
    if (slot < 0 || slot >= NB_CANVAS_MAX || !g_nbgl[slot].used) return NULL;
    if (eglMakeCurrent(g_egl_dpy, g_nbgl[slot].surf, g_nbgl[slot].surf, g_nbgl[slot].ctx) != EGL_TRUE)
        return NULL;
    return &g_nbgl[slot];
}
/* readback into the shared canvas px for display publish (GL flip) */
static void nb_gl_present(JSContext *ctx, JSValueConst o) {
    JSValue v = JS_GetPropertyStr(ctx, o, "__nb_canvas_id");
    int id = -1;
    if (JS_IsNumber(v)) JS_ToInt32(ctx, &id, v);
    JS_FreeValue(ctx, v);
    if (id < 0 || id >= NB_CANVAS_MAX || !g_canvases[id].used || !g_canvases[id].px) return;
    NbCanvas *cv = &g_canvases[id];
    glFinish();
    glReadPixels(0, 0, cv->w, cv->h, GL_RGBA, GL_UNSIGNED_BYTE, cv->px);
    size_t stride = (size_t)cv->w * 4;
    unsigned char *tmp = malloc(stride);
    if (!tmp) return;
    for (int y = 0; y < cv->h / 2; y++) {
        unsigned char *a = cv->px + (size_t)y * stride;
        unsigned char *b = cv->px + (size_t)(cv->h - 1 - y) * stride;
        memcpy(tmp, a, stride); memcpy(a, b, stride); memcpy(b, tmp, stride);
    }
    free(tmp);
    cv->dirty = 1;
}
static int nb_gl_argi(JSContext *ctx, JSValueConst *argv, int argc, int i, int def) {
    int32_t v = def;
    if (i < argc && JS_IsNumber(argv[i])) JS_ToInt32(ctx, &v, argv[i]);
    return (int)v;
}
static double nb_gl_argf(JSContext *ctx, JSValueConst *argv, int argc, int i, double def) {
    double d = def;
    if (i < argc && JS_IsNumber(argv[i])) JS_ToFloat64(ctx, &d, argv[i]);
    return d;
}
static const char *nb_gl_argstr(JSContext *ctx, JSValueConst *argv, int argc, int i, char **owned) {
    *owned = NULL;
    if (i >= argc || !JS_IsString(argv[i])) return NULL;
    const char *s = JS_ToCString(ctx, argv[i]);
    *owned = (char *)s;
    return s;
}
static float *nb_gl_argfloats(JSContext *ctx, JSValueConst v, int *out_n) {
    *out_n = 0;
    if (!JS_IsObject(v)) return NULL;
    JSValue lv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = 0;
    if (JS_IsNumber(lv)) JS_ToInt32(ctx, &len, lv);
    JS_FreeValue(ctx, lv);
    if (len <= 0 || len > 16 * 1024 * 1024) return NULL;
    float *buf = malloc(sizeof(float) * (size_t)len);
    if (!buf) return NULL;
    for (int32_t i = 0; i < len; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
        double d = 0;
        if (JS_IsNumber(e)) JS_ToFloat64(ctx, &d, e);
        JS_FreeValue(ctx, e);
        buf[i] = (float)d;
    }
    *out_n = len;
    return buf;
}
static JSValue nb_gl_clearColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glClearColor((GLfloat)nb_gl_argf(ctx, argv, argc, 0, 0), (GLfloat)nb_gl_argf(ctx, argv, argc, 1, 0),
                 (GLfloat)nb_gl_argf(ctx, argv, argc, 2, 0), (GLfloat)nb_gl_argf(ctx, argv, argc, 3, 0));
    nb_gl_present(ctx, this_val);
    return JS_UNDEFINED;
}
static JSValue nb_gl_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glClear((GLbitfield)nb_gl_argi(ctx, argv, argc, 0, 0));
    nb_gl_present(ctx, this_val);
    return JS_UNDEFINED;
}
static JSValue nb_gl_viewport(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glViewport(nb_gl_argi(ctx, argv, argc, 0, 0), nb_gl_argi(ctx, argv, argc, 1, 0),
               nb_gl_argi(ctx, argv, argc, 2, g->w), nb_gl_argi(ctx, argv, argc, 3, g->h));
    return JS_UNDEFINED;
}
static JSValue nb_gl_createShader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    return JS_NewInt32(ctx, (int32_t)glCreateShader((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0)));
}
static JSValue nb_gl_shaderSource(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g || argc < 2) return JS_UNDEFINED;
    char *owned = NULL;
    const char *s = nb_gl_argstr(ctx, argv, argc, 1, &owned);
    if (s) { glShaderSource((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0), 1, &s, NULL); JS_FreeCString(ctx, owned); }
    return JS_UNDEFINED;
}
static JSValue nb_gl_compileShader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glCompileShader((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_shaderParam(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int is_prog) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    GLint v = 0;
    GLuint o = (GLuint)nb_gl_argi(ctx, argv, argc, 0, 0);
    GLenum p = (GLenum)nb_gl_argi(ctx, argv, argc, 1, 0);
    if (is_prog) glGetProgramiv(o, p, &v); else glGetShaderiv(o, p, &v);
    return JS_NewInt32(ctx, v);
}
static JSValue nb_gl_getShaderParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_shaderParam(ctx, this_val, argc, argv, 0);
}
static JSValue nb_gl_getProgramParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_shaderParam(ctx, this_val, argc, argv, 1);
}
static JSValue nb_gl_infoLog(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int is_prog) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_NewString(ctx, "");
    char buf[4096]; GLsizei n = 0;
    if (is_prog) glGetProgramInfoLog((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0), (GLsizei)sizeof(buf), &n, buf);
    else glGetShaderInfoLog((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0), (GLsizei)sizeof(buf), &n, buf);
    if (n < 0) n = 0; if ((size_t)n >= sizeof(buf)) n = (GLsizei)sizeof(buf) - 1;
    buf[n] = 0;
    return JS_NewString(ctx, buf);
}
static JSValue nb_gl_getShaderInfoLog(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_infoLog(ctx, this_val, argc, argv, 0);
}
static JSValue nb_gl_getProgramInfoLog(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_infoLog(ctx, this_val, argc, argv, 1);
}
static JSValue nb_gl_createProgram(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    return JS_NewInt32(ctx, (int32_t)glCreateProgram());
}
static JSValue nb_gl_attachShader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glAttachShader((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0), (GLuint)nb_gl_argi(ctx, argv, argc, 1, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_linkProgram(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glLinkProgram((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_useProgram(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glUseProgram((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_deleteShader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glDeleteShader((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_deleteProgram(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glDeleteProgram((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_createBuffer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    GLuint b = 0;
    glGenBuffers(1, &b);
    return JS_NewInt32(ctx, (int32_t)b);
}
static JSValue nb_gl_bindBuffer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glBindBuffer((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x8892), (GLuint)nb_gl_argi(ctx, argv, argc, 1, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_bufferData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g || argc < 3) return JS_UNDEFINED;
    GLenum tgt = (GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x8892);
    GLenum usage = (GLenum)nb_gl_argi(ctx, argv, argc, 2, 0x88E4);
    if (JS_IsNumber(argv[1])) {
        glBufferData(tgt, (GLsizeiptr)nb_gl_argi(ctx, argv, argc, 1, 0), NULL, usage);
        return JS_UNDEFINED;
    }
    int fln = 0;
    float *fl = NULL;
    {
        JSValue ctor = JS_GetPropertyStr(ctx, argv[1], "constructor");
        JSValue nm = JS_IsObject(ctor) ? JS_GetPropertyStr(ctx, ctor, "name") : JS_UNDEFINED;
        const char *cn = JS_IsString(nm) ? JS_ToCString(ctx, nm) : NULL;
        int is_float = cn && (!strcmp(cn, "Float32Array") || !strcmp(cn, "Float64Array"));
        if (cn) JS_FreeCString(ctx, cn);
        JS_FreeValue(ctx, nm); JS_FreeValue(ctx, ctor);
        if (is_float) fl = nb_gl_argfloats(ctx, argv[1], &fln);
    }
    if (fl) {
        glBufferData(tgt, (GLsizeiptr)(fln * sizeof(float)), fl, usage);
        free(fl);
    } else {
        size_t n = 0;
        JSValue lv = JS_GetPropertyStr(ctx, argv[1], "length");
        int32_t len = 0;
        if (JS_IsNumber(lv)) JS_ToInt32(ctx, &len, lv);
        JS_FreeValue(ctx, lv);
        if (len > 0 && len <= 64 * 1024 * 1024) {
            unsigned char *bytes = malloc((size_t)len);
            if (bytes) {
                for (int32_t i = 0; i < len; i++) {
                    JSValue e = JS_GetPropertyUint32(ctx, argv[1], (uint32_t)i);
                    int32_t ev = 0;
                    if (JS_IsNumber(e)) JS_ToInt32(ctx, &ev, e);
                    JS_FreeValue(ctx, e);
                    bytes[i] = (unsigned char)(ev & 0xFF);
                }
                glBufferData(tgt, (GLsizeiptr)len, bytes, usage);
                free(bytes);
            }
        }
    }
    return JS_UNDEFINED;
}
static JSValue nb_gl_deleteBuffer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    GLuint b = (GLuint)nb_gl_argi(ctx, argv, argc, 0, 0);
    glDeleteBuffers(1, &b);
    return JS_UNDEFINED;
}
/* ---- textures (slice 2): create/bind/upload/params/mipmap/delete ----
 * Upload sources: TypedArray pixels, ImageData-like {width,height,data},
 * canvas elements (backing store). HTMLImageElement needs async decode
 * wiring - out of slice. UNPACK_* pixel-store modes ignored (tight packing). */
static JSValue nb_gl_createTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    GLuint t = 0;
    glGenTextures(1, &t);
    return JS_NewInt32(ctx, (int32_t)t);
}
static JSValue nb_gl_deleteTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    GLuint t = (GLuint)nb_gl_argi(ctx, argv, argc, 0, 0);
    glDeleteTextures(1, &t);
    return JS_UNDEFINED;
}
static JSValue nb_gl_bindTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glBindTexture((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0DE1), (GLuint)nb_gl_argi(ctx, argv, argc, 1, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_activeTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glActiveTexture((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x84C0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_texParameteri(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glTexParameteri((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0DE1),
                    (GLenum)nb_gl_argi(ctx, argv, argc, 1, 0),
                    nb_gl_argi(ctx, argv, argc, 2, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_generateMipmap(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glGenerateMipmap((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0DE1));
    return JS_UNDEFINED;
}
static JSValue nb_gl_texImage2D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g || argc < 6) return JS_UNDEFINED;
    GLenum tgt = (GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0DE1);
    GLint level = nb_gl_argi(ctx, argv, argc, 1, 0);
    GLint internal = nb_gl_argi(ctx, argv, argc, 2, 0x1908);
    /* 9-arg numeric form: (t,l,i,w,h,border,format,type,pixels?) */
    if (argc >= 9 && (JS_IsNumber(argv[3]) || JS_IsNull(argv[8]) || JS_IsObject(argv[8]))) {
        GLsizei w = nb_gl_argi(ctx, argv, argc, 3, 0), h = nb_gl_argi(ctx, argv, argc, 4, 0);
        GLenum fmt = (GLenum)nb_gl_argi(ctx, argv, argc, 6, 0x1908);
        GLenum typ = (GLenum)nb_gl_argi(ctx, argv, argc, 7, 0x1401);
        if (w <= 0 || h <= 0) return JS_UNDEFINED;
        if (argc < 9 || JS_IsNull(argv[8]) || JS_IsUndefined(argv[8])) {
            glTexImage2D(tgt, level, internal, w, h, 0, fmt, typ, NULL);
            return JS_UNDEFINED;
        }
        size_t n = 0;
        unsigned char *bytes = NULL;
        int fln = 0;
        float *fl = NULL;
        {
            JSValue ctor = JS_GetPropertyStr(ctx, argv[8], "constructor");
            JSValue nm = JS_IsObject(ctor) ? JS_GetPropertyStr(ctx, ctor, "name") : JS_UNDEFINED;
            const char *cn = JS_IsString(nm) ? JS_ToCString(ctx, nm) : NULL;
            int is_float = cn && (!strcmp(cn, "Float32Array") || !strcmp(cn, "Float64Array"));
            if (cn) JS_FreeCString(ctx, cn);
            JS_FreeValue(ctx, nm); JS_FreeValue(ctx, ctor);
            if (is_float) fl = nb_gl_argfloats(ctx, argv[8], &fln);
            else {
                JSValue lv = JS_GetPropertyStr(ctx, argv[8], "length");
                int32_t len = 0;
                if (JS_IsNumber(lv)) JS_ToInt32(ctx, &len, lv);
                JS_FreeValue(ctx, lv);
                if (len > 0 && len <= 64 * 1024 * 1024) {
                    bytes = malloc((size_t)len);
                    if (bytes) {
                        for (int32_t i = 0; i < len; i++) {
                            JSValue e = JS_GetPropertyUint32(ctx, argv[8], (uint32_t)i);
                            int32_t ev = 0;
                            if (JS_IsNumber(e)) JS_ToInt32(ctx, &ev, e);
                            JS_FreeValue(ctx, e);
                            bytes[i] = (unsigned char)(ev & 0xFF);
                        }
                        n = (size_t)len;
                    }
                }
            }
        }
        if (fl) { glTexImage2D(tgt, level, internal, w, h, 0, fmt, typ, fl); free(fl); }
        else if (bytes) { glTexImage2D(tgt, level, internal, w, h, 0, fmt, typ, bytes); free(bytes); }
        return JS_UNDEFINED;
    }
    /* 6-arg DOM form: (t,l,internal,format,type,src) */
    GLenum fmt = (GLenum)nb_gl_argi(ctx, argv, argc, 3, 0x1908);
    GLenum typ = (GLenum)nb_gl_argi(ctx, argv, argc, 4, 0x1401);
    int sw = 0, sh = 0, owned = 0;
    unsigned char *spx = NULL;
    if (!nb_c2d_source(ctx, argv[5], &sw, &sh, &spx, &owned) || sw <= 0 || sh <= 0) {
        if (owned) free(spx);
        return JS_UNDEFINED;
    }
    glTexImage2D(tgt, level, internal, sw, sh, 0, fmt, typ, spx);
    if (owned) free(spx);
    return JS_UNDEFINED;
}
static JSValue nb_gl_texSubImage2D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g || argc < 7) return JS_UNDEFINED;
    GLenum tgt = (GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0DE1);
    GLint level = nb_gl_argi(ctx, argv, argc, 1, 0);
    GLint xoff = nb_gl_argi(ctx, argv, argc, 2, 0), yoff = nb_gl_argi(ctx, argv, argc, 3, 0);
    GLsizei w = nb_gl_argi(ctx, argv, argc, 4, 0), h = nb_gl_argi(ctx, argv, argc, 5, 0);
    GLenum fmt = (GLenum)nb_gl_argi(ctx, argv, argc, 6, 0x1908);
    GLenum typ = (GLenum)nb_gl_argi(ctx, argv, argc, 7, 0x1401);
    if (w <= 0 || h <= 0 || argc < 9) return JS_UNDEFINED;
    int fln = 0;
    float *fl = nb_gl_argfloats(ctx, argv[8], &fln);
    if (fl) {
        glTexSubImage2D(tgt, level, xoff, yoff, w, h, fmt, typ, fl);
        free(fl);
        return JS_UNDEFINED;
    }
    JSValue lv = JS_GetPropertyStr(ctx, argv[8], "length");
    int32_t len = 0;
    if (JS_IsNumber(lv)) JS_ToInt32(ctx, &len, lv);
    JS_FreeValue(ctx, lv);
    if (len <= 0 || len > 64 * 1024 * 1024) return JS_UNDEFINED;
    unsigned char *bytes = malloc((size_t)len);
    if (!bytes) return JS_UNDEFINED;
    for (int32_t i = 0; i < len; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, argv[8], (uint32_t)i);
        int32_t ev = 0;
        if (JS_IsNumber(e)) JS_ToInt32(ctx, &ev, e);
        JS_FreeValue(ctx, e);
        bytes[i] = (unsigned char)(ev & 0xFF);
    }
    glTexSubImage2D(tgt, level, xoff, yoff, w, h, fmt, typ, bytes);
    free(bytes);
    return JS_UNDEFINED;
}
static JSValue nb_gl_pixelStorei(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    /* UNPACK modes assumed tight; FLIP_Y recorded but applied at upload
     * only for canvas sources is out of slice - accept and ignore. */
    glPixelStorei((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0), nb_gl_argi(ctx, argv, argc, 1, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_vertexAttribPointer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glVertexAttribPointer((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0),
                          nb_gl_argi(ctx, argv, argc, 1, 4),
                          (GLenum)nb_gl_argi(ctx, argv, argc, 2, 0x1406),
                          nb_gl_argi(ctx, argv, argc, 3, 0),
                          nb_gl_argi(ctx, argv, argc, 4, 0),
                          (const void *)(intptr_t)nb_gl_argi(ctx, argv, argc, 5, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_enableVertexAttribArray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glEnableVertexAttribArray((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_disableVertexAttribArray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glDisableVertexAttribArray((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_vertexAttrib1f(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glVertexAttrib1f((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0), (GLfloat)nb_gl_argf(ctx, argv, argc, 1, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_getAttribLocation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g || argc < 2) return JS_NewInt32(ctx, -1);
    char *owned = NULL;
    const char *s = nb_gl_argstr(ctx, argv, argc, 1, &owned);
    GLint l = s ? glGetAttribLocation((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0), s) : -1;
    if (owned) JS_FreeCString(ctx, owned);
    return JS_NewInt32(ctx, l);
}
static JSValue nb_gl_getUniformLocation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g || argc < 2) return JS_NULL;
    char *owned = NULL;
    const char *s = nb_gl_argstr(ctx, argv, argc, 1, &owned);
    GLint l = s ? glGetUniformLocation((GLuint)nb_gl_argi(ctx, argv, argc, 0, 0), s) : -1;
    if (owned) JS_FreeCString(ctx, owned);
    if (l < 0) return JS_NULL;
    return JS_NewInt32(ctx, l);
}
static JSValue nb_gl_uniformf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int n) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    GLint l = nb_gl_argi(ctx, argv, argc, 0, -1);
    float v[4] = {0, 0, 0, 0};
    for (int i = 0; i < n && i < 4; i++) v[i] = (float)nb_gl_argf(ctx, argv, argc, 1 + i, 0);
    if (n == 1) glUniform1f(l, v[0]);
    else if (n == 2) glUniform2f(l, v[0], v[1]);
    else if (n == 3) glUniform3f(l, v[0], v[1], v[2]);
    else glUniform4f(l, v[0], v[1], v[2], v[3]);
    return JS_UNDEFINED;
}
static JSValue nb_gl_uniform1f(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_uniformf(ctx, this_val, argc, argv, 1);
}
static JSValue nb_gl_uniform2f(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_uniformf(ctx, this_val, argc, argv, 2);
}
static JSValue nb_gl_uniform3f(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_uniformf(ctx, this_val, argc, argv, 3);
}
static JSValue nb_gl_uniform4f(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_uniformf(ctx, this_val, argc, argv, 4);
}
static JSValue nb_gl_uniform1i(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glUniform1i(nb_gl_argi(ctx, argv, argc, 0, -1), nb_gl_argi(ctx, argv, argc, 1, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_uniformMatrix(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int n) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g || argc < 3) return JS_UNDEFINED;
    int fln = 0;
    float *fl = nb_gl_argfloats(ctx, argv[2], &fln);
    if (fl && fln >= n * n) {
        if (n == 4) glUniformMatrix4fv(nb_gl_argi(ctx, argv, argc, 0, -1), 1, 0, fl);
        else if (n == 3) glUniformMatrix3fv(nb_gl_argi(ctx, argv, argc, 0, -1), 1, 0, fl);
        else glUniformMatrix2fv(nb_gl_argi(ctx, argv, argc, 0, -1), 1, 0, fl);
    }
    free(fl);
    return JS_UNDEFINED;
}
static JSValue nb_gl_uniformMatrix4fv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_uniformMatrix(ctx, this_val, argc, argv, 4);
}
static JSValue nb_gl_uniformMatrix3fv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_gl_uniformMatrix(ctx, this_val, argc, argv, 3);
}
static JSValue nb_gl_drawArrays(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glDrawArrays((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0004),
                 nb_gl_argi(ctx, argv, argc, 1, 0), nb_gl_argi(ctx, argv, argc, 2, 0));
    nb_gl_present(ctx, this_val);
    return JS_UNDEFINED;
}
static JSValue nb_gl_drawElements(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glDrawElements((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0004),
                   nb_gl_argi(ctx, argv, argc, 1, 0),
                   (GLenum)nb_gl_argi(ctx, argv, argc, 2, 0x1403),
                   (const void *)(intptr_t)nb_gl_argi(ctx, argv, argc, 3, 0));
    nb_gl_present(ctx, this_val);
    return JS_UNDEFINED;
}
static JSValue nb_gl_getError(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_NewInt32(ctx, 0);
    return JS_NewInt32(ctx, (int32_t)glGetError());
}

static JSValue nb_gl_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

/* getExtension: WEBGL_debug_renderer_info for fingerprinting (UNMASKED_VENDOR_WEBGL, UNMASKED_RENDERER_WEBGL) */
static JSValue nb_gl_getExtension(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    if (argc < 1 || !JS_IsString(argv[0])) return JS_NULL;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_NULL;
    JSValue ext = JS_UNDEFINED;
    if (strcmp(name, "WEBGL_debug_renderer_info") == 0) {
        JSValue ext_obj = JS_NewObject(ctx);
        const char *vendor = (const char *)glGetString(GL_VENDOR);
        const char *renderer = (const char *)glGetString(GL_RENDERER);
        if (vendor) JS_SetPropertyStr(ctx, ext_obj, "UNMASKED_VENDOR_WEBGL", JS_NewString(ctx, vendor));
        if (renderer) JS_SetPropertyStr(ctx, ext_obj, "UNMASKED_RENDERER_WEBGL", JS_NewString(ctx, renderer));
        ext = ext_obj;
    } else if (strcmp(name, "OES_vertex_array_object") == 0) {
        ext = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ext, "createVertexArrayOES", JS_NewCFunction(ctx, nb_gl_noop, "createVertexArrayOES", 0));
        JS_SetPropertyStr(ctx, ext, "deleteVertexArrayOES", JS_NewCFunction(ctx, nb_gl_noop, "deleteVertexArrayOES", 1));
        JS_SetPropertyStr(ctx, ext, "bindVertexArrayOES", JS_NewCFunction(ctx, nb_gl_noop, "bindVertexArrayOES", 1));
        JS_SetPropertyStr(ctx, ext, "isVertexArrayOES", JS_NewCFunction(ctx, nb_gl_noop, "isVertexArrayOES", 1));
    } else if (strcmp(name, "ANGLE_instanced_arrays") == 0) {
        ext = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ext, "drawArraysInstancedANGLE", JS_NewCFunction(ctx, nb_gl_noop, "drawArraysInstancedANGLE", 4));
        JS_SetPropertyStr(ctx, ext, "drawElementsInstancedANGLE", JS_NewCFunction(ctx, nb_gl_noop, "drawElementsInstancedANGLE", 5));
        JS_SetPropertyStr(ctx, ext, "vertexAttribDivisorANGLE", JS_NewCFunction(ctx, nb_gl_noop, "vertexAttribDivisorANGLE", 2));
    } else if (strcmp(name, "EXT_texture_filter_anisotropic") == 0) {
        ext = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ext, "TEXTURE_MAX_ANISOTROPY_EXT", JS_NewInt32(ctx, 0x84FE));
        JS_SetPropertyStr(ctx, ext, "MAX_TEXTURE_MAX_ANISOTROPY_EXT", JS_NewInt32(ctx, 0x84FF));
    } else if (strcmp(name, "WEBGL_depth_texture") == 0 ||
               strcmp(name, "OES_texture_float") == 0 ||
               strcmp(name, "OES_texture_half_float") == 0 ||
               strcmp(name, "OES_standard_derivatives") == 0 ||
               strcmp(name, "WEBGL_draw_buffers") == 0 ||
               strcmp(name, "EXT_frag_depth") == 0 ||
               strcmp(name, "EXT_shader_texture_lod") == 0 ||
               strcmp(name, "EXT_sRGB") == 0 ||
               strcmp(name, "OES_element_index_uint") == 0) {
        /* Return empty object for these extensions - they're just markers */
        ext = JS_NewObject(ctx);
    }
    JS_FreeCString(ctx, name);
    return ext;
}

static JSValue nb_gl_getSupportedExtensions(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    JSValue arr = JS_NewArray(ctx);
    const char *exts[] = {
        "WEBGL_debug_renderer_info",
        "OES_texture_float",
        "OES_texture_half_float",
        "OES_standard_derivatives",
        "OES_vertex_array_object",
        "WEBGL_depth_texture",
        "WEBGL_draw_buffers",
        "EXT_texture_filter_anisotropic",
        "EXT_frag_depth",
        "EXT_shader_texture_lod",
        "EXT_sRGB",
        "ANGLE_instanced_arrays",
        "OES_element_index_uint",
        NULL
    };
    int i = 0;
    while (exts[i]) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, JS_NewString(ctx, exts[i]));
        i++;
    }
    return arr;
}

static JSValue nb_gl_getParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    GLenum p = (GLenum)nb_gl_argi(ctx, argv, argc, 0, 0);
    if (p == 0x1F00 || p == 0x1F01 || p == 0x1F02 || p == 0x8B8C) {
        const char *s = (const char *)glGetString(p);
        return JS_NewString(ctx, s ? s : "");
    }
    if (p == 0x0D33 || p == 0x0BA2) {
        GLint v[4] = {0, 0, 0, 0};
        glGetIntegerv(p, v);
        JSValue a = JS_NewArray(ctx);
        for (int i = 0; i < 4; i++) {
            JSValue e = JS_NewInt32(ctx, v[i]);
            JS_SetPropertyUint32(ctx, a, (uint32_t)i, e);
        }
        return a;
    }
    GLint v = 0;
    glGetIntegerv(p, &v);
    return JS_NewInt32(ctx, v);
}
static JSValue nb_gl_enable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glEnable((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_disable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glDisable((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0));
    return JS_UNDEFINED;
}
static JSValue nb_gl_blendFunc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glBlendFunc((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0302), (GLenum)nb_gl_argi(ctx, argv, argc, 1, 0x0303));
    return JS_UNDEFINED;
}
static JSValue nb_gl_depthFunc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glDepthFunc((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0203));
    return JS_UNDEFINED;
}
static JSValue nb_gl_depthMask(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glDepthMask(nb_gl_argi(ctx, argv, argc, 0, 1) ? GL_TRUE : GL_FALSE);
    return JS_UNDEFINED;
}
static JSValue nb_gl_colorMask(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glColorMask(nb_gl_argi(ctx, argv, argc, 0, 1), nb_gl_argi(ctx, argv, argc, 1, 1),
                nb_gl_argi(ctx, argv, argc, 2, 1), nb_gl_argi(ctx, argv, argc, 3, 1));
    return JS_UNDEFINED;
}
static JSValue nb_gl_readPixels(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g || argc < 7) return JS_UNDEFINED;
    GLint x = nb_gl_argi(ctx, argv, argc, 0, 0), y = nb_gl_argi(ctx, argv, argc, 1, 0);
    GLsizei w = nb_gl_argi(ctx, argv, argc, 2, 0), h = nb_gl_argi(ctx, argv, argc, 3, 0);
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048) return JS_UNDEFINED;
    size_t n = (size_t)w * h * 4;
    unsigned char *buf = malloc(n);
    if (!buf) return JS_UNDEFINED;
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    if (JS_IsObject(argv[6])) {
        for (size_t i = 0; i < n; i++) {
            JSValue e = JS_NewInt32(ctx, buf[i]);
            JS_SetPropertyUint32(ctx, argv[6], (uint32_t)i, e);
        }
    }
    free(buf);
    return JS_UNDEFINED;
}
static JSValue nb_gl_cullFace(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glCullFace((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0405));
    return JS_UNDEFINED;
}
static JSValue nb_gl_frontFace(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbGL *g = nb_gl_for(ctx, this_val); if (!g) return JS_UNDEFINED;
    glFrontFace((GLenum)nb_gl_argi(ctx, argv, argc, 0, 0x0901));
    return JS_UNDEFINED;
}
/* WebGL enum table (common subset; unlisted names read undefined) */
static void nb_gl_consts(JSContext *ctx, JSValue c) {
    static const struct { const char *n; int v; } k[] = {
        {"DEPTH_BUFFER_BIT",0x00000100},{"STENCIL_BUFFER_BIT",0x00000400},{"COLOR_BUFFER_BIT",0x00004000},
        {"POINTS",0x0000},{"LINES",0x0001},{"LINE_LOOP",0x0002},{"LINE_STRIP",0x0003},
        {"TRIANGLES",0x0004},{"TRIANGLE_STRIP",0x0005},{"TRIANGLE_FAN",0x0006},
        {"ZERO",0},{"ONE",1},{"SRC_COLOR",0x0300},{"ONE_MINUS_SRC_COLOR",0x0301},
        {"SRC_ALPHA",0x0302},{"ONE_MINUS_SRC_ALPHA",0x0303},{"DST_ALPHA",0x0304},{"ONE_MINUS_DST_ALPHA",0x0305},
        {"DST_COLOR",0x0306},{"FUNC_ADD",0x8006},
        {"BLEND",0x0BE2},{"CULL_FACE",0x0B44},{"DEPTH_TEST",0x0B71},{"SCISSOR_TEST",0x0C11},{"DITHER",0x0BD0},
        {"FRONT",0x0404},{"BACK",0x0405},{"FRONT_AND_BACK",0x0408},{"CW",0x0900},{"CCW",0x0901},
        {"NEVER",0x0200},{"LESS",0x0201},{"EQUAL",0x0202},{"LEQUAL",0x0203},{"GREATER",0x0204},
        {"NOTEQUAL",0x0205},{"GEQUAL",0x0206},{"ALWAYS",0x0207},
        {"BYTE",0x1400},{"UNSIGNED_BYTE",0x1401},{"SHORT",0x1402},{"UNSIGNED_SHORT",0x1403},
        {"INT",0x1404},{"UNSIGNED_INT",0x1405},{"FLOAT",0x1406},
        {"ARRAY_BUFFER",0x8892},{"ELEMENT_ARRAY_BUFFER",0x8893},
        {"STATIC_DRAW",0x88E4},{"DYNAMIC_DRAW",0x88E8},{"STREAM_DRAW",0x88E0},
        {"VERTEX_SHADER",0x8B31},{"FRAGMENT_SHADER",0x8B30},
        {"COMPILE_STATUS",0x8B81},{"LINK_STATUS",0x8B82},{"VALIDATE_STATUS",0x8B83},
        {"DELETE_STATUS",0x8B80},{"ATTACHED_SHADERS",0x8B85},{"ACTIVE_ATTRIBUTES",0x8B89},
        {"ACTIVE_UNIFORMS",0x8B86},
        {"RGBA",0x1908},{"RGB",0x1907},{"ALPHA",0x1906},{"LUMINANCE",0x1909},{"LUMINANCE_ALPHA",0x190A},
        {"TEXTURE_2D",0x0DE1},{"TEXTURE0",0x84C0},{"TEXTURE1",0x84C1},{"TEXTURE2",0x84C2},
        {"TEXTURE_MAG_FILTER",0x2800},{"TEXTURE_MIN_FILTER",0x2801},
        {"TEXTURE_WRAP_S",0x2802},{"TEXTURE_WRAP_T",0x2803},
        {"NEAREST",0x2600},{"LINEAR",0x2601},{"REPEAT",0x2901},{"CLAMP_TO_EDGE",0x812F},
        {"MIRRORED_REPEAT",0x8370},
        {"MAX_TEXTURE_SIZE",0x0D33},{"MAX_VIEWPORT_DIMS",0x0D3A},
        {"VERSION",0x1F02},{"VENDOR",0x1F00},{"RENDERER",0x1F01},{"SHADING_LANGUAGE_VERSION",0x8B8C},
        {"VIEWPORT",0x0BA2},
        {"NO_ERROR",0},{"INVALID_ENUM",0x0500},{"INVALID_VALUE",0x0501},{"INVALID_OPERATION",0x0502},
        {"OUT_OF_MEMORY",0x0505},
        {"DEPTH_COMPONENT16",0x81A5},{"UNSIGNED_SHORT_5_6_5",0x8363},
        {"GENERATE_MIPMAP_HINT",0x8192},
        {NULL,0}
    };
    for (int i = 0; k[i].n; i++)
        JS_SetPropertyStr(ctx, c, k[i].n, JS_NewInt32(ctx, k[i].v));
}
/* build a WebGL context object bound to canvas slot cid + fresh EGL state */
static JSValue nb_gl_new_context(JSContext *ctx, JSValueConst elem, int cid, int w, int h) {
    if (!nb_egl_ready()) return JS_UNDEFINED;
    int slot = -1;
    for (int i = 0; i < NB_CANVAS_MAX; i++)
        if (!g_nbgl[i].used) { slot = i; break; }
    if (slot < 0) return JS_UNDEFINED;
    EGLint pb[] = { EGL_WIDTH, w > 0 ? w : 300, EGL_HEIGHT, h > 0 ? h : 150, EGL_NONE };
    EGLSurface surf = eglCreatePbufferSurface(g_egl_dpy, g_egl_cfg, pb);
    if (surf == EGL_NO_SURFACE) return JS_UNDEFINED;
    EGLint ca[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext gc = eglCreateContext(g_egl_dpy, g_egl_cfg, EGL_NO_CONTEXT, ca);
    if (gc == EGL_NO_CONTEXT) { eglDestroySurface(g_egl_dpy, surf); return JS_UNDEFINED; }
    g_nbgl[slot].used = 1;
    g_nbgl[slot].surf = surf; g_nbgl[slot].ctx = gc;
    g_nbgl[slot].w = w > 0 ? w : 300; g_nbgl[slot].h = h > 0 ? h : 150;
    JSValue c = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, c, "__nb_glslot", JS_NewInt32(ctx, slot));
    JS_SetPropertyStr(ctx, c, "__nb_canvas_id", JS_NewInt32(ctx, cid));
    nb_gl_consts(ctx, c);
    JS_SetPropertyStr(ctx, c, "clearColor", JS_NewCFunction(ctx, nb_gl_clearColor, "clearColor", 4));
    JS_SetPropertyStr(ctx, c, "clear", JS_NewCFunction(ctx, nb_gl_clear, "clear", 1));
    JS_SetPropertyStr(ctx, c, "viewport", JS_NewCFunction(ctx, nb_gl_viewport, "viewport", 4));
    JS_SetPropertyStr(ctx, c, "createShader", JS_NewCFunction(ctx, nb_gl_createShader, "createShader", 1));
    JS_SetPropertyStr(ctx, c, "shaderSource", JS_NewCFunction(ctx, nb_gl_shaderSource, "shaderSource", 2));
    JS_SetPropertyStr(ctx, c, "compileShader", JS_NewCFunction(ctx, nb_gl_compileShader, "compileShader", 1));
    JS_SetPropertyStr(ctx, c, "getShaderParameter", JS_NewCFunction(ctx, nb_gl_getShaderParameter, "getShaderParameter", 2));
    JS_SetPropertyStr(ctx, c, "getShaderInfoLog", JS_NewCFunction(ctx, nb_gl_getShaderInfoLog, "getShaderInfoLog", 1));
    JS_SetPropertyStr(ctx, c, "createProgram", JS_NewCFunction(ctx, nb_gl_createProgram, "createProgram", 0));
    JS_SetPropertyStr(ctx, c, "attachShader", JS_NewCFunction(ctx, nb_gl_attachShader, "attachShader", 2));
    JS_SetPropertyStr(ctx, c, "linkProgram", JS_NewCFunction(ctx, nb_gl_linkProgram, "linkProgram", 1));
    JS_SetPropertyStr(ctx, c, "getProgramParameter", JS_NewCFunction(ctx, nb_gl_getProgramParameter, "getProgramParameter", 2));
    JS_SetPropertyStr(ctx, c, "getProgramInfoLog", JS_NewCFunction(ctx, nb_gl_getProgramInfoLog, "getProgramInfoLog", 1));
    JS_SetPropertyStr(ctx, c, "useProgram", JS_NewCFunction(ctx, nb_gl_useProgram, "useProgram", 1));
    JS_SetPropertyStr(ctx, c, "deleteShader", JS_NewCFunction(ctx, nb_gl_deleteShader, "deleteShader", 1));
    JS_SetPropertyStr(ctx, c, "deleteProgram", JS_NewCFunction(ctx, nb_gl_deleteProgram, "deleteProgram", 1));
    JS_SetPropertyStr(ctx, c, "createBuffer", JS_NewCFunction(ctx, nb_gl_createBuffer, "createBuffer", 0));
    JS_SetPropertyStr(ctx, c, "bindBuffer", JS_NewCFunction(ctx, nb_gl_bindBuffer, "bindBuffer", 2));
    JS_SetPropertyStr(ctx, c, "bufferData", JS_NewCFunction(ctx, nb_gl_bufferData, "bufferData", 3));
    JS_SetPropertyStr(ctx, c, "deleteBuffer", JS_NewCFunction(ctx, nb_gl_deleteBuffer, "deleteBuffer", 1));
    JS_SetPropertyStr(ctx, c, "vertexAttribPointer", JS_NewCFunction(ctx, nb_gl_vertexAttribPointer, "vertexAttribPointer", 6));
    JS_SetPropertyStr(ctx, c, "enableVertexAttribArray", JS_NewCFunction(ctx, nb_gl_enableVertexAttribArray, "enableVertexAttribArray", 1));
    JS_SetPropertyStr(ctx, c, "disableVertexAttribArray", JS_NewCFunction(ctx, nb_gl_disableVertexAttribArray, "disableVertexAttribArray", 1));
    JS_SetPropertyStr(ctx, c, "vertexAttrib1f", JS_NewCFunction(ctx, nb_gl_vertexAttrib1f, "vertexAttrib1f", 2));
    JS_SetPropertyStr(ctx, c, "getAttribLocation", JS_NewCFunction(ctx, nb_gl_getAttribLocation, "getAttribLocation", 2));
    JS_SetPropertyStr(ctx, c, "getUniformLocation", JS_NewCFunction(ctx, nb_gl_getUniformLocation, "getUniformLocation", 2));
    JS_SetPropertyStr(ctx, c, "uniform1f", JS_NewCFunction(ctx, nb_gl_uniform1f, "uniform1f", 2));
    JS_SetPropertyStr(ctx, c, "uniform2f", JS_NewCFunction(ctx, nb_gl_uniform2f, "uniform2f", 3));
    JS_SetPropertyStr(ctx, c, "uniform3f", JS_NewCFunction(ctx, nb_gl_uniform3f, "uniform3f", 4));
    JS_SetPropertyStr(ctx, c, "uniform4f", JS_NewCFunction(ctx, nb_gl_uniform4f, "uniform4f", 5));
    JS_SetPropertyStr(ctx, c, "uniform1i", JS_NewCFunction(ctx, nb_gl_uniform1i, "uniform1i", 2));
    JS_SetPropertyStr(ctx, c, "uniformMatrix4fv", JS_NewCFunction(ctx, nb_gl_uniformMatrix4fv, "uniformMatrix4fv", 3));
    JS_SetPropertyStr(ctx, c, "uniformMatrix3fv", JS_NewCFunction(ctx, nb_gl_uniformMatrix3fv, "uniformMatrix3fv", 3));
    JS_SetPropertyStr(ctx, c, "drawArrays", JS_NewCFunction(ctx, nb_gl_drawArrays, "drawArrays", 3));
    JS_SetPropertyStr(ctx, c, "drawElements", JS_NewCFunction(ctx, nb_gl_drawElements, "drawElements", 4));
    JS_SetPropertyStr(ctx, c, "getError", JS_NewCFunction(ctx, nb_gl_getError, "getError", 0));
    JS_SetPropertyStr(ctx, c, "getParameter", JS_NewCFunction(ctx, nb_gl_getParameter, "getParameter", 1));
    JS_SetPropertyStr(ctx, c, "getExtension", JS_NewCFunction(ctx, nb_gl_getExtension, "getExtension", 1));
    JS_SetPropertyStr(ctx, c, "getSupportedExtensions", JS_NewCFunction(ctx, nb_gl_getSupportedExtensions, "getSupportedExtensions", 0));
    JS_SetPropertyStr(ctx, c, "enable", JS_NewCFunction(ctx, nb_gl_enable, "enable", 1));
    JS_SetPropertyStr(ctx, c, "disable", JS_NewCFunction(ctx, nb_gl_disable, "disable", 1));
    JS_SetPropertyStr(ctx, c, "blendFunc", JS_NewCFunction(ctx, nb_gl_blendFunc, "blendFunc", 2));
    JS_SetPropertyStr(ctx, c, "depthFunc", JS_NewCFunction(ctx, nb_gl_depthFunc, "depthFunc", 1));
    JS_SetPropertyStr(ctx, c, "depthMask", JS_NewCFunction(ctx, nb_gl_depthMask, "depthMask", 1));
    JS_SetPropertyStr(ctx, c, "colorMask", JS_NewCFunction(ctx, nb_gl_colorMask, "colorMask", 4));
    JS_SetPropertyStr(ctx, c, "cullFace", JS_NewCFunction(ctx, nb_gl_cullFace, "cullFace", 1));
    JS_SetPropertyStr(ctx, c, "frontFace", JS_NewCFunction(ctx, nb_gl_frontFace, "frontFace", 1));
    JS_SetPropertyStr(ctx, c, "createTexture", JS_NewCFunction(ctx, nb_gl_createTexture, "createTexture", 0));
    JS_SetPropertyStr(ctx, c, "deleteTexture", JS_NewCFunction(ctx, nb_gl_deleteTexture, "deleteTexture", 1));
    JS_SetPropertyStr(ctx, c, "bindTexture", JS_NewCFunction(ctx, nb_gl_bindTexture, "bindTexture", 2));
    JS_SetPropertyStr(ctx, c, "activeTexture", JS_NewCFunction(ctx, nb_gl_activeTexture, "activeTexture", 1));
    JS_SetPropertyStr(ctx, c, "texParameteri", JS_NewCFunction(ctx, nb_gl_texParameteri, "texParameteri", 3));
    JS_SetPropertyStr(ctx, c, "generateMipmap", JS_NewCFunction(ctx, nb_gl_generateMipmap, "generateMipmap", 1));
    JS_SetPropertyStr(ctx, c, "texImage2D", JS_NewCFunction(ctx, nb_gl_texImage2D, "texImage2D", 9));
    JS_SetPropertyStr(ctx, c, "texSubImage2D", JS_NewCFunction(ctx, nb_gl_texSubImage2D, "texSubImage2D", 9));
    JS_SetPropertyStr(ctx, c, "pixelStorei", JS_NewCFunction(ctx, nb_gl_pixelStorei, "pixelStorei", 2));
    JS_SetPropertyStr(ctx, c, "readPixels", JS_NewCFunction(ctx, nb_gl_readPixels, "readPixels", 7));
    JS_SetPropertyStr(ctx, elem, "__nb_glctx", JS_DupValue(ctx, c));
    if (eglMakeCurrent(g_egl_dpy, surf, surf, gc) == EGL_TRUE) {
        glViewport(0, 0, g_nbgl[slot].w, g_nbgl[slot].h);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        nb_gl_present(ctx, c);
    }
    return c;
}
#else
static void nb_gl_end_all(void) { }
#endif /* NB_HAVE_GLES */
