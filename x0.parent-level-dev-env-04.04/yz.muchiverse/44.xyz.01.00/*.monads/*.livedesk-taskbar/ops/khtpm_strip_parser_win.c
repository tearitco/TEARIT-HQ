/* khtpm_strip_parser_win.c — Windows GDI surface for the livedesk strip.
 *
 * Linux truth remains khtpm_strip_parser.c (Xlib). This file is compiled
 * ONLY on Windows to the same binary name crypt_autostart looks up:
 *   ops/+x/khtpm_strip_parser.exe
 * Linux build_khtpm_strip.sh / khtpm_strip_parser.c are not touched.
 *
 * Same two-process contract as Linux:
 *   spawn khtpm_taskbar_manager_main.exe (sibling)
 *   poll #.desktop/strip_state.txt + strip_frame_changed.txt
 *   write bare-decimal codes to #.desktop/strip_history.txt
 *
 * Visual: top header strip + bottom tab bar + HQ popup. Gold-on-black
 * default matches livedesk_theme.pdl. Not a full Xft/layout-engine port
 * — enough to confirm we are on the real khtpm path, not legacy tp_taskbar.
 */
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <wchar.h>
#include <io.h>
#include <sys/stat.h>
#include "khtpm_taskbar_manager.h"
#include "khtpm_strip_codes.h"

#define SP_PATH 4352
#define MAX_TABS KTB_MAX_TABS
#define MAX_HQ   KTB_LIVEDESK_DYN_MAX
#define N_CELLS  15

static char g_house[SP_PATH];
static HANDLE g_manager_proc = NULL;
static HWND g_hdr, g_bot, g_pop;
static int g_popup_on = 0;
static FILE *g_log = NULL;

static COLORREF g_bg = RGB(0, 0, 0);
static COLORREF g_fg = RGB(0xea, 0xb3, 0x08);
static int g_hq_open = 0;
static int g_hq_n = 0;
static int g_n_tabs = 0;
static int g_tab_focus = 0;
static int g_hdr_focus = 0;
static char g_cells[N_CELLS][64];
static char g_tabs[MAX_TABS][64];
static char g_hqitem[MAX_HQ][64];
static long g_dirty_sz = -1;
static int g_ox = 0, g_oy = 0;

static const char *k_fallback[N_CELLS] = {
    "HQ", "user", "file", "desks", "pals", "palettes", "edit",
    "player", "db", "plugins", "toys", "store", "network", "h-ai", "time"
};

static void slog(const char *fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

static FILE *rel_fopen(const char *rel, const char *mode) {
    char path[SP_PATH];
    if (g_house[0] && strcmp(g_house, ".") != 0)
        snprintf(path, sizeof(path), "%s\\%s", g_house, rel);
    else
        snprintf(path, sizeof(path), "%s", rel);
    for (char *p = path; *p; p++) if (*p == '/') *p = '\\';
    wchar_t wp[SP_PATH], wm[16];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, SP_PATH)) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, 0, mode, -1, wm, 16)) return NULL;
    return _wfopen(wp, wm);
}

static void read_small(const char *rel, char *out, size_t n) {
    out[0] = '\0';
    FILE *f = rel_fopen(rel, "r");
    if (!f) return;
    size_t got = fread(out, 1, n - 1, f);
    out[got] = '\0';
    while (got > 0 && (out[got - 1] == '\n' || out[got - 1] == '\r'))
        out[--got] = '\0';
    fclose(f);
}

static void trim(char *s) {
    char *a = s;
    while (*a == ' ' || *a == '\t') a++;
    if (a != s) memmove(s, a, strlen(a) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                      s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
}

static COLORREF parse_color(const char *s, COLORREF fb) {
    if (!s || !s[0]) return fb;
    if (_stricmp(s, "black") == 0) return RGB(0, 0, 0);
    if (_stricmp(s, "white") == 0) return RGB(255, 255, 255);
    const char *h = (s[0] == '#') ? s + 1 : s;
    unsigned r = 0, g = 0, b = 0;
    if (strlen(h) >= 6 && sscanf(h, "%02x%02x%02x", &r, &g, &b) == 3)
        return RGB(r, g, b);
    return fb;
}

static void load_offsets(void) {
    g_ox = 0;
    g_oy = 0; /* Windows: no GNOME panel. PDL y=50 is a Linux gnome pad. */
    FILE *f = rel_fopen("#.desktop/livedesk_taskbar.pdl", "r");
    if (!f) return;
    char line[SP_PATH];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "strip_x_offset")) {
            char *p = strrchr(line, '|');
            if (p) g_ox = atoi(p + 1);
        } else if (strstr(line, "strip_y_offset")) {
            /* keep y=0 on Win so the strip is visible at the top (no GNOME panel) */
        }
    }
    fclose(f);
}

