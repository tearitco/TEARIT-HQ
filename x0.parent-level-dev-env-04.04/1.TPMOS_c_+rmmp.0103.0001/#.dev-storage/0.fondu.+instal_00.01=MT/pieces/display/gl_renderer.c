/* macOS compatible includes */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef _WIN32
#include <windows.h>
/* GL_BGRA is defined in glext.h but we define it here for simplicity */
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#endif
#ifdef __APPLE__
#include <GLUT/glut.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define CTRL_KEY(k) ((k) & 0x1f)
#define MAX_HISTORY 50
#define MAX_LINES_PER_FRAME 200

int window_width = 800;
int window_height = 1000;
float background_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

// FreeType for emoji rendering
FT_Library ft_library;
FT_Face emoji_face;
int emoji_initialized = 0;
float emoji_scale = 1.0f;

typedef struct {
    char **lines;
    int num_lines;
    char timestamp[32];
} FrameEntry;

FrameEntry history[MAX_HISTORY];
int history_count = 0;
int history_head = 0;
int scroll_offset = 0;
int total_lines_in_history = 0;
off_t last_marker_size = 0;

// Mouse Interaction
int is_dragging = 0;
int drag_start_y = 0;
int scroll_start_offset = 0;

void writeCommand(int key) {
    FILE *fp = fopen("pieces/keyboard/history.txt", "a");
    if (!fp) return;
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    fprintf(fp, "KEY_PRESSED: %d\n", key);
    fclose(fp);
}

