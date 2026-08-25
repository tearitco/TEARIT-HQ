#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

// Include FreeType for text rendering
#include <ft2build.h>
#include FT_FREETYPE_H

#define MAX_WORDS 1000
#define NODE_RADIUS 0.04f
#define SPRING_LENGTH 0.5f
#define SPRING_CONSTANT 0.1f
#define REPULSION_CONSTANT 0.5f
#define DAMPING 0.8f
#define TIME_STEP 0.1f

// Font settings
const char *font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
FT_Library ft;
FT_Face face;
float emoji_scale = 1.0f;

// Forward declarations
void render_text(const char *text, float x, float y, float z, float scale);
void render_emoji(const char *emoji, float x, float y, float z, const float* right, const float* up);

// UTF-8 decoding function
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0) {
        if ((str[1] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
            return 2;
        }
    }
    if ((str[0] & 0xF0) == 0xE0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
            return 3;
        }
    }
    if ((str[0] & 0xF8) == 0xF0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
            return 4;
        }
    }
    *codepoint = '?';
    return 1;
}

typedef struct {
    char word[100];
    float x, y, z;
    float vx, vy, vz;
} Node;

typedef struct {
    int from, to;
    float strength;
} Edge;

Node nodes[MAX_WORDS];
Edge edges[MAX_WORDS * MAX_WORDS];
int node_count = 0;
int edge_count = 0;

// Camera and view variables
float camera_angle_y = 0.0f;
float camera_distance = 5.0f;
float camera_target_x = 0.0f, camera_target_y = 0.0f, camera_target_z = 0.0f;

int dragging = 0;
int last_x, last_y;

// Function to initialize FreeType
int init_freetype() {
    if (FT_Init_FreeType(&ft)) {
        printf("Could not init FreeType Library\n");
        return 0;
    }
    
    if (FT_New_Face(ft, font_path, 0, &face)) {
        printf("Failed to load font: %s\n", font_path);
        if (FT_New_Face(ft, "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 0, &face)) {
            printf("Failed to load fallback font\n");
            return 0;
        }
    }
    
    int font_size = 32;
    if (FT_IS_SCALABLE(face)) {
        FT_Set_Pixel_Sizes(face, 0, font_size);
    } else if (face->num_fixed_sizes > 0) {
        int best_diff = 999999;
        int best_index = 0;
        for (int i = 0; i < face->num_fixed_sizes; i++) {
            int diff = abs(face->available_sizes[i].height - font_size);
            if (diff < best_diff) {
                best_diff = diff;
                best_index = i;
            }
        }
        FT_Select_Size(face, best_index);
    }
    
    if (face->size) {
        int loaded_emoji_size = face->size->metrics.y_ppem;
        if (loaded_emoji_size > 0) {
            emoji_scale = 0.064f / (float)loaded_emoji_size;
        }
    }
    
    return 1;
}

// Function to render emojis using FreeType
void render_emoji(const char *emoji, float x, float y, float z, const float* right, const float* up) {
    if (!emoji || !face) return;
    
    unsigned int codepoint;
    decode_utf8((const unsigned char*)emoji, &codepoint);
    
    FT_Error err = FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) {
        glColor3f(0.0f, 0.0f, 0.0f);
        render_text(emoji, x, y, z, 1.0f);
        return;
    }
    
    FT_GlyphSlot slot = face->glyph;
    if (!slot->bitmap.buffer || slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
        glColor3f(0.0f, 0.0f, 0.0f);
        render_text(emoji, x, y, z, 1.0f);
        return;
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glColor3f(1.0f, 1.0f, 1.0f);
    
    float scale_factor = emoji_scale;
    float w = slot->bitmap.width * scale_factor;
    float h = slot->bitmap.rows * scale_factor;
    
    float w_vec[3] = { right[0] * w/2, right[1] * w/2, right[2] * w/2 };
    float h_vec[3] = { up[0] * h/2, up[1] * h/2, up[2] * h/2 };

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0); glVertex3f(-w_vec[0] + h_vec[0], -w_vec[1] + h_vec[1], -w_vec[2] + h_vec[2]);
    glTexCoord2f(1.0, 0.0); glVertex3f( w_vec[0] + h_vec[0],  w_vec[1] + h_vec[1],  w_vec[2] + h_vec[2]);
    glTexCoord2f(1.0, 1.0); glVertex3f( w_vec[0] - h_vec[0],  w_vec[1] - h_vec[1],  w_vec[2] - h_vec[2]);
    glTexCoord2f(0.0, 1.0); glVertex3f(-w_vec[0] - h_vec[0], -w_vec[1] - h_vec[1], -w_vec[2] - h_vec[2]);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);
    
    glColor3f(1.0f, 1.0f, 1.0f);
}

// Function to render text using GLUT bitmap characters
void render_text(const char *text, float x, float y, float z, float scale) {
    glRasterPos3f(x, y, z);
    
    if (text) {
        int len = strlen(text);
        for (int i = 0; i < len; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, text[i]);
        }
    }
}

