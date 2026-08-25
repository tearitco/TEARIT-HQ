#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define MAX_EMOJIS 4000
#define PICKER_COLS 8
#define PICKER_ROWS 10
#define CELL_SIZE 40

typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

typedef struct {
    char codepoint[32];
    char symbol[16];
    char name[256];
    int index;
} EmojiEntry;

// Globals
EmojiEntry emoji_list[MAX_EMOJIS];
int emoji_count = 0;
int selected_emoji_idx = -1;
int resolution = 8;

RGBA_Pixel* voxel_data = NULL;
int current_res = 0;
char active_piece_name[256] = "";

// FreeType
FT_Library ft;
FT_Face face;

// Camera
float cam_rot_x = 20.0f;
float cam_rot_y = 45.0f;
float zoom = -5.0f;

// Textures
GLuint emoji_tex_cache[PICKER_COLS * PICKER_ROWS];
int tex_cache_indices[PICKER_COLS * PICKER_ROWS];
int picker_offset = 0;

void load_emoji_list() {
    FILE* f = fopen("parsed_emojis.txt", "r");
    if (!f) {
        perror("Failed to open parsed_emojis.txt");
        return;
    }
    char line[1024];
    int idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        char* cp = strtok(line, "|");
        char* sym = strtok(NULL, "|");
        char* name = strtok(NULL, "\n");
        if (cp && sym && name) {
            strncpy(emoji_list[emoji_count].codepoint, cp, 31);
            strncpy(emoji_list[emoji_count].symbol, sym, 15);
            strncpy(emoji_list[emoji_count].name, name, 255);
            emoji_list[emoji_count].index = idx++;
            emoji_count++;
            if (emoji_count >= MAX_EMOJIS) break;
        }
    }
    fclose(f);
    printf("Loaded %d emojis\n", emoji_count);
}

void init_freetype() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init freetype library\n");
        exit(1);
    }
    if (FT_New_Face(ft, "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf", 0, &face)) {
        fprintf(stderr, "Could not open font /usr/share/fonts/truetype/noto/NotoColorEmoji.ttf\n");
        exit(1);
    }
    
    if (face->num_fixed_sizes > 0) {
        FT_Select_Size(face, 0); 
    } else {
        FT_Set_Pixel_Sizes(face, 0, 64);
    }
}

GLuint get_emoji_texture(int emoji_idx, int cache_slot) {
    if (tex_cache_indices[cache_slot] == emoji_idx && emoji_tex_cache[cache_slot] != 0) {
        return emoji_tex_cache[cache_slot];
    }

    unsigned long codepoint = strtoul(emoji_list[emoji_idx].codepoint, NULL, 16);
    FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_COLOR)) {
        return 0;
    }

    FT_Bitmap* bitmap = &face->glyph->bitmap;
    if (emoji_tex_cache[cache_slot] == 0) {
        glGenTextures(1, &emoji_tex_cache[cache_slot]);
    }
    glBindTexture(GL_TEXTURE_2D, emoji_tex_cache[cache_slot]);
    
    // NotoColorEmoji bitmaps are BGRA
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width, bitmap->rows, 0, 
                 GL_BGRA, GL_UNSIGNED_BYTE, bitmap->buffer);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    tex_cache_indices[cache_slot] = emoji_idx;
    return emoji_tex_cache[cache_slot];
}

void generate_voxel_csv(int emoji_index, int res) {
    char res_str[16], idx_str[16];
    snprintf(res_str, sizeof(res_str), "%d", res);
    snprintf(idx_str, sizeof(idx_str), "%d", 0); // Always 0 for mini-atlas
    
    char dir_path[1024];
    snprintf(dir_path, sizeof(dir_path), "pieces/%s", emoji_list[emoji_index].name);
    mkdir("pieces", 0777);
    mkdir(dir_path, 0777);
    
    char png_path[1024];
    snprintf(png_path, sizeof(png_path), "pieces/%s/mini_atlas.png", emoji_list[emoji_index].name);
    
    char csv_path[1024];
    snprintf(csv_path, sizeof(csv_path), "pieces/%s/voxels_%d.csv", emoji_list[emoji_index].name, res);
    
    struct stat st;
    if (stat(csv_path, &st) == 0) return; // Already exists

    printf("Step 1: Generating mini-atlas for %s...\n", emoji_list[emoji_index].name);
    pid_t pid = fork();
    if (pid == 0) {
        execl("./emoji-gen-atlas", "./emoji-gen-atlas", emoji_list[emoji_index].symbol, png_path, NULL);
        perror("execl emoji-gen-atlas failed");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
    }

    printf("Step 2: Extracting voxels from mini-atlas at %d...\n", res);
    pid = fork();
    if (pid == 0) {
        execl("./emoji-xtract", "./emoji-xtract", png_path, "0", res_str, csv_path, NULL);
        perror("execl emoji-xtract failed");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
    }
}

