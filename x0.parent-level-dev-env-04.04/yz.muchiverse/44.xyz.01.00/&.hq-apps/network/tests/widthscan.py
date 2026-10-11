#!/usr/bin/env python3
# widthscan.py - every column start per trow row in a captured frame.
# Test asset for nb_table_width_test.sh: computed-width tables must
# share one column grid across rows; ragged/no-counts rows keep the
# equal split. Stdlib only (struct+zlib), no new dependencies.
# Usage: widthscan.py <png> <frame.txt>
#   -> "label|x|w|nstarts|start,.." per trow row, in frame order.
# A text pixel is near-#cccccc; a column starts after a >=10px ink gap
# (inter-word spaces at this size run ~4px, column gaps >=16px).
# Geometry (x,y,w) comes from the ascii frame's own row boxes, so no
# window size is ever hardcoded here.
import struct
import sys
import zlib


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


def main():
    png, frame = sys.argv[1], sys.argv[2]
    w, h, px = decode(png)

    def ink(x, y):
        i = (y * w + x) * 3
        r, g, b = px[i], px[i + 1], px[i + 2]
        return abs(r - 0xcc) <= 14 and abs(g - 0xcc) <= 14 and abs(b - 0xcc) <= 14

    for line in open(frame, encoding='utf-8', errors='replace'):
        f = line.rstrip('\n').split('|')
        if len(f) < 12 or f[0] != 'text' or f[2] != 'trow':
            continue
        lab, x, y, ww = f[3], int(f[8]), int(f[9]), int(f[10])
        col = [0] * w
        for yy in range(y + 4, min(y + 26, h)):
            for xx in range(w):
                if ink(xx, yy):
                    col[xx] += 1
        runs = []
        s = None
        for xx in range(w):
            if col[xx] > 0 and s is None:
                s = xx
            elif col[xx] == 0 and s is not None:
                runs.append((s, xx - 1, sum(col[s:xx])))
                s = None
        if s is not None:
            runs.append((s, w - 1, sum(col[s:])))
        # A lone speckle pixel (border/antialias breaching the ink
        # band for one row) is not a column: real glyphs deposit an
        # order of magnitude more ink across the band.
        runs = [(a, b) for (a, b, m) in runs if m >= 10]
        starts = [runs[0][0]] if runs else []
        for i in range(1, len(runs)):
            if runs[i][0] - runs[i - 1][1] - 1 >= 10:
                starts.append(runs[i][0])
        print('%s|%d|%d|%d|%s' % (lab, x, ww, len(starts), ','.join(map(str, starts))))


main()