static void send_code(int code) {
    FILE *f = rel_fopen("#.desktop/strip_history.txt", "a");
    if (!f) {
        slog("send_code fopen failed code=%d", code);
        return;
    }
    fprintf(f, "%d\n", code);
    fclose(f);
    slog("send_code %d", code);
}

static int dirty(void) {
    struct _stat st;
    char path[SP_PATH];
    snprintf(path, sizeof(path), "#.desktop\\strip_frame_changed.txt");
    if (_stat(path, &st) != 0) return 0;
    if (st.st_size != g_dirty_sz) {
        g_dirty_sz = st.st_size;
        return 1;
    }
    return 0;
}

static void load_state(void) {
    int i;
    for (i = 0; i < N_CELLS; i++)
        snprintf(g_cells[i], sizeof(g_cells[i]), "%s", k_fallback[i]);
    g_n_tabs = 0;
    g_hq_n = 0;
    g_hq_open = 0;

    char u[128], fl[128], dl[128], dt[128];
    read_small("#.desktop/strip_var_username.txt", u, sizeof(u));
    read_small("#.desktop/strip_var_file_label.txt", fl, sizeof(fl));
    read_small("#.desktop/strip_var_desks_label.txt", dl, sizeof(dl));
    read_small("#.desktop/strip_var_datetime.txt", dt, sizeof(dt));
    if (u[0]) snprintf(g_cells[1], sizeof(g_cells[1]), "%s", u);
    if (fl[0]) snprintf(g_cells[2], sizeof(g_cells[2]), "%s", fl);
    if (dl[0]) snprintf(g_cells[3], sizeof(g_cells[3]), "%s", dl);
    if (dt[0]) snprintf(g_cells[14], sizeof(g_cells[14]), "%s", dt);

    FILE *f = rel_fopen("#.desktop/strip_state.txt", "r");
    if (!f) return;
    char line[SP_PATH];
    while (fgets(line, sizeof(line), f)) {
        char buf[SP_PATH];
        snprintf(buf, sizeof(buf), "%s", line);
        char *c0 = strtok(buf, "|");
        if (!c0) continue;
        trim(c0);
        if (strcmp(c0, "TAB") == 0) {
            strtok(NULL, "|"); /* pid */
            strtok(NULL, "|"); /* nav */
            char *ent = strtok(NULL, "|");
            if (ent && g_n_tabs < MAX_TABS) {
                trim(ent);
                snprintf(g_tabs[g_n_tabs], sizeof(g_tabs[g_n_tabs]), "%s", ent);
                g_n_tabs++;
            }
        } else if (strcmp(c0, "HQITEM") == 0) {
            char *lab = strtok(NULL, "|");
            if (lab && g_hq_n < MAX_HQ) {
                trim(lab);
                snprintf(g_hqitem[g_hq_n], sizeof(g_hqitem[g_hq_n]), "%s", lab);
                g_hq_n++;
            }
        } else if (strcmp(c0, "KEY") == 0) {
            char *key = strtok(NULL, "|");
            char *val = strtok(NULL, "\n");
            if (!key || !val) continue;
            trim(key); trim(val);
            if (strcmp(key, "theme_bg") == 0) g_bg = parse_color(val, g_bg);
            else if (strcmp(key, "theme_fg") == 0) g_fg = parse_color(val, g_fg);
            else if (strcmp(key, "hq_open") == 0) g_hq_open = atoi(val);
            else if (strcmp(key, "tab_focus_idx") == 0) g_tab_focus = atoi(val);
            else if (strcmp(key, "strip_focus_cell") == 0) {
                int c = atoi(val);
                if (c >= 0 && c < N_CELLS) g_hdr_focus = c;
            }
        }
    }
    fclose(f);
}

static void utf8_to_wide(const char *s, wchar_t *w, int wn) {
    if (!s) { w[0] = 0; return; }
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, w, wn))
        MultiByteToWideChar(CP_ACP, 0, s, -1, w, wn);
}

static void fill_win(HWND hwnd, COLORREF bg) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    HDC hdc = GetDC(hwnd);
    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    ReleaseDC(hwnd, hdc);
}