// Function to read CSV file and create nodes/edges (modified for 3D)
int read_associations(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening associations file");
        return 0;
    }
    
    char line[4096];
    char *words[MAX_WORDS];
    int word_count = 0;
    
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }
    
    char *token = strtok(line, ",");
    token = strtok(NULL, ",\n");
    while (token && word_count < MAX_WORDS) {
        words[word_count] = malloc(strlen(token) + 1);
        strcpy(words[word_count], token);
        word_count++;
        token = strtok(NULL, ",\n");
    }
    
    printf("Loaded %d words\n", word_count);
    
    node_count = word_count > 50 ? 50 : word_count;
    for (int i = 0; i < node_count; i++) {
        strcpy(nodes[i].word, words[i]);
        nodes[i].x = (float)(rand() % 100) / 50.0f - 1.0f;
        nodes[i].y = (float)(rand() % 100) / 50.0f - 1.0f;
        nodes[i].z = (float)(rand() % 100) / 50.0f - 1.0f;
        nodes[i].vx = 0.0f;
        nodes[i].vy = 0.0f;
        nodes[i].vz = 0.0f;
    }
    
    edge_count = 0;
    int row = 0;
    while (fgets(line, sizeof(line), file) && row < node_count) {
        token = strtok(line, ",");
        int col = 0;
        token = strtok(NULL, ",\n");
        while (token && col < node_count && edge_count < MAX_WORDS * MAX_WORDS) {
            float assoc = atof(token);
            if (assoc > 0.7 && row != col) {
                edges[edge_count].from = row;
                edges[edge_count].to = col;
                edges[edge_count].strength = assoc;
                edge_count++;
            }
            col++;
            token = strtok(NULL, ",\n");
        }
        row++;
    }
    
    fclose(file);
    
    for (int i = 0; i < word_count; i++) {
        free(words[i]);
    }
    
    printf("Created %d nodes and %d edges\n", node_count, edge_count);
    return 1;
}


// Physics simulation (modified for 3D)
void update_physics() {
    for (int i = 0; i < node_count; i++) {
        nodes[i].vx *= DAMPING;
        nodes[i].vy *= DAMPING;
        nodes[i].vz *= DAMPING;
    }
    
    for (int i = 0; i < edge_count; i++) {
        int from = edges[i].from;
        int to = edges[i].to;
        float dx = nodes[to].x - nodes[from].x;
        float dy = nodes[to].y - nodes[from].y;
        float dz = nodes[to].z - nodes[from].z;
        float distance = sqrtf(dx*dx + dy*dy + dz*dz);
        if (distance > 0) {
            float force = (distance - SPRING_LENGTH * (2.0f - edges[i].strength)) * SPRING_CONSTANT;
            float fx = force * dx / distance;
            float fy = force * dy / distance;
            float fz = force * dz / distance;
            
            nodes[from].vx += fx * TIME_STEP;
            nodes[from].vy += fy * TIME_STEP;
            nodes[from].vz += fz * TIME_STEP;
            nodes[to].vx -= fx * TIME_STEP;
            nodes[to].vy -= fy * TIME_STEP;
            nodes[to].vz -= fz * TIME_STEP;
        }
    }
    
    for (int i = 0; i < node_count; i++) {
        for (int j = i + 1; j < node_count; j++) {
            float dx = nodes[j].x - nodes[i].x;
            float dy = nodes[j].y - nodes[i].y;
            float dz = nodes[j].z - nodes[i].z;
            float distance_sq = dx*dx + dy*dy + dz*dz;
            if (distance_sq > 0.01f) {
                float force = REPULSION_CONSTANT / distance_sq;
                float fx = force * dx;
                float fy = force * dy;
                float fz = force * dz;
                
                nodes[i].vx -= fx * TIME_STEP;
                nodes[i].vy -= fy * TIME_STEP;
                nodes[i].vz -= fz * TIME_STEP;
                nodes[j].vx += fx * TIME_STEP;
                nodes[j].vy += fy * TIME_STEP;
                nodes[j].vz += fz * TIME_STEP;
            }
        }
    }
    
    for (int i = 0; i < node_count; i++) {
        nodes[i].x += nodes[i].vx * TIME_STEP;
        nodes[i].y += nodes[i].vy * TIME_STEP;
        nodes[i].z += nodes[i].vz * TIME_STEP;
    }
}

