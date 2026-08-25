# -*- coding: utf-8 -*-
import pathlib
house = next(p for p in pathlib.Path(".").iterdir() if p.is_dir() and p.name.startswith("44.xyz") and "00.17" in p.name)
ops = house / "_.monads" / "_.livedesk-taskbar" / "ops"

# --- dump path + fit-to-screen in layout_pass ---
hq = ops / "khtpm_hq_render.c"
t = hq.read_text(encoding="utf-8")

old_dump = '''    int ok = stbi_write_png("/tmp/db-hq-frame.png", w, h, 3, rgb, w * 3);
    fprintf(stderr, ok ? "db-hq: wrote /tmp/db-hq-frame.png (%dx%d)\\n" : "db-hq: dump_frame_png: write failed\\n", w, h);
'''
new_dump = '''    char dump_path[PATH_BUF];
#ifdef _WIN32
    snprintf(dump_path, sizeof(dump_path), "%s/#.desktop/db-hq-frame.png", g_house_root[0] ? g_house_root : ".");
#else
    snprintf(dump_path, sizeof(dump_path), "/tmp/db-hq-frame.png");
#endif
    int ok = stbi_write_png(dump_path, w, h, 3, rgb, w * 3);
    fprintf(stderr, ok ? "db-hq: wrote %s (%dx%d)\\n" : "db-hq: dump_frame_png: write failed %s\\n", dump_path, w, h);
'''
if old_dump in t:
    t = t.replace(old_dump, new_dump, 1)
    print("dump path")
else:
    print("MISS dump")

old_fit = '''    g_close_w = scaled(56); g_close_h = g_chrome_h - scaled(6); /* wide enough for "[>NN] x" - badge + label both now, see draw_elem()'s own comment */
    g_close_x = window->w - g_close_w - scaled(4);
    g_close_y = scaled(3);
'''
new_fit = '''    g_close_w = scaled(56); g_close_h = g_chrome_h - scaled(6); /* wide enough for "[>NN] x" - badge + label both now, see draw_elem()'s own comment */
    /* Win: 15 scaled tabs make a ~1340px window; at window_x=100 on a
     * 1536-wide desk the [x] sits near the right edge and is easy to
     * miss / clip. Cap to the work area and pin x so chrome stays on
     * screen. Same helper idea for events-hq / chat-hai later. */
#ifdef _WIN32
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
    g_close_x = window->w - g_close_w - scaled(4);
    g_close_y = scaled(3);
'''
if old_fit in t:
    t = t.replace(old_fit, new_fit, 1)
    print("layout fit")
else:
    print("MISS fit")

hq.write_text(t, encoding="utf-8")

# --- XLookupString: map VK_* / arrows for all HQ + strip ---
c = ops / "khtpm_strip_x11_win.c"
ct = c.read_text(encoding="utf-8")
old_lu = '''int XLookupString(XKeyEvent *ev, char *buf, int n, KeySym *ks, void *compose) {
    (void)compose;
    if (!ev || !buf || n <= 0) return 0;
    unsigned kc = ev->keycode;
    if (kc & 0x10000) {
        buf[0] = (char)(kc & 0xff);
        if (n > 1) buf[1] = 0;
        if (ks) *ks = (KeySym)(kc & 0xff);
        return 1;
    }
    return 0;
}
'''
new_lu = '''int XLookupString(XKeyEvent *ev, char *buf, int n, KeySym *ks, void *compose) {
    (void)compose;
    if (!ev || !buf || n <= 0) return 0;
    buf[0] = 0;
    unsigned kc = ev->keycode;
    if (kc & 0x10000) {
        buf[0] = (char)(kc & 0xff);
        if (n > 1) buf[1] = 0;
        if (ks) *ks = (KeySym)(kc & 0xff);
        return 1;
    }
    KeySym mapped = XLookupKeysym(ev, 0);
    if (ks) *ks = mapped;
    if (mapped == XK_Return || mapped == XK_KP_Enter) { buf[0] = '\\r'; if (n > 1) buf[1] = 0; return 1; }
    if (mapped == XK_Escape) { buf[0] = 27; if (n > 1) buf[1] = 0; return 1; }
    if (mapped == XK_BackSpace) { buf[0] = 8; if (n > 1) buf[1] = 0; return 1; }
    if (mapped == XK_Tab) { buf[0] = '\\t'; if (n > 1) buf[1] = 0; return 1; }
    return mapped ? 1 : 0;
}
'''
if old_lu in ct:
    ct = ct.replace(old_lu, new_lu, 1)
    c.write_text(ct, encoding="utf-8")
    print("XLookupString")
else:
    print("MISS XLookupString")
    i = ct.find("int XLookupString")
    print(repr(ct[i:i+400]) if i>=0 else "no fn")
