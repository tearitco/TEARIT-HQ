/* gfx.c — GLSL shaders, generative materials, ship meshes, instancing */
#include "sw.h"
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

static unsigned g_prog_lit = 0, g_prog_sky = 0, g_prog_laser = 0;
static unsigned g_tex_noise = 0, g_tex_stars = 0, g_tex_hull[SHIP_COUNT];
static unsigned g_list_ship[SHIP_COUNT];
static unsigned g_list_tree = 0, g_list_rock = 0, g_list_post = 0;
static int g_w = WIN_W, g_h = WIN_H;
static int g_use_shaders = 0;

static const char *VS_LIT =
    "varying vec3 vN; varying vec3 vW; varying vec2 vUV;\n"
    "void main(){\n"
    "  vN = normalize(gl_NormalMatrix * gl_Normal);\n"
    "  vec4 w = gl_ModelViewMatrix * gl_Vertex;\n"
    "  vW = w.xyz;\n"
    "  vUV = gl_MultiTexCoord0.xy;\n"
    "  gl_Position = ftransform();\n"
    "  gl_FrontColor = gl_Color;\n"
    "}\n";

static const char *FS_LIT =
    "uniform sampler2D uTex;\n"
    "uniform vec3 uLightDir;\n"
    "uniform vec3 uLightCol;\n"
    "uniform vec3 uAmbient;\n"
    "uniform float uTime;\n"
    "uniform float uEmissive;\n"
    "varying vec3 vN; varying vec3 vW; varying vec2 vUV;\n"
    "void main(){\n"
    "  vec3 N = normalize(vN);\n"
    "  vec3 L = normalize(uLightDir);\n"
    "  float ndl = max(dot(N, L), 0.0);\n"
    "  float wrap = ndl * 0.75 + 0.25;\n"
    "  vec3 halfv = normalize(L + normalize(-vW));\n"
    "  float spec = pow(max(dot(N, halfv), 0.0), 48.0);\n"
    "  vec4 tex = texture2D(uTex, vUV * 2.0);\n"
    "  vec3 base = gl_Color.rgb * tex.rgb;\n"
    "  vec3 col = base * (uAmbient + uLightCol * wrap) + uLightCol * spec * 0.35;\n"
    "  col += base * uEmissive;\n"
    "  /* rim */\n"
    "  float rim = pow(1.0 - max(dot(N, normalize(-vW)), 0.0), 3.0);\n"
    "  col += rim * uLightCol * 0.15;\n"
    "  gl_FragColor = vec4(col, gl_Color.a);\n"
    "}\n";

static const char *VS_SKY =
    "varying vec3 vDir;\n"
    "void main(){\n"
    "  vDir = gl_Vertex.xyz;\n"
    "  gl_Position = ftransform();\n"
    "}\n";

static const char *FS_SKY =
    "uniform vec3 uTop; uniform vec3 uBot; uniform vec3 uSun;\n"
    "uniform float uSpace; uniform sampler2D uStars; uniform float uTime;\n"
    "varying vec3 vDir;\n"
    "void main(){\n"
    "  vec3 d = normalize(vDir);\n"
    "  float h = d.y * 0.5 + 0.5;\n"
    "  vec3 col = mix(uBot, uTop, h);\n"
    "  float sun = pow(max(dot(d, normalize(uSun)), 0.0), 128.0);\n"
    "  col += vec3(1.0, 0.9, 0.6) * sun * 1.5;\n"
    "  float sunH = pow(max(dot(d, normalize(uSun)), 0.0), 8.0);\n"
    "  col += vec3(1.0, 0.6, 0.2) * sunH * 0.25;\n"
    "  if (uSpace > 0.5) {\n"
    "    vec2 uv = d.xz * 0.5 + 0.5 + d.y * 0.1;\n"
    "    float stars = texture2D(uStars, uv * 3.0 + uTime * 0.001).r;\n"
    "    col = mix(vec3(0.01,0.02,0.05), vec3(0.05,0.07,0.12), h);\n"
    "    col += stars * stars * 1.4;\n"
    "    col += sun * vec3(1.0,0.95,0.8);\n"
    "  }\n"
    "  gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

static const char *VS_LASER =
    "void main(){ gl_Position = ftransform(); gl_FrontColor = gl_Color; }\n";
