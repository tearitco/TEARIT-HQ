#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

/* macOS/Windows compatibility: linux/joystick.h doesn't exist
 * We define the minimal struct needed for joystick events
 * 
 * NOTE (April 2, 2026): Windows XInput (Xbox) controllers use a completely
 * different API (xinput.h). This file is Linux-focused. Windows support
 * should be split into a separate joystick_input_win.c file using XInput.
 * See: PITFALLS_ACTIVE_2026-03-18.txt #45
 */
#if defined(__APPLE__) || defined(_WIN32)
// macOS/Windows doesn't have linux/joystick.h, define minimal structures
struct js_event {
    unsigned int time;     /* milliseconds */
    short value;           /* value for axis/button */
    unsigned char type;    /* event type */
    unsigned char number;  /* axis/button number */
};
#define JS_EVENT_BUTTON 0x01
#define JS_EVENT_AXIS 0x02
#define JS_EVENT_INIT 0x80
#else
#include <linux/joystick.h>
#endif

/* Standard ASCII-like codes (2000 range)
 * Buttons: 2000 + button_number
 * Axes: 2100 + (axis_number * 2) [+ 0 for negative, + 1 for positive]
 */

#define THRESHOLD 16384

char base_path[256] = ".";  // Default to current directory

/* Check if GL-OS has input focus (TPM isolation protocol)
 * GL-OS receives input directly via GLUT callbacks, so joystick
 * should not write to CHTPM input files when GL-OS is active.
 */
int gl_os_has_focus(void) {
    struct stat st;
    char *lock_path = NULL;
    int focus = 0;
    if (asprintf(&lock_path, "%s/pieces/apps/gl_os/session/input_focus.lock", base_path) != -1) {
        focus = (stat(lock_path, &st) == 0);  /* File exists = GL-OS has focus */
        free(lock_path);
    }
    return focus;
}

void ensureDir(const char *path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s/%s", base_path, path);
    system(cmd);
}

void writeJoystickEvent(int key_code, const char *source_info) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    char full_path[512];
    
    // 1. Piece Ledger (Audit)
    snprintf(full_path, sizeof(full_path), "%s/pieces/joystick", base_path);
    ensureDir("pieces/joystick");
    snprintf(full_path, sizeof(full_path), "%s/pieces/joystick/ledger.txt", base_path);
    FILE *ledger = fopen(full_path, "a");
    if (ledger) {
        fprintf(ledger, "[%s] JoyCaptured: %d | Info: %s\n", timestamp, key_code, source_info);
        fclose(ledger);
    }
    
    // 2. Piece History (Stream) - SHARED with keyboard
    snprintf(full_path, sizeof(full_path), "%s/pieces/keyboard", base_path);
    ensureDir("pieces/keyboard");
    snprintf(full_path, sizeof(full_path), "%s/pieces/keyboard/history.txt", base_path);
    FILE *history = fopen(full_path, "a");
    if (history) {
        fprintf(history, "[%s] KEY_PRESSED: %d\n", timestamp, key_code);
        fclose(history);
    }
    
    // 3. Master Ledger (Global Audit)
    snprintf(full_path, sizeof(full_path), "%s/pieces/master_ledger", base_path);
    ensureDir("pieces/master_ledger");
    snprintf(full_path, sizeof(full_path), "%s/pieces/master_ledger/master_ledger.txt", base_path);
    FILE *master = fopen(full_path, "a");
    if (master) {
        fprintf(master, "[%s] InputReceived: key_code=%d | Source: joystick_input\n", timestamp, key_code);
        fclose(master);
    }
}

int main(int argc, char *argv[]) {
    const char *device = "/dev/input/js0";
    
    // Parse arguments: [base_path] [device]
    if (argc > 1) {
        if (argv[1][0] != '/') {
            // Relative path - treat as base_path
            strncpy(base_path, argv[1], sizeof(base_path) - 1);
            base_path[sizeof(base_path) - 1] = '\0';
            if (argc > 2) device = argv[2];
        } else {
            // Absolute path - treat as device
            device = argv[1];
        }
    }
    
    // Ensure directories exist before logging
    ensureDir("pieces/joystick");
    ensureDir("pieces/keyboard");
    ensureDir("pieces/master_ledger");
    
    // TPM Audit: Log startup attempt
    char boot_path[512];
    snprintf(boot_path, sizeof(boot_path), "%s/pieces/joystick/ledger.txt", base_path);
    FILE *boot_log = fopen(boot_path, "a");
    if (boot_log) {
        fprintf(boot_log, "--- Joystick Piece Starting (Base: %s, Device: %s) ---\n", base_path, device);
        fclose(boot_log);
    }

    int js = open(device, O_RDONLY);
    if (js == -1) {
        // Log failure but don't exit if we want the orchestrator thread to just "hang around" or fail quietly
        FILE *err_log = fopen("pieces/joystick/ledger.txt", "a");
        if (err_log) {
            fprintf(err_log, "[ERROR] Could not open joystick device: %s\n", device);
            fclose(err_log);
        }
        perror("Could not open joystick");
        return 1;
    }

    struct js_event event;
    int axis_pushed[128] = {0}; // Track if axis direction is already "pushed" (to avoid spam)

    while (read(js, &event, sizeof(event)) == sizeof(event)) {
        char info[128];
        if (event.type & JS_EVENT_INIT) continue;

        /* TPM INPUT ISOLATION: Check if GL-OS has focus
         * GL-OS receives input directly via GLUT callbacks.
         * When GL-OS is active, skip writing to CHTPM input files.
         */
        if (gl_os_has_focus()) {
            continue;
        }

        if (event.type == JS_EVENT_BUTTON) {
            if (event.value) { // Pressed
                int code = 2000 + event.number;
                snprintf(info, sizeof(info), "Button %u pressed", event.number);
                writeJoystickEvent(code, info);
            }
        } else if (event.type == JS_EVENT_AXIS) {
            int axis_num = event.number;
            int base_code = 2100 + (axis_num * 2);
            
            if (axis_num * 2 + 1 >= 128) continue; // Safety boundary

            // Negative direction (Left/Up)
            if (event.value < -THRESHOLD) {
                if (!axis_pushed[base_code]) {
                    snprintf(info, sizeof(info), "Axis %d negative threshold crossed", axis_num);
                    writeJoystickEvent(base_code, info);
                    axis_pushed[base_code] = 1;
                }
            } else if (event.value > -THRESHOLD / 2) { // Deadzone return
                axis_pushed[base_code] = 0;
            }

            // Positive direction (Right/Down)
            if (event.value > THRESHOLD) {
                if (!axis_pushed[base_code + 1]) {
                    snprintf(info, sizeof(info), "Axis %d positive threshold crossed", axis_num);
                    writeJoystickEvent(base_code + 1, info);
                    axis_pushed[base_code + 1] = 1;
                }
            } else if (event.value < THRESHOLD / 2) { // Deadzone return
                axis_pushed[base_code + 1] = 0;
            }
        }
    }

    close(js);
    return 0;
}
