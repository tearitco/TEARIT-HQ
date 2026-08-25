/* be_main.c — Muchi Blend Phase-1 (Blender-shaped 3D viewport)
 *
 * freeglut + OpenGL fixed-function + Assimp (.obj / .fbx / common meshes).
 * Layout: File menu | tool strip | 3D viewport | outliner
 * CPU-safe: ≤20fps idle/orbit, dirty redraw, main-loop sleep, no busy-spin.
 * Title-bar ✕ works (WM_DELETE not swallowed by XDND).
 *
 *   sh button.sh r   Esc quit
 */
#define _GNU_SOURCE
#include <GL/freeglut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../../shared/media_drop_path.h"
#include "../../shared/chtpm_nav_mock.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- layout / budget ---- */
static int g_win_w = 1400, g_win_h = 860;
#define WIN_W (g_win_w)
#define WIN_H (g_win_h)
#define MENU_H 24
#define TOOL_W 52
#define RIGHT_W 220
#define STATUS_H 24
#define MAX_OBJS 32
#define MAX_V 200000
#define MAX_I 400000

#define UI_FPS_ACTIVE 20
#define UI_FPS_IDLE    8
#define UI_TIMER_MS   50
#define UI_TIMER_IDLE 150
#define SLEEP_ACTIVE_US 4000
#define SLEEP_IDLE_US   20000

typedef enum {
    TOOL_SELECT = 0,
    TOOL_GRAB,
    TOOL_ROTATE,
    TOOL_SCALE,
    TOOL_COUNT
} Tool;

typedef struct {
    int used;
    char name[48];
    float *v;     /* xyz * nv */
    float *n;     /* xyz * nv */
    unsigned *idx;
    int nv, ni;
    float pos[3];
    float rot[3]; /* degrees euler XYZ */
    float scl[3];
    float col[3];
    float aabb_min[3], aabb_max[3]; /* local */
} Obj3;

static Obj3 g_objs[MAX_OBJS];
static int g_n_objs = 0;
static int g_sel = -1;
static Tool g_tool = TOOL_SELECT;
static int g_file_menu = 0;
static int g_wire = 0;
static int g_shade = 1; /* 1 solid 0 wire forced */
static char g_status[256] = "";
static char g_project_root[1024] = ".";
static char g_scene_name[64] = "Untitled";
static volatile int g_quit = 0;

/* camera (orbit) */
static float g_cam_yaw = 35.f;
static float g_cam_pitch = 25.f;
static float g_cam_dist = 8.f;
static float g_cam_target[3] = { 0, 0.5f, 0 };
static float g_fov = 50.f;

/* interaction */
static int g_orbiting = 0, g_panning = 0, g_xform = 0;
static int g_last_mx, g_last_my;
static float g_xform_start[3];
static int g_xform_axis = -1; /* -1 free, 0X 1Y 2Z */

/* CPU / dirty */
static int g_ui_dirty = 1;
static int g_glut_ready = 0;
static int g_nav_active = 0;
static double g_last_ui = 0;
static int g_fps_n = 0;
static double g_fps_t0 = 0;
static float g_ui_fps = 0;
static int g_canvas_tick = 0;

/* XDND */
static Display *g_xdpy = NULL;
static Window g_xwin = 0;
static Atom g_xa_XdndAware, g_xa_XdndEnter, g_xa_XdndPosition, g_xa_XdndStatus;
static Atom g_xa_XdndLeave, g_xa_XdndDrop, g_xa_XdndFinished, g_xa_XdndSelection;
static Atom g_xa_XdndActionCopy, g_xa_text_uri_list;
static Window g_xdnd_source = 0;
static int g_xdnd_setup = 0;

static const char *tool_names[] = { "Select", "Grab", "Rotate", "Scale" };

/* ---- utils ---- */
static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
static float clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}
static float deg2rad(float d) { return d * (float)M_PI / 180.f; }
static void mark_ui_dirty(void) {
    g_ui_dirty = 1;
    if (g_glut_ready) glutPostRedisplay();
}
static void v3_set(float *a, float x, float y, float z) { a[0]=x; a[1]=y; a[2]=z; }
static void v3_add(float *a, const float *b) { a[0]+=b[0]; a[1]+=b[1]; a[2]+=b[2]; }
static void v3_scale(float *a, float s) { a[0]*=s; a[1]*=s; a[2]*=s; }

static void obj_free(Obj3 *o) {
    free(o->v); free(o->n); free(o->idx);
    o->v = o->n = NULL; o->idx = NULL;
    o->nv = o->ni = 0; o->used = 0;
}

static void objs_clear(void) {
    for (int i = 0; i < MAX_OBJS; i++)
        if (g_objs[i].used) obj_free(&g_objs[i]);
    g_n_objs = 0;
    g_sel = -1;
}

static void recompute_aabb(Obj3 *o) {
    if (!o->nv) {
        v3_set(o->aabb_min, -0.5f, -0.5f, -0.5f);
        v3_set(o->aabb_max, 0.5f, 0.5f, 0.5f);
        return;
    }
    float mn[3] = { 1e9f, 1e9f, 1e9f };
    float mx[3] = { -1e9f, -1e9f, -1e9f };
    for (int i = 0; i < o->nv; i++) {
        float *p = o->v + i * 3;
        for (int c = 0; c < 3; c++) {
            if (p[c] < mn[c]) mn[c] = p[c];
            if (p[c] > mx[c]) mx[c] = p[c];
        }
    }
    memcpy(o->aabb_min, mn, sizeof(mn));
    memcpy(o->aabb_max, mx, sizeof(mx));
}

static void normalize_mesh(Obj3 *o) {
    /* center + unit-ish size for import convenience */
    recompute_aabb(o);
    float cx = 0.5f * (o->aabb_min[0] + o->aabb_max[0]);
    float cy = 0.5f * (o->aabb_min[1] + o->aabb_max[1]);
    float cz = 0.5f * (o->aabb_min[2] + o->aabb_max[2]);
    float dx = o->aabb_max[0] - o->aabb_min[0];
    float dy = o->aabb_max[1] - o->aabb_min[1];
    float dz = o->aabb_max[2] - o->aabb_min[2];
    float ext = fmaxf(dx, fmaxf(dy, dz));
    if (ext < 1e-6f) ext = 1.f;
    float s = 2.f / ext;
    for (int i = 0; i < o->nv; i++) {
        o->v[i * 3 + 0] = (o->v[i * 3 + 0] - cx) * s;
        o->v[i * 3 + 1] = (o->v[i * 3 + 1] - cy) * s;
        o->v[i * 3 + 2] = (o->v[i * 3 + 2] - cz) * s;
    }
    recompute_aabb(o);
}