static const char *FS_LASER =
    "void main(){\n"
    "  vec3 c = gl_Color.rgb;\n"
    "  float a = gl_Color.a;\n"
    "  gl_FragColor = vec4(c * 1.4, a);\n"
    "}\n";

static unsigned compile_shader(GLenum type, const char *src) {
    unsigned s = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, NULL, log);
        fprintf(stderr, "shader err: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static unsigned link_prog(const char *vs, const char *fs) {
    unsigned v = compile_shader(GL_VERTEX_SHADER, vs);
    unsigned f = compile_shader(GL_FRAGMENT_SHADER, fs);
    unsigned p;
    GLint ok = 0;
    if (!v || !f) return 0;
    p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(v);
    glDeleteShader(f);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, 512, NULL, log);
        fprintf(stderr, "link err: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static void mesh_box(float x0, float y0, float z0, float x1, float y1, float z1) {
    float nx, ny, nz;
    /* +Y */
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0); glVertex3f(x0, y1, z0);
    glTexCoord2f(1, 0); glVertex3f(x1, y1, z0);
    glTexCoord2f(1, 1); glVertex3f(x1, y1, z1);
    glTexCoord2f(0, 1); glVertex3f(x0, y1, z1);
    /* -Y */
    glNormal3f(0, -1, 0);
    glTexCoord2f(0, 0); glVertex3f(x0, y0, z1);
    glTexCoord2f(1, 0); glVertex3f(x1, y0, z1);
    glTexCoord2f(1, 1); glVertex3f(x1, y0, z0);
    glTexCoord2f(0, 1); glVertex3f(x0, y0, z0);
    /* +X */
    glNormal3f(1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(x1, y0, z0);
    glTexCoord2f(1, 0); glVertex3f(x1, y0, z1);
    glTexCoord2f(1, 1); glVertex3f(x1, y1, z1);
    glTexCoord2f(0, 1); glVertex3f(x1, y1, z0);
    /* -X */
    glNormal3f(-1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(x0, y0, z1);
    glTexCoord2f(1, 0); glVertex3f(x0, y0, z0);
    glTexCoord2f(1, 1); glVertex3f(x0, y1, z0);
    glTexCoord2f(0, 1); glVertex3f(x0, y1, z1);
    /* +Z */
    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0); glVertex3f(x0, y0, z1);
    glTexCoord2f(1, 0); glVertex3f(x1, y0, z1);
    glTexCoord2f(1, 1); glVertex3f(x1, y1, z1);
    glTexCoord2f(0, 1); glVertex3f(x0, y1, z1);
    /* -Z */
    glNormal3f(0, 0, -1);
    glTexCoord2f(0, 0); glVertex3f(x1, y0, z0);
    glTexCoord2f(1, 0); glVertex3f(x0, y0, z0);
    glTexCoord2f(1, 1); glVertex3f(x0, y1, z0);
    glTexCoord2f(0, 1); glVertex3f(x1, y1, z0);
    (void)nx; (void)ny; (void)nz;
}

