#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "../../libraries/stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>

/*
 * emoji-studio_host.c - TPM Host
 * Specialized GL renderer for emoji-studio.
 * Renders an emoji selection grid and a 3D voxel preview.
 */

typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

typedef struct {
    char name[256];
    int index;
} EmojiEntry;

EmojiEntry emoji_list[100]; // Limited for UI
int emoji_count = 0;
int resolution = 8;
char active_piece[256] = "";
RGBA_Pixel* voxel_data = NULL;
int current_res = 0;

GLuint atlas_tex = 0;
int atlas_w, atlas_h;

float cam_rot_x = 20.0f;
float cam_rot_y = 45.0f;
float zoom = -5.0f;

void load_gui_state() {
    FILE *f = fopen("projects/emoji-studio/manager/gui_state.txt", "r");
    if (!f) return;
    char line[1024];
    int in_list = 0;
    emoji_count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "active_piece=", 13) == 0) {
            char *val = line + 13;
            val[strcspn(val, "\n\r")] = 0;
            if (strcmp(active_piece, val) != 0) {
                strncpy(active_piece, val, 255);
                current_res = 0; // Force reload
            }
        } else if (strncmp(line, "resolution=", 11) == 0) {
            resolution = atoi(line + 11);
        } else if (strstr(line, "emoji_list_start")) {
            in_list = 1;
        } else if (strstr(line, "emoji_list_end")) {
            in_list = 0;
        } else if (in_list && emoji_count < 100) {
            char *p = line;
            char *idx_str = strsep(&p, ":");
            char *utf8_str = strsep(&p, ":");
            char *name_str = strsep(&p, ":");
            if (idx_str && utf8_str && name_str) {
                emoji_list[emoji_count].index = atoi(idx_str);
                // Note: emoji_list struct might need 'str' field added,
                // but for now keeping it compatible with existing struct definition.
                strncpy(emoji_list[emoji_count].name, name_str, 255);
                emoji_list[emoji_count].name[strcspn(emoji_list[emoji_count].name, "\n\r")] = 0;
                emoji_count++;
            }
        }
    }
    fclose(f);
}

void load_voxels() {
    if (active_piece[0] == '\0') return;
    if (current_res == resolution) return;

    char path[1024];
    snprintf(path, sizeof(path), "projects/emoji-studio/pieces/%s/voxels.csv", active_piece);
    FILE *f = fopen(path, "r");
    if (!f) return;

    if (voxel_data) free(voxel_data);
    voxel_data = malloc(resolution * resolution * sizeof(RGBA_Pixel));

    char line[1024];
    int idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || strstr(line, "r,g,b,a")) continue;
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            if (idx < resolution * resolution) {
                voxel_data[idx].r = r;
                voxel_data[idx].g = g;
                voxel_data[idx].b = b;
                voxel_data[idx].a = a;
                idx++;
            }
        }
    }
    fclose(f);
    current_res = resolution;
}

