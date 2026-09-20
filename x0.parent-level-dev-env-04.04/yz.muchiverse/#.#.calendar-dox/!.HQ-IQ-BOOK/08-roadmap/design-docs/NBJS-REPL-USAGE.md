# NB-JS interactive REPL — how to use it

A short, user-facing guide to the `nbjs` interactive shell. This is the
REPL that runs on the QuickJS engine (since the 2026-09-18 swap; it was
built originally on duktape and the loop itself is engine-agnostic — it
still behaves exactly as it did under duk).

## Start a session

The REPL lives in the network app's build root, next to the worker:

```sh
cd "<...>/44.xyz.01.00/&.hq-apps/network"
make nbjs            # if not already built
./nbjs               # interactive (needs a real terminal)
echo '1 + 2' | ./nbjs -i     # -i / --interactive: REPL even when stdin is piped
```

- Bare `nbjs` on a terminal starts the REPL.
- `nbjs -i` forces the REPL even when input is piped (good for quick
  one-liner pipes and scripting).
- On start you get a `> ` prompt. Exit with `Ctrl-D`, or type
  `exit`, `quit`, or `.exit`.
- The banner still says `nbjs (duk)` — leftover text from the duktape
  build; cosmetic only.

## What a session gives you

State persists across lines (`var`/`let`/`const`, and anything attached
to globals like `globalThis`). Each line is evaluated with the same
browser host the page worker uses, so this is basically a live DOM + JS
sandbox, plus the node-ish module bits:

| Thing | Works? | Notes |
|---|---|---|
| Plain JS (ES2020+, QuickJS stdlib) | yes | `Date`, `JSON`, `Map`/`Set`, `Promise`, `async`/`await` inside functions, template literals, etc. |
| `console.log/info/warn/error` | yes | `error` → stderr; others → stdout |
| `window`, `document`, DOM | yes | `document.createElement`, `.querySelector`, element events, `click()` all work |
| timers | yes | `setTimeout`/`setInterval` fire shortly after each line (a 200 ms drain runs after every line) |
| Promise microtasks | yes | flush after each line — put `.then(...)` on its own line, then read the result on the *next* line |
| top-level `await` | no | `await` at line top is a syntax error in `eval`; use `.then()` or an `async` IIFE |
| `require('fs')` | yes | the fs-lite natives: `readFileSync` / `writeFileSync` / `appendFileSync` / `existsSync` / `mkdirSync` (sync, string-only) |
| `require('./foo.js')`, JSON `require` | yes | relative/absolute disk modules only |
| single-line ESM | yes | one-line `import`/`export` statements are auto-transpiled to CommonJS |
| `process` | no | node *file mode* only (`nbjs file.js`) — not installed in the REPL |
| `require('path')` or any npm package | no | the only builtin is `fs`; anything else throws `... (nbjs has no packages/builtins)` |

## Worked examples

```js
> var x = 40 + 2
> x
42
> console.log("hello " + x)
hello 42

> var el = document.createElement("button")
> var clicked = 0
> el.addEventListener("click", function(){ clicked++; })
> el.click()
> clicked
1

> setTimeout(function(){ globalThis.st = 1; }, 5)   // drains after this line
> st
1

> Promise.resolve(42).then(function(v){ globalThis.pv = v })
> pv
42

> var f = require("fs")
> f.writeFileSync("/tmp/nbjs-note.txt", "hi")
> f.readFileSync("/tmp/nbjs-note.txt")
hi
```

## Things to remember

- **Not a node shell.** No `process`, no `npm`/packages; reads are capped
  like the worker (512 kB). Don't rely on async I/O.
- **Microtasks/timers need a line boundary.** Queued work flushes a short
  time *after* a line completes, so read the result on the line after the
  one that schedules it — never on the same line.
- **Eval budget.** Each eval runs under the same alarm budget as the page
  worker (`NB_EVAL_BUDGET` env var override; default 2 s). Long-running
  lines will be cut off by the alarm.
- **Same binary, three moods.** `nbjs file.js [args]` → node-style runner
  (this is where `process` lives); `nbjs --browser page.js [fetch.dom]` →
  released DOM page runner; `nbjs` / `nbjs -i` → this REPL.

## Where the small print lives

Implementation: `repl_main()` in
`44.xyz.01.00/&.hq-apps/network/ops/nb_js_worker.c` (~line 4218).
CLI/REPL design + node-mode plan: `NB-JS-CLI-NODE-LIKE-MODE.md` in this
folder. Engine swap notes: `JS-ENGINE-QUICKJS-SWAP-INSIGHT.md`.