void load_voxels() {
    if (selected_emoji_idx == -1) return;
    if (current_res == resolution && strcmp(active_piece_name, emoji_list[selected_emoji_idx].name) == 0) return;

    char csv_path[1024];
    snprintf(csv_path, sizeof(csv_path), "pieces/%s/voxels_%d.csv", emoji_list[selected_emoji_idx].name, resolution);
    
    FILE* f = fopen(csv_path, "r");
    if (!f) {
        generate_voxel_csv(selected_emoji_idx, resolution);
        f = fopen(csv_path, "r");
    }
    if (!f) return;

    if (voxel_data) free(voxel_data);
    voxel_data = malloc(resolution * resolution * sizeof(RGBA_Pixel));

    char line[1024];
    int idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') {
            // Future: Parse scale, transform here if needed
            continue;
        }
        if (strstr(line, "r,g,b,a")) continue;
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
    strncpy(active_piece_name, emoji_list[selected_emoji_idx].name, 255);
}

void draw_cube(float x, float y, float z, float sx, float sy, float sz, RGBA_Pixel p) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);
    glColor4ub(p.r, p.g, p.b, p.a);
    
    float s = 0.5f;
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
    glPopMatrix();
}

void render_picker() {
    glViewport(0, 0, 320, 600);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 320, 0, 600, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(0.1f, 0.1f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0); glVertex2f(320, 0); glVertex2f(320, 600); glVertex2f(0, 600);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < PICKER_ROWS * PICKER_COLS; i++) {
        int emoji_idx = picker_offset + i;
        if (emoji_idx >= emoji_count) break;

        int r = i / PICKER_COLS;
        int c = i % PICKER_COLS;
        float x = 10 + c * CELL_SIZE;
        float y = 550 - r * CELL_SIZE;

        if (emoji_idx == selected_emoji_idx) {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.3f, 0.3f, 0.5f);
            glBegin(GL_QUADS);
            glVertex2f(x, y); glVertex2f(x + CELL_SIZE, y); 
            glVertex2f(x + CELL_SIZE, y + CELL_SIZE); glVertex2f(x, y + CELL_SIZE);
            glEnd();
            glEnable(GL_TEXTURE_2D);
        }

        GLuint tex = get_emoji_texture(emoji_idx, i);
        if (tex) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glColor3f(1, 1, 1);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex2f(x, y);
            glTexCoord2f(1, 1); glVertex2f(x + CELL_SIZE, y);
            glTexCoord2f(1, 0); glVertex2f(x + CELL_SIZE, y + CELL_SIZE);
            glTexCoord2f(0, 0); glVertex2f(x, y + CELL_SIZE);
            glEnd();
        }
    }
    glDisable(GL_TEXTURE_2D);
}

void display() {
    load_voxels();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 3D View
    glViewport(320, 0, 480, 600);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 480.0/600.0, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, zoom);
    glRotatef(cam_rot_x, 1, 0, 0);
    glRotatef(cam_rot_y, 0, 1, 0);

    // Floor
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_LINES);
    for(float i=-2; i<=2; i+=0.5f) {
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
                if (p.a > 10) {
                    // Extrude: Render a column
                    float h = total_size;
                    draw_cube(offset + x*v_size + v_size/2, h/2.0f, offset + y*v_size + v_size/2, 
                              v_size, h, v_size, p);
                }
            }
        }
    }

    // 2D Picker
    glDisable(GL_DEPTH_TEST);
    render_picker();

    glutSwapBuffers();
}

void mouse(int button, int state, int x, int y) {
    if (state == GLUT_DOWN) {
        if (x < 320) {
            int grid_y = 600 - y;
            int r = (550 + CELL_SIZE - grid_y) / CELL_SIZE;
            int c = (x - 10) / CELL_SIZE;
            if (r >= 0 && r < PICKER_ROWS && c >= 0 && c < PICKER_COLS) {
                int idx = picker_offset + r * PICKER_COLS + c;
                if (idx < emoji_count) {
                    selected_emoji_idx = idx;
                    glutPostRedisplay();
                }
            }
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 'w') zoom += 0.2f;
    if (key == 's') zoom -= 0.2f;
    if (key == 'a') cam_rot_y -= 5.0f;
    if (key == 'd') cam_rot_y += 5.0f;
    if (key == 'q') cam_rot_x -= 5.0f;
    if (key == 'e') cam_rot_x += 5.0f;
    if (key == '1') resolution = 8;
    if (key == '2') resolution = 16;
    if (key == '3') resolution = 32;
    if (key == '4') resolution = 64;
    if (key == '[') { picker_offset -= PICKER_COLS; if (picker_offset < 0) picker_offset = 0; }
    if (key == ']') { picker_offset += PICKER_COLS; if (picker_offset > emoji_count - PICKER_COLS) picker_offset = emoji_count - PICKER_COLS; }
    if (key == 27) exit(0); // ESC to quit
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Emoji Studio");

    load_emoji_list();
    init_freetype(); 

    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glutMainLoop();
    return 0;
}
