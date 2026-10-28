#!/usr/bin/env python3
"""Tiny wasm fixtures for tests/nb_wasm_test.sh.

wat2wasm is not installed in this house, so the two modules are hand-encoded
here (readable section layout, no toolchain). `--dump <dir>` writes the .wasm
files; `--base64` prints M1= / M2= for the JS test to embed.

M1 (self-contained): add(i32,i32)->i32, mul(f64,f64)->f64,
i64add(i64,i64)->i64, store8, load8, exported memory (min 1, max 2 pages),
mutable exported global g (starts at 7), and a start function that runs
g = g + 1, so a correct instantiation leaves g at 8.

M2 (imports): env.jsadd(i32,i32)->i32, env.jslog(i32)->void,
env.jsround(f64)->f64 bridged back to JS; exports calladd/calllog/roundtrip.
"""
import base64
import sys

I32, I64, F64 = 0x7F, 0x7E, 0x7C
MAGIC = b"\x00asm\x01\x00\x00\x00"


def uleb(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def vec(items):
    return uleb(len(items)) + b"".join(items)


def section(sid, payload):
    return bytes([sid]) + uleb(len(payload)) + payload


def name(s):
    b = s.encode()
    return uleb(len(b)) + b


def functype(params, results):
    return bytes([0x60]) + vec([bytes([t]) for t in params]) + vec([bytes([t]) for t in results])


def limits(min_pages, max_pages=None):
    if max_pages is None:
        return b"\x00" + uleb(min_pages)
    return b"\x01" + uleb(min_pages) + uleb(max_pages)


def module1():
    types = [
        functype([I32, I32], [I32]),   # 0 add
        functype([F64, F64], [F64]),   # 1 mul
        functype([I64, I64], [I64]),   # 2 i64add
        functype([I32, I32], []),      # 3 store8
        functype([I32], [I32]),        # 4 load8
        functype([], []),              # 5 bump (start)
    ]
    funcs = [0, 1, 2, 3, 4, 5]
    globals_ = [bytes([I32, 1]) + b"\x41\x07\x0b"]        # mutable i32 = 7
    exports = [
        name("add") + b"\x00" + uleb(0),
        name("mul") + b"\x00" + uleb(1),
        name("i64add") + b"\x00" + uleb(2),
        name("store8") + b"\x00" + uleb(3),
        name("load8") + b"\x00" + uleb(4),
        name("bump") + b"\x00" + uleb(5),
        name("memory") + b"\x02" + uleb(0),
        name("g") + b"\x03" + uleb(0),
    ]
    bodies = [
        b"\x00\x20\x00\x20\x01\x6a\x0b",              # local.get0 local.get1 i32.add
        b"\x00\x20\x00\x20\x01\xa2\x0b",              # f64.mul
        b"\x00\x20\x00\x20\x01\x7c\x0b",              # i64.add
        b"\x00\x20\x00\x20\x01\x3a\x00\x00\x0b",      # i32.store8
        b"\x00\x20\x00\x2d\x00\x00\x0b",              # i32.load8_u
        b"\x00\x23\x00\x41\x01\x6a\x24\x00\x0b",      # g = g + 1
    ]
    out = MAGIC
    out += section(1, vec(types))
    out += section(3, vec([uleb(t) for t in funcs]))
    out += section(5, vec([limits(1, 2)]))
    out += section(6, vec(globals_))
    out += section(7, vec(exports))
    out += section(8, uleb(5))                       # start = bump
    out += section(10, vec([uleb(len(b)) + b for b in bodies]))
    return out


def module2():
    types = [
        functype([I32, I32], [I32]),   # 0 jsadd / calladd
        functype([I32], []),           # 1 jslog / calllog
        functype([F64], [F64]),        # 2 jsround / roundtrip
    ]
    imports = [
        name("env") + name("jsadd") + b"\x00" + uleb(0),
        name("env") + name("jslog") + b"\x00" + uleb(1),
        name("env") + name("jsround") + b"\x00" + uleb(2),
    ]
    funcs = [0, 1, 2]
    exports = [
        name("calladd") + b"\x00" + uleb(3),
        name("calllog") + b"\x00" + uleb(4),
        name("roundtrip") + b"\x00" + uleb(5),
    ]
    bodies = [
        b"\x00\x20\x00\x20\x01\x10\x00\x0b",   # call jsadd
        b"\x00\x20\x00\x10\x01\x0b",           # call jslog
        b"\x00\x20\x00\x10\x02\x0b",           # call jsround
    ]
    out = MAGIC
    out += section(1, vec(types))
    out += section(2, vec(imports))
    out += section(3, vec([uleb(t) for t in funcs]))
    out += section(7, vec(exports))
    out += section(10, vec([uleb(len(b)) + b for b in bodies]))
    return out


def main():
    m1, m2 = module1(), module2()
    if "--dump" in sys.argv:
        d = sys.argv[sys.argv.index("--dump") + 1]
        with open(d + "/m1.wasm", "wb") as f:
            f.write(m1)
        with open(d + "/m2.wasm", "wb") as f:
            f.write(m2)
    print("M1=" + base64.b64encode(m1).decode())
    print("M2=" + base64.b64encode(m2).decode())


if __name__ == "__main__":
    main()
