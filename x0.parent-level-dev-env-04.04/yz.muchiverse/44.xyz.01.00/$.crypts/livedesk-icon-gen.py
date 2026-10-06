#!/usr/bin/env python3
"""livedesk-icon-gen.py - draw the Livedesk app icon in the user's CURRENT theme colors.

usage: livedesk-icon-gen.py <house_root> <out.png> [size=256]

Reads COLOR | bg and COLOR | fg from <house_root>/#.desktop/livedesk_theme.pdl (the same file the taskbar and the boot splash
theme from), so the icon matches the desktop instead of a hard-coded palette. Missing/unreadable theme -> charcoal + amber.
Everything else is derived from those two colors: the rounded base is bg, the strips and loading bar are fg, the entity cells
alternate fg and a 50% blend, the outline is bg darkened. Exit 3 if Pillow is not installed (the caller falls back to the
shipped livedesk-icon-256.png).
"""
import os
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit(3)


def read_theme(house):
    bg, fg = "#1a1a1a", "#eab308"
    try:
        with open(os.path.join(house, "#.desktop", "livedesk_theme.pdl"), encoding="utf-8") as f:
            for line in f:
                parts = [p.strip() for p in line.split("|")]
                if len(parts) >= 3 and parts[0] == "COLOR" and parts[2].startswith("#") and len(parts[2]) == 7:
                    if parts[1] == "bg":
                        bg = parts[2]
                    elif parts[1] == "fg":
                        fg = parts[2]
    except OSError:
        pass
    return bg, fg


def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def mix(a, b, t):
    return tuple(int(round(a[i] * (1 - t) + b[i] * t)) for i in range(3))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    house, out = sys.argv[1], sys.argv[2]
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 256
    bg_h, fg_h = read_theme(house)
    bg, fg = rgb(bg_h), rgb(fg_h)
    dim = mix(bg, fg, 0.5)
    edge = mix(bg, (0, 0, 0), 0.5)
    trough = mix(bg, fg, 0.18)

    S = 512                                  # draw large, shrink for smooth edges
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((16, 16, S - 16, S - 16), radius=96, fill=bg + (255,), outline=fg + (255,), width=10)
    d.rounded_rectangle((60, 70, S - 60, 120), radius=14, fill=fg + (255,))                 # header strip
    for i in range(5):
        d.ellipse((80 + i * 40, 86, 100 + i * 40, 106), fill=bg + (255,))
    d.rounded_rectangle((100, 230, S - 100, 262), radius=16, fill=trough + (255,), outline=edge + (255,), width=2)
    d.rounded_rectangle((100, 230, 330, 262), radius=16, fill=fg + (255,))                  # loading bar
    d.rounded_rectangle((60, S - 170, S - 60, S - 70), radius=18, fill=trough + (255,))     # bottom bar
    for i in range(6):                                                                      # entity cells
        x = 84 + i * 62
        d.rounded_rectangle((x, S - 150, x + 44, S - 90), radius=10, fill=(fg if i % 2 == 0 else dim) + (255,))
    im.resize((size, size), Image.LANCZOS).save(out)
    print("icon %dpx bg=%s fg=%s -> %s" % (size, bg_h, fg_h, out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