void init_atlas() {
    int channels;
    unsigned char* data = stbi_load("../#.emoji.xtract.stb]c4/emoji_atlas.png", &atlas_w, &atlas_h, &channels, 4);
    if (!data) return;

    glGenTextures(1, &atlas_tex);
    glBindTexture(GL_TEXTURE_2D, atlas_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_w, atlas_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
}

void draw_cube(float size, RGBA_Pixel p) {
    float s = size / 2.0f;
    glColor4ub(p.r, p.g, p.b, p.a);
    glBegin(GL_QUADS);
    // Front
    glVertex3f(-s, -s,  s); glVertex3f( s, -s,  s); glVertex3f( s,  s,  s); glVertex3f(-s,  s,  s);
    // Back
    glVertex3f(-s, -s, -s); glVertex3f(-s,  s, -s); glVertex3f( s,  s, -s); glVertex3f( s, -s, -s);
    // Top
    glVertex3f(-s,  s, -s); glVertex3f(-s,  s,  s); glVertex3f( s,  s,  s); glVertex3f( s,  s, -s);
    // Bottom
    glVertex3f(-s, -s, -s); glVertex3f( s, -s, -s); glVertex3f( s, -s,  s); glVertex3f(-s, -s,  s);
    // Right
    glVertex3f( s, -s, -s); glVertex3f( s,  s, -s); glVertex3f( s,  s,  s); glVertex3f( s, -s,  s);
    // Left
    glVertex3f(-s, -s, -s); glVertex3f(-s, -s,  s); glVertex3f(-s,  s,  s); glVertex3f(-s,  s, -s);
    glEnd();
}

void display() {
    load_gui_state();
    load_voxels();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* 3D Viewport */
    glViewport(200, 0, 600, 600);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 1.0, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, zoom);
    glRotatef(cam_rot_x, 1, 0, 0);
    glRotatef(cam_rot_y, 0, 1, 0);

    // Draw Floor Grid
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINES);
    for(float i = -2; i <= 2; i += 0.5f) {
        glVertex3f(i, 0, -2); glVertex3f(i, 0, 2);
        glVertex3f(-2, 0, i); glVertex3f(2, 0, i);
    }
    glEnd();

    if (voxel_data) {
        float total_size = 2.0f;
        float v_size = total_size / resolution;
        float offset = -total_size / 2.0f;
        
        for (int y = 0; y < resolution; y++) {
            for (int x = 0; x < resolution; x++) {
                RGBA_Pixel p = voxel_data[y * resolution + x];
                if (p.a > 50) {
                    glPushMatrix();
                    glTranslatef(offset + x * v_size + v_size/2, 0, offset + y * v_size + v_size/2);
                    // Single stretched cube for extrusion
                    glScalef(1, resolution, 1);
                    draw_cube(v_size, p);
                    glPopMatrix();
                }
            }
        }
    }

    /* 2D Overlay (UI) */
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, 800, 600);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 800, 0, 600, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Side Bar
    glColor3f(0.1f, 0.1f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0); glVertex2f(200, 0); glVertex2f(200, 600); glVertex2f(0, 600);
    glEnd();

    // Emoji Grid
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, atlas_tex);
    for (int i = 0; i < emoji_count; i++) {
        int row = i / 4;
        int col = i % 4;
        float x = 10 + col * 45;
        float y = 550 - row * 45;
        
        float u_min = (float)emoji_list[i].index * 64.0f / atlas_w;
        float u_max = (float)(emoji_list[i].index + 1) * 64.0f / atlas_w;
        
        glColor3f(1, 1, 1);
        glBegin(GL_QUADS);
        glTexCoord2f(u_min, 1); glVertex2f(x, y);
        glTexCoord2f(u_max, 1); glVertex2f(x + 40, y);
        glTexCoord2f(u_max, 0); glVertex2f(x + 40, y + 40);
        glTexCoord2f(u_min, 0); glVertex2f(x, y + 40);
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);

    // Resolution Buttons
    glColor3f(0.2f, 0.2f, 0.3f);
    int res_opts[] = {8, 16, 32, 64};
    for (int i = 0; i < 4; i++) {
        float x = 10 + i * 45;
        float y = 50;
        if (resolution == res_opts[i]) glColor3f(0.4f, 0.4f, 0.8f);
        else glColor3f(0.2f, 0.2f, 0.3f);
        glBegin(GL_QUADS);
        glVertex2f(x, y); glVertex2f(x + 40, y); glVertex2f(x + 40, y + 30); glVertex2f(x, y + 30);
        glEnd();
        
        glColor3f(1, 1, 1);
        glRasterPos2f(x + 10, y + 10);
        char buf[8]; snprintf(buf, 8, "%d", res_opts[i]);
        for (char *c = buf; *c; c++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }

    glutSwapBuffers();
}

void mouse(int button, int state, int x, int y) {
    if (state == GLUT_DOWN) {
        float gl_y = 600 - y;
        if (x < 200) {
            // Check emoji grid
            if (gl_y > 100) {
                int col = (x - 10) / 45;
                int row = (590 - gl_y) / 45;
                int idx = row * 4 + col;
                if (idx >= 0 && idx < emoji_count) {
                    FILE *hf = fopen("projects/emoji-studio/session/history.txt", "a");
                    if (hf) {
                        fprintf(hf, "COMMAND: SELECT %d\n", emoji_list[idx].index);
                        fclose(hf);
                    }
                }
            } else if (gl_y > 40 && gl_y < 90) {
                // Check resolution buttons
                int i = (x - 10) / 45;
                if (i >= 0 && i < 4) {
                    int res_opts[] = {8, 16, 32, 64};
                    FILE *hf = fopen("projects/emoji-studio/session/history.txt", "a");
                    if (hf) {
                        fprintf(hf, "COMMAND: SET_RES %d\n", res_opts[i]);
                        fclose(hf);
                    }
                }
            }
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 'w') zoom += 0.5f;
    if (key == 's') zoom -= 0.5f;
    if (key == 'a') cam_rot_y -= 5.0f;
    if (key == 'd') cam_rot_y += 5.0f;
    if (key == 'q') cam_rot_x -= 5.0f;
    if (key == 'e') cam_rot_x += 5.0f;
    glutPostRedisplay();
}

void idle() {
    struct stat st;
    static long last_st_size = 0;
    if (stat("projects/emoji-studio/session/frame_changed.txt", &st) == 0) {
        if (st.st_size != last_st_size) {
            last_st_size = st.st_size;
            glutPostRedisplay();
        }
    }
    usleep(16667); // 60 FPS
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Emoji Studio Host");

    init_atlas();
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(idle);

    glutMainLoop();
    return 0;
}
