#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <math.h>

// --- Globals ---

FT_Library ft;
FT_Face emoji_face; // For emojis

Display *x_display = NULL;
Window x_window;

#define MAX_WORDS 100
#define NODE_RADIUS 30.0f
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

typedef struct {
    char word[100];
    float x, y;
    float vx, vy;
    bool is_selected;
} Node;

typedef struct {
    int from, to;
    float strength;
} Edge;

Node nodes[MAX_WORDS];
Edge edges[MAX_WORDS * MAX_WORDS];
int node_count = 0;
int edge_count = 0;

int window_width = WINDOW_WIDTH;
int window_height = WINDOW_HEIGHT;

float zoom = 1.0f;
float offset_x = 0.0f, offset_y = 0.0f;
int dragging = 0;
int last_x, last_y;

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // White background

char status_message[256] = "";

// --- Function Prototypes ---

void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y);
void render_text(const char* str, float x, float y);
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void motion(int x, int y);
void init();
void idle();
void draw_circle(float x, float y, float radius, float color[3]);
int read_associations(const char *filename);
void update_physics();
unsigned int get_codepoint(const char* str);

// --- UTF8 & Font Helpers ---

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

void render_emoji(unsigned int codepoint, float x, float y) {
    if (!emoji_face) return;
    
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) {
        // Fallback to text rendering
        char fallback[5] = {0};
        if (codepoint < 128) {
            fallback[0] = (char)codepoint;
        } else {
            fallback[0] = '?';
        }
        render_text(fallback, x, y);
        return;
    }

    FT_GlyphSlot slot = emoji_face->glyph;
    if (!slot->bitmap.buffer) {
        // Fallback to text rendering
        char fallback[5] = {0};
        if (codepoint < 128) {
            fallback[0] = (char)codepoint;
        } else {
            fallback[0] = '?';
        }
        render_text(fallback, x, y);
        return;
    }
    if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
        // Fallback to text rendering
        char fallback[5] = {0};
        if (codepoint < 128) {
            fallback[0] = (char)codepoint;
        } else {
            fallback[0] = '?';
        }
        render_text(fallback, x, y);
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

    float scale_factor = emoji_scale;
    float w = slot->bitmap.width * scale_factor;
    float h = slot->bitmap.rows * scale_factor;
    // Center the bitmap at (x, y) by offsetting by half the scaled bitmap size
    float x2 = x - w / 2;
    float y2 = y - h / 2;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex2f(x2, y2);
    glTexCoord2f(1.0, 1.0); glVertex2f(x2 + w, y2);
    glTexCoord2f(1.0, 0.0); glVertex2f(x2 + w, y2 + h);
    glTexCoord2f(0.0, 0.0); glVertex2f(x2, y2 + h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);
    glColor3f(1.0f, 1.0f, 1.0f);
}

// Simple text rendering without face (reuse from editor, but minimal)
void render_text(const char* str, float x, float y) {
    glColor3fv(font_color);
    glRasterPos2f(x, y);
    while (*str) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str++);
    }
    glColor3f(1.0f, 1.0f, 1.0f);
}

void initFreeType() {
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        exit(1);
    }

    // Load emoji face
    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    FT_Error err = FT_New_Face(ft, emoji_font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error: Could not load emoji font at %s, error code: %d\n", emoji_font_path, err);
        emoji_face = NULL;
        exit(1);
    }
    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, 32);  // Set font size
        if (err) {
            fprintf(stderr, "Error: Could not set pixel size for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            exit(1);
        }
    } else if (emoji_face->num_fixed_sizes > 0) {
        err = FT_Select_Size(emoji_face, 0);
        if (err) {
            fprintf(stderr, "Error: Could not select size for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            emoji_face = NULL;
            exit(1);
        }
    } else {
        fprintf(stderr, "Error: No fixed sizes available in emoji font\n");
        FT_Done_Face(emoji_face);
        emoji_face = NULL;
        exit(1);
    }

    // Calculate emoji scale
    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = (float)(NODE_RADIUS * 1.5f) / (float)loaded_emoji_size;
    fprintf(stderr, "Emoji font loaded, loaded size: %d, scale: %f\n", loaded_emoji_size, emoji_scale);
}

void draw_circle(float x, float y, float radius, float color[3]) {
    glColor3fv(color);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= 20; i++) {
        float angle = i * 2.0f * M_PI / 20;
        glVertex2f(x + cosf(angle) * radius, y + sinf(angle) * radius);
    }
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
}