static void build_ship_mesh(int type) {
    const ShipDef *d = sim_ship_def(type);
    g_list_ship[type] = glGenLists(1);
    glNewList(g_list_ship[type], GL_COMPILE);
    glBegin(GL_QUADS);
    glColor3f(d->r, d->g, d->b);
    switch (type) {
    case SHIP_INTERCEPTOR:
        /* slim fuselage + dual wings */
        mesh_box(-0.25f, -0.15f, -1.4f, 0.25f, 0.15f, 1.0f);
        glColor3f(d->r * 0.85f, d->g * 0.85f, d->b * 0.9f);
        mesh_box(-1.6f, -0.05f, -0.2f, -0.25f, 0.05f, 0.6f);
        mesh_box(0.25f, -0.05f, -0.2f, 1.6f, 0.05f, 0.6f);
        glColor3f(d->er, d->eg, d->eb);
        mesh_box(-0.2f, -0.12f, 1.0f, 0.2f, 0.12f, 1.35f);
        break;
    case SHIP_FIGHTER:
        /* X-wing-ish body + 4 foils */
        mesh_box(-0.35f, -0.25f, -1.6f, 0.35f, 0.3f, 1.2f);
        glColor3f(0.85f, 0.85f, 0.9f);
        mesh_box(-0.15f, 0.3f, -0.4f, 0.15f, 0.55f, 0.4f); /* cockpit */
        glColor3f(d->r * 0.9f, d->g * 0.9f, d->b);
        mesh_box(-1.8f, 0.15f, -0.3f, -0.35f, 0.25f, 0.8f);
        mesh_box(0.35f, 0.15f, -0.3f, 1.8f, 0.25f, 0.8f);
        mesh_box(-1.8f, -0.25f, -0.3f, -0.35f, -0.15f, 0.8f);
        mesh_box(0.35f, -0.25f, -0.3f, 1.8f, -0.15f, 0.8f);
        glColor3f(d->er, d->eg, d->eb);
        mesh_box(-0.5f, -0.15f, 1.1f, -0.2f, 0.15f, 1.5f);
        mesh_box(0.2f, -0.15f, 1.1f, 0.5f, 0.15f, 1.5f);
        break;
    case SHIP_BOMBER:
        mesh_box(-0.5f, -0.35f, -1.8f, 0.5f, 0.4f, 1.4f);
        glColor3f(d->r * 0.7f, d->g * 0.7f, d->b * 0.75f);
        mesh_box(-1.2f, -0.5f, -0.5f, -0.5f, 0.1f, 0.9f);
        mesh_box(0.5f, -0.5f, -0.5f, 1.2f, 0.1f, 0.9f);
        glColor3f(d->er, d->eg, d->eb);
        mesh_box(-0.3f, -0.2f, 1.3f, 0.3f, 0.2f, 1.8f);
        break;
    case SHIP_FREIGHTER:
        mesh_box(-0.8f, -0.5f, -2.2f, 0.8f, 0.6f, 1.8f);
        glColor3f(0.55f, 0.55f, 0.6f);
        mesh_box(-1.4f, -0.3f, -0.8f, -0.8f, 0.4f, 1.0f);
        mesh_box(0.8f, -0.3f, -0.8f, 1.4f, 0.4f, 1.0f);
        glColor3f(d->er, d->eg, d->eb);
        mesh_box(-0.5f, -0.3f, 1.7f, 0.5f, 0.3f, 2.3f);
        break;
    case SHIP_SPEEDER:
    default:
        mesh_box(-0.6f, -0.15f, -1.0f, 0.6f, 0.2f, 1.0f);
        glColor3f(0.2f, 0.25f, 0.35f);
        mesh_box(-0.35f, 0.2f, -0.3f, 0.35f, 0.45f, 0.5f);
        glColor3f(d->er, d->eg, d->eb);
        mesh_box(-0.4f, -0.1f, 0.9f, -0.15f, 0.1f, 1.25f);
        mesh_box(0.15f, -0.1f, 0.9f, 0.4f, 0.1f, 1.25f);
        break;
    }
    glEnd();
    glEndList();
}

static void build_props(void) {
    int i;
    g_list_tree = glGenLists(1);
    glNewList(g_list_tree, GL_COMPILE);
    glBegin(GL_QUADS);
    glColor3f(0.35f, 0.22f, 0.10f);
    mesh_box(-0.2f, 0, -0.2f, 0.2f, 3.5f, 0.2f);
    glColor3f(0.12f, 0.45f, 0.15f);
    mesh_box(-1.2f, 2.5f, -1.2f, 1.2f, 5.5f, 1.2f);
    glColor3f(0.18f, 0.55f, 0.20f);
    mesh_box(-0.9f, 4.5f, -0.9f, 0.9f, 6.8f, 0.9f);
    glEnd();
    glEndList();

    g_list_rock = glGenLists(1);
    glNewList(g_list_rock, GL_COMPILE);
    glBegin(GL_QUADS);
    glColor3f(0.45f, 0.42f, 0.40f);
    mesh_box(-0.8f, 0, -0.6f, 0.8f, 0.9f, 0.6f);
    glColor3f(0.38f, 0.36f, 0.34f);
    mesh_box(-0.5f, 0.9f, -0.4f, 0.5f, 1.4f, 0.4f);
    glEnd();
    glEndList();

    g_list_post = glGenLists(1);
    glNewList(g_list_post, GL_COMPILE);
    glBegin(GL_QUADS);
    glColor3f(0.55f, 0.55f, 0.6f);
    mesh_box(-1.5f, 0, -1.5f, 1.5f, 0.4f, 1.5f);
    mesh_box(-0.4f, 0.4f, -0.4f, 0.4f, 4.0f, 0.4f);
    glColor3f(0.9f, 0.85f, 0.2f);
    mesh_box(-0.6f, 3.5f, -0.6f, 0.6f, 4.2f, 0.6f);
    glEnd();
    glEndList();

    for (i = 0; i < SHIP_COUNT; i++) build_ship_mesh(i);
}

