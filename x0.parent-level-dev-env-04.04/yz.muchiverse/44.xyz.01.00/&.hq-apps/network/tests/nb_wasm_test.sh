#!/bin/bash
# nb_wasm_test.sh - the WebAssembly gate (Cloudflare-unblock foundation).
#
# What this proves, all through the real shared host (ops/nb_host.h) in the
# headless one-shot path:
#   - WebAssembly.validate separates a real module from a corrupted one
#   - new WebAssembly.Module(bytes) + Module.exports() report the real
#     export names, read from wasm3's export section
#   - new WebAssembly.Instance(module) runs the module's START function
#     (fixture's `g` is 8, not its declared 7, only if start ran)
#   - exported functions execute: i32, f64, i64 (<-> BigInt), 0 results
#   - exported memory is live in both directions (JS writes are read by
#     wasm, wasm writes are read by JS) and grow() extends it to the
#     declared maximum
#   - an exported mutable global reads and writes through .value
#   - imported JS functions are called from wasm: i32 and f64 args/returns,
#     a void import's side effect, and a trap from bad args is catchable
#   - the Promise API: instantiate(bytes) -> {module,instance},
#     instantiate(module) -> instance, compile(bytes) -> Module
#
# The fixtures are hand-encoded (no wat2wasm in the house). Where node is
# installed we first run the same bytes through V8 as an independent oracle,
# so a fixture bug cannot masquerade as a browser bug.
#
# Deliberately NOT in nb_all_tests.sh yet (same rule as the flex/grid/gBCR
# suites): opt in explicitly.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
PKG="$HR/&.hq-apps/network"
EVAL="$PKG/ops/+x/nb_js_eval.+x"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$EVAL" ] || { echo "FAIL: $EVAL missing - run build.sh first"; exit 1; }

