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

// --- Globals ---

FT_Library ft;
FT_Face emoji_face; // For emojis

Display *x_display = NULL;
Window x_window;

#define GRID_ROWS 8
#define GRID_COLS 8
#define NUM_CANDY_TYPES 6
#define TILE_SIZE 64  // Pixel size for each candy tile
#define MATCH_MIN 3   // Minimum match length

const char *candy_emojis[NUM_CANDY_TYPES] = {"❤️", "🧡", "💛", "💚", "💙", "💜"};

int grid[GRID_ROWS][GRID_COLS];
bool is_animating = false;
int score = 0;

int window_width = GRID_COLS * TILE_SIZE + 20;
int window_height = GRID_ROWS * TILE_SIZE + 60;  // Extra for score

int selected_row = -1;
int selected_col = -1;

float emoji_scale;
float font_color[3] = {1.0f, 1.0f, 1.0f};
float background_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};

char status_message[256] = "";

// --- Function Prototypes ---

void initFreeType();
void render_emoji(unsigned int codepoint, float x, float y);
void render_text(const char* str, float x, float y);  // Simple text for score
void display();
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void init();
void idle();
void draw_rect(float x, float y, float w, float h, float color[3]);
void fill_grid();
void swap_candies(int r1, int c1, int r2, int c2);
bool find_matches();
void remove_matches();
void drop_candies();
bool is_adjacent(int r1, int c1, int r2, int c2);
void set_status_message(const char* msg);

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
    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) {
        fprintf(stderr, "Error: Could not load glyph for codepoint U+%04X, error code: %d\n", codepoint, err);
        return;
    }

    FT_GlyphSlot slot = emoji_face->glyph;
    if (!slot->bitmap.buffer) {
        fprintf(stderr, "Error: No bitmap for glyph U+%04X\n", codepoint);
        return;
    }
    if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
        fprintf(stderr, "Error: Incorrect pixel mode for glyph U+%04X\n", codepoint);
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
        err = FT_Set_Pixel_Sizes(emoji_face, 0, TILE_SIZE - 10);  // Adjust for tile
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

    // Calculate emoji scale to fit tile
    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = (float)(TILE_SIZE * 0.8f) / (float)loaded_emoji_size;  // 80% of tile
    fprintf(stderr, "Emoji font loaded, loaded size: %d, scale: %f\n", loaded_emoji_size, emoji_scale);
}

void draw_rect(float x, float y, float w, float h, float color[3]) {
    glColor3fv(color);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
}

void set_status_message(const char* msg) {
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

// --- Game Logic ---

void fill_grid() {
    srand(time(NULL));
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            grid[r][c] = rand() % NUM_CANDY_TYPES;
        }
    }
    // Remove initial matches for fair start (optional)
    while (find_matches()) {
        remove_matches();
        drop_candies();
    }
}

bool is_adjacent(int r1, int c1, int r2, int c2) {
    return (abs(r1 - r2) + abs(c1 - c2) == 1);
}

void swap_candies(int r1, int c1, int r2, int c2) {
    int temp = grid[r1][c1];
    grid[r1][c1] = grid[r2][c2];
    grid[r2][c2] = temp;
}

bool find_matches() {
    bool has_match = false;
    bool visited[GRID_ROWS][GRID_COLS] = {false};

    // Horizontal matches
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (visited[r][c]) continue;
            int type = grid[r][c];
            if (type == -1) continue;  // Empty
            int count = 1;
            for (int cc = c + 1; cc < GRID_COLS && grid[r][cc] == type; cc++) {
                count++;
            }
            if (count >= MATCH_MIN) {
                has_match = true;
                for (int cc = c; cc < c + count; cc++) {
                    visited[r][cc] = true;
                }
                score += count * 10;
            }
        }
    }

    // Vertical matches
    for (int c = 0; c < GRID_COLS; c++) {
        for (int r = 0; r < GRID_ROWS; r++) {
            if (visited[r][c]) continue;
            int type = grid[r][c];
            if (type == -1) continue;
            int count = 1;
            for (int rr = r + 1; rr < GRID_ROWS && grid[rr][c] == type; rr++) {
                count++;
            }
            if (count >= MATCH_MIN) {
                has_match = true;
                for (int rr = r; rr < r + count; rr++) {
                    visited[rr][c] = true;
                }
                score += count * 10;
            }
        }
    }

    return has_match;
}