int gfx_init(void) {
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW: %s — shaders disabled\n", glewGetErrorString(err));
        g_use_shaders = 0;
    } else {
        g_prog_lit = link_prog(VS_LIT, FS_LIT);
        g_prog_sky = link_prog(VS_SKY, FS_SKY);
        g_prog_laser = link_prog(VS_LASER, FS_LASER);
        g_use_shaders = (g_prog_lit && g_prog_sky) ? 1 : 0;
        fprintf(stderr, "shaders: %s\n", g_use_shaders ? "ON" : "OFF");
    }
    gen_make_noise_tex(&g_tex_noise, 256);
    gen_make_star_tex(&g_tex_stars, 512);
    {
        int i;
        for (i = 0; i < SHIP_COUNT; i++) {
            const ShipDef *d = sim_ship_def(i);
            gen_make_hull_tex(&g_tex_hull[i], 128, d->r, d->g, d->b);
        }
    }
    build_props();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glClearColor(0.02f, 0.03f, 0.06f, 1.f);
    return 0;
}

void gfx_shutdown(void) {
    int i;
    if (g_prog_lit) glDeleteProgram(g_prog_lit);
    if (g_prog_sky) glDeleteProgram(g_prog_sky);
    if (g_prog_laser) glDeleteProgram(g_prog_laser);
    for (i = 0; i < SHIP_COUNT; i++) {
        if (g_list_ship[i]) glDeleteLists(g_list_ship[i], 1);
        if (g_tex_hull[i]) glDeleteTextures(1, &g_tex_hull[i]);
    }
}

void gfx_resize(int w, int h) {
    g_w = w > 1 ? w : 1;
    g_h = h > 1 ? h : 1;
    glViewport(0, 0, g_w, g_h);
}

unsigned gfx_ship_list(int ship) {
    if (ship < 0 || ship >= SHIP_COUNT) return g_list_ship[0];
    return g_list_ship[ship];
}

void gfx_draw_sky(const Game *g, float yaw, float pitch) {
    float top[3], bot[3], sun[3] = {0.4f, 0.75f, 0.35f};
    float space = (g->planet == PLANET_SPACE) ? 1.f : 0.f;
    (void)yaw; (void)pitch;
    switch (g->planet) {
    case PLANET_ENDOR:
        top[0] = 0.25f; top[1] = 0.55f; top[2] = 0.95f;
        bot[0] = 0.55f; bot[1] = 0.72f; bot[2] = 0.45f;
        break;
    case PLANET_TATOOINE:
        top[0] = 0.55f; top[1] = 0.65f; top[2] = 0.95f;
        bot[0] = 0.95f; bot[1] = 0.70f; bot[2] = 0.35f;
        sun[0] = 0.6f; sun[1] = 0.5f; sun[2] = 0.2f;
        break;
    case PLANET_HOTH:
        top[0] = 0.55f; top[1] = 0.70f; top[2] = 0.95f;
        bot[0] = 0.75f; bot[1] = 0.85f; bot[2] = 0.95f;
        break;
    case PLANET_MUSTAFAR:
        top[0] = 0.15f; top[1] = 0.08f; top[2] = 0.06f;
        bot[0] = 0.55f; bot[1] = 0.18f; bot[2] = 0.05f;
        sun[0] = 0.9f; sun[1] = 0.3f; sun[2] = 0.05f;
        break;
    default:
        top[0] = 0.02f; top[1] = 0.03f; top[2] = 0.08f;
        bot[0] = 0.0f; bot[1] = 0.0f; bot[2] = 0.02f;
        break;
    }
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glPushMatrix();
    glLoadIdentity();
    /* camera rotation only */
    {
        const Entity *p = &g->ents[g->local];
        glRotatef(-p->pitch * 180.f / (float)M_PI, 1, 0, 0);
        glRotatef(-p->yaw * 180.f / (float)M_PI, 0, 1, 0);
    }
    if (g_use_shaders && g_prog_sky) {
        glUseProgram(g_prog_sky);
        glUniform3fv(glGetUniformLocation(g_prog_sky, "uTop"), 1, top);
        glUniform3fv(glGetUniformLocation(g_prog_sky, "uBot"), 1, bot);
        glUniform3fv(glGetUniformLocation(g_prog_sky, "uSun"), 1, sun);
        glUniform1f(glGetUniformLocation(g_prog_sky, "uSpace"), space);
        glUniform1f(glGetUniformLocation(g_prog_sky, "uTime"), g->time);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_tex_stars);
        glUniform1i(glGetUniformLocation(g_prog_sky, "uStars"), 0);
    }
    glBegin(GL_QUADS);
    /* large sky cube */
    {
        float s = 200.f;
        glColor3f(top[0], top[1], top[2]);
        /* simplified sphere-ish box */
        glVertex3f(-s, -s, -s); glVertex3f(s, -s, -s); glVertex3f(s, s, -s); glVertex3f(-s, s, -s);
        glVertex3f(-s, -s, s); glVertex3f(-s, s, s); glVertex3f(s, s, s); glVertex3f(s, -s, s);
        glVertex3f(-s, -s, -s); glVertex3f(-s, -s, s); glVertex3f(s, -s, s); glVertex3f(s, -s, -s);
        glVertex3f(-s, s, -s); glVertex3f(s, s, -s); glVertex3f(s, s, s); glVertex3f(-s, s, s);
        glVertex3f(-s, -s, -s); glVertex3f(-s, s, -s); glVertex3f(-s, s, s); glVertex3f(-s, -s, s);
        glVertex3f(s, -s, -s); glVertex3f(s, -s, s); glVertex3f(s, s, s); glVertex3f(s, s, -s);
    }
    glEnd();
    if (g_use_shaders) glUseProgram(0);
    glPopMatrix();
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

