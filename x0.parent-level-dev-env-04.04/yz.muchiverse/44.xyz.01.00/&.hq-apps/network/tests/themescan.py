#!/usr/bin/env python3
# themescan.py - readability scan of a fixture capture for nb_theme_test.sh.
# Usage: themescan.py <png> <frame.txt>
#   INK <rowid> <npx>            ink pixels in the row's band
#   DARK <rowid> <y> <x> <w> <core>   dark-neutral run, no bright neighbor
# Rows are frame-mapped (tags text/item/cli_io/bar, ids th-*); geometry is
# peeled from the 21-field tail so action pipes can never shift it.
# A DARK run is the 2026-10-04 signature: near-black (luma<0.05),
# near-neutral glyph-scale ink (3..120px) on a dark local background
# with no bright pixel nearby (fringe of light text always has one).
# Stdlib only.
import struct
import sys
import zlib
from collections import Counter


def decode(png):
    d = open(png, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    pos, raw = 8, b''
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos + 4])[0]
        typ = d[pos + 4:pos + 8]
        body = d[pos + 8:pos + 8 + ln]
        if typ == b'IHDR':
            w, h, bitd, ctyp = struct.unpack('>IIBB', body[:10])
        elif typ == b'IDAT':
            raw += body
        pos += 12 + ln
    assert bitd == 8 and ctyp == 2, (bitd, ctyp)
    px = zlib.decompress(raw)
    ch, stride = 3, w * 3
    out = bytearray(w * h * 3)
    prev = bytearray(stride)
    i, o = 0, 0
    for _ in range(h):
        f = px[i]
        i += 1
        line = bytearray(px[i:i + stride])
        i += stride
        if f == 1:
            for x in range(ch, stride):
                line[x] = (line[x] + line[x - ch]) & 0xff
        elif f == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xff
        elif f == 3:
            for x in range(stride):
                a = line[x - ch] if x >= ch else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xff
        elif f == 4:
            for x in range(stride):
                a = line[x - ch] if x >= ch else 0
                b, c = prev[x], prev[x - ch] if x >= ch else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xff
        out[o:o + stride] = line
        o += stride
        prev = line
    return w, h, out


def lum(r, g, b):
    f = lambda v: (v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4)
    return 0.2126 * f(r / 255) + 0.7152 * f(g / 255) + 0.0722 * f(b / 255)


def main():
    png, frame = sys.argv[1], sys.argv[2]
    w, h, px = decode(png)

    def P(x, y):
        x = max(0, min(w - 1, x))
        y = max(0, min(h - 1, y))
        i = (y * w + x) * 3
        return (px[i], px[i + 1], px[i + 2])

    def dist(a, b):
        return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])

    for line in open(frame, encoding='utf-8', errors='replace'):
        f = line.rstrip('\n').split('|')
        if not f or f[0] not in ('text', 'item', 'cli_io', 'bar'):
            continue
        rid = f[1] if len(f) > 1 else ''
        if not rid.startswith('th-'):
            continue
        try:
            nav, active, x, y, ww, hh = [int(v) for v in f[-21:-15]]
        except ValueError:
            print('GEOMFAIL ' + rid)
            continue
        if ww <= 0 or hh <= 0 or y < 0 or x < 0:
            print('NOBOX ' + rid)
            continue
        y0, y1 = max(0, y + 4), min(h, y + hh - 4)
        cnt = Counter()
        for yy in range(y0, y1):
            for xx in range(max(0, x), min(w, x + ww)):
                cnt[P(xx, yy)] += 1
        if not cnt:
            print('INK %s 0' % rid)
            continue
        bg = cnt.most_common(1)[0][0]
        ink = sum(n for c, n in cnt.items() if dist(c, bg) > 60)
        print('INK %s %d' % (rid, ink))
        for yy in range(y0, y1, 2):
            xx = max(0, x)
            xe = min(w, x + ww)
            while xx < xe:
                c = P(xx, yy)
                if dist(c, bg) <= 60:
                    xx += 1
                    continue
                x0 = xx
                core, best = c, lum(*c)
                while xx < xe and dist(P(xx, yy), bg) > 60:
                    L = lum(*P(xx, yy))
                    if L < best:
                        best, core = L, P(xx, yy)
                    xx += 1
                width = xx - x0
                if width < 3 or width > 120:
                    continue
                if best >= 0.05:
                    continue
                if abs(core[0] - core[1]) >= 12 or abs(core[1] - core[2]) >= 12:
                    continue
                if core == (20, 20, 20):  # badge backing chip, not text
                    continue
                if lum(*bg) > 0.08:
                    continue
                if any(lum(*P(qx, qy)) > 0.2
                       for qx in range(x0 - 12, xx + 12)
                       for qy in range(yy - 4, yy + 5)):
                    continue
                print('DARK %s %d %d %d #%02x%02x%02x'
                      % (rid, yy, x0, width, core[0], core[1], core[2]))


main()