/* ---- primitive builders ---- */
static int obj_alloc_slot(void) {
    for (int i = 0; i < MAX_OBJS; i++)
        if (!g_objs[i].used) return i;
    return -1;
}

static void add_cube(const char *name, float x, float y, float z) {
    int si = obj_alloc_slot();
    if (si < 0) return;
    Obj3 *o = &g_objs[si];
    memset(o, 0, sizeof(*o));
    o->used = 1;
    snprintf(o->name, sizeof(o->name), "%s", name);
    o->nv = 24; /* 6 faces * 4 verts with normals */
    o->ni = 36;
    o->v = (float *)malloc((size_t)o->nv * 3 * sizeof(float));
    o->n = (float *)malloc((size_t)o->nv * 3 * sizeof(float));
    o->idx = (unsigned *)malloc((size_t)o->ni * sizeof(unsigned));
    /* unit cube faces */
    const float f[6][3] = {
        {0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}
    };
    const float corners[8][3] = {
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
        {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}
    };
    const int faces[6][4] = {
        {4,5,6,7}, {1,0,3,2}, {5,1,2,6}, {0,4,7,3}, {7,6,2,3}, {0,1,5,4}
    };
    int vi = 0, ii = 0;
    for (int face = 0; face < 6; face++) {
        int base = vi;
        for (int k = 0; k < 4; k++) {
            int c = faces[face][k];
            o->v[vi * 3 + 0] = corners[c][0] * 0.5f;
            o->v[vi * 3 + 1] = corners[c][1] * 0.5f;
            o->v[vi * 3 + 2] = corners[c][2] * 0.5f;
            o->n[vi * 3 + 0] = f[face][0];
            o->n[vi * 3 + 1] = f[face][1];
            o->n[vi * 3 + 2] = f[face][2];
            vi++;
        }
        o->idx[ii++] = base; o->idx[ii++] = base + 1; o->idx[ii++] = base + 2;
        o->idx[ii++] = base; o->idx[ii++] = base + 2; o->idx[ii++] = base + 3;
    }
    o->nv = vi; o->ni = ii;
    v3_set(o->pos, x, y, z);
    v3_set(o->rot, 0, 0, 0);
    v3_set(o->scl, 1, 1, 1);
    v3_set(o->col, 0.75f, 0.55f, 0.35f);
    recompute_aabb(o);
    if (si + 1 > g_n_objs) g_n_objs = si + 1;
    g_sel = si;
}

static void add_plane(const char *name) {
    int si = obj_alloc_slot();
    if (si < 0) return;
    Obj3 *o = &g_objs[si];
    memset(o, 0, sizeof(*o));
    o->used = 1;
    snprintf(o->name, sizeof(o->name), "%s", name);
    o->nv = 4; o->ni = 6;
    o->v = (float *)malloc(4 * 3 * sizeof(float));
    o->n = (float *)malloc(4 * 3 * sizeof(float));
    o->idx = (unsigned *)malloc(6 * sizeof(unsigned));
    float verts[4][3] = { {-2,0,-2},{2,0,-2},{2,0,2},{-2,0,2} };
    for (int i = 0; i < 4; i++) {
        o->v[i*3]=verts[i][0]; o->v[i*3+1]=verts[i][1]; o->v[i*3+2]=verts[i][2];
        o->n[i*3]=0; o->n[i*3+1]=1; o->n[i*3+2]=0;
    }
    unsigned id[] = {0,1,2, 0,2,3};
    memcpy(o->idx, id, sizeof(id));
    v3_set(o->pos, 0, 0, 0);
    v3_set(o->rot, 0, 0, 0);
    v3_set(o->scl, 1, 1, 1);
    v3_set(o->col, 0.45f, 0.48f, 0.42f);
    recompute_aabb(o);
    if (si + 1 > g_n_objs) g_n_objs = si + 1;
}

static void add_uv_sphere(const char *name, int seg) {
    int si = obj_alloc_slot();
    if (si < 0) return;
    if (seg < 8) seg = 8;
    if (seg > 32) seg = 32;
    int rings = seg / 2;
    int nv = (rings + 1) * (seg + 1);
    int ni = rings * seg * 6;
    Obj3 *o = &g_objs[si];
    memset(o, 0, sizeof(*o));
    o->used = 1;
    snprintf(o->name, sizeof(o->name), "%s", name);
    o->v = (float *)malloc((size_t)nv * 3 * sizeof(float));
    o->n = (float *)malloc((size_t)nv * 3 * sizeof(float));
    o->idx = (unsigned *)malloc((size_t)ni * sizeof(unsigned));
    int vi = 0;
    for (int y = 0; y <= rings; y++) {
        float v = (float)y / rings;
        float phi = v * (float)M_PI;
        for (int x = 0; x <= seg; x++) {
            float u = (float)x / seg;
            float th = u * 2.f * (float)M_PI;
            float px = sinf(phi) * cosf(th);
            float py = cosf(phi);
            float pz = sinf(phi) * sinf(th);
            o->v[vi*3]=px*0.5f; o->v[vi*3+1]=py*0.5f; o->v[vi*3+2]=pz*0.5f;
            o->n[vi*3]=px; o->n[vi*3+1]=py; o->n[vi*3+2]=pz;
            vi++;
        }
    }
    int ii = 0;
    for (int y = 0; y < rings; y++) {
        for (int x = 0; x < seg; x++) {
            int i0 = y * (seg + 1) + x;
            int i1 = i0 + seg + 1;
            o->idx[ii++]=i0; o->idx[ii++]=i1; o->idx[ii++]=i0+1;
            o->idx[ii++]=i0+1; o->idx[ii++]=i1; o->idx[ii++]=i1+1;
        }
    }
    o->nv = vi; o->ni = ii;
    v3_set(o->pos, 1.5f, 0.5f, 0);
    v3_set(o->rot, 0, 0, 0);
    v3_set(o->scl, 1, 1, 1);
    v3_set(o->col, 0.35f, 0.55f, 0.85f);
    recompute_aabb(o);
    if (si + 1 > g_n_objs) g_n_objs = si + 1;
    g_sel = si;
}

