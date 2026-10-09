#!/usr/bin/env python3
# bluebands.py - find contiguous y-bands of near-#8fb8ff pixels in a PNG.
# Test asset for nb_span_test.sh's click-through section: the 1px-tall
# underline band under a link span identifies the rich row in a captured
# frame without trusting any layout guess. Stdlib only (struct+zlib),
# no new dependencies.
# Usage: bluebands.py <png>  ->  "y0 y1 x0 x1 count" per band, top-down.
import struct
import sys
import zlib


def decode(png):
    d = open(png, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    pos, w, h, raw, bitd, ctyp = 8, 0, 0, b'', 0, 0
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


def main(png):
    w, h, px = decode(png)
    match = (0x8f, 0xb8, 0xff)
    tol = 12
    bands = []
    cur = None
    for y in range(h):
        xs = []
        base = y * w * 3
        for x in range(w):
            r, g, b = px[base + x * 3:base + x * 3 + 3]
            if abs(r - match[0]) <= tol and abs(g - match[1]) <= tol and abs(b - match[2]) <= tol:
                xs.append(x)
        if xs:
            if cur and y == cur[1] + 1:
                cur[1] = y
                cur[2] = min(cur[2], xs[0])
                cur[3] = max(cur[3], xs[-1])
                cur[4] += len(xs)
            else:
                if cur:
                    bands.append(cur)
                cur = [y, y, xs[0], xs[-1], len(xs)]
        else:
            if cur:
                bands.append(cur)
                cur = None
    if cur:
        bands.append(cur)
    for y0, y1, x0, x1, n in bands:
        print(y0, y1, x0, x1, n)


main(sys.argv[1])