static void draw_row(HWND hwnd, const char labels[][64], int n, int focus,
                     int cell_w, int with_gt) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    HDC hdc = GetDC(hwnd);
    HBRUSH br = CreateSolidBrush(g_bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, g_fg);
    HFONT font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                             DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                             FF_DONTCARE, L"Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, font);
    HPEN pen = CreatePen(PS_SOLID, 1, g_fg);
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    int i;
    for (i = 0; i < n; i++) {
        int x0 = i * cell_w;
        int x1 = x0 + cell_w;
        MoveToEx(hdc, x1, 2, NULL);
        LineTo(hdc, x1, rc.bottom - 2);
        RECT tr = { x0 + 4, 4, x1 - 4, rc.bottom - 4 };
        wchar_t wlab[128];
        char shown[96];
        if (with_gt && i == focus)
            snprintf(shown, sizeof(shown), "[>] %s", labels[i]);
        else
            snprintf(shown, sizeof(shown), "%s", labels[i]);
        utf8_to_wide(shown, wlab, 128);
        DrawTextW(hdc, wlab, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    SelectObject(hdc, oldp);
    DeleteObject(pen);
    SelectObject(hdc, old);
    DeleteObject(font);
    ReleaseDC(hwnd, hdc);
}

static void layout_windows(void) {
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int sw = wa.right - wa.left;
    int sh = wa.bottom - wa.top;
    int hdr_w = sw - g_ox;
    if (hdr_w < 400) hdr_w = sw;
    SetWindowPos(g_hdr, HWND_TOPMOST, wa.left + g_ox, wa.top + g_oy,
                 hdr_w, KTB_BAR_H, SWP_SHOWWINDOW);
    SetWindowPos(g_bot, HWND_TOPMOST, wa.left, wa.bottom - KTB_BAR_H,
                 sw, KTB_BAR_H, SWP_SHOWWINDOW);
    if (g_hq_open && g_hq_n > 0) {
        int ph = 8 + g_hq_n * KTB_BAR_H;
        if (ph > sh / 2) ph = sh / 2;
        SetWindowPos(g_pop, HWND_TOPMOST, wa.left + g_ox, wa.top + g_oy + KTB_BAR_H,
                     280, ph, SWP_SHOWWINDOW);
        g_popup_on = 1;
        ShowWindow(g_pop, SW_SHOW);
    } else {
        g_popup_on = 0;
        ShowWindow(g_pop, SW_HIDE);
    }
    (void)sh;
}

static void paint_all(void) {
    RECT rc;
    GetClientRect(g_hdr, &rc);
    int cw = (rc.right > 0) ? (rc.right / N_CELLS) : 80;
    if (cw < 40) cw = 40;
    draw_row(g_hdr, g_cells, N_CELLS, g_hdr_focus, cw, 1);

    GetClientRect(g_bot, &rc);
    int n = g_n_tabs > 0 ? g_n_tabs : 1;
    int tw = (rc.right > 0) ? (rc.right / n) : KTB_TAB_W;
    if (g_n_tabs == 0) {
        char empty[1][64];
        snprintf(empty[0], 64, "(no tabs)");
        draw_row(g_bot, empty, 1, 0, rc.right, 0);
    } else {
        draw_row(g_bot, g_tabs, g_n_tabs, g_tab_focus, tw, 1);
    }
    if (g_popup_on && g_hq_n > 0) {
        GetClientRect(g_pop, &rc);
        draw_row(g_pop, g_hqitem, g_hq_n, 0, rc.right, 0);
        /* stack rows vertically: redraw with per-row height */
        HDC hdc = GetDC(g_pop);
        RECT full;
        GetClientRect(g_pop, &full);
        HBRUSH br = CreateSolidBrush(g_bg);
        FillRect(hdc, &full, br);
        DeleteObject(br);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, g_fg);
        HFONT font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                 DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                                 FF_DONTCARE, L"Segoe UI");
        HFONT old = (HFONT)SelectObject(hdc, font);
        int i;
        for (i = 0; i < g_hq_n; i++) {
            RECT tr = { 8, 4 + i * KTB_BAR_H, full.right - 8, (i + 1) * KTB_BAR_H };
            wchar_t wlab[128];
            char shown[96];
            snprintf(shown, sizeof(shown), "%d. %s", i + 1, g_hqitem[i]);
            utf8_to_wide(shown, wlab, 128);
            DrawTextW(hdc, wlab, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        SelectObject(hdc, old);
        DeleteObject(font);
        ReleaseDC(g_pop, hdc);
    }
}

static int hit_index(HWND hwnd, int x, int n, int vertical) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    if (n <= 0) return -1;
    if (vertical) {
        int row = x /* actually y passed as x when vertical */;
        (void)row;
        return -1;
    }
    int cw = rc.right / n;
    if (cw <= 0) return 0;
    int i = x / cw;
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return i;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP) {
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        if (msg == WM_RBUTTONUP) {
            send_code(KSC_NAV_ARM);
            return 0;
        }
        if (hwnd == g_hdr) {
            int i = hit_index(hwnd, x, N_CELLS, 0);
            if (i >= 0) send_code(KSC_HQ_HEADER_BASE + (i + 1));
        } else if (hwnd == g_bot) {
            int i = hit_index(hwnd, x, g_n_tabs > 0 ? g_n_tabs : 1, 0);
            if (i >= 0 && g_n_tabs > 0) send_code(KSC_TAB_BASE + i);
        } else if (hwnd == g_pop) {
            int i = y / KTB_BAR_H;
            if (i >= 0 && i < g_hq_n) send_code(KSC_HQ_ITEM_BASE + i);
        }
        return 0;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        paint_all();
        return 0;
    }
    if (msg == WM_TIMER) {
        if (dirty()) {
            load_state();
            layout_windows();
            paint_all();
        }
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static HWND make_win(HINSTANCE hi, const wchar_t *cls, const wchar_t *title) {
    HWND h = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        cls, title,
        WS_POPUP,
        0, 0, 100, KTB_BAR_H,
        NULL, NULL, hi, NULL);
    if (h)
        SetLayeredWindowAttributes(h, 0, 230, LWA_ALPHA);
    return h;
}