/* ---- Assimp import (OBJ / FBX / glTF / …) ---- */
static void ai_mesh_append(Obj3 *o, const struct aiMesh *m,
                           float **vb, float **nb, unsigned **ib,
                           int *nv, int *ni, int *vcap, int *icap) {
    if (!m || !m->mVertices) return;
    int base = *nv;
    int need_v = *nv + (int)m->mNumVertices;
    if (need_v > *vcap) {
        *vcap = need_v + 1024;
        *vb = (float *)realloc(*vb, (size_t)(*vcap) * 3 * sizeof(float));
        *nb = (float *)realloc(*nb, (size_t)(*vcap) * 3 * sizeof(float));
    }
    for (unsigned i = 0; i < m->mNumVertices; i++) {
        (*vb)[(*nv) * 3 + 0] = m->mVertices[i].x;
        (*vb)[(*nv) * 3 + 1] = m->mVertices[i].y;
        (*vb)[(*nv) * 3 + 2] = m->mVertices[i].z;
        if (m->mNormals) {
            (*nb)[(*nv) * 3 + 0] = m->mNormals[i].x;
            (*nb)[(*nv) * 3 + 1] = m->mNormals[i].y;
            (*nb)[(*nv) * 3 + 2] = m->mNormals[i].z;
        } else {
            (*nb)[(*nv) * 3 + 0] = 0;
            (*nb)[(*nv) * 3 + 1] = 1;
            (*nb)[(*nv) * 3 + 2] = 0;
        }
        (*nv)++;
        if (*nv >= MAX_V) break;
    }
    int need_i = *ni + (int)m->mNumFaces * 3;
    if (need_i > *icap) {
        *icap = need_i + 2048;
        *ib = (unsigned *)realloc(*ib, (size_t)(*icap) * sizeof(unsigned));
    }
    for (unsigned f = 0; f < m->mNumFaces && *ni + 3 <= MAX_I; f++) {
        const struct aiFace *face = &m->mFaces[f];
        if (face->mNumIndices < 3) continue;
        /* fan triangulation for n-gons */
        for (unsigned k = 1; k + 1 < face->mNumIndices && *ni + 3 <= MAX_I; k++) {
            (*ib)[(*ni)++] = base + face->mIndices[0];
            (*ib)[(*ni)++] = base + face->mIndices[k];
            (*ib)[(*ni)++] = base + face->mIndices[k + 1];
        }
    }
    (void)o;
}

static void ai_node_walk(const struct aiScene *sc, const struct aiNode *node,
                         float **vb, float **nb, unsigned **ib,
                         int *nv, int *ni, int *vcap, int *icap) {
    if (!node) return;
    for (unsigned i = 0; i < node->mNumMeshes && *nv < MAX_V; i++) {
        unsigned mi = node->mMeshes[i];
        if (mi < sc->mNumMeshes)
            ai_mesh_append(NULL, sc->mMeshes[mi], vb, nb, ib, nv, ni, vcap, icap);
    }
    for (unsigned c = 0; c < node->mNumChildren; c++)
        ai_node_walk(sc, node->mChildren[c], vb, nb, ib, nv, ni, vcap, icap);
}

static int import_mesh_path(const char *path) {
    if (!path || !path[0] || !media_path_is_readable_file(path)) {
        snprintf(g_status, sizeof(g_status), "Not a readable file");
        mark_ui_dirty();
        return 0;
    }
    int kind = media_kind_from_path(path);
    if (kind != 4 && kind != 0) {
        snprintf(g_status, sizeof(g_status), "Not a mesh (.obj/.fbx/…): %.80s", path);
        mark_ui_dirty();
        return 0;
    }
    unsigned flags =
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_FlipUVs;
    const struct aiScene *sc = aiImportFile(path, flags);
    if (!sc || !sc->mRootNode) {
        snprintf(g_status, sizeof(g_status), "Import failed: %s", aiGetErrorString());
        mark_ui_dirty();
        return 0;
    }
    int si = obj_alloc_slot();
    if (si < 0) {
        aiReleaseImport(sc);
        snprintf(g_status, sizeof(g_status), "Scene full (%d objects)", MAX_OBJS);
        mark_ui_dirty();
        return 0;
    }
    float *vb = NULL, *nb = NULL;
    unsigned *ib = NULL;
    int nv = 0, ni = 0, vcap = 0, icap = 0;
    ai_node_walk(sc, sc->mRootNode, &vb, &nb, &ib, &nv, &ni, &vcap, &icap);
    aiReleaseImport(sc);
    if (nv < 3 || ni < 3) {
        free(vb); free(nb); free(ib);
        snprintf(g_status, sizeof(g_status), "Empty mesh");
        mark_ui_dirty();
        return 0;
    }
    Obj3 *o = &g_objs[si];
    memset(o, 0, sizeof(*o));
    o->used = 1;
    o->v = vb; o->n = nb; o->idx = ib;
    o->nv = nv; o->ni = ni;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(o->name, sizeof(o->name), "%.44s", base);
    v3_set(o->pos, 0, 0.5f, 0);
    v3_set(o->rot, 0, 0, 0);
    v3_set(o->scl, 1, 1, 1);
    float hue = (si % 6) / 6.f;
    v3_set(o->col, 0.4f + 0.4f * hue, 0.55f, 0.75f - 0.3f * hue);
    normalize_mesh(o);
    if (si + 1 > g_n_objs) g_n_objs = si + 1;
    g_sel = si;
    snprintf(g_status, sizeof(g_status), "Imported %s (%d tris)", o->name, ni / 3);
    mark_ui_dirty();
    return 1;
}

static void demo_scene(void) {
    objs_clear();
    add_plane("Ground");
    add_cube("Cube", -1.2f, 0.5f, 0);
    add_uv_sphere("Sphere", 20);
    add_cube("Cube.001", 0, 0.5f, 1.5f);
    g_objs[g_sel].col[0] = 0.85f; g_objs[g_sel].col[1] = 0.35f; g_objs[g_sel].col[2] = 0.4f;
    snprintf(g_scene_name, sizeof(g_scene_name), "Demo");
    snprintf(g_status, sizeof(g_status), "Demo scene — drop .obj/.fbx · G/R/S · MMB orbit");
    g_cam_yaw = 40; g_cam_pitch = 22; g_cam_dist = 9;
    v3_set(g_cam_target, 0, 0.5f, 0);
    mark_ui_dirty();
}

static void new_scene(void) {
    objs_clear();
    add_cube("Cube", 0, 0.5f, 0);
    snprintf(g_scene_name, sizeof(g_scene_name), "Untitled");
    snprintf(g_status, sizeof(g_status), "New scene");
    mark_ui_dirty();
}

/* ---- camera / matrices ---- */
static void cam_eye(float *eye) {
    float yr = deg2rad(g_cam_yaw), pr = deg2rad(g_cam_pitch);
    eye[0] = g_cam_target[0] + g_cam_dist * cosf(pr) * sinf(yr);
    eye[1] = g_cam_target[1] + g_cam_dist * sinf(pr);
    eye[2] = g_cam_target[2] + g_cam_dist * cosf(pr) * cosf(yr);
}

static void apply_object_matrix(const Obj3 *o) {
    glTranslatef(o->pos[0], o->pos[1], o->pos[2]);
    glRotatef(o->rot[2], 0, 0, 1);
    glRotatef(o->rot[1], 0, 1, 0);
    glRotatef(o->rot[0], 1, 0, 0);
    glScalef(o->scl[0], o->scl[1], o->scl[2]);
}