void add_to_history(const char *content) {
    if (!content || strlen(content) == 0) return;

    int last_idx = (history_head + history_count - 1) % MAX_HISTORY;
    if (history_count > 0) {
        char *first_newline = strchr(content, '\n');
        if (first_newline) {
            int len = first_newline - content;
            if (history[last_idx].num_lines > 0 && strncmp(history[last_idx].lines[0], content, len) == 0) {
                // Potential duplicate
            }
        }
    }

    FrameEntry entry;
    entry.lines = malloc(sizeof(char*) * MAX_LINES_PER_FRAME);
    entry.num_lines = 0;
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(entry.timestamp, sizeof(entry.timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    char *copy = strdup(content);
    char *line = strtok(copy, "\n");
    while (line && entry.num_lines < MAX_LINES_PER_FRAME) {
        entry.lines[entry.num_lines++] = strdup(line);
        line = strtok(NULL, "\n");
    }
    free(copy);

    if (history_count == MAX_HISTORY) {
        total_lines_in_history -= (history[history_head].num_lines + 1);
        for (int i = 0; i < history[history_head].num_lines; i++) free(history[history_head].lines[i]);
        free(history[history_head].lines);
        history[history_head] = entry;
        history_head = (history_head + 1) % MAX_HISTORY;
    } else {
        history[history_count++] = entry;
    }

    total_lines_in_history += (entry.num_lines + 1);
    scroll_offset = 0;
}

void keyboard(unsigned char key, int x, int y) {
    /* Ctrl+C (ASCII 3) */
    if (key == 3) {
        printf("[GL-RENDERER] Ctrl+C detected. Exiting...\n");
        exit(0);
    }
    
    /* ESC (ASCII 27) */
    if (key == 27) {
        printf("[GL-RENDERER] ESC pressed. Exiting...\n");
        exit(0);
    }

    if (key == CTRL_KEY('c')) exit(0);
    writeCommand((int)key);
}

void special_keyboard(int key, int x, int y) {
    // Arrow keys send input to keyboard history (same as terminal renderer)
    // No scrolling here - mouse is used for scrolling
    switch(key) {
        case GLUT_KEY_UP: writeCommand(1002); break;   // K_UP
        case GLUT_KEY_DOWN: writeCommand(1003); break; // K_DOWN
        case GLUT_KEY_LEFT: writeCommand(1000); break; // K_LEFT
        case GLUT_KEY_RIGHT: writeCommand(1001); break;// K_RIGHT
        case GLUT_KEY_PAGE_UP: writeCommand(1004); break;
        case GLUT_KEY_PAGE_DOWN: writeCommand(1005); break;
        case GLUT_KEY_HOME: writeCommand(1006); break;
        case GLUT_KEY_END: writeCommand(1007); break;
    }
}

void parse_frame_file() {
    FILE *fp = fopen("pieces/display/current_frame.txt", "r");
    if (!fp) return;
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size > 0) {
        char *buffer = malloc(file_size + 1);
        fread(buffer, 1, file_size, fp);
        buffer[file_size] = '\0';
        add_to_history(buffer);
        free(buffer);
    }
    fclose(fp);
}

// Initialize FreeType and load emoji font
void init_emoji_rendering() {
    if (FT_Init_FreeType(&ft_library)) {
        fprintf(stderr, "Could not init FreeType Library\n");
        return;
    }

    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
#ifdef _WIN32
    emoji_font_path = "C:/Windows/Fonts/seguiemj.ttf"; // Segoe UI Emoji
#endif

    FT_Error err = FT_New_Face(ft_library, emoji_font_path, 0, &emoji_face);
    if (err) {
        fprintf(stderr, "Error: Could not load emoji font at %s, error code: %d\n", emoji_font_path, err);
#ifdef _WIN32
        /* Windows fallback */
        emoji_font_path = "C:/Windows/Fonts/arial.ttf";
        err = FT_New_Face(ft_library, emoji_font_path, 0, &emoji_face);
        if (err) {
            FT_Done_FreeType(ft_library);
            return;
        }
#else
        /* On Linux, just continue without emojis - don't destroy library or return early */
        return;
#endif
    }

    if (FT_IS_SCALABLE(emoji_face)) {
        err = FT_Set_Pixel_Sizes(emoji_face, 0, 32);
        if (err) {
            fprintf(stderr, "Error: Could not set pixel size for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            FT_Done_FreeType(ft_library);
            return;
        }
    } else if (emoji_face->num_fixed_sizes > 0) {
        err = FT_Select_Size(emoji_face, 0);
        if (err) {
            fprintf(stderr, "Error: Could not select size for emoji font, error code: %d\n", err);
            FT_Done_Face(emoji_face);
            FT_Done_FreeType(ft_library);
            return;
        }
    }

    int loaded_emoji_size = emoji_face->size->metrics.y_ppem;
    emoji_scale = 32.0f / (float)loaded_emoji_size;
    emoji_initialized = 1;
    fprintf(stderr, "Emoji font loaded, size: %d, scale: %f\n", loaded_emoji_size, emoji_scale);
}

// Decode UTF-8 to Unicode codepoint
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

// Check if a character is likely an emoji (high Unicode range)
int is_emoji(unsigned int codepoint) {
    // Common emoji ranges
    if (codepoint >= 0x1F300 && codepoint <= 0x1F9FF) return 1;  // Miscellaneous Symbols and Pictographs, Emoticons, etc.
    if (codepoint >= 0x2600 && codepoint <= 0x26FF) return 1;   // Miscellaneous Symbols
    if (codepoint >= 0x2700 && codepoint <= 0x27BF) return 1;   // Dingbats
    if (codepoint >= 0x1F600 && codepoint <= 0x1F64F) return 1; // Emoticons
    if (codepoint >= 0x1F680 && codepoint <= 0x1F6FF) return 1; // Transport and Map Symbols
    if (codepoint >= 0x2600 && codepoint <= 0x26FF) return 1;   // Weather, zodiac
    if (codepoint >= 0x1F1E0 && codepoint <= 0x1F1FF) return 1; // Regional indicator symbols
    if (codepoint >= 0x1F900 && codepoint <= 0x1F9FF) return 1; // Supplemental Symbols and Pictographs
    return 0;
}

// Render a single emoji character
void render_emoji_char(unsigned int codepoint, float x, float y) {
    if (!emoji_initialized) return;

    FT_Error err = FT_Load_Char(emoji_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) {
        return;
    }

    FT_GlyphSlot slot = emoji_face->glyph;
    if (!slot->bitmap.buffer) return;
    if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) return;

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
    float x2 = x;
    float y2 = y - 4.0f;  // Adjust vertical position so emoji sits on baseline

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

// Render a line of text, handling both ASCII and emoji
void render_line_mixed(const char* line, float x, float y) {
    const unsigned char* p = (const unsigned char*)line;
    float current_x = x;
    float line_height = 18.0f;

    while (*p) {
        unsigned int codepoint;
        int bytes = decode_utf8(p, &codepoint);

        if (is_emoji(codepoint) && emoji_initialized) {
            render_emoji_char(codepoint, current_x, y);
            current_x += 20.0f;  // Emoji width (was 24, adjusted for proper spacing)
        } else if (codepoint < 128) {
            // ASCII character - use bitmap font
            glRasterPos2f(current_x, y);
            glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)codepoint);
            current_x += 9.0f;  // Approximate ASCII width
        } else {
            // Non-emoji Unicode - try to render with bitmap font as fallback
            glRasterPos2f(current_x, y);
            glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (codepoint < 128) ? (int)codepoint : '?');
            current_x += 9.0f;
        }

        p += bytes;
    }
}

void draw_global_ui(void) {
    int btn_size = 30;
    int margin = 10;
    int x = window_width - btn_size - margin;
    int y = window_height - btn_size - margin;
    
    /* Button background (Red) */
    glColor3f(0.8f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
        glVertex2f(x, y); glVertex2f(x + btn_size, y);
        glVertex2f(x + btn_size, y + btn_size); glVertex2f(x, y + btn_size);
    glEnd();
    
    /* X text */
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x + 10, y + 8);
    glutBitmapCharacter(GLUT_BITMAP_9_BY_15, 'X');
}

