#include <GL/glut.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define MAX_HISTORY 50
#define MAX_LINES_PER_FRAME 200

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

int window_width = 800;
int window_height = 1200;
float background_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

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

void writeCommand(int key) {
    // Ensure directory exists
    struct stat st = {0};
    if (stat("pieces/keyboard", &st) == -1) {
        mkdir("pieces/keyboard", 0777);
    }

    FILE *fp = fopen("pieces/keyboard/history.txt", "a");
    if (!fp) return;
    
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    fprintf(fp, "[%s] KEY_PRESSED: %d\n", timestamp, key);
    fclose(fp);
}

void add_to_history(const char *content) {
    if (!content || strlen(content) == 0) return;

    // Allocate new entry
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

    // Circular buffer management
    if (history_count == MAX_HISTORY) {
        // Free oldest
        total_lines_in_history -= (history[history_head].num_lines + 1); // +1 for the header line
        for (int i = 0; i < history[history_head].num_lines; i++) {
            free(history[history_head].lines[i]);
        }
        free(history[history_head].lines);
        
        history[history_head] = entry;
        history_head = (history_head + 1) % MAX_HISTORY;
    } else {
        history[history_count++] = entry;
    }

    total_lines_in_history += (entry.num_lines + 1);
    scroll_offset = 0; // Auto-scroll to bottom
}

void keyboard(unsigned char key, int x, int y) {
    if (key == CTRL_KEY('c')) exit(0);
    writeCommand((int)key);
}

void special_keyboard(int key, int x, int y) {
    switch(key) {
        case GLUT_KEY_UP: scroll_offset++; break;
        case GLUT_KEY_DOWN: scroll_offset = (scroll_offset > 0) ? scroll_offset - 1 : 0; break;
        case GLUT_KEY_PAGE_UP: scroll_offset += 20; break;
        case GLUT_KEY_PAGE_DOWN: scroll_offset = (scroll_offset > 20) ? scroll_offset - 20 : 0; break;
    }
    if (scroll_offset > total_lines_in_history - 10) scroll_offset = total_lines_in_history - 10;
    if (scroll_offset < 0) scroll_offset = 0;
    glutPostRedisplay();
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

void render_frame() {
    glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float line_height = 18.0f;
    float start_x = 10.0f;
    int max_visible_lines = (window_height / line_height) - 1;

    int total_drawn = 0;
    
    // We draw from bottom to top
    // newest frame is at the end of the history array (conceptually)
    for (int i = 0; i < history_count; i++) {
        int idx = (history_head + history_count - 1 - i) % MAX_HISTORY;
        FrameEntry *fe = &history[idx];

        // Draw frame lines in reverse
        for (int j = fe->num_lines - 1; j >= -1; j--) {
            total_drawn++;
            if (total_drawn > scroll_offset && total_drawn <= scroll_offset + max_visible_lines) {
                float y = 20.0f + (total_drawn - scroll_offset - 1) * line_height;
                glRasterPos2f(start_x, y);
                
                if (j == -1) { // Render the header
                    char header[64];
                    snprintf(header, sizeof(header), "--- FRAME UPDATE at %s ---", fe->timestamp);
                    glColor3f(0.4f, 0.4f, 0.4f);
                    for (char *p = header; *p; p++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
                    glColor3f(1.0f, 1.0f, 1.0f);
                } else {
                    for (char *p = fe->lines[j]; *p; p++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
                }
            }
            if (total_drawn > scroll_offset + max_visible_lines) break;
        }
        if (total_drawn > scroll_offset + max_visible_lines) break;
    }

    // Scrollbar
    if (total_lines_in_history > max_visible_lines) {
        float sb_w = 10.0f;
        float sb_x = window_width - sb_w - 2.0f;
        float track_h = window_height - 10.0f;
        
        glColor3f(0.1f, 0.1f, 0.1f);
        glBegin(GL_QUADS);
            glVertex2f(sb_x, 5); glVertex2f(sb_x+sb_w, 5);
            glVertex2f(sb_x+sb_w, 5+track_h); glVertex2f(sb_x, 5+track_h);
        glEnd();

        float thumb_h = (float)max_visible_lines / total_lines_in_history * track_h;
        if (thumb_h < 30.0f) thumb_h = 30.0f;
        
        float scroll_perc = (float)scroll_offset / (total_lines_in_history - max_visible_lines);
        float thumb_y = 5.0f + scroll_perc * (track_h - thumb_h);

        glColor3f(0.4f, 0.4f, 0.4f);
        glBegin(GL_QUADS);
            glVertex2f(sb_x+1, thumb_y); glVertex2f(sb_x+sb_w-1, thumb_y);
            glVertex2f(sb_x+sb_w-1, thumb_y+thumb_h); glVertex2f(sb_x+1, thumb_y+thumb_h);
        glEnd();
    }

    glutSwapBuffers();
}

void timer(int value) {
    struct stat st;
    if (stat("pieces/master_ledger/frame_changed.txt", &st) == 0) {
        if (st.st_size != last_marker_size) {
            parse_frame_file();
            last_marker_size = st.st_size;
        }
    }
    glutPostRedisplay();
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

    struct stat st;
    if (stat("pieces/master_ledger/frame_changed.txt", &st) == 0) last_marker_size = st.st_size;
    parse_frame_file();

    glutMainLoop();
    return 0;
}