static void draw_terrain_chunk(const Game *g, float cx, float cz, int step) {
    int ix, iz;
    float half = 90.f;
    float tr[3], tg[3], tb[3];
    switch (g->planet) {
    case PLANET_ENDOR:   tr[0]=0.18f;tg[0]=0.42f;tb[0]=0.12f; tr[1]=0.25f;tg[1]=0.35f;tb[1]=0.15f; break;
    case PLANET_TATOOINE:tr[0]=0.78f;tg[0]=0.62f;tb[0]=0.32f; tr[1]=0.65f;tg[1]=0.50f;tb[1]=0.28f; break;
    case PLANET_HOTH:    tr[0]=0.85f;tg[0]=0.90f;tb[0]=0.95f; tr[1]=0.70f;tg[1]=0.78f;tb[1]=0.88f; break;
    case PLANET_MUSTAFAR:tr[0]=0.35f;tg[0]=0.12f;tb[0]=0.08f; tr[1]=0.55f;tg[1]=0.15f;tb[1]=0.05f; break;
    default: return;
    }
    glBegin(GL_TRIANGLES);
    for (iz = -half; iz < half; iz += step) {
        for (ix = -half; ix < half; ix += step) {
            float x0 = cx + ix, z0 = cz + iz;
            float x1 = x0 + step, z1 = z0 + step;
            float h00 = gen_height(g->planet, x0, z0);
            float h10 = gen_height(g->planet, x1, z0);
            float h01 = gen_height(g->planet, x0, z1);
            float h11 = gen_height(g->planet, x1, z1);
            float n = gen_noise2(x0 * 0.05f, z0 * 0.05f);
            float r = lerpf(tr[0], tr[1], n);
            float gg = lerpf(tg[0], tg[1], n);
            float b = lerpf(tb[0], tb[1], n);
            /* lava tint */
            if (g->planet == PLANET_MUSTAFAR && h00 < 3.f) {
                r = 0.95f; gg = 0.25f + 0.1f * sinf(g->time * 3.f + x0); b = 0.05f;
            }
            /* normals approx via height diffs */
            {
                float hx = h10 - h00, hz = h01 - h00;
                float nx = -hx, ny = (float)step, nz = -hz;
                norm3(&nx, &ny, &nz);
                glNormal3f(nx, ny, nz);
            }
            glColor3f(r, gg, b);
            glTexCoord2f(0, 0); glVertex3f(x0, h00, z0);
            glTexCoord2f(1, 0); glVertex3f(x1, h10, z0);
            glTexCoord2f(1, 1); glVertex3f(x1, h11, z1);
            glTexCoord2f(0, 0); glVertex3f(x0, h00, z0);
            glTexCoord2f(1, 1); glVertex3f(x1, h11, z1);
            glTexCoord2f(0, 1); glVertex3f(x0, h01, z1);
        }
    }
    glEnd();
}

