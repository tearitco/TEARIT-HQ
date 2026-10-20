#!/usr/bin/env python3
# xsendevent.py - synthesize one X11 input event via XSendEvent (test
# instrument for nb_span_drag_test.sh). Our dispatcher never checks
# send_event, so synthetic ButtonPress/Motion/Release drive the REAL
# handler path (ButtonPress branch -> history pairing -> poll, motion
# branch, release branch) with stdlib-only ctypes - no xdotool/XTest.
# Motion carries state=Button1Mask, exactly what a held-button motion
# carries. x_root/y_root are zero (unused by the span paths).
# Usage: xsendevent.py <win_hex> press|release|move <x> <y> [button]
#        xsendevent.py <win_hex> key <keysym_or_char> [modifier]
#
# KEYS (added 2026-10-14 for G1.4 focus safety): the dispatcher reads keys
# through XLookupString(&ev->xkey, ...), so a key event is only real if it
# carries a genuine KEYCODE from the server's keymap - a synthetic
# character in some other field is ignored. So: keysym -> keycode via the
# server, then XKeyEvent in its CANONICAL layout (note the mouse path
# above packs its fields +4 from canonical and that is deliberate and
# untouched: it is a different event struct and it works).
#
#   key a            -> the letter 'a'
#   key A            -> shift+a  (uppercase implies ShiftMask, exactly as
#                        a real keyboard does, so XLookupString returns "A")
#   key Return       -> named keysym (Return, BackSpace, Home, End, Left...)
#   key BackSpace shift
#
# Usage: xsendevent.py <win_hex> key <keysym_or_char> [modifier]
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
x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
x11.XStringToKeysym.restype = ctypes.c_ulong
x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XKeysymToKeycode.restype = ctypes.c_ubyte

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


op = sys.argv[2]

if op == 'key':
    # Resolve the name: a single character is that character, anything
    # longer is a keysym name (Return, BackSpace, Home...).
    name = sys.argv[3]
    if len(name) == 1:
        ks = ord(name)
    else:
        ks = x11.XStringToKeysym(name.encode())
        if ks == 0:
            sys.exit("unknown keysym: %s" % name)
    kc = x11.XKeysymToKeycode(d, ks)
    if kc == 0:
        sys.exit("no keycode for keysym %s (0x%x) on this server" % (name, ks))

    state = 0
    if name.isupper() or (len(name) == 1 and name.isalpha() and name.isupper()):
        state |= 1          # ShiftMask
    if len(sys.argv) > 4 and sys.argv[4] == 'ctrl':
        state |= 4          # ControlMask

    def keyev(t, st, code):
        b = bytearray(192)
        struct.pack_into('i', b, 0, t)          # type
        struct.pack_into('Q', b, 32, W)         # window
        struct.pack_into('I', b, 76, st)        # state
        struct.pack_into('I', b, 80, code)      # keycode
        struct.pack_into('i', b, 84, 1)         # same_screen
        return bytes(b)

    # press then release, like a real keystroke: the renderer's focus and
    # repeat logic all key off the pair.
    x11.XSendEvent(d, W, False, 1, keyev(2, state, kc))   # KeyPress, KeyPressMask
    x11.XSendEvent(d, W, False, 2, keyev(3, state, kc))   # KeyRelease, KeyReleaseMask
    x11.XFlush(d)
    print("sent key %s (keysym 0x%x keycode %d state %d)" % (name, ks, kc, state))
    sys.exit(0)

x, y = int(sys.argv[3]), int(sys.argv[4])
t, mask = TYPES[op]
if op == 'move':
    x11.XSendEvent(d, W, False, mask, ev(t, x, y, 256, 0))
else:
    btn = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    x11.XSendEvent(d, W, False, mask, ev(t, x, y, 0, btn))
x11.XFlush(d)
print('sent %s %d,%d' % (op, x, y))
