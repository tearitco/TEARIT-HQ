#!/usr/bin/env python3
# colscan.py - column-2 left edge per row in a captured frame.
# Test asset for nb_table_test.sh: equal-column table rows must start
# their second column at the same x on every row. Stdlib only
# (struct+zlib), no new dependencies.
# Usage: colscan.py <png> [x0] [y0] [y1]  ->  "y x" per row that has a
# text run starting at/after x0 (default 330), y in [y0, y1).
# A text pixel is a mid-grey (not background, not link blue): tune by
# eye against colscan output, never by assertion message alone.
import struct
import sys
import zlib


def decode(png):
    d = open(png, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    pos, w, h, raw = 8, 0, 0, b''
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos + 4])[0]
        typ = d[pos + 4:pos + 8]
        if typ == b'IHDR':
            w, h, bitd, ctyp = struct.unpack('>IIBB', d[pos + 8:pos + 18])
        elif typ == b'IDAT':
            raw += d[pos + 8:pos + 8 + ln]
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
                b = prev[x]
                c = prev[x - ch] if x >= ch else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xff
        out[o:o + stride] = line
        o += stride
        prev = line
    return w, h, out


def is_grey(px, w, x, y):
    r, g, b = px[(y * w + x) * 3:(y * w + x) * 3 + 3]
    return r > 0x50 and abs(r - g) < 40 and abs(g - b) < 40 and r < 0xe0


def main():
    png = sys.argv[1]
    x0 = int(sys.argv[2]) if len(sys.argv) > 2 else 330
    y0 = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    y1 = int(sys.argv[4]) if len(sys.argv) > 4 else 10**9
    w, h, px = decode(png)
    y1 = min(y1, h)
    for y in range(max(0, y0), y1):
        x = x0
        while x < w and not is_grey(px, w, x, y):
            x += 1
        if x < w and is_grey(px, w, x + 1, y):
            print(y, x)


if __name__ == "__main__":
    main()