static void draw_instances(const Game *g, float cx, float cz) {
    int i, j;
    if (g->planet == PLANET_SPACE) {
        /* asteroid field — instanced rocks */
        for (i = -6; i <= 6; i++) {
            for (j = -6; j <= 6; j++) {
                float ax, ay, az, n;
                if (i == 0 && j == 0) continue;
                n = gen_noise2(i * 1.7f + 2, j * 1.3f);
                if (n < 0.45f) continue;
                ax = cx + i * 40.f + n * 10.f;
                ay = 20.f + gen_noise2(j * 2.f, i * 2.f) * 60.f;
                az = cz + j * 40.f;
                glPushMatrix();
                glTranslatef(ax, ay, az);
                glScalef(2.f + n * 5.f, 2.f + n * 3.f, 2.f + n * 4.f);
                glRotatef(n * 360.f, 0.2f, 1.f, 0.1f);
                glCallList(g_list_rock);
                glPopMatrix();
            }
        }
        return;
    }
    /* trees / rocks on surface */
    for (i = -12; i <= 12; i++) {
        for (j = -12; j <= 12; j++) {
            float x = cx + i * 12.f + 3.f;
            float z = cz + j * 12.f + 2.f;
            float n = gen_noise2(x * 0.07f, z * 0.07f);
            float h = gen_height(g->planet, x, z);
            if (g->planet == PLANET_ENDOR && n > 0.55f) {
                glPushMatrix();
                glTranslatef(x, h, z);
                glScalef(0.8f + n * 0.6f, 0.8f + n * 0.8f, 0.8f + n * 0.6f);
                glCallList(g_list_tree);
                glPopMatrix();
            } else if (n > 0.72f) {
                glPushMatrix();
                glTranslatef(x, h, z);
                glScalef(1.f + n, 1.f + n * 0.5f, 1.f + n);
                glCallList(g_list_rock);
                glPopMatrix();
            }
        }
    }
}

void gfx_begin_frame(const Game *g, float aspect) {
    const Entity *p = &g->ents[g->local];
    float eye_y = p->y + (p->in_ship ? 0.6f : 1.6f);
    float shx = 0, shy = 0;
    if (g->shake > 0) {
        shx = (frand() - 0.5f) * g->shake;
        shy = (frand() - 0.5f) * g->shake;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(p->in_ship ? 70.0 : 65.0, aspect, 0.15, 900.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-p->pitch * 180.f / (float)M_PI, 1, 0, 0);
    glRotatef(-p->yaw * 180.f / (float)M_PI, 0, 1, 0);
    glTranslatef(-p->x + shx, -eye_y + shy, -p->z);
}

void gfx_draw_world(const Game *g) {
    const Entity *p = &g->ents[g->local];
    float light_dir[3] = {0.45f, 0.85f, 0.35f};
    float light_col[3] = {1.0f, 0.95f, 0.85f};
    float ambient[3] = {0.18f, 0.20f, 0.28f};
    gfx_draw_sky(g, p->yaw, p->pitch);

    if (g->planet == PLANET_MUSTAFAR) {
        light_col[0] = 1.f; light_col[1] = 0.55f; light_col[2] = 0.25f;
        ambient[0] = 0.2f; ambient[1] = 0.08f; ambient[2] = 0.05f;
    } else if (g->planet == PLANET_SPACE) {
        light_col[0] = 1.f; light_col[1] = 1.f; light_col[2] = 1.f;
        ambient[0] = 0.05f; ambient[1] = 0.05f; ambient[2] = 0.08f;
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_tex_noise);
    if (g_use_shaders && g_prog_lit) {
        glUseProgram(g_prog_lit);
        glUniform1i(glGetUniformLocation(g_prog_lit, "uTex"), 0);
        glUniform3fv(glGetUniformLocation(g_prog_lit, "uLightDir"), 1, light_dir);
        glUniform3fv(glGetUniformLocation(g_prog_lit, "uLightCol"), 1, light_col);
        glUniform3fv(glGetUniformLocation(g_prog_lit, "uAmbient"), 1, ambient);
        glUniform1f(glGetUniformLocation(g_prog_lit, "uTime"), g->time);
        glUniform1f(glGetUniformLocation(g_prog_lit, "uEmissive"), 0.f);
    } else {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        {
            float pos[4] = {light_dir[0], light_dir[1], light_dir[2], 0};
            glLightfv(GL_LIGHT0, GL_POSITION, pos);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, light_col);
            glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
        }
    }

    draw_terrain_chunk(g, p->x, p->z, 4);
    draw_instances(g, p->x, p->z);

    /* command posts */
    {
        int i;
        for (i = 0; i < g->n_posts; i++) {
            const CommandPost *cp = &g->posts[i];
            float h = gen_height(g->planet, cp->x, cp->z);
            glPushMatrix();
            glTranslatef(cp->x, h, cp->z);
            if (cp->team == TEAM_REBEL) glColor3f(0.2f, 0.5f, 1.f);
            else if (cp->team == TEAM_EMPIRE) glColor3f(1.f, 0.2f, 0.2f);
            else glColor3f(0.7f, 0.7f, 0.3f);
            glCallList(g_list_post);
            /* capture ring */
            {
                float a, rr = cp->radius;
                glDisable(GL_TEXTURE_2D);
                glBegin(GL_LINE_LOOP);
                for (a = 0; a < 6.28f; a += 0.2f)
                    glVertex3f(cosf(a) * rr, 0.5f, sinf(a) * rr);
                glEnd();
                glEnable(GL_TEXTURE_2D);
            }
            glPopMatrix();
        }
    }

    if (g_use_shaders) glUseProgram(0);
    else glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
}