// Function to read CSV file and create nodes/edges
int read_associations(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening associations file");
        return 0;
    }
    
    char line[4096];
    char *words[MAX_WORDS];
    int word_count = 0;
    
    // Read header line to get word list
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }
    
    // Parse header
    char *token = strtok(line, ",");
    token = strtok(NULL, ",\n"); // Skip first empty token
    while (token && word_count < MAX_WORDS) {
        words[word_count] = malloc(strlen(token) + 1);
        strcpy(words[word_count], token);
        word_count++;
        token = strtok(NULL, ",\n");
    }
    
    printf("Loaded %d words\n", word_count);
    
    // Create nodes
    node_count = word_count > 50 ? 50 : word_count; // Limit to 50 for performance
    for (int i = 0; i < node_count; i++) {
        strcpy(nodes[i].word, words[i]);
        nodes[i].x = (float)(rand() % (WINDOW_WIDTH - 100)) + 50;
        nodes[i].y = (float)(rand() % (WINDOW_HEIGHT - 100)) + 50;
        nodes[i].vx = 0.0f;
        nodes[i].vy = 0.0f;
        nodes[i].is_selected = false;
    }
    
    // Read association data and create edges
    edge_count = 0;
    int row = 0;
    while (fgets(line, sizeof(line), file) && row < node_count) {
        token = strtok(line, ",");
        int col = 0;
        token = strtok(NULL, ",\n"); // Skip row label
        while (token && col < node_count && edge_count < MAX_WORDS * MAX_WORDS) {
            float assoc = atof(token);
            // Create edge if association is strong enough
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
    
    // Free allocated memory
    for (int i = 0; i < word_count; i++) {
        free(words[i]);
    }
    
    printf("Created %d nodes and %d edges\n", node_count, edge_count);
    return 1;
}

// Physics simulation
void update_physics() {
    const float SPRING_LENGTH = 100.0f;
    const float SPRING_CONSTANT = 0.01f;
    const float REPULSION_CONSTANT = 10000.0f;
    const float DAMPING = 0.9f;
    const float TIME_STEP = 0.1f;
    
    // Reset forces
    for (int i = 0; i < node_count; i++) {
        nodes[i].vx *= DAMPING;
        nodes[i].vy *= DAMPING;
    }
    
    // Spring forces between connected nodes
    for (int i = 0; i < edge_count; i++) {
        int from = edges[i].from;
        int to = edges[i].to;
        float dx = nodes[to].x - nodes[from].x;
        float dy = nodes[to].y - nodes[from].y;
        float distance = sqrtf(dx*dx + dy*dy);
        if (distance > 0) {
            float force = (distance - SPRING_LENGTH * (2.0f - edges[i].strength)) * SPRING_CONSTANT;
            float fx = force * dx / distance;
            float fy = force * dy / distance;
            
            nodes[from].vx += fx * TIME_STEP;
            nodes[from].vy += fy * TIME_STEP;
            nodes[to].vx -= fx * TIME_STEP;
            nodes[to].vy -= fy * TIME_STEP;
        }
    }
    
    // Repulsive forces between all nodes
    for (int i = 0; i < node_count; i++) {
        for (int j = i + 1; j < node_count; j++) {
            float dx = nodes[j].x - nodes[i].x;
            float dy = nodes[j].y - nodes[i].y;
            float distance_sq = dx*dx + dy*dy;
            if (distance_sq > 1.0f) { // Avoid division by zero
                float force = REPULSION_CONSTANT / distance_sq;
                float fx = force * dx;
                float fy = force * dy;
                
                nodes[i].vx -= fx * TIME_STEP;
                nodes[i].vy -= fy * TIME_STEP;
                nodes[j].vx += fx * TIME_STEP;
                nodes[j].vy += fy * TIME_STEP;
            }
        }
    }
    
    // Update positions
    for (int i = 0; i < node_count; i++) {
        nodes[i].x += nodes[i].vx * TIME_STEP;
        nodes[i].y += nodes[i].vy * TIME_STEP;
    }
}

// Get codepoint for a string (first character)
unsigned int get_codepoint(const char* str) {
    unsigned int codepoint;
    decode_utf8((const unsigned char*)str, &codepoint);
    return codepoint;
}

// --- Display ---

void display() {
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Draw edges
    for (int i = 0; i < edge_count; i++) {
        int from = edges[i].from;
        int to = edges[i].to;
        float strength = edges[i].strength;
        
        // Color based on strength
        float color[3] = {strength, strength * 0.5f, 1.0f - strength};
        glColor3fv(color);
        glLineWidth(2.0f * strength);
        glBegin(GL_LINES);
        glVertex2f(nodes[from].x, nodes[from].y);
        glVertex2f(nodes[to].x, nodes[to].y);
        glEnd();
    }

    // Draw nodes
    for (int i = 0; i < node_count; i++) {
        // Draw circle
        float node_color[3] = {0.5f, 0.7f, 1.0f};
        if (nodes[i].is_selected) {
            node_color[0] = 1.0f;
            node_color[1] = 1.0f;
            node_color[2] = 0.0f;
        }
        draw_circle(nodes[i].x, nodes[i].y, NODE_RADIUS, node_color);
        
        // Draw border
        glColor3f(0.0f, 0.0f, 0.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j <= 20; j++) {
            float angle = j * 2.0f * M_PI / 20;
            glVertex2f(nodes[i].x + cosf(angle) * NODE_RADIUS, nodes[i].y + sinf(angle) * NODE_RADIUS);
        }
        glEnd();
        
        // Render emoji or text
        unsigned int codepoint = get_codepoint(nodes[i].word);
        render_emoji(codepoint, nodes[i].x, nodes[i].y);
    }

    // Status
    render_text(status_message, 10, 20);

    glutSwapBuffers();
}

// --- GLUT Callbacks ---

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            // Convert GLUT y (top-left origin) to OpenGL y (bottom-left origin)
            float gl_y = window_height - y;
            
            // Check if clicked on a node
            for (int i = 0; i < node_count; i++) {
                float dx = nodes[i].x - x;
                float dy = nodes[i].y - gl_y;
                float distance = sqrtf(dx*dx + dy*dy);
                if (distance <= NODE_RADIUS) {
                    nodes[i].is_selected = !nodes[i].is_selected;
                    break;
                }
            }
            
            dragging = 1;
            last_x = x;
            last_y = y;
        } else {
            dragging = 0;
        }
    } else if (button == GLUT_RIGHT_BUTTON) {
        if (state == GLUT_DOWN) {
            // Reset view
            zoom = 1.0f;
            offset_x = 0.0f;
            offset_y = 0.0f;
        }
    } else if (button == 3) { // Scroll up
        zoom *= 1.1f;
    } else if (button == 4) { // Scroll down
        zoom /= 1.1f;
    }
    
    glutPostRedisplay();
}

