/*
 * Joystick Input for Windows (Raw Input / XInput)
 * Uses XInput for Xbox controllers (built into Windows Vista+)
 * No external dependencies required
 * 
 * Compile: gcc -D_WIN32 joystick_input_win.c -o joystick_input.+x -lxinput -lpthread
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#pragma comment(lib, "xinput.lib")

#define THRESHOLD 16384
#define POLL_INTERVAL_MS 50

char base_path[256] = ".";

/* Check if GL-OS has input focus (TPM isolation protocol) */
int gl_os_has_focus(void) {
    struct stat st;
    char lock_path[512];
    snprintf(lock_path, sizeof(lock_path), "%s/pieces/apps/gl_os/session/input_focus.lock", base_path);
    return (stat(lock_path, &st) == 0);
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

    /* 1. Piece Ledger (Audit) */
    snprintf(full_path, sizeof(full_path), "%s/pieces/joystick", base_path);
    ensureDir("pieces/joystick");
    snprintf(full_path, sizeof(full_path), "%s/pieces/joystick/ledger.txt", base_path);
    FILE *ledger = fopen(full_path, "a");
    if (ledger) {
        fprintf(ledger, "[%s] JoyCaptured: %d | Info: %s\n", timestamp, key_code, source_info);
        fclose(ledger);
    }

    /* 2. Piece History (Stream) - SHARED with keyboard */
    snprintf(full_path, sizeof(full_path), "%s/pieces/keyboard", base_path);
    ensureDir("pieces/keyboard");
    snprintf(full_path, sizeof(full_path), "%s/pieces/keyboard/history.txt", base_path);
    FILE *history = fopen(full_path, "a");
    if (history) {
        fprintf(history, "[%s] KEY_PRESSED: %d\n", timestamp, key_code);
        fclose(history);
    }

    /* 3. Master Ledger (Global Audit) */
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
    /* Parse arguments: [base_path] */
    if (argc > 1) {
        if (argv[1][0] != '/') {
            /* Relative path - treat as base_path */
            strncpy(base_path, argv[1], sizeof(base_path) - 1);
            base_path[sizeof(base_path) - 1] = '\0';
        }
    }
    
    printf("Joystick Input (Windows/XInput) started\n");
    printf("Base path: %s\n", base_path);
    printf("Waiting for Xbox controller...\n");
    
    DWORD last_poll_time = 0;
    WORD last_buttons = 0;
    SHORT last_lx = 0, last_ly = 0, last_rx = 0, last_ry = 0;
    BYTE last_lt = 0, last_rt = 0;
    
    printf("Polling for input (press any button)...\n");
    
    while (1) {
        Sleep(POLL_INTERVAL_MS);
        
        DWORD current_time = GetTickCount();
        if (current_time - last_poll_time < POLL_INTERVAL_MS) {
            continue;
        }
        last_poll_time = current_time;
        
        /* Check all 4 controllers */
        for (DWORD i = 0; i < 4; i++) {
            XINPUT_STATE state;
            ZeroMemory(&state, sizeof(XINPUT_STATE));
            
            DWORD result = XInputGetState(i, &state);
            if (result != ERROR_SUCCESS) {
                continue;  /* Controller not connected */
            }
            
            /* Controller is connected */
            
            /* Check buttons (map to key codes 2000+) */
            WORD buttons = state.Gamepad.wButtons;
            if (buttons != last_buttons) {
                /* A button */
                if ((buttons & XINPUT_GAMEPAD_A) && !(last_buttons & XINPUT_GAMEPAD_A)) {
                    writeJoystickEvent(2000, "xinput_a");
                    printf("Controller %d: A button\n", i);
                }
                /* B button */
                if ((buttons & XINPUT_GAMEPAD_B) && !(last_buttons & XINPUT_GAMEPAD_B)) {
                    writeJoystickEvent(2001, "xinput_b");
                    printf("Controller %d: B button\n", i);
                }
                /* X button */
                if ((buttons & XINPUT_GAMEPAD_X) && !(last_buttons & XINPUT_GAMEPAD_X)) {
                    writeJoystickEvent(2002, "xinput_x");
                    printf("Controller %d: X button\n", i);
                }
                /* Y button */
                if ((buttons & XINPUT_GAMEPAD_Y) && !(last_buttons & XINPUT_GAMEPAD_Y)) {
                    writeJoystickEvent(2003, "xinput_y");
                    printf("Controller %d: Y button\n", i);
                }
                /* Left bumper */
                if ((buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) && !(last_buttons & XINPUT_GAMEPAD_LEFT_SHOULDER)) {
                    writeJoystickEvent(2004, "xinput_lb");
                    printf("Controller %d: Left bumper\n", i);
                }
                /* Right bumper */
                if ((buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) && !(last_buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
                    writeJoystickEvent(2005, "xinput_rb");
                    printf("Controller %d: Right bumper\n", i);
                }
                /* Back button */
                if ((buttons & XINPUT_GAMEPAD_BACK) && !(last_buttons & XINPUT_GAMEPAD_BACK)) {
                    writeJoystickEvent(2006, "xinput_back");
                    printf("Controller %d: Back button\n", i);
                }
                /* Start button */
                if ((buttons & XINPUT_GAMEPAD_START) && !(last_buttons & XINPUT_GAMEPAD_START)) {
                    writeJoystickEvent(2007, "xinput_start");
                    printf("Controller %d: Start button\n", i);
                }
                /* Left stick press */
                if ((buttons & XINPUT_GAMEPAD_LEFT_THUMB) && !(last_buttons & XINPUT_GAMEPAD_LEFT_THUMB)) {
                    writeJoystickEvent(2008, "xinput_l3");
                    printf("Controller %d: Left stick press\n", i);
                }
                /* Right stick press */
                if ((buttons & XINPUT_GAMEPAD_RIGHT_THUMB) && !(last_buttons & XINPUT_GAMEPAD_RIGHT_THUMB)) {
                    writeJoystickEvent(2009, "xinput_r3");
                    printf("Controller %d: Right stick press\n", i);
                }
                /* D-Pad */
                if ((buttons & XINPUT_GAMEPAD_DPAD_UP) && !(last_buttons & XINPUT_GAMEPAD_DPAD_UP)) {
                    writeJoystickEvent(2010, "xinput_dpad_up");
                    printf("Controller %d: D-Pad Up\n", i);
                }
                if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) && !(last_buttons & XINPUT_GAMEPAD_DPAD_DOWN)) {
                    writeJoystickEvent(2011, "xinput_dpad_down");
                    printf("Controller %d: D-Pad Down\n", i);
                }
                if ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) && !(last_buttons & XINPUT_GAMEPAD_DPAD_LEFT)) {
                    writeJoystickEvent(2012, "xinput_dpad_left");
                    printf("Controller %d: D-Pad Left\n", i);
                }
                if ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) && !(last_buttons & XINPUT_GAMEPAD_DPAD_RIGHT)) {
                    writeJoystickEvent(2013, "xinput_dpad_right");
                    printf("Controller %d: D-Pad Right\n", i);
                }
                
                last_buttons = buttons;
            }
            
            /* Check left stick X axis (map to 2100+) */
            SHORT lx = state.Gamepad.sThumbLX;
            if (abs(lx - last_lx) > THRESHOLD) {
                if (lx < -THRESHOLD) {
                    writeJoystickEvent(2100, "xinput_lx_neg");
                    printf("Controller %d: Left stick left\n", i);
                } else if (lx > THRESHOLD) {
                    writeJoystickEvent(2101, "xinput_lx_pos");
                    printf("Controller %d: Left stick right\n", i);
                }
                last_lx = lx;
            }
            
            /* Check left stick Y axis (map to 2102+) */
            SHORT ly = state.Gamepad.sThumbLY;
            if (abs(ly - last_ly) > THRESHOLD) {
                if (ly < -THRESHOLD) {
                    writeJoystickEvent(2102, "xinput_ly_neg");
                    printf("Controller %d: Left stick down\n", i);
                } else if (ly > THRESHOLD) {
                    writeJoystickEvent(2103, "xinput_ly_pos");
                    printf("Controller %d: Left stick up\n", i);
                }
                last_ly = ly;
            }
            
            /* Check right stick X axis (map to 2104+) */
            SHORT rx = state.Gamepad.sThumbRX;
            if (abs(rx - last_rx) > THRESHOLD) {
                if (rx < -THRESHOLD) {
                    writeJoystickEvent(2104, "xinput_rx_neg");
                } else if (rx > THRESHOLD) {
                    writeJoystickEvent(2105, "xinput_rx_pos");
                }
                last_rx = rx;
            }
            
            /* Check right stick Y axis (map to 2106+) */
            SHORT ry = state.Gamepad.sThumbRY;
            if (abs(ry - last_ry) > THRESHOLD) {
                if (ry < -THRESHOLD) {
                    writeJoystickEvent(2106, "xinput_ry_neg");
                } else if (ry > THRESHOLD) {
                    writeJoystickEvent(2107, "xinput_ry_pos");
                }
                last_ry = ry;
            }
            
            /* Check triggers (map to 2108+) */
            BYTE lt = state.Gamepad.bLeftTrigger;
            if (abs(lt - last_lt) > 127) {
                if (lt > 127) {
                    writeJoystickEvent(2108, "xinput_lt");
                    printf("Controller %d: Left trigger\n", i);
                }
                last_lt = lt;
            }
            
            BYTE rt = state.Gamepad.bRightTrigger;
            if (abs(rt - last_rt) > 127) {
                if (rt > 127) {
                    writeJoystickEvent(2109, "xinput_rt");
                    printf("Controller %d: Right trigger\n", i);
                }
                last_rt = rt;
            }
        }
    }
    
    return 0;
}