// Rendering (modified for 3D)
void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Set up camera
    float cam_x = camera_distance * sinf(camera_angle_y);
    float cam_z = camera_distance * cosf(camera_angle_y);
    gluLookAt(cam_x + camera_target_x, camera_target_y, cam_z + camera_target_z,
              camera_target_x, camera_target_y, camera_target_z,
              0.0f, 1.0f, 0.0f);

    float modelview[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    float right[3] = { modelview[0], modelview[4], modelview[8] };
    float up[3] = { modelview[1], modelview[5], modelview[9] };
    
    // Draw edges
    glBegin(GL_LINES);
    for (int i = 0; i < edge_count; i++) {
        int from = edges[i].from;
        int to = edges[i].to;
        float strength = edges[i].strength;
        glColor3f(strength, strength * 0.5f, 1.0f - strength);
        glVertex3f(nodes[from].x, nodes[from].y, nodes[from].z);
        glVertex3f(nodes[to].x, nodes[to].y, nodes[to].z);
    }
    glEnd();
    
    // Draw nodes
    for (int i = 0; i < node_count; i++) {
        glPushMatrix();
        glTranslatef(nodes[i].x, nodes[i].y, nodes[i].z);
        
        // Draw sphere
        glColor3f(0.5f, 0.7f, 1.0f);
        glutSolidSphere(NODE_RADIUS, 16, 16);
        
        // Draw border
        glColor3f(0.0f, 0.0f, 0.0f);
        glLineWidth(1.0f);
        glutWireSphere(NODE_RADIUS, 16, 16);
        
        glDisable(GL_DEPTH_TEST);
        // Draw emoji (as a flat texture on a quad)
        glColor3f(1.0f, 1.0f, 1.0f);
        render_emoji(nodes[i].word, 0, 0, 0, right, up);
        glEnable(GL_DEPTH_TEST);
        
        glPopMatrix();
    }
    
    glutSwapBuffers();
}

// Idle function for animation
void idle() {
    update_physics();
    glutPostRedisplay();
}

// Mouse handling (modified for 3D)
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            dragging = 1;
            last_x = x;
            last_y = y;
        } else {
            dragging = 0;
        }
    } else if (button == GLUT_RIGHT_BUTTON) {
        if (state == GLUT_DOWN) {
            // Reset view
            camera_angle_y = 0.0f;
            camera_distance = 5.0f;
            camera_target_x = 0.0f;
            camera_target_y = 0.0f;
            camera_target_z = 0.0f;
            glutPostRedisplay();
        }
    } else if (button == 3) { // Scroll up
        camera_distance /= 1.1f;
        glutPostRedisplay();
    } else if (button == 4) { // Scroll down
        camera_distance *= 1.1f;
        glutPostRedisplay();
    }
}

void motion(int x, int y) {
    if (dragging) {
        float dx = (x - last_x) / 100.0f;
        float dy = (y - last_y) / 100.0f;

        // Pan the camera target
        camera_target_x -= dx * cosf(camera_angle_y);
        camera_target_z += dx * sinf(camera_angle_y);
        camera_target_y += dy;

        last_x = x;
        last_y = y;
        glutPostRedisplay();
    }
}

// Keyboard handling (modified for 3D rotation)
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: // ESC
            exit(0);
            break;
        case 'q':
        case 'Q':
            camera_angle_y -= 0.1f;
            break;
        case 'e':
        case 'E':
            camera_angle_y += 0.1f;
            break;
        case 'r':
        case 'R':
            for (int i = 0; i < node_count; i++) {
                nodes[i].x = (float)(rand() % 100) / 50.0f - 1.0f;
                nodes[i].y = (float)(rand() % 100) / 50.0f - 1.0f;
                nodes[i].z = (float)(rand() % 100) / 50.0f - 1.0f;
                nodes[i].vx = 0.0f;
                nodes[i].vy = 0.0f;
                nodes[i].vz = 0.0f;
            }
            break;
    }
    glutPostRedisplay();
}

// Reshape function for perspective projection
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (h == 0) h = 1;
    gluPerspective(45.0f, (float)w / (float)h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

// Initialization (modified for 3D and background color)
void init() {
    glClearColor(0.82f, 0.71f, 0.55f, 1.0f); // Tan background
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    srand(0);
    
    if (!init_freetype()) {
        printf("Warning: Failed to initialize FreeType, falling back to bitmap text\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <associations.csv>\n", argv[0]);
        fprintf(stderr, "Controls:\n");
        fprintf(stderr, "  Left mouse: Pan view\n");
        fprintf(stderr, "  Right mouse: Reset view\n");
        fprintf(stderr, "  Scroll: Zoom in/out\n");
        fprintf(stderr, "  Q/E: Rotate camera\n");
        fprintf(stderr, "  R: Reset node positions\n");
        fprintf(stderr, "  ESC: Quit\n");
        return 1;
    }
    
    if (!read_associations(argv[1])) {
        fprintf(stderr, "Failed to read associations file\n");
        return 1;
    }
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("3D Word Association Visualization");
    glutFullScreen();
    
    init();
    
    glutDisplayFunc(render);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    
    printf("3D Word Association Visualization\n");
    printf("Controls:\n");
    printf("  Left mouse: Pan view\n");
    printf("  Right mouse: Reset view\n");
    printf("  Scroll: Zoom in/out\n");
    printf("  Q/E: Rotate camera\n");
    printf("  R: Reset node positions\n");
    printf("  ESC: Quit\n");
    
    glutMainLoop();
    
    // Cleanup FreeType
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    
    return 0;
}