void render_frame() {
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float line_height = 18.0f;
    int max_visible_lines = (window_height / line_height) - 1;
    int total_drawn = 0;

    for (int i = 0; i < history_count; i++) {
        int idx = (history_head + history_count - 1 - i) % MAX_HISTORY;
        FrameEntry *fe = &history[idx];

        for (int j = fe->num_lines - 1; j >= -1; j--) {
            total_drawn++;
            if (total_drawn > scroll_offset && total_drawn <= scroll_offset + max_visible_lines) {
                float y = 20.0f + (total_drawn - scroll_offset - 1) * line_height;

                if (j == -1) {
                    char *header = NULL;
                    if (asprintf(&header, "--- FRAME UPDATE at %s ---", fe->timestamp) != -1) {
                        glColor3f(0.4f, 0.4f, 0.4f);
                        glRasterPos2f(15.0f, y);
                        for (char *p = header; *p; p++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
                        free(header);
                    }
                } else {
                    if (i == 0) glColor3f(1.0f, 1.0f, 1.0f);
                    else glColor3f(0.8f, 0.8f, 0.8f);
                    render_line_mixed(fe->lines[j], 15.0f, y);
                }
            }
        }
        if (total_drawn > scroll_offset + max_visible_lines) break;
    }

    // Scrollbar Thumb
    float sb_w = 16.0f;
    float sb_x = window_width - sb_w - 5.0f;
    float track_h = window_height - 20.0f;

    glColor3f(0.1f, 0.1f, 0.1f);
    glBegin(GL_QUADS);
        glVertex2f(sb_x, 10); glVertex2f(sb_x+sb_w, 10);
        glVertex2f(sb_x+sb_w, 10+track_h); glVertex2f(sb_x, 10+track_h);
    glEnd();

    if (total_lines_in_history > 0) {
        int max_visible = (window_height / 18) - 1;
        float thumb_h = (float)max_visible / (total_lines_in_history + 1) * track_h;
        if (thumb_h < 30.0f) thumb_h = 30.0f;
        if (thumb_h > track_h) thumb_h = track_h;

        float scroll_perc = (total_lines_in_history > max_visible) ?
                            (float)scroll_offset / (total_lines_in_history - max_visible + 5) : 0;
        float thumb_y = 10.0f + scroll_perc * (track_h - thumb_h);

        glColor3f(0.6f, 0.6f, 0.6f);
        glBegin(GL_QUADS);
            glVertex2f(sb_x+1, thumb_y); glVertex2f(sb_x+sb_w-1, thumb_y);
            glVertex2f(sb_x+sb_w-1, thumb_y+thumb_h); glVertex2f(sb_x+1, thumb_y+thumb_h);
        glEnd();
    }

    draw_global_ui();
    glutSwapBuffers();
}

void mouse_func(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        float sb_w = 16.0f;
        float sb_x = window_width - sb_w - 5.0f;

        if (state == GLUT_DOWN) {
            if (x >= sb_x && x <= sb_x + sb_w) {
                is_dragging = 1;
                drag_start_y = y;
                scroll_start_offset = scroll_offset;
            }
        } else {
            is_dragging = 0;
        }
    }
}

void motion_func(int x, int y) {
    if (is_dragging) {
        int dy = y - drag_start_y;
        int max_visible = (window_height / 18) - 1;

        if (total_lines_in_history > max_visible) {
            float pixels_to_lines = (float)total_lines_in_history / (window_height - 20);
            scroll_offset = scroll_start_offset + (int)(dy * pixels_to_lines);

            if (scroll_offset > total_lines_in_history - 5) scroll_offset = total_lines_in_history - 5;
            if (scroll_offset < 0) scroll_offset = 0;
            glutPostRedisplay();
        }
    }
}

void timer(int value) {
    struct stat st;
    int dirty = 0;
    // Watch renderer_pulse.txt - parser hits this after compose_frame
    if (stat("pieces/display/renderer_pulse.txt", &st) == 0) {
        if (st.st_size != last_marker_size) {
            parse_frame_file();
            last_marker_size = st.st_size;
            dirty = 1;
        }
    }
    
    if (dirty) {
        glutPostRedisplay();
    }
    
    /* Check every 16ms, but only redraw if dirty */
    glutTimerFunc(16, timer, 0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(800, 1000);
    glutCreateWindow("TPM GL Terminal");
    glutDisplayFunc(render_frame);
    glutTimerFunc(16, timer, 0);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keyboard);
    glutMouseFunc(mouse_func);
    glutMotionFunc(motion_func);

    // Initialize emoji rendering
    init_emoji_rendering();

    struct stat st;
    if (stat("pieces/display/renderer_pulse.txt", &st) == 0) last_marker_size = st.st_size;
    parse_frame_file();
    glutMainLoop();

    // Cleanup
    if (emoji_initialized) {
        FT_Done_Face(emoji_face);
        FT_Done_FreeType(ft_library);
    }
    return 0;
}