static void draw_grid(void) {
    glDisable(GL_LIGHTING);
    glLineWidth(1);
    glBegin(GL_LINES);
    for (int i = -10; i <= 10; i++) {
        float a = (i == 0) ? 0.35f : 0.18f;
        glColor3f(a, a, a + 0.02f);
        glVertex3f((float)i, 0, -10); glVertex3f((float)i, 0, 10);
        glVertex3f(-10, 0, (float)i); glVertex3f(10, 0, (float)i);
    }
    /* axes */
    glColor3f(0.85f, 0.25f, 0.2f); glVertex3f(0,0,0); glVertex3f(2,0,0);
    glColor3f(0.3f, 0.8f, 0.3f);  glVertex3f(0,0,0); glVertex3f(0,2,0);
    glColor3f(0.3f, 0.45f, 0.95f); glVertex3f(0,0,0); glVertex3f(0,0,2);
    glEnd();
}

static void draw_obj(const Obj3 *o, int selected) {
    if (!o->used || !o->v || !o->idx) return;
    glPushMatrix();
    apply_object_matrix(o);
    if (g_wire || !g_shade) {
        glDisable(GL_LIGHTING);
        glColor3f(selected ? 1.f : o->col[0], selected ? 0.85f : o->col[1], selected ? 0.2f : o->col[2]);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glEnable(GL_LIGHTING);
        GLfloat diff[] = { o->col[0], o->col[1], o->col[2], 1.f };
        GLfloat amb[] = { o->col[0] * 0.25f, o->col[1] * 0.25f, o->col[2] * 0.25f, 1.f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diff);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < o->ni; i++) {
        unsigned vi = o->idx[i];
        if ((int)vi >= o->nv) continue;
        if (o->n) glNormal3fv(o->n + vi * 3);
        glVertex3fv(o->v + vi * 3);
    }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (selected) {
        glDisable(GL_LIGHTING);
        glColor3f(1.f, 0.9f, 0.15f);
        glLineWidth(2);
        /* simple AABB wire */
        const float *a = o->aabb_min, *b = o->aabb_max;
        glBegin(GL_LINE_LOOP);
        glVertex3f(a[0],a[1],a[2]); glVertex3f(b[0],a[1],a[2]);
        glVertex3f(b[0],b[1],a[2]); glVertex3f(a[0],b[1],a[2]);
        glEnd();
        glBegin(GL_LINE_LOOP);
        glVertex3f(a[0],a[1],b[2]); glVertex3f(b[0],a[1],b[2]);
        glVertex3f(b[0],b[1],b[2]); glVertex3f(a[0],b[1],b[2]);
        glEnd();
        glBegin(GL_LINES);
        glVertex3f(a[0],a[1],a[2]); glVertex3f(a[0],a[1],b[2]);
        glVertex3f(b[0],a[1],a[2]); glVertex3f(b[0],a[1],b[2]);
        glVertex3f(b[0],b[1],a[2]); glVertex3f(b[0],b[1],b[2]);
        glVertex3f(a[0],b[1],a[2]); glVertex3f(a[0],b[1],b[2]);
        glEnd();
        glLineWidth(1);
    }
    glPopMatrix();
}

/* crude pick: project AABB center, nearest in screen space */
static void world_to_screen(const float w[3], int *sx, int *sy) {
    GLdouble model[16], proj[16];
    GLint vp[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, vp);
    GLdouble wx, wy, wz;
    if (gluProject(w[0], w[1], w[2], model, proj, vp, &wx, &wy, &wz) == GL_TRUE) {
        *sx = (int)wx;
        *sy = vp[3] - (int)wy; /* to window top-left coords */
    } else {
        *sx = -99999; *sy = -99999;
    }
}

static void object_world_center(const Obj3 *o, float *out) {
    float lx = 0.5f * (o->aabb_min[0] + o->aabb_max[0]);
    float ly = 0.5f * (o->aabb_min[1] + o->aabb_max[1]);
    float lz = 0.5f * (o->aabb_min[2] + o->aabb_max[2]);
    /* approx: ignore rotation for pick center, apply scale+pos */
    out[0] = o->pos[0] + lx * o->scl[0];
    out[1] = o->pos[1] + ly * o->scl[1];
    out[2] = o->pos[2] + lz * o->scl[2];
}

