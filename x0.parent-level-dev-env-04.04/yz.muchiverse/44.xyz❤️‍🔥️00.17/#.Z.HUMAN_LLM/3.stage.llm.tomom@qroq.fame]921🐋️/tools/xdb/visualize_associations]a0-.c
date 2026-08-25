#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#define MAX_WORDS 1000
#define NODE_RADIUS 0.06f
#define SPRING_LENGTH 0.5f
#define SPRING_CONSTANT 0.1f
#define REPULSION_CONSTANT 0.5f
#define DAMPING 0.8f
#define TIME_STEP 0.1f

typedef struct {
    char word[100];
    float x, y;
    float vx, vy;
} Node;

typedef struct {
    int from, to;
    float strength;
} Edge;

Node nodes[MAX_WORDS];
Edge edges[MAX_WORDS * MAX_WORDS];
int node_count = 0;
int edge_count = 0;
float zoom = 1.0f;
float offset_x = 0.0f, offset_y = 0.0f;
int dragging = 0;
int last_x, last_y;

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
    node_count = word_count > 100 ? 100 : word_count; // Limit to 100 for performance
    for (int i = 0; i < node_count; i++) {
        strcpy(nodes[i].word, words[i]);
        nodes[i].x = (float)(rand() % 100) / 50.0f - 1.0f;
        nodes[i].y = (float)(rand() % 100) / 50.0f - 1.0f;
        nodes[i].vx = 0.0f;
        nodes[i].vy = 0.0f;
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
            if (assoc > 0.8 && row != col) {
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
            if (distance_sq > 0.01f) { // Avoid division by zero
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

// Rendering
void render() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    glPushMatrix();
    glScalef(zoom, zoom, 1.0f);
    glTranslatef(offset_x, offset_y, 0.0f);
    
    // Draw edges
    glBegin(GL_LINES);
    for (int i = 0; i < edge_count; i++) {
        int from = edges[i].from;
        int to = edges[i].to;
        float strength = edges[i].strength;
        
        // Color based on strength
        glColor3f(strength, strength * 0.5f, 1.0f - strength);
        glVertex2f(nodes[from].x, nodes[from].y);
        glVertex2f(nodes[to].x, nodes[to].y);
    }
    glEnd();
    
    // Draw nodes
    for (int i = 0; i < node_count; i++) {
        glPushMatrix();
        glTranslatef(nodes[i].x, nodes[i].y, 0.0f);
        
        // Draw circle
        glColor3f(0.5f, 0.7f, 1.0f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(0.0f, 0.0f);
        for (int j = 0; j <= 20; j++) {
            float angle = j * 2.0f * M_PI / 20;
            glVertex2f(cosf(angle) * NODE_RADIUS, sinf(angle) * NODE_RADIUS);
        }
        glEnd();
        
        // Draw border
        glColor3f(0.0f, 0.0f, 0.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j <= 20; j++) {
            float angle = j * 2.0f * M_PI / 20;
            glVertex2f(cosf(angle) * NODE_RADIUS, sinf(angle) * NODE_RADIUS);
        }
        glEnd();
        
        glPopMatrix();
    }
    
    // Draw labels for some important nodes
    for (int i = 0; i < node_count && i < 20; i++) {  // Limit labels for clarity
        glPushMatrix();
        glTranslatef(nodes[i].x, nodes[i].y, 0.0f);
        
        glColor3f(0.0f, 0.0f, 0.0f);
        glRasterPos2f(NODE_RADIUS, NODE_RADIUS);
        
        // Simple character drawing (just first few chars for demo)
        for (int j = 0; nodes[i].word[j] && j < 5; j++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, nodes[i].word[j]);
        }
        
        glPopMatrix();
    }
    
    glPopMatrix();
    
    glutSwapBuffers();
}

// Idle function for animation
void idle() {
    update_physics();
    glutPostRedisplay();
}

// Mouse handling
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
            zoom = 1.0f;
            offset_x = 0.0f;
            offset_y = 0.0f;
        }
    } else if (button == 3) { // Scroll up
        zoom *= 1.1f;
    } else if (button == 4) { // Scroll down
        zoom /= 1.1f;
    }
}

void motion(int x, int y) {
    if (dragging) {
        offset_x += (x - last_x) / (100.0f * zoom);
        offset_y -= (y - last_y) / (100.0f * zoom);
        last_x = x;
        last_y = y;
        glutPostRedisplay();
    }
}

// Keyboard handling
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: // ESC
            exit(0);
            break;
        case 'r':
        case 'R':
            // Reset node positions
            for (int i = 0; i < node_count; i++) {
                nodes[i].x = (float)(rand() % 100) / 50.0f - 1.0f;
                nodes[i].y = (float)(rand() % 100) / 50.0f - 1.0f;
                nodes[i].vx = 0.0f;
                nodes[i].vy = 0.0f;
            }
            break;
    }
}

// Initialization
void init() {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    srand(0); // For deterministic node placement
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <associations.csv>\n", argv[0]);
        fprintf(stderr, "Controls:\n");
        fprintf(stderr, "  Left mouse: Pan view\n");
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
    glutInitWindowSize(800, 600);
    glutCreateWindow("Word Association Visualization");
    
    init();
    
    glutDisplayFunc(render);
    glutIdleFunc(idle);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    
    printf("Word Association Visualization\n");
    printf("Controls:\n");
    printf("  Left mouse: Pan view\n");
    printf("  Right mouse: Reset view\n");
    printf("  Scroll: Zoom in/out\n");
    printf("  R: Reset node positions\n");
    printf("  ESC: Quit\n");
    
    glutMainLoop();
    return 0;
}