void remove_matches() {
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            // Horizontal
            int h_count = 1;
            int type = grid[r][c];
            if (type == -1) continue;
            for (int cc = c + 1; cc < GRID_COLS && grid[r][cc] == type; cc++) h_count++;
            if (h_count >= MATCH_MIN) {
                for (int cc = c; cc < c + h_count; cc++) grid[r][cc] = -1;
            }

            // Vertical
            int v_count = 1;
            type = grid[r][c];
            for (int rr = r + 1; rr < GRID_ROWS && grid[rr][c] == type; rr++) v_count++;
            if (v_count >= MATCH_MIN) {
                for (int rr = r; rr < r + v_count; rr++) grid[rr][c] = -1;
            }
        }
    }
}

void drop_candies() {
    for (int c = 0; c < GRID_COLS; c++) {
        int write_r = GRID_ROWS - 1;
        for (int r = GRID_ROWS - 1; r >= 0; r--) {
            if (grid[r][c] != -1) {
                grid[write_r][c] = grid[r][c];
                if (write_r != r) grid[r][c] = -1;
                write_r--;
            }
        }
        // Fill top with new
        for (int r = write_r; r >= 0; r--) {
            grid[r][c] = rand() % NUM_CANDY_TYPES;
        }
    }
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

    // Draw grid background
    float grid_color[3] = {0.4f, 0.4f, 0.4f};  // Lighter grey for better contrast
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float x = 10 + c * TILE_SIZE;
            float y = window_height - 50 - (r + 1) * TILE_SIZE;  // Top-down
            draw_rect(x, y, TILE_SIZE, TILE_SIZE, grid_color);
        }
    }

    // Render candies
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (grid[r][c] == -1) continue;
            // Position emoji at the exact center of the tile
            float x = 10 + c * TILE_SIZE + TILE_SIZE / 2;
            float y = window_height - 50 - (r + 1) * TILE_SIZE + TILE_SIZE / 2;
            const char *emoji = candy_emojis[grid[r][c]];
            unsigned int codepoint;
            decode_utf8((const unsigned char*)emoji, &codepoint);
            render_emoji(codepoint, x, y);
        }
    }

    // Draw selected highlight
    if (selected_row != -1) {
        float sel_color[3] = {1.0f, 1.0f, 0.0f};
        float x = 10 + selected_col * TILE_SIZE;
        float y = window_height - 50 - (selected_row + 1) * TILE_SIZE;
        glLineWidth(3.0f);
        glColor3fv(sel_color);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + TILE_SIZE, y);
        glVertex2f(x + TILE_SIZE, y + TILE_SIZE);
        glVertex2f(x, y + TILE_SIZE);
        glEnd();
        glColor3f(1.0f, 1.0f, 1.0f);
    }

    // Score
    char score_str[50];
    snprintf(score_str, 50, "Score: %d", score);
    render_text(score_str, 10, window_height - 30);

    // Status
    render_text(status_message, 10, 10);

    glutSwapBuffers();
}

// --- GLUT Callbacks ---

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && !is_animating) {
        int col = (x - 10) / TILE_SIZE;
        // Convert GLUT y (top-left origin) to OpenGL y (bottom-left origin)
        float gl_y = window_height - y;
        // Calculate row from the top of the grid (window_height - 50)
        int row = (int)((window_height - 50 - gl_y) / TILE_SIZE);
        if (col >= 0 && col < GRID_COLS && row >= 0 && row < GRID_ROWS) {
            if (selected_row == -1) {
                selected_row = row;
                selected_col = col;
            } else {
                if (is_adjacent(selected_row, selected_col, row, col)) {
                    swap_candies(selected_row, selected_col, row, col);
                    bool valid = find_matches();
                    if (!valid) {
                        // Invalid swap, revert
                        swap_candies(selected_row, selected_col, row, col);
                        set_status_message("Invalid move!");
                    } else {
                        is_animating = true;  // Trigger processing in idle
                    }
                }
                selected_row = -1;
                selected_col = -1;
            }
            glutPostRedisplay();
        }
    }
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

void idle() {
    if (is_animating) {
        remove_matches();
        drop_candies();
        if (!find_matches()) {
            is_animating = false;
        }
        glutPostRedisplay();
    }
}

// --- Main ---

void init() {
    initFreeType();
    fill_grid();
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Candy Crush Clone");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutIdleFunc(idle);

    init();

    glutMainLoop();

    if (emoji_face) FT_Done_Face(emoji_face);
    FT_Done_FreeType(ft);

    return 0;
}
