#!/usr/bin/env python3
# xclipget.py - read the X11 CLIPBOARD as UTF-8 text (test instrument for
# nb_span_select_test.sh). The renderer owns CLIPBOARD after any copy
# and answers SelectionRequest itself, so this is a real end-to-end
# read through the live protocol, not a state-file peek. Stdlib only
# (ctypes against libX11 - the same trick nb_theme_test.sh already
# uses to raise windows). Prints the clipboard bytes, exit 2 on timeout.
import ctypes
import struct
import sys
import time

x11 = ctypes.CDLL('libX11.so.6')
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XDefaultScreen.argtypes = [ctypes.c_void_p]
x11.XDefaultScreen.restype = ctypes.c_int
x11.XRootWindow.argtypes = [ctypes.c_void_p, ctypes.c_int]
x11.XRootWindow.restype = ctypes.c_ulong
x11.XCreateSimpleWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                                    ctypes.c_int, ctypes.c_int,
                                    ctypes.c_uint, ctypes.c_uint,
                                    ctypes.c_uint, ctypes.c_ulong,
                                    ctypes.c_ulong]
x11.XCreateSimpleWindow.restype = ctypes.c_ulong
x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
x11.XInternAtom.restype = ctypes.c_ulong
x11.XConvertSelection.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                                  ctypes.c_ulong, ctypes.c_ulong,
                                  ctypes.c_ulong, ctypes.c_ulong]
x11.XFlush.argtypes = [ctypes.c_void_p]
x11.XPending.argtypes = [ctypes.c_void_p]
x11.XPending.restype = ctypes.c_int
x11.XNextEvent.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
x11.XGetWindowProperty.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                                   ctypes.c_ulong, ctypes.c_long,
                                   ctypes.c_long, ctypes.c_int,
                                   ctypes.c_ulong,
                                   ctypes.POINTER(ctypes.c_ulong),
                                   ctypes.POINTER(ctypes.c_int),
                                   ctypes.POINTER(ctypes.c_ulong),
                                   ctypes.POINTER(ctypes.c_ulong),
                                   ctypes.POINTER(ctypes.c_void_p)]
x11.XGetWindowProperty.restype = ctypes.c_int

d = x11.XOpenDisplay(None)
assert d, 'no display'
scr = x11.XDefaultScreen(d)
root = x11.XRootWindow(d, scr)
req = x11.XCreateSimpleWindow(d, root, 0, 0, 1, 1, 0, 0, 0)
clip = x11.XInternAtom(d, b'CLIPBOARD', False)
utf8 = x11.XInternAtom(d, b'UTF8_STRING', False)
prop = x11.XInternAtom(d, b'XCLIPGET_OUT', False)
x11.XConvertSelection(d, clip, utf8, prop, req, 0)
x11.XFlush(d)
deadline = time.time() + 10
got = False
ev = ctypes.create_string_buffer(192)
while time.time() < deadline:
    while x11.XPending(d):
        x11.XNextEvent(d, ev)
        if struct.unpack('i', ev.raw[:4])[0] == 31:  # SelectionNotify
            got = True
            break
    if got:
        break
    time.sleep(0.05)
if not got:
    print('TIMEOUT')
    sys.exit(2)
actual, fmt = ctypes.c_ulong(), ctypes.c_int()
n, rem, data = ctypes.c_ulong(), ctypes.c_ulong(), ctypes.c_void_p()
rc = x11.XGetWindowProperty(d, req, prop, 0, 65536, False, 0,
                            ctypes.byref(actual), ctypes.byref(fmt),
                            ctypes.byref(n), ctypes.byref(rem),
                            ctypes.byref(data))
if rc == 0 and data and n.value:
    sys.stdout.write(ctypes.string_at(data, n.value).decode('utf-8', 'replace'))