static int spawn_manager(void) {
    wchar_t self[SP_PATH];
    DWORD n = GetModuleFileNameW(NULL, self, SP_PATH);
    if (n == 0 || n >= SP_PATH) return 0;
    wchar_t *slash = wcsrchr(self, L'\\');
    if (!slash) return 0;
    slash[1] = 0;
    wcscat(self, L"khtpm_taskbar_manager_main.exe");

    wchar_t cmd[SP_PATH + 32];
    _snwprintf(cmd, SP_PATH + 31, L"\"%s\" \".\"", self);
    cmd[SP_PATH + 31] = 0;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    wchar_t cwd[8] = L".";
    DWORD flags = CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS | CREATE_BREAKAWAY_FROM_JOB;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, flags, NULL, cwd, &si, &pi)) {
        flags = CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS;
        if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, flags, NULL, cwd, &si, &pi)) {
            slog("CreateProcessW manager failed %lu", (unsigned long)GetLastError());
            return 0;
        }
    }
    g_manager_proc = pi.hProcess;
    CloseHandle(pi.hThread);
    slog("manager spawned pid=%lu", (unsigned long)pi.dwProcessId);
    return 1;
}

static void wait_publish(void) {
    int i;
    for (i = 0; i < 40; i++) {
        FILE *f = rel_fopen("#.desktop/strip_state.txt", "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fclose(f);
            if (sz > 0) return;
        }
        Sleep(25);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: khtpm_strip_parser <house_root>\n");
        return 1;
    }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);
    for (char *p = g_house; *p; p++) if (*p == '/') *p = '\\';

    if (strcmp(g_house, ".") != 0) {
        wchar_t wh[SP_PATH];
        utf8_to_wide(g_house, wh, SP_PATH);
        SetCurrentDirectoryW(wh);
        snprintf(g_house, sizeof(g_house), ".");
    }

    g_log = rel_fopen("#.desktop/khtpm_strip_parser.log", "w");
    slog("khtpm_strip_parser_win house=%s", g_house);

    load_offsets();
    spawn_manager();
    wait_publish();
    load_state();

    HINSTANCE hi = GetModuleHandleW(NULL);
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"KhtpmStripWin";
    RegisterClassW(&wc);

    g_hdr = make_win(hi, L"KhtpmStripWin", L"khtpm-header");
    g_bot = make_win(hi, L"KhtpmStripWin", L"khtpm-tabs");
    g_pop = make_win(hi, L"KhtpmStripWin", L"khtpm-popup");
    if (!g_hdr || !g_bot || !g_pop) {
        slog("CreateWindow failed");
        return 1;
    }
    layout_windows();
    ShowWindow(g_hdr, SW_SHOW);
    ShowWindow(g_bot, SW_SHOW);
    paint_all();
    SetTimer(g_hdr, 1, 50, NULL);
    slog("windows up n_tabs=%d hq_open=%d", g_n_tabs, g_hq_open);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_manager_proc) {
        TerminateProcess(g_manager_proc, 0);
        CloseHandle(g_manager_proc);
    }
    if (g_log) fclose(g_log);
    return 0;
}
