# -*- coding: utf-8 -*-
import pathlib
house = next(p for p in pathlib.Path(".").iterdir() if p.is_dir() and p.name.startswith("44.xyz") and "00.17" in p.name)
ops = house / "_.monads" / "_.livedesk-taskbar" / "ops"

# shim: x11_max_client
h = ops / "khtpm_strip_x11_win.h"
ht = h.read_text(encoding="utf-8")
if "x11_max_client" not in ht:
    ht = ht.replace(
        "int x11_spawn_cwd(const char *exe, const char *arg1);",
        "int x11_spawn_cwd(const char *exe, const char *arg1);\nvoid x11_max_client(int *w, int *h);",
    )
    h.write_text(ht, encoding="utf-8")
    print("h")

c = ops / "khtpm_strip_x11_win.c"
ct = c.read_text(encoding="utf-8")
if "void x11_max_client" not in ct:
    ct += r'''
void x11_max_client(int *w, int *h) {
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int aw = wa.right - wa.left - 24;
    int ah = wa.bottom - wa.top - 24;
    int smw = GetSystemMetrics(SM_CXSCREEN) - 24;
    int smh = GetSystemMetrics(SM_CYSCREEN) - 24;
    if (smw > 64 && (aw <= 0 || smw < aw)) aw = smw;
    if (smh > 64 && (ah <= 0 || smh < ah)) ah = smh;
    /* Never let an HQ eat the whole desk — chrome [x] must stay on-screen. */
    if (aw > 1280) aw = 1280;
    if (w) *w = aw > 400 ? aw : 400;
    if (h) *h = ah > 200 ? ah : 200;
}
'''
    c.write_text(ct, encoding="utf-8")
    print("c")

hq = ops / "khtpm_hq_render.c"
t = hq.read_text(encoding="utf-8")
old = """#ifdef _WIN32
    if (dpy) {
        int maxw = DisplayWidth(dpy, DefaultScreen(dpy)) - 16;
        int maxh = DisplayHeight(dpy, DefaultScreen(dpy)) - 16;
        if (maxw > 400 && window->w > maxw) window->w = maxw;
        if (maxh > 200 && window->h > maxh) window->h = maxh;
        g_win_x = 8;
        if (g_win_y < 8) g_win_y = 8;
        if (g_win_y + window->h > maxh) g_win_y = 8;
    }
#endif
"""
new = """#ifdef _WIN32
    {
        int maxw = 1280, maxh = 800;
        x11_max_client(&maxw, &maxh);
        if (window->w > maxw) window->w = maxw;
        if (window->h > maxh) window->h = maxh;
        g_win_x = 8;
        if (g_win_y < 8) g_win_y = 8;
        if (g_win_y + window->h > maxh) g_win_y = 8;
    }
#endif
"""
if old in t:
    t = t.replace(old, new, 1)
    hq.write_text(t, encoding="utf-8")
    print("hq cap")
else:
    print("MISS cap block")
    i = t.find("x11_max_client")
    print("already", i)
    i2 = t.find("int maxw = DisplayWidth")
    print("old maxw", i2)
