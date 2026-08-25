/* macOS compatible includes */
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif
#define MAX_WINDOWS 10
#define MAX_LINE 1024

typedef struct {
    int id;
    int x, y, w, h;
    char title[64];
    char content_source[MAX_PATH];
} GLVirtualWindow;

GLVirtualWindow windows[MAX_WINDOWS];
int window_count = 0;
int window_width = 800;
int window_height = 600;
off_t last_view_size = 0;
char project_root[MAX_PATH] = ".";

void resolve_root() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) kvp = fopen("../../../pieces/locations/location_kvp", "r");
    
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                v[strcspn(v, "\n\r")] = 0;
                if (strlen(v) > 0) strncpy(project_root, v, MAX_PATH-1);
                break;
            }
        }
        fclose(kvp);
    }
}

void build_p_path(char* dst, size_t sz, const char* rel) {
    snprintf(dst, sz, "%s/%s", project_root, rel);
}

void log_input(const char* event, int x, int y, int button, int state) {
    char path[MAX_PATH];
    build_p_path(path, sizeof(path), "pieces/apps/gl_os/session/history.txt");
    FILE *fp = fopen(path, "a");
    if (!fp) return;
    
    char buf[256];
    if (button != -1) {
        snprintf(buf, sizeof(buf), "MOUSE_CLICK: btn=%d, state=%d, x=%d, y=%d\n", button, state, x, y);
    } else {
        snprintf(buf, sizeof(buf), "MOUSE_MOTION: x=%d, y=%d\n", x, y);
    }
    
    fputs(buf, fp);
    fclose(fp);
}

void mouse_func(int button, int state, int x, int y) {
    log_input("CLICK", x, y, button, state);
}

void motion_func(int x, int y) {
    log_input("MOTION", x, y, -1, -1);
}

void parse_gl_os_view() {
    char path[MAX_PATH];
    build_p_path(path, sizeof(path), "pieces/apps/gl_os/view.txt");
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    
    char line[MAX_LINE];
    window_count = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "WINDOW", 6) == 0) {
            GLVirtualWindow *win = &windows[window_count++];
            sscanf(line, "WINDOW | %d | %d | %d | %d | %d | %63[^|] | %1023s", 
                   &win->id, &win->x, &win->y, &win->w, &win->h, win->title, win->content_source);
        }
    }
    fclose(fp);
}

void render_text_in_window(const char* content, int x, int y, int w, int h) {
    glColor3f(1.0f, 1.0f, 1.0f);
    float start_x = (float)x + 10.0f;
    float start_y = (float)(window_height - y) - 30.0f;
    glRasterPos2f(start_x, start_y);
    
    for (const char *p = content; *p; p++) {
        if (*p == '\n') {
            start_y -= 15.0f;
            glRasterPos2f(start_x, start_y);
        } else {
            glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
        }
    }
}

void display() {
    glClearColor(0.0f, 0.0f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, (float)window_width, 0, (float)window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    for (int i = 0; i < window_count; i++) {
        GLVirtualWindow *win = &windows[i];
        
        glColor3f(0.5f, 0.5f, 0.5f);
        glBegin(GL_LINE_LOOP);
            glVertex2f((float)win->x, (float)(window_height - win->y));
            glVertex2f((float)(win->x + win->w), (float)(window_height - win->y));
            glVertex2f((float)(win->x + win->w), (float)(window_height - (win->y + win->h)));
            glVertex2f((float)win->x, (float)(window_height - (win->y + win->h)));
        glEnd();
        
        // Window Title Bar (Slightly larger and darker to be distinct)
        glColor3f(0.2f, 0.2f, 0.2f);
        glBegin(GL_QUADS);
            glVertex2f((float)win->x, (float)(window_height - win->y));
            glVertex2f((float)(win->x + win->w), (float)(window_height - win->y));
            glVertex2f((float)(win->x + win->w), (float)(window_height - (win->y + 25)));
            glVertex2f((float)win->x, (float)(window_height - (win->y + 25)));
        glEnd();
        
        // Title Bar Border
        glColor3f(0.7f, 0.7f, 0.7f);
        glBegin(GL_LINE_LOOP);
            glVertex2f((float)win->x, (float)(window_height - win->y));
            glVertex2f((float)(win->x + win->w), (float)(window_height - win->y));
            glVertex2f((float)(win->x + win->w), (float)(window_height - (win->y + 25)));
            glVertex2f((float)win->x, (float)(window_height - (win->y + 25)));
        glEnd();
        
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f((float)(win->x + 5), (float)(window_height - (win->y + 15)));
        for (const char *p = win->title; *p; p++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
        
        FILE *content_f = fopen(win->content_source, "r");
        if (content_f) {
            fseek(content_f, 0, SEEK_END);
            long size = ftell(content_f);
            fseek(content_f, 0, SEEK_SET);
            char *buf = malloc(size + 1);
            if (buf) {
                fread(buf, 1, size, content_f);
                buf[size] = '\0';
                render_text_in_window(buf, win->x, win->y, win->w, win->h);
                free(buf);
            }
            fclose(content_f);
        }
    }
    glutSwapBuffers();
}

void timer(int value) {
    struct stat st;
    char path[MAX_PATH];
    build_p_path(path, sizeof(path), "pieces/apps/gl_os/view_changed.txt");
    if (stat(path, &st) == 0) {
        if (st.st_size != last_view_size) {
            parse_gl_os_view();
            last_view_size = st.st_size;
        }
    }
    glutPostRedisplay();
    glutTimerFunc(100, timer, 0);
}

int main(int argc, char** argv) {
    resolve_root();
    chdir(project_root);
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("PMO GL-OS Desktop");
    glutDisplayFunc(display);
    glutTimerFunc(100, timer, 0);
    glutMouseFunc(mouse_func);
    glutMotionFunc(motion_func);
    
    struct stat st;
    char path[MAX_PATH];
    build_p_path(path, sizeof(path), "pieces/apps/gl_os/view_changed.txt");
    if (stat(path, &st) == 0) last_view_size = st.st_size;
    parse_gl_os_view();
    
    glutMainLoop();
    return 0;
}