void gfx_draw_entity(const Game *g, const Entity *e, int is_local) {
    float h;
    if (!e->alive) return;
    if (is_local && !e->in_ship) return; /* FPS: don't draw self body */
    h = (g->planet == PLANET_SPACE) ? e->y : e->y;
    glPushMatrix();
    glTranslatef(e->x, h, e->z);
    glRotatef(e->yaw * 180.f / (float)M_PI, 0, 1, 0);
    glRotatef(e->pitch * 180.f / (float)M_PI, 1, 0, 0);
    if (e->in_ship) {
        const ShipDef *d = sim_ship_def(e->ship);
        glScalef(d->scale, d->scale, d->scale);
        if (g_use_shaders && g_prog_lit) {
            glUseProgram(g_prog_lit);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_tex_hull[e->ship]);
            glEnable(GL_TEXTURE_2D);
            glUniform1i(glGetUniformLocation(g_prog_lit, "uTex"), 0);
            glUniform1f(glGetUniformLocation(g_prog_lit, "uEmissive"), 0.08f);
        }
        glCallList(g_list_ship[e->ship]);
        /* engine trail glow */
        if (len3(e->vx, e->vy, e->vz) > 2.f) {
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_LIGHTING);
            if (g_use_shaders) glUseProgram(0);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glColor4f(d->er, d->eg, d->eb, 0.7f);
            glBegin(GL_QUADS);
            mesh_box(-0.15f, -0.15f, 1.2f, 0.15f, 0.15f, 2.0f + frand() * 0.5f);
            glEnd();
            glDisable(GL_BLEND);
        }
        if (g_use_shaders) glUseProgram(0);
        glDisable(GL_TEXTURE_2D);
    } else {
        /* infantry */
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        if (e->team == TEAM_REBEL) glColor3f(0.25f, 0.45f, 0.9f);
        else glColor3f(0.85f, 0.15f, 0.15f);
        mesh_box(-0.3f, 0.f, -0.25f, 0.3f, 1.5f, 0.25f);
        glColor3f(0.9f, 0.75f, 0.6f);
        mesh_box(-0.2f, 1.5f, -0.2f, 0.2f, 1.9f, 0.2f);
        glEnd();
        if (e->weapon == WPN_SABER) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glColor4f(0.3f, 0.9f, 0.4f, 0.95f);
            glBegin(GL_QUADS);
            mesh_box(-0.04f, 0.9f, 0.2f, 0.04f, 2.4f, 0.28f);
            glEnd();
            glDisable(GL_BLEND);
        }
    }
    glPopMatrix();
    (void)g;
}

