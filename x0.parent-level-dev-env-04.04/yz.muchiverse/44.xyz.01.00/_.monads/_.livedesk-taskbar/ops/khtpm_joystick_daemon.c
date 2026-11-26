/*
 * khtpm_joystick_daemon.c - house-wide joystick arrow input, v1.
 *
 * REAL, NEW 2026-09-15, direct live request ("we want to make sure
 * entire x11-hq house has ability to use joystick arrows for input...
 * up down left right should be mapped like arrow behavior in 1.tmpos.
 * the rest can be done later") - v1 scope is arrows only (axis 0/1);
 * buttons/.pdl remap are real, deliberate follow-up work, not stubbed
 * silently - see 08-roadmap/design-docs/JOYSTICK-INPUT-HOUSE-WIDE-
 * DESIGN.md for the full plan this is step 1 of.
 *
 * Reads the raw Linux joystick API (struct js_event off /dev/input/
 * jsN - NOT SDL/evdev/XInput2), same API the sibling prototype tree
 * 1.TPMOS_c_+rmmp.0103.0001's own pieces/joystick/plugins/
 * joystick_input.c uses (confirmed via direct code read - this is a
 * fresh implementation for this house, not a copy of that file, but
 * the axis-threshold/hysteresis numbers below match its real,
 * confirmed-working values). Live-verified against real hardware this
 * session (a USB Gamepad at /dev/input/js0, 6 axes/12 buttons per
 * JSIOCGAXES/JSIOCGBUTTONS - its D-pad reports on axis 0 (left/right,
 * negative=LEFT/positive=RIGHT) and axis 1 (up/down, negative=UP/
 * positive=DOWN), full ±32767/0 swing, no intermediate values - a
 * live capture confirmed this sign convention directly, not assumed).
 *
 * REAL FIX 2026-09-15, direct live report ("the joy keys should be
 * consumed and interpreted from same file as arrow keys, just like in
 * tpmos. is that std?") - yes. First version of this daemon wrote to
 * a new shared house-wide file with each window self-gating on its
 * own focus flag; scrapped in favor of this, which writes directly
 * into the SAME per-pid relay file real arrow-key relay/agent testing
 * already uses and already proved reliable:
 *   #.desktop/entity_menu_history/<pid>.txt
 * using the same reserved codes every khtpm_core_render.c window's
 * poll_agent_history() already understands (200/201/202/203 =
 * Up/Down/Left/Right) - so a focused window needs ZERO new
 * consumption code; it already polls its own file every tick.
 * The daemon resolves WHICH pid to target by reading a tiny, universal
 * marker file every window's own redraw() now writes while it
 * genuinely holds real X focus (the same focus_win==win check the
 * "^"/"." title indicator already uses):
 *   #.desktop/joystick_focused_pid.txt
 *
 * Threshold/hysteresis matches the TPMOS reference's own real,
 * confirmed values (THRESHOLD 16384, deadzone THRESHOLD/2) - avoids
 * repeat-spam on a held axis without needing a timer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

typedef struct {
    unsigned int time;
    short value;
    unsigned char type;
    unsigned char number;
} JsEvent;

#define JS_EVENT_BUTTON 0x01
#define JS_EVENT_AXIS   0x02
#define JS_EVENT_INIT   0x80

#define THRESHOLD      16384
#define DEADZONE       (THRESHOLD / 2)

#define KEY_UP    200
#define KEY_DOWN  201
#define KEY_LEFT  202
#define KEY_RIGHT 203

/* Reads the tiny "who last painted itself as focused" marker - see
 * this file's own header comment. Returns 0 if no window has ever
 * claimed focus yet (nothing to target - a real, harmless startup
 * state, not an error). */
static int read_focused_pid(const char *house_root) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/#.desktop/joystick_focused_pid.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int pid = 0;
    if (fscanf(f, "%d", &pid) != 1) pid = 0;
    fclose(f);
    return pid;
}

static void append_key(const char *house_root, int code) {
    int pid = read_focused_pid(house_root);
    if (pid <= 0) return; /* nothing focused - drop it, same as a keypress with no window to receive it */
    char path[4096];
    snprintf(path, sizeof(path), "%s/#.desktop/entity_menu_history/%d.txt", house_root, pid);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "KEY_PRESSED: %d\n", code);
    fclose(f);
}

