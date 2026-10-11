#!/usr/bin/env python3
# xsendevent.py - synthesize one X11 input event via XSendEvent (test
# instrument for nb_span_drag_test.sh). Our dispatcher never checks
# send_event, so synthetic ButtonPress/Motion/Release drive the REAL
# handler path (ButtonPress branch -> history pairing -> poll, motion
# branch, release branch) with stdlib-only ctypes - no xdotool/XTest.
# Motion carries state=Button1Mask, exactly what a held-button motion
# carries. x_root/y_root are zero (unused by the span paths).
# Usage: xsendevent.py <win_hex> press|release|move <x> <y> [button]
import ctypes
import struct
import sys

x11 = ctypes.CDLL('libX11.so.6')
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XFlush.argtypes = [ctypes.c_void_p]
x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                           ctypes.c_long, ctypes.c_char_p]
x11.XSendEvent.restype = ctypes.c_int

d = x11.XOpenDisplay(None)
assert d, 'no display'
W = int(sys.argv[1], 16)
TYPES = {'press': (4, 4), 'release': (5, 8), 'move': (6, 64)}


def ev(t, x, y, state, button):
    b = bytearray(192)
    struct.pack_into('i', b, 0, t)
    struct.pack_into('Q', b, 32, W)
    struct.pack_into('i', b, 64, x)
    struct.pack_into('i', b, 68, y)
    struct.pack_into('I', b, 80, state)
    struct.pack_into('I', b, 84, button)
    struct.pack_into('i', b, 88, 1)
    return bytes(b)


op, x, y = sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
t, mask = TYPES[op]
if op == 'move':
    x11.XSendEvent(d, W, False, mask, ev(t, x, y, 256, 0))
else:
    btn = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    x11.XSendEvent(d, W, False, mask, ev(t, x, y, 0, btn))
x11.XFlush(d)
print('sent %s %d,%d' % (op, x, y))