void gfx_draw_bullets(const Game *g) {
    int i;
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    if (g_use_shaders && g_prog_laser) glUseProgram(g_prog_laser);
    glLineWidth(3.f);
    glBegin(GL_LINES);
    for (i = 0; i < MAX_BULLETS; i++) {
        const Bullet *b = &g->bullets[i];
        float L;
        if (!b->alive) continue;
        L = 1.2f;
        glColor4f(b->r, b->g, b->b, 0.95f);
        glVertex3f(b->x, b->y, b->z);
        glVertex3f(b->x - b->vx * 0.02f * L, b->y - b->vy * 0.02f * L, b->z - b->vz * 0.02f * L);
    }
    glEnd();
    glLineWidth(1.f);
    if (g_use_shaders) glUseProgram(0);
    glDisable(GL_BLEND);
}

void gfx_draw_fx(const Game *g) {
    int i;
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    for (i = 0; i < MAX_FX; i++) {
        const Fx *f = &g->fx[i];
        float t, s;
        if (!f->alive) continue;
        t = f->life / f->max_life;
        s = f->size * (1.2f - t * 0.4f);
        glPushMatrix();
        glTranslatef(f->x, f->y, f->z);
        glColor4f(f->r, f->g, f->b, t);
        glScalef(s, s, s);
        glBegin(GL_QUADS);
        mesh_box(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
        glEnd();
        glPopMatrix();
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void gfx_draw_builds(const Game *g) {
    int i;
    glDisable(GL_TEXTURE_2D);
    for (i = 0; i < MAX_BUILD; i++) {
        const Building *b = &g->builds[i];
        float h;
        if (!b->used) continue;
        h = gen_height(g->planet, b->x, b->z);
        glPushMatrix();
        glTranslatef(b->x, h, b->z);
        glBegin(GL_QUADS);
        switch (b->type) {
        case BLD_TURRET:
            glColor3f(0.5f, 0.55f, 0.6f);
            mesh_box(-0.6f, 0, -0.6f, 0.6f, 1.2f, 0.6f);
            glColor3f(0.9f, 0.4f, 0.1f);
            mesh_box(-0.15f, 1.2f, -0.15f, 0.15f, 2.0f, 0.15f);
            break;
        case BLD_SHIELD_GEN:
            glColor3f(0.3f, 0.5f, 0.9f);
            mesh_box(-0.8f, 0, -0.8f, 0.8f, 1.5f, 0.8f);
            break;
        case BLD_OUTPOST:
            glColor3f(0.6f, 0.55f, 0.45f);
            mesh_box(-1.5f, 0, -1.5f, 1.5f, 2.5f, 1.5f);
            break;
        case BLD_FARM:
            glColor3f(0.3f, 0.6f, 0.25f);
            mesh_box(-1.2f, 0, -1.2f, 1.2f, 0.4f, 1.2f);
            break;
        case BLD_MINE:
            glColor3f(0.45f, 0.4f, 0.35f);
            mesh_box(-1.f, 0, -1.f, 1.f, 1.f, 1.f);
            break;
        default: break;
        }
        glEnd();
        glPopMatrix();
    }
}

void gfx_end_frame(void) {
    /* restore 2D for UI */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, g_w, 0, g_h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
}

void gfx_spawn_explosion(Game *g, float x, float y, float z, float size) {
    int i, n = 0;
    for (i = 0; i < MAX_FX && n < 8; i++) {
        Fx *f = &g->fx[i];
        if (f->alive) continue;
        f->alive = 1;
        f->kind = 0;
        f->x = x + (frand() - 0.5f) * size;
        f->y = y + (frand() - 0.5f) * size;
        f->z = z + (frand() - 0.5f) * size;
        f->vx = (frand() - 0.5f) * 8.f;
        f->vy = frand() * 6.f;
        f->vz = (frand() - 0.5f) * 8.f;
        f->life = f->max_life = 0.4f + frand() * 0.4f;
        f->size = size * (0.4f + frand() * 0.6f);
        f->r = 1.f; f->g = 0.5f + frand() * 0.4f; f->b = 0.1f;
        n++;
    }
    g->shake = fmaxf(g->shake, size * 0.15f);
}
