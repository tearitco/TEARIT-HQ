#!/bin/sh
# House 'p' dump for a pc-hq board. The board window does not own an X id
# the way khtpm_core_render does, so this writes the same receipt path from
# the frame the board just painted. Prefer the 2D frame. A project that
# only painted the flat buffer (piececraft-hq writes rgb_frame.raw) uses
# that file instead, same width and height keys.
# Output: /tmp/entity-menu-frame.png and /tmp/entity-menu-frame.png.receipt.txt
ROOT="${1:-}"
[ -n "$ROOT" ] && [ -d "$ROOT" ] || { echo "dump_board_frame: need the board project root" >&2; exit 1; }
RAW="$ROOT/pieces/display/rgb_frame_2d.raw"
REC="$ROOT/pieces/display/rgb_frame_2d.receipt.txt"
SRC=rgb_frame_2d.raw
if [ ! -f "$RAW" ] || [ ! -f "$REC" ]; then
    RAW="$ROOT/pieces/display/rgb_frame.raw"
    REC="$ROOT/pieces/display/rgb_frame.receipt.txt"
    SRC=rgb_frame.raw
fi
[ -f "$RAW" ] && [ -f "$REC" ] || { echo "dump_board_frame: no 2d frame under $ROOT" >&2; exit 1; }
python3 - "$RAW" "$REC" "$SRC" <<'PY'
import pathlib, struct, sys, time, zlib
raw, rec, src = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
w = h = 0
for line in rec.read_text().splitlines():
    if line.startswith("frame_w="): w = int(line.split("=", 1)[1])
    if line.startswith("frame_h="): h = int(line.split("=", 1)[1])
data = raw.read_bytes()
if w < 1 or h < 1 or len(data) < w * h * 4:
    sys.exit("dump_board_frame: bad frame size")
def chunk(tag, payload):
    return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
rows = b"".join(b"\x00" + data[y*w*4:(y+1)*w*4] for y in range(h))
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
out = pathlib.Path("/tmp/entity-menu-frame.png")
out.write_bytes(png)
pathlib.Path("/tmp/entity-menu-frame.png.receipt.txt").write_text(
    "ok=1 png=/tmp/entity-menu-frame.png w=%d h=%d t=%d source=%s\n" % (w, h, int(time.time()), src))
print("ok /tmp/entity-menu-frame.png %dx%d" % (w, h))
PY