/* v2 (owner 2026-11-26: "nothing is mapped to enter yet"): BUTTONS. Every button press is (1) remembered as one line in #.desktop/joystick_buttons_seen.txt, the LAST 16 only (a cap by construction, rewritten whole), so the owner can press a button at any time and an agent reads which number it was, and (2) turned into a key if #.desktop/joystick_map.pdl has a row `BUTTON | <number> | <key code>` (the file is read on every press, so a remap needs no restart; no row = no key, nothing is invented). Design: 08-roadmap/design-docs/JOYSTICK-INPUT-HOUSE-WIDE-DESIGN.md section 3. */
static void note_button(const char *house_root, int n) {
    char path[4096], tmp[4200], keep[16][32]; int cnt = 0;
    snprintf(path, sizeof(path), "%s/#.desktop/joystick_buttons_seen.txt", house_root); snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(path, "r"); if (f) { char l[64]; while (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (cnt == 15) { memmove(keep[0], keep[1], 15 * 32); cnt = 14; } snprintf(keep[cnt++], 32, "%.31s", l); } fclose(f); }
    f = fopen(tmp, "w"); if (!f) return; for (int i = 0; i < cnt; i++) fprintf(f, "%s\n", keep[i]); fprintf(f, "button %d\n", n); fclose(f); rename(tmp, path); }
static int mapped_code(const char *house_root, int n) {
    char path[4096], l[256]; snprintf(path, sizeof(path), "%s/#.desktop/joystick_map.pdl", house_root); FILE *f = fopen(path, "r"); if (!f) return 0; int code = 0;
    while (fgets(l, sizeof l, f)) { char *a = strtok(l, "|"), *b = strtok(NULL, "|"), *c = strtok(NULL, "|"); if (!a || !b || !c) continue; while (*a == ' ') a++; if (strncmp(a, "BUTTON", 6)) continue; if (atoi(b) == n && (b[strspn(b, " ")] >= '0' && b[strspn(b, " ")] <= '9')) { code = atoi(c); break; } }
    fclose(f); return code; }
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <house_root> [device]\n", argv[0]);
        return 1;
    }
    const char *house_root = argv[1];
    const char *device = argc > 2 ? argv[2] : "/dev/input/js0";

    FILE *jf = fopen(device, "rb");
    if (!jf) {
        /* No joystick plugged in - a real, non-fatal condition, not an
         * error the launcher should treat as a crash (matches the
         * design doc's own "don't hard-fail desktop boot" note). */
        fprintf(stderr, "khtpm_joystick_daemon: no device at %s, exiting quietly\n", device);
        return 0;
    }
    setvbuf(jf, NULL, _IONBF, 0);

    /* Real, small state machine per axis: last "zone" (neg/zero/pos)
     * with hysteresis, so a held axis emits ONE KEY_PRESSED on the
     * crossing, not a flood every event - same real problem/fix the
     * TPMOS reference's own THRESHOLD/DEADZONE pair solves. */
    int zone_x = 0, zone_y = 0; /* -1, 0, +1 */

    JsEvent ev;
    while (fread(&ev, sizeof(ev), 1, jf) == 1) {
        if (ev.type & JS_EVENT_INIT) continue;
        unsigned char type = ev.type & ~JS_EVENT_INIT;
        if (type == JS_EVENT_BUTTON) { if (ev.value == 1) { note_button(house_root, ev.number); int mc = mapped_code(house_root, ev.number); if (mc > 0) append_key(house_root, mc); } continue; }
        if (type != JS_EVENT_AXIS) continue;
        if (ev.number == 0) {
            int nz = zone_x;
            if (ev.value < -THRESHOLD) nz = -1;
            else if (ev.value > THRESHOLD) nz = 1;
            else if (ev.value > -DEADZONE && ev.value < DEADZONE) nz = 0;
            if (nz != zone_x) {
                zone_x = nz;
                if (nz == -1) append_key(house_root, KEY_LEFT);
                else if (nz == 1) append_key(house_root, KEY_RIGHT);
            }
        } else if (ev.number == 1) {
            int nz = zone_y;
            if (ev.value < -THRESHOLD) nz = -1;
            else if (ev.value > THRESHOLD) nz = 1;
            else if (ev.value > -DEADZONE && ev.value < DEADZONE) nz = 0;
            if (nz != zone_y) {
                zone_y = nz;
                if (nz == -1) append_key(house_root, KEY_UP);
                else if (nz == 1) append_key(house_root, KEY_DOWN);
            }
        }
        /* other axes (2-5 on the live-tested pad - triggers/right
         * stick) deliberately ignored in v1, same real scope note as
         * the file header. */
    }
    fclose(jf);
    return 0;
}