static int pick_object(int mx, int my) {
    /* must be called with 3D matrices active — use last drawn state by
     * temporarily setting matrices same as viewport draw */
    int best = -1;
    float best_d = 40.f * 40.f;
    for (int i = 0; i < MAX_OBJS; i++) {
        if (!g_objs[i].used) continue;
        float c[3];
        object_world_center(&g_objs[i], c);
        int sx, sy;
        world_to_screen(c, &sx, &sy);
        float dx = (float)(sx - mx), dy = (float)(sy - my);
        float d = dx * dx + dy * dy;
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

/* ---- viewport geometry ---- */
static void viewport_rect(int *x, int *y, int *w, int *h) {
    int bar = chtpm_nav_bar_h();
    *x = TOOL_W;
    *y = MENU_H; /* content coords (below mock bar) */
    *w = WIN_W - TOOL_W - RIGHT_W;
    *h = WIN_H - MENU_H - STATUS_H - bar;
    if (*h < 80) *h = 80;
}

static void setup_3d_projection(void) {
    int vx, vy, vw, vh;
    int bar = chtpm_nav_bar_h();
    viewport_rect(&vx, &vy, &vw, &vh);
    /* OpenGL viewport origin bottom-left; shift by mock bar */
    glViewport(vx, WIN_H - (vy + bar + vh), vw, vh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (vw > 0 && vh > 0) ? (float)vw / (float)vh : 1.f;
    gluPerspective(g_fov, aspect, 0.05, 500.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float eye[3];
    cam_eye(eye);
    gluLookAt(eye[0], eye[1], eye[2],
              g_cam_target[0], g_cam_target[1], g_cam_target[2],
              0, 1, 0);
}

/* ---- 2D overlay UI ---- */
static void rect2(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
static void text2(float x, float y, const char *s) {
    glColor3f(0.93f, 0.94f, 0.96f);
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s++);
}
static void text2_dim(float x, float y, const char *s) {
    glColor3f(0.55f, 0.58f, 0.62f);
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s++);
}

static void setup_2d(void) {
    glViewport(0, 0, WIN_W, WIN_H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIN_W, WIN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
}

static void draw_menu(void) {
    rect2(0, 0, WIN_W, MENU_H, 0.20f, 0.21f, 0.23f, 1);
    rect2(4, 2, 48, MENU_H - 4, g_file_menu ? 0.32f : 0.22f, g_file_menu ? 0.36f : 0.23f, 0.28f, 1);
    text2(14, 16, "File");
    text2_dim(64, 16, "Add");
    text2_dim(110, 16, "Object");
    text2_dim(175, 16, "View");
    char pn[96];
    snprintf(pn, sizeof(pn), "  %s  —  Muchi Blend", g_scene_name);
    text2_dim(230, 16, pn);
    if (g_file_menu) {
        const char *items[] = {
            "New Scene", "Demo Scene", "Add Cube", "Add Sphere",
            "Export note", "Quit  Esc"
        };
        float mx = 4, my = MENU_H, mw = 160, mh = 22;
        rect2(mx, my, mw, mh * 6 + 4, 0.18f, 0.19f, 0.21f, 0.98f);
        for (int i = 0; i < 6; i++) {
            float iy = my + 2 + i * mh;
            rect2(mx + 2, iy, mw - 4, mh - 2, 0.24f, 0.26f, 0.30f, 1);
            text2(mx + 10, iy + 14, items[i]);
        }
    }
}

static void draw_tools(void) {
    rect2(0, MENU_H, TOOL_W, WIN_H - MENU_H - STATUS_H, 0.16f, 0.17f, 0.19f, 1);
    const char *lab[] = { "Sel", "G", "R", "S" };
    for (int i = 0; i < TOOL_COUNT; i++) {
        float iy = MENU_H + 8 + i * 44;
        int on = (g_tool == (Tool)i);
        rect2(6, iy, TOOL_W - 12, 38, on ? 0.30f : 0.20f, on ? 0.42f : 0.21f, on ? 0.55f : 0.24f, 1);
        text2(14, iy + 24, lab[i]);
    }
    text2_dim(6, MENU_H + 200, "Z wire");
    text2_dim(6, MENU_H + 216, "H hide");
}

static void draw_outliner(void) {
    float x = WIN_W - RIGHT_W;
    rect2(x, MENU_H, RIGHT_W, WIN_H - MENU_H - STATUS_H, 0.14f, 0.15f, 0.17f, 1);
    text2(x + 10, MENU_H + 18, "OUTLINER");
    text2_dim(x + 10, MENU_H + 36, "click · 1-9 select");
    int row = 0;
    for (int i = 0; i < MAX_OBJS; i++) {
        if (!g_objs[i].used) continue;
        float iy = MENU_H + 52 + row * 28;
        int on = (i == g_sel);
        rect2(x + 6, iy, RIGHT_W - 12, 24, on ? 0.32f : 0.20f, on ? 0.36f : 0.21f, on ? 0.45f : 0.24f, 1);
        char line[56];
        snprintf(line, sizeof(line), "%s %s", on ? "●" : "○", g_objs[i].name);
        text2(x + 12, iy + 16, line);
        row++;
    }
    if (g_sel >= 0 && g_objs[g_sel].used) {
        Obj3 *o = &g_objs[g_sel];
        float y = WIN_H - STATUS_H - 120;
        char b[64];
        text2(x + 10, y, "TRANSFORM");
        snprintf(b, sizeof(b), "Loc %.2f %.2f %.2f", o->pos[0], o->pos[1], o->pos[2]);
        text2_dim(x + 10, y + 18, b);
        snprintf(b, sizeof(b), "Rot %.0f %.0f %.0f", o->rot[0], o->rot[1], o->rot[2]);
        text2_dim(x + 10, y + 34, b);
        snprintf(b, sizeof(b), "Scl %.2f %.2f %.2f", o->scl[0], o->scl[1], o->scl[2]);
        text2_dim(x + 10, y + 50, b);
        snprintf(b, sizeof(b), "Tris %d", o->ni / 3);
        text2_dim(x + 10, y + 66, b);
    }
}

static void draw_status(void) {
    rect2(0, WIN_H - STATUS_H, WIN_W, STATUS_H, 0.12f, 0.13f, 0.15f, 1);
    char line[320];
    if (!g_status[0])
        snprintf(g_status, sizeof(g_status), "Blender-shaped MVP · drop .obj/.fbx");
    snprintf(line, sizeof(line), "%s  ·  %s  ·  UI %.0ffps  ·  MMB orbit  Shift+MMB pan",
             g_status, tool_names[g_tool], g_ui_fps);
    text2_dim(8, WIN_H - 8, line);
}

static void write_canvas_raw_throttled(void) {
    /* optional house hook: dump a tiny receipt only; full FBO readback is heavy —
     * write receipt so canvas-widgit path is known; skip huge RGBA every frame. */
    if (++g_canvas_tick < 30) return;
    g_canvas_tick = 0;
    char path[1200];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/canvas.receipt.txt", g_project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "width=%d\nheight=%d\nbytes_per_pixel=4\nmode=viewport3d\n", 640, 360);
        fclose(f);
    }
}

static void draw_ui(void) {
    double t = now_sec();
    int cap = (g_nav_active || g_xform) ? UI_FPS_ACTIVE : UI_FPS_IDLE;
    if (g_last_ui > 0 && (t - g_last_ui) < (1.0 / (double)cap))
        return;
    if (!g_nav_active && !g_xform && !g_ui_dirty && g_last_ui > 0 && (t - g_last_ui) < 0.4)
        return;
    g_last_ui = t;
    g_fps_n++;
    if (g_fps_t0 <= 0) g_fps_t0 = t;
    if (t - g_fps_t0 >= 1.0) {
        g_ui_fps = (float)g_fps_n / (float)(t - g_fps_t0);
        g_fps_n = 0; g_fps_t0 = t;
    }

    glClearColor(0.12f, 0.13f, 0.15f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* 3D viewport */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHT0);
    {
        GLfloat pos[] = { 4.f, 8.f, 5.f, 1.f };
        GLfloat amb[] = { 0.25f, 0.25f, 0.28f, 1.f };
        GLfloat dif[] = { 0.9f, 0.9f, 0.85f, 1.f };
        glLightfv(GL_LIGHT0, GL_POSITION, pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    }
    setup_3d_projection();
    draw_grid();
    for (int i = 0; i < MAX_OBJS; i++)
        if (g_objs[i].used) draw_obj(&g_objs[i], i == g_sel);

    /* 2D chrome — shifted below CHTPM mock methods bar */
    chtpm_nav_set_window(WIN_W, WIN_H);
    {
        int vx, vy, vw, vh;
        viewport_rect(&vx, &vy, &vw, &vh);
        chtpm_nav_begin();
        chtpm_nav_add("Methods/File", 0, 0, (float)WIN_W, (float)MENU_H, 0);
        chtpm_nav_add("ToolStrip", 0, (float)MENU_H, (float)TOOL_W,
                      (float)(WIN_H - MENU_H - STATUS_H), 1);
        chtpm_nav_add("Viewport3D", (float)vx, (float)vy, (float)vw, (float)vh, 2);
        chtpm_nav_add("Outliner", (float)(WIN_W - RIGHT_W), (float)MENU_H,
                      (float)RIGHT_W, (float)(WIN_H - MENU_H - STATUS_H), 3);
        chtpm_nav_add("Status", 0, (float)(WIN_H - STATUS_H), (float)WIN_W, (float)STATUS_H, 4);
    }
    setup_2d();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPushMatrix();
    glTranslatef(0, (float)chtpm_nav_bar_h(), 0);
    draw_tools();
    draw_outliner();
    draw_menu();
    draw_status();
    if (g_file_menu) draw_menu();
    chtpm_nav_draw();
    glPopMatrix();

    glutSwapBuffers();
    g_ui_dirty = 0;
    write_canvas_raw_throttled();
}

/* ---- XDND ---- */
static void xdnd_setup(void) {
    if (g_xdnd_setup) return;
    g_xdpy = glXGetCurrentDisplay();
    if (!g_xdpy) return;
    g_xwin = glXGetCurrentDrawable();
    if (!g_xwin) return;
    g_xa_XdndAware = XInternAtom(g_xdpy, "XdndAware", False);
    g_xa_XdndEnter = XInternAtom(g_xdpy, "XdndEnter", False);
    g_xa_XdndPosition = XInternAtom(g_xdpy, "XdndPosition", False);
    g_xa_XdndStatus = XInternAtom(g_xdpy, "XdndStatus", False);
    g_xa_XdndLeave = XInternAtom(g_xdpy, "XdndLeave", False);
    g_xa_XdndDrop = XInternAtom(g_xdpy, "XdndDrop", False);
    g_xa_XdndFinished = XInternAtom(g_xdpy, "XdndFinished", False);
    g_xa_XdndSelection = XInternAtom(g_xdpy, "XdndSelection", False);
    g_xa_XdndActionCopy = XInternAtom(g_xdpy, "XdndActionCopy", False);
    g_xa_text_uri_list = XInternAtom(g_xdpy, "text/uri-list", False);
    Atom ver = 5;
    XChangeProperty(g_xdpy, g_xwin, g_xa_XdndAware, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&ver, 1);
    g_xdnd_setup = 1;
}

static void xdnd_send_status(Window src, int accept) {
    XEvent ev; memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.display = g_xdpy;
    ev.xclient.window = src;
    ev.xclient.message_type = g_xa_XdndStatus;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (long)g_xwin;
    ev.xclient.data.l[1] = accept ? 1 : 0;
    ev.xclient.data.l[4] = (long)g_xa_XdndActionCopy;
    XSendEvent(g_xdpy, src, False, NoEventMask, &ev);
}

static void xdnd_send_finished(Window src) {
    XEvent ev; memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.display = g_xdpy;
    ev.xclient.window = src;
    ev.xclient.message_type = g_xa_XdndFinished;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (long)g_xwin;
    ev.xclient.data.l[1] = 1;
    ev.xclient.data.l[2] = (long)g_xa_XdndActionCopy;
    XSendEvent(g_xdpy, src, False, NoEventMask, &ev);
}

static void on_drop_path(const char *path, void *user) {
    (void)user;
    import_mesh_path(path);
}

static void xdnd_poll(void) {
    if (!g_xdnd_setup) { xdnd_setup(); if (!g_xdnd_setup) return; }
    XEvent ev;
    Atom wm_protocols = XInternAtom(g_xdpy, "WM_PROTOCOLS", False);
    Atom wm_delete = XInternAtom(g_xdpy, "WM_DELETE_WINDOW", False);
    while (XCheckTypedEvent(g_xdpy, ClientMessage, &ev)) {
        Atom t = ev.xclient.message_type;
        if (t == g_xa_XdndEnter) {
            g_xdnd_source = (Window)ev.xclient.data.l[0];
        } else if (t == g_xa_XdndPosition) {
            g_xdnd_source = (Window)ev.xclient.data.l[0];
            xdnd_send_status(g_xdnd_source, 1);
            snprintf(g_status, sizeof(g_status), "Drop .obj / .fbx to import");
            mark_ui_dirty();
        } else if (t == g_xa_XdndLeave) {
            g_xdnd_source = 0;
        } else if (t == g_xa_XdndDrop) {
            g_xdnd_source = (Window)ev.xclient.data.l[0];
            Atom prop = XInternAtom(g_xdpy, "BE_DROP_PROP", False);
            XConvertSelection(g_xdpy, g_xa_XdndSelection, g_xa_text_uri_list,
                              prop, g_xwin, CurrentTime);
        } else if (t == wm_protocols && (Atom)ev.xclient.data.l[0] == wm_delete) {
            g_quit = 1;
        } else {
            XPutBackEvent(g_xdpy, &ev);
            break;
        }
    }
    while (XCheckTypedEvent(g_xdpy, SelectionNotify, &ev)) {
        if (ev.xselection.property != None) {
            Atom actual_type; int actual_format;
            unsigned long nitems, bytes_after;
            unsigned char *data = NULL;
            if (XGetWindowProperty(g_xdpy, g_xwin, ev.xselection.property, 0, 65536, True,
                                   AnyPropertyType, &actual_type, &actual_format,
                                   &nitems, &bytes_after, &data) == Success && data) {
                char st[256];
                media_import_uri_list((char *)data, g_project_root, on_drop_path, NULL, st, sizeof(st));
                if (st[0]) snprintf(g_status, sizeof(g_status), "%s", st);
                XFree(data);
            }
        }
        if (g_xdnd_source) xdnd_send_finished(g_xdnd_source);
        g_xdnd_source = 0;
        mark_ui_dirty();
    }
}

/* ---- input ---- */
static void file_action(int item) {
    if (item == 0) new_scene();
    else if (item == 1) demo_scene();
    else if (item == 2) { add_cube("Cube", 0, 0.5f, 0); snprintf(g_status, sizeof(g_status), "Added Cube"); mark_ui_dirty(); }
    else if (item == 3) { add_uv_sphere("Sphere", 20); snprintf(g_status, sizeof(g_status), "Added Sphere"); mark_ui_dirty(); }
    else if (item == 4) {
        snprintf(g_status, sizeof(g_status), "Export: drop-import is primary; OBJ write later");
        mark_ui_dirty();
    } else if (item == 5) g_quit = 1;
    g_file_menu = 0;
}

static void begin_xform(Tool t) {
    if (g_sel < 0 || !g_objs[g_sel].used) {
        snprintf(g_status, sizeof(g_status), "Nothing selected");
        mark_ui_dirty();
        return;
    }
    g_tool = t;
    g_xform = 1;
    g_xform_axis = -1;
    Obj3 *o = &g_objs[g_sel];
    if (t == TOOL_GRAB) memcpy(g_xform_start, o->pos, sizeof(g_xform_start));
    else if (t == TOOL_ROTATE) memcpy(g_xform_start, o->rot, sizeof(g_xform_start));
    else if (t == TOOL_SCALE) memcpy(g_xform_start, o->scl, sizeof(g_xform_start));
    snprintf(g_status, sizeof(g_status), "%s — drag mouse, X/Y/Z constrain, LMB confirm, RMB/Esc cancel",
             tool_names[t]);
    mark_ui_dirty();
}

static void confirm_xform(void) {
    g_xform = 0;
    snprintf(g_status, sizeof(g_status), "Transform confirmed");
    mark_ui_dirty();
}

static void cancel_xform(void) {
    if (!g_xform || g_sel < 0) { g_xform = 0; return; }
    Obj3 *o = &g_objs[g_sel];
    if (g_tool == TOOL_GRAB) memcpy(o->pos, g_xform_start, sizeof(o->pos));
    else if (g_tool == TOOL_ROTATE) memcpy(o->rot, g_xform_start, sizeof(o->rot));
    else if (g_tool == TOOL_SCALE) memcpy(o->scl, g_xform_start, sizeof(o->scl));
    g_xform = 0;
    snprintf(g_status, sizeof(g_status), "Transform cancelled");
    mark_ui_dirty();
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    if (key == 27 || key == 3) {
        if (g_xform) { cancel_xform(); return; }
        g_quit = 1;
        return;
    }
    /* CHTPM mock Tab/` — do not steal G/R/S while not in digit mode */
    if (!g_xform) {
        int sh = glutGetModifiers() & GLUT_ACTIVE_SHIFT;
        if (chtpm_nav_on_key(key, sh)) {
            char m[160];
            chtpm_nav_status(m, sizeof(m));
            snprintf(g_status, sizeof(g_status), "%s", m);
            mark_ui_dirty();
            return;
        }
    }
    if (g_xform) {
        if (key == 'x' || key == 'X') { g_xform_axis = 0; snprintf(g_status, sizeof(g_status), "Constrain X"); mark_ui_dirty(); return; }
        if (key == 'y' || key == 'Y') { g_xform_axis = 1; snprintf(g_status, sizeof(g_status), "Constrain Y"); mark_ui_dirty(); return; }
        if (key == 'z' || key == 'Z') { g_xform_axis = 2; snprintf(g_status, sizeof(g_status), "Constrain Z"); mark_ui_dirty(); return; }
        if (key == 13) { confirm_xform(); return; }
    }
    if (key == 'g' || key == 'G') { begin_xform(TOOL_GRAB); return; }
    if (key == 'r' || key == 'R') { begin_xform(TOOL_ROTATE); return; }
    if (key == 's' || key == 'S') { begin_xform(TOOL_SCALE); return; }
    if (key == 'a' || key == 'A') {
        /* select first / cycle */
        int next = -1;
        for (int i = 0; i < MAX_OBJS; i++)
            if (g_objs[i].used) { next = i; break; }
        g_sel = next;
        mark_ui_dirty();
        return;
    }
    if (key == 'z' || key == 'Z') {
        g_wire = !g_wire;
        snprintf(g_status, sizeof(g_status), g_wire ? "Wireframe" : "Solid");
        mark_ui_dirty();
        return;
    }
    if (key == 'x' || key == 'X') {
        if (g_sel >= 0 && g_objs[g_sel].used) {
            obj_free(&g_objs[g_sel]);
            snprintf(g_status, sizeof(g_status), "Deleted object");
            g_sel = -1;
            mark_ui_dirty();
        }
        return;
    }
    if (key == 'n' || key == 'N') { new_scene(); return; }
    if (key == 'd' || key == 'D') { demo_scene(); return; }
    if (key == 'c' || key == 'C') { add_cube("Cube", 0, 0.5f, 0); mark_ui_dirty(); return; }
    if (key == 'u' || key == 'U') { add_uv_sphere("Sphere", 20); mark_ui_dirty(); return; }
    if (key == '.' ) {
        if (g_sel >= 0 && g_objs[g_sel].used) {
            object_world_center(&g_objs[g_sel], g_cam_target);
            snprintf(g_status, sizeof(g_status), "Frame selected");
            mark_ui_dirty();
        }
        return;
    }
    if (key == '1') { g_cam_yaw = 0; g_cam_pitch = 0; mark_ui_dirty(); }
    if (key == '3') { g_cam_yaw = 90; g_cam_pitch = 0; mark_ui_dirty(); }
    if (key == '7') { g_cam_yaw = 0; g_cam_pitch = 89; mark_ui_dirty(); }
    if (key >= '1' && key <= '9') {
        int want = key - '1';
        int n = 0;
        for (int i = 0; i < MAX_OBJS; i++) {
            if (!g_objs[i].used) continue;
            if (n == want) { g_sel = i; mark_ui_dirty(); break; }
            n++;
        }
    }
    if (key == '+' || key == '=') { g_cam_dist = clampf(g_cam_dist * 0.9f, 0.5f, 200.f); mark_ui_dirty(); }
    if (key == '-' || key == '_') { g_cam_dist = clampf(g_cam_dist * 1.1f, 0.5f, 200.f); mark_ui_dirty(); }
}

static void special(int key, int x, int y) {
    (void)x; (void)y;
    float step = 0.25f;
    if (g_sel >= 0 && g_objs[g_sel].used && !g_xform) {
        Obj3 *o = &g_objs[g_sel];
        if (key == GLUT_KEY_LEFT) o->pos[0] -= step;
        else if (key == GLUT_KEY_RIGHT) o->pos[0] += step;
        else if (key == GLUT_KEY_UP) o->pos[2] -= step;
        else if (key == GLUT_KEY_DOWN) o->pos[2] += step;
        else if (key == GLUT_KEY_PAGE_UP) o->pos[1] += step;
        else if (key == GLUT_KEY_PAGE_DOWN) o->pos[1] -= step;
        mark_ui_dirty();
    }
}

static int in_viewport(int mx, int my) {
    int vx, vy, vw, vh;
    viewport_rect(&vx, &vy, &vw, &vh);
    return mx >= vx && mx < vx + vw && my >= vy && my < vy + vh;
}

static void mouse(int button, int state, int mx, int my) {
    my = chtpm_nav_mouse_y(my);
    int mods = glutGetModifiers();
    if (button == 3 && state == GLUT_DOWN) {
        g_cam_dist = clampf(g_cam_dist * 0.9f, 0.5f, 200.f);
        mark_ui_dirty();
        return;
    }
    if (button == 4 && state == GLUT_DOWN) {
        g_cam_dist = clampf(g_cam_dist * 1.1f, 0.5f, 200.f);
        mark_ui_dirty();
        return;
    }

    if (state == GLUT_UP) {
        if (button == GLUT_MIDDLE_BUTTON) {
            g_orbiting = g_panning = 0;
            g_nav_active = 0;
        }
        if (button == GLUT_LEFT_BUTTON && g_xform) {
            confirm_xform();
        }
        mark_ui_dirty();
        return;
    }

    g_last_mx = mx; g_last_my = my;

    if (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN) {
        if (mods & GLUT_ACTIVE_SHIFT) g_panning = 1;
        else g_orbiting = 1;
        g_nav_active = 1;
        return;
    }

    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

    if (g_xform) { confirm_xform(); return; }

    if (my < MENU_H) {
        if (mx >= 4 && mx < 52) { g_file_menu = !g_file_menu; mark_ui_dirty(); return; }
        g_file_menu = 0;
        mark_ui_dirty();
        return;
    }
    if (g_file_menu) {
        if (mx >= 4 && mx < 164 && my >= MENU_H && my < MENU_H + 22 * 6 + 4) {
            int item = (my - MENU_H - 2) / 22;
            if (item >= 0 && item < 6) file_action(item);
            return;
        }
        g_file_menu = 0;
    }

    if (mx < TOOL_W) {
        int idx = (my - MENU_H - 8) / 44;
        if (idx >= 0 && idx < TOOL_COUNT) {
            if (idx == TOOL_SELECT) g_tool = TOOL_SELECT;
            else begin_xform((Tool)idx);
            mark_ui_dirty();
        }
        return;
    }

    if (mx >= WIN_W - RIGHT_W) {
        int row = (my - (MENU_H + 52)) / 28;
        int n = 0;
        for (int i = 0; i < MAX_OBJS; i++) {
            if (!g_objs[i].used) continue;
            if (n == row) {
                g_sel = i;
                snprintf(g_status, sizeof(g_status), "Selected %s", g_objs[i].name);
                mark_ui_dirty();
                return;
            }
            n++;
        }
        return;
    }

    if (in_viewport(mx, my)) {
        /* set 3D matrices for pick */
        glEnable(GL_DEPTH_TEST);
        setup_3d_projection();
        int hit = pick_object(mx, my);
        if (hit >= 0) {
            g_sel = hit;
            snprintf(g_status, sizeof(g_status), "Selected %s", g_objs[hit].name);
            if (g_tool == TOOL_GRAB || g_tool == TOOL_ROTATE || g_tool == TOOL_SCALE)
                begin_xform(g_tool);
        } else {
            g_sel = -1;
            snprintf(g_status, sizeof(g_status), "Nothing under cursor");
        }
        mark_ui_dirty();
    }
}

static void motion(int mx, int my) {
    my = chtpm_nav_mouse_y(my);
    int dx = mx - g_last_mx;
    int dy = my - g_last_my;
    g_last_mx = mx; g_last_my = my;

    if (g_orbiting) {
        g_cam_yaw += dx * 0.35f;
        g_cam_pitch = clampf(g_cam_pitch + dy * 0.35f, -89.f, 89.f);
        mark_ui_dirty();
        return;
    }
    if (g_panning) {
        float eye[3]; cam_eye(eye);
        float right[3] = { eye[2] - g_cam_target[2], 0, g_cam_target[0] - eye[0] };
        float len = sqrtf(right[0]*right[0]+right[2]*right[2]);
        if (len > 1e-5f) { right[0]/=len; right[2]/=len; }
        float scale = g_cam_dist * 0.002f;
        g_cam_target[0] -= right[0] * dx * scale;
        g_cam_target[2] -= right[2] * dx * scale;
        g_cam_target[1] += dy * scale;
        mark_ui_dirty();
        return;
    }
    if (g_xform && g_sel >= 0 && g_objs[g_sel].used) {
        Obj3 *o = &g_objs[g_sel];
        float m = 0.01f * g_cam_dist;
        if (g_tool == TOOL_GRAB) {
            if (g_xform_axis == 0) o->pos[0] += dx * m;
            else if (g_xform_axis == 1) o->pos[1] += -dy * m;
            else if (g_xform_axis == 2) o->pos[2] += dx * m;
            else {
                o->pos[0] += dx * m;
                o->pos[1] += -dy * m;
            }
        } else if (g_tool == TOOL_ROTATE) {
            float deg = dx * 0.5f;
            if (g_xform_axis == 0) o->rot[0] += deg;
            else if (g_xform_axis == 2) o->rot[2] += deg;
            else o->rot[1] += deg; /* default Y */
        } else if (g_tool == TOOL_SCALE) {
            float f = 1.f + dx * 0.01f;
            if (f < 0.05f) f = 0.05f;
            if (g_xform_axis < 0) {
                o->scl[0] *= f; o->scl[1] *= f; o->scl[2] *= f;
            } else {
                o->scl[g_xform_axis] *= f;
            }
        }
        mark_ui_dirty();
    }
}

static void timer_cb(int v) {
    (void)v;
    if (g_quit) {
        objs_clear();
        exit(0);
    }
    xdnd_poll();
    if (g_ui_dirty || g_nav_active || g_xform)
        glutPostRedisplay();
    glutTimerFunc((g_nav_active || g_xform) ? UI_TIMER_MS : UI_TIMER_IDLE, timer_cb, 0);
}

static void reshape(int w, int h) {
    if (w < 900) w = 900;
    if (h < 560) h = 560;
    g_win_w = w; g_win_h = h;
    mark_ui_dirty();
}

static void on_sig(int s) { (void)s; g_quit = 1; }
static void on_window_close(void) { g_quit = 1; }

int main(int argc, char **argv) {
    if (argc > 1) snprintf(g_project_root, sizeof(g_project_root), "%s", argv[1]);
    else {
        const char *e = getenv("PRISC_PROJECT_ROOT");
        if (e && e[0]) snprintf(g_project_root, sizeof(g_project_root), "%s", e);
    }
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);

    demo_scene();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(1400, 860);
    glutCreateWindow("Muchi Blend — drop .obj / .fbx · G/R/S · MMB orbit");
    g_glut_ready = 1;
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
    glutCloseFunc(on_window_close);
    glutDisplayFunc(draw_ui);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutIgnoreKeyRepeat(1);
    glutTimerFunc(UI_TIMER_IDLE, timer_cb, 0);
    mark_ui_dirty();

    while (!g_quit) {
        xdnd_poll();
        glutMainLoopEvent();
        usleep((g_nav_active || g_xform) ? SLEEP_ACTIVE_US : SLEEP_IDLE_US);
    }
    objs_clear();
    return 0;
}