WORK="$(mktemp -d /tmp/nb_wasm.XXXXXX)"
# NB_WASM_KEEP=1 leaves the work dir (test.js + out.txt) behind for debugging.
cleanup() { [ -n "${NB_WASM_KEEP:-}" ] || rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

python3 "$HERE/nb_wasm_gen.py" --dump "$WORK" > "$WORK/mods.txt" || { echo "FAIL: fixture generator"; exit 1; }
M1="$(sed -n 's/^M1=//p' "$WORK/mods.txt")"
M2="$(sed -n 's/^M2=//p' "$WORK/mods.txt")"
[ -n "$M1" ] && [ -n "$M2" ] || { echo "FAIL: fixture generator emitted no modules"; exit 1; }

# Independent oracle: the same bytes must run in V8 before we ask wasm3.
if command -v node >/dev/null; then
    if node -e '
const fs=require("fs"), d=process.argv[1];
const m1=new WebAssembly.Module(fs.readFileSync(d+"/m1.wasm"));
const e=new WebAssembly.Instance(m1,{}).exports;
if (e.add(2,3)!==5) throw new Error("oracle add");
if (Math.abs(e.mul(1.5,2)-3)>1e-12) throw new Error("oracle mul");
if (e.i64add(5n,7n)!==12n) throw new Error("oracle i64");
if (e.g.value!==8) throw new Error("oracle start g="+e.g.value);
e.store8(10,65); if (e.load8(10)!==65) throw new Error("oracle memory");
const m2=new WebAssembly.Module(fs.readFileSync(d+"/m2.wasm"));
let seen=null;
const e2=new WebAssembly.Instance(m2,{env:{jsadd:(a,b)=>a+b,jslog:x=>{seen=x;},jsround:x=>Math.round(x)}}).exports;
if (e2.calladd(20,22)!==42) throw new Error("oracle import add");
if (e2.roundtrip(2.6)!==3) throw new Error("oracle import f64");
e2.calllog(9); if (seen!==9) throw new Error("oracle import void");
console.log("V8 oracle ok");
' "$WORK" 2>&1 | sed 's/^/  /'; then
        :
    else
        fail "the fixture does not run under V8 either - fixture bug, not browser bug"
    fi
else
    echo "  (node absent: skipping the independent V8 oracle)"
fi

cat > "$WORK/test.js" <<JS
var FAILS = 0;
function ok(name, cond, detail) {
    if (cond) { console.log("PASS " + name); }
    else { FAILS++; console.log("FAIL " + name + (detail === undefined ? "" : " [" + detail + "]")); }
}
function bytes(b64) {
    var s = atob(b64), u = new Uint8Array(s.length);
    for (var i = 0; i < s.length; i++) u[i] = s.charCodeAt(i);
    return u;
}
var M1 = bytes("$M1");
var M2 = bytes("$M2");

ok("validate accepts a real module", WebAssembly.validate(M1) === true);
var corrupt = new Uint8Array(M1);
corrupt[8] ^= 0xFF;
ok("validate rejects a corrupted module", WebAssembly.validate(corrupt) === false);
ok("API surface present",
   typeof WebAssembly.compile === "function" &&
   typeof WebAssembly.instantiate === "function" &&
   typeof WebAssembly.Module === "function" &&
   typeof WebAssembly.Instance === "function");

var mod = new WebAssembly.Module(M1);
var descs = WebAssembly.Module.exports(mod);
var names = [];
for (var i = 0; i < descs.length; i++) names.push(descs[i].name);
ok("Module.exports names the real exports",
   names.indexOf("add") >= 0 && names.indexOf("memory") >= 0 && names.indexOf("g") >= 0,
   names.join(","));
var idsc = WebAssembly.Module.imports(new WebAssembly.Module(M2));
ok("Module.imports names the real imports",
   idsc.length === 3 && idsc[0].module === "env" && idsc[0].name === "jsadd",
   idsc.length);

var inst = new WebAssembly.Instance(mod, {});
var e = inst.exports;
ok("exported function i32 args/result", e.add(2, 3) === 5, e.add(2, 3));
ok("exported function f64 args/result", Math.abs(e.mul(1.5, 2) - 3) < 1e-12, e.mul(1.5, 2));
ok("exported function i64 <-> BigInt", e.i64add(5n, 7n) === 12n, String(e.i64add(5n, 7n)));
ok("start function ran at instantiate", e.g.value === 8, e.g.value);
e.g.value = 20;
ok("exported mutable global writes", e.g.value === 20, e.g.value);
var trapped = false;
try { e.add(Symbol("nope"), 1); } catch (err) { trapped = true; }
ok("bad argument throws a catchable trap", trapped);

e.store8(10, 65);
ok("wasm store -> JS load", e.load8(10) === 65, e.load8(10));
var view = new Uint8Array(e.memory.buffer);
ok("exported memory has the declared size", e.memory.buffer.byteLength === 65536, e.memory.buffer.byteLength);
view[100] = 200;
ok("JS write -> wasm load", e.load8(100) === 200, e.load8(100));
var old = e.memory.grow(1);
ok("memory.grow returns the old page count", old === 1, old);
ok("memory.grow extended the buffer", e.memory.buffer.byteLength === 131072, e.memory.buffer.byteLength);

var seen = null;
var inst2 = new WebAssembly.Instance(new WebAssembly.Module(M2), {
    env: {
        jsadd: function (a, b) { return a + b; },
        jslog: function (x) { seen = x; },
        jsround: function (x) { return Math.round(x); }
    }
});
ok("imported JS i32 function called from wasm", inst2.exports.calladd(20, 22) === 42, inst2.exports.calladd(20, 22));
ok("imported JS f64 function called from wasm", inst2.exports.roundtrip(2.6) === 3, inst2.exports.roundtrip(2.6));
inst2.exports.calllog(9);
ok("imported void function side effect", seen === 9, seen);

WebAssembly.instantiate(M1).then(function (pair) {
    ok("instantiate(bytes) resolves {module,instance}",
       !!pair.module && !!pair.instance && pair.instance.exports.add(1, 1) === 2);
    return WebAssembly.instantiate(new WebAssembly.Module(M1), {});
}).then(function (i) {
    ok("instantiate(Module) resolves Instance", i.exports.add(4, 4) === 8);
    return WebAssembly.compile(M1);
}).then(function (m) {
    var n = WebAssembly.Module.exports(m).length;
    ok("compile(bytes) resolves a Module", n > 0, n);
}).then(function () {
    console.log(FAILS === 0 ? "WASM-ALL-PASS" : ("WASM-FAILURES=" + FAILS));
}).catch(function (err) {
    console.log("FAIL async: " + (err && err.message ? err.message : String(err)));
});
JS

"$EVAL" "$WORK/test.js" "$WORK/out.txt" >/dev/null 2>"$WORK/stderr.txt" || true

if [ -s "$WORK/stderr.txt" ]; then
    fail "nb_js_eval wrote to stderr"
    sed 's/^/  /' "$WORK/stderr.txt" | head -20
fi
if grep -q '^ERROR|' "$WORK/out.txt"; then
    fail "host reported an error"
    grep '^ERROR|' "$WORK/out.txt" | sed 's/^/  /'
fi
if ! grep -qx 'OK|1' "$WORK/out.txt"; then
    fail "script execution did not finish cleanly"
fi
if grep -q '^LOG|FAIL' "$WORK/out.txt"; then
    fail "one or more assertions failed"
fi
if ! grep -qx 'LOG|WASM-ALL-PASS' "$WORK/out.txt"; then
    fail "async assertions never completed (microtask drain)"
fi

grep '^LOG|' "$WORK/out.txt" | sed 's/^LOG|/  /'

if [ "$FAIL" -eq 0 ]; then
    echo "PASS: WebAssembly executes scalar exports, memory, globals and JS imports"
    echo "nb_wasm_test.sh PASS"
else
    echo "nb_wasm_test.sh FAIL"
fi
exit "$FAIL"