void motion(int x, int y) {
    if (dragging) {
        offset_x += (x - last_x) / zoom;
        offset_y -= (y - last_y) / zoom;
        last_x = x;
        last_y = y;
        glutPostRedisplay();
    }
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void idle() {
    update_physics();
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: // ESC
            exit(0);
            break;
        case 'r':
        case 'R':
            // Reset node positions
            for (int i = 0; i < node_count; i++) {
                nodes[i].x = (float)(rand() % (WINDOW_WIDTH - 100)) + 50;
                nodes[i].y = (float)(rand() % (WINDOW_HEIGHT - 100)) + 50;
                nodes[i].vx = 0.0f;
                nodes[i].vy = 0.0f;
                nodes[i].is_selected = false;
            }
            break;
    }
}

// --- Main ---

void init() {
    initFreeType();
    srand(time(NULL));
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <associations.csv>\n", argv[0]);
        fprintf(stderr, "Controls:\n");
        fprintf(stderr, "  Left mouse: Select nodes / Pan view\n");
        fprintf(stderr, "  Right mouse: Reset view\n");
        fprintf(stderr, "  Scroll: Zoom in/out\n");
        fprintf(stderr, "  R: Reset node positions\n");
        fprintf(stderr, "  ESC: Quit\n");
        return 1;
    }
    
    if (!read_associations(argv[1])) {
        fprintf(stderr, "Failed to read associations file\n");
        return 1;
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Word Association Visualization");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyboard);

    init();

    printf("Word Association Visualization\n");
    printf("Controls:\n");
    printf("  Left mouse: Select nodes / Pan view\n");
    printf("  Right mouse: Reset view\n");
    printf("  Scroll: Zoom in/out\n");
    printf("  R: Reset node positions\n");
    printf("  ESC: Quit\n");

    glutMainLoop();

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);

    return 0;
}