# PRESS RELEASE — "duk": a real JavaScript engine you can install and type into

**Status:** DRAFT — founder-owned (see `../00-INDEX.md`). Not for
distribution until the founder edits and approves.
**Date:** 2026-09-07
**Engine:** NB-JS (embedded Duktape 2.7.0 + hand-built host in house C)
**Command hook:** `duk`
**Works for:** one-page release (§1), dev-tool launch post (§2), and a
few tweets per outlet (§3).

> ## Facts lock (update when the product changes)
> Ground truth used by every claim below — keep the release in sync:
> - Rung ladder done 2026-09-03 → 09-07 (roadmap `08-roadmap/
>   design-docs/NB-JS-ENGINE-ROADMAP.md`): globals/DOM/events+loop/
>   fetch+XHR/RENDER-feedback/BOM odds+ends.
> - `make` → `nbjs`; `./install-duk.sh` → `duk` command + `$duk` env
>   var (commits `d7797d74`, `1588f1fd`); bare `duk` on a terminal →
>   REPL (`cee6e587`); `make check` three suites green (dom/fetch/
>   events).
> - CPU safety (roadmap §2 + phase-1 step 5): 2 s eval budget,
>   `MAX_TIMER_INVOCATIONS 5000`, `MAX_RAF_FRAMES 120`, 50 k DOM node
>   cap, 250 k wrapper cap, worker kill-and-respawn.
> - Honest limits: Duktape 2.7.0 edition — no arrow functions, no
>   `let` (use `var`/`const`); Linux-first today (macOS/Windows are
>   designed-for, not shipped); pre-traction project.

---

## §1 — One-page press release (classic)

### Headline options

1. **House ships `duk`: a from-scratch JavaScript engine for the
   terminal, born inside a hand-built browser**
2. `duk` — a real JS engine you can install, run a page against, or
   just type into
3. The muchiverse house opens its JS engine to developers — node-style
   CLI, REPL, no browser required

### Body

**DATE — CITY —** The house behind the muchiverse desktop environment
today released **`duk`**, a standalone JavaScript engine that runs
pages and scripts from a terminal without a browser, a window, or any
other dependency. Type `duk` bare and it drops into an interactive
REPL; `duk page.js` runs a script the way `node` does — console output
to stdout, real exit codes, silence on success.

`duk` is the engine the house has been building since 2026-09-03 for
its own hand-built network browser. Rather than embed a browser kit
("don't pull all the chromium"), the team embedded the small Duktape
language core and wrote the rest of the host environment by hand, one
shippable piece at a time:

- a real **DOM tree** — `getElementById`, `querySelector`,
  `innerHTML`, `classList`, tree walking;
- an **event system + event loop** — `addEventListener`,
  `dispatchEvent`, `Event`, `el.onclick = fn`, `setTimeout` /
  `setInterval` / `queueMicrotask`, DOMContentLoaded/load lifecycle;
- **network from JS** — `fetch()` and a minimal `XMLHttpRequest`, with
  a Promise polyfill, over a native curl transport (verified against
  real `file://` and `http` endpoints);
- a **render feedback loop** — after scripts and events quiesce, the
  mutated DOM serializes back into the window's state rows.

The same run of code is CPU-budgeted end to end: a 2-second eval
alarm, caps on timer invocations, animation frames, DOM nodes, and
object handles, and a kill-and-respawn worker model so no misbehaving
script can stall the machine — properties that make `duk` agent- and
CI-friendly.

`duk` installs with one script (`./install-duk.sh`) that puts the
binary on PATH and exports a `$duk` variable the current shell can
use immediately. A `make check` suite runs the headless DOM / network /
events test sets green.

**Roadmap:** a fuller node/bun-like mode (`require`, `fs`, `process`;
CommonJS first), async network so slow sites don't stall, layout/CSS
awareness for real-page rendering, and macOS/Windows builds. The
browser it grew up inside keeps consuming it.

> Quote — placeholder, founder to write:
> "We keep our platform file-driven and AI-drivable. `duk` is that
> same engine with the browser pane taken away — something you can
> install, script, and hand to an agent without a GUI in the way."
> — [founder name], [title]

**Availability:** source-first, built and runnable on Linux today.
Project pages and docs are in-repo; a public link is TBD before
distribution.

**About the house:** muchiverse / "THE HOUSE" is a file-driven desktop
and app platform — every window, menu, and slice of state is a plain
text file, and an AI agent is a first-class user.

**Contact / boilerplate:** [founder contact — TBD] — do not publish
until filled in.

---

## §2 — Dev-tool launch post (blog style)

**Led** `duk` — real JS engine for your terminal, from the from-scratch
browser.

Quick install:

```
git clone …                      # the house repo
cd …/network && make             # builds nbjs
./install-duk.sh                 # → ~/.local/bin/duk, $duk exported
```

Then:

```
$ duk
nbjs (duk) — type JS; Ctrl-D or 'exit' to quit
> 8-8
0
> var j = 9
> j * 2
18
> setTimeout(function(){ console.log("tick after eval"); }, 5)
1
tick after eval
> exit
```

Or run a page (same engine the browser uses, minus the window):

```
$ duk page.js
console output → stdout · renders → stdout · exit 0 ok / 1 err / 2 usage
```

What's inside — the honest list:

- Duktape 2.7.0 (real language: closures, regex, JSON, prototypes) with
  a hand-built host: DOM tree, events + event loop, timers, microtasks,
  `fetch`/XHR + Promise, `URL`/`URLSearchParams`, `history` stubs,
  Base64, and a DOM→rows render-back so scripted pages actually redraw.
- CPU-safe by design: 2 s eval budget, timers/rAF/DOM/wrapper caps,
  worker kill-and-respawn. A `while(true){}` page can't hurt you.
- Zero GUI/desktop dependency for the CLI; `make check` runs three
  headless suites green.

Two engine gotchas users hit day one (we'll keep them listed here, not
hidden): **no arrow functions** (`function(){}` works) and **no `let`**
(`var`/`const` work). It's an ES5.1-era language core dressed up with
`const` — if a transpiled-style snippet fails, it's a language-level
gap, not a bug in your code.

What's next: `require`/`fs`/`process` for real node-style scripting,
async network for slow responses, layout awareness so real sites look
like real sites, and macOS/Windows builds.

---

## §3 — Tweets per outlet

Voice rules for this release (cross-ref `OUTLETS/_ALL-cross-cutting.md`):
- Lore OFF everywhere — this is a cold, product-first announcement.
- Only factual claims from the Facts lock. No fake screenshots, no
  invented traction.
- Who may NOT tweet this: `JBM` (academic), `JB.BLOCKROACH.EZ`
  (services), `CONSULTING` — wrong audiences.

### HARNECIENT — AI tools / AI-harness users / investors
Technical, forward-looking, credible. (Lore OFF per chart.)

1. `duk` is out of the muchiverse browser and into your terminal: a
   real JS engine you can `duk page.js` like node or just `duk` into a
   REPL. Same code that runs scripted pages, minus the window.
   Agent-ready by design — CPU-budgeted, capped, kill-and-respawn.
2. We build the JS engine for our hand-built browser instead of
   "pulling all the chromium". Today it's also a node-style CLI with
   real exit codes + `$duk` env var. `make check` = dom/fetch/events
   suites green. Everything is a file; anything can drive it.
3. Roadmap: `require`/`fs`/`process`, async fetch, layout awareness,
   macOS/Windows. The browser it grew inside keeps consuming it. — a
   scar-less day-one: bash history has regex, JSON, prototypes.

(3 is over-stuffed — trim before posting.)

### ROBOT-TRAP-HOUSE — mainstream gaming/tech news, retro, casual
Casual, meme-forward, 90s nostalgia; impersonal account.

1. we wrote our own javascript engine this week. it was supposed to be
   for our weird homemade browser. then we realized you could just …
   install it and type JS into a terminal. it's called `duk`.
2. `8-8` → `0`. `var j = 9` → `j*2` → `18`. timers fire. fetch works.
   all from a terminal, no electron, no "we ship 200MB of chromium so
   you can have a popup". small machine energy. it's fine.
3. honest part: it's an old-school language core. no `=>`, no `let`.
   we post the gotchas in the readme instead of pretending. Cred.
   roadmap has node-ish `require`, layout so pages actually look like
   pages, and windows/mac builds.

### JB-EZ — founder's dev/personal build-in-public
Mr. Robot flavor, personal, humble-brag, warm.

1. New thing I shipped this week: `duk` — the JS engine from our
   homebrew browser, now standalone. `duk` on a terminal = REPL,
   `duk file.js` = node-like run, real exit codes, no GUI. build in
   public, shipping daily.
2. The part I'm proudest of is the safety work: every eval runs under
   a 2s alarm, timers/rAF/DOM are all capped, and a runaway worker
   gets killed and respawned, not the machine. A page can't DoS
   anyone, including itself.
3. Gotchas are in the open: this language core predates arrow
   functions and `let` — use `var`/`const`, `function(){}`. If code
   fails, the error is honest about it. next: node-style
   `require`/`fs`/`process` + non-Linux builds.

### TEAR-IT-CO — RPG Maker devs/players, otaku, bilingual-friendly
Dev-to-dev, otaku-fluent, light lore.

1. `duk` — a JS engine in a box. Install it, type JS into it, or
   `duk script.js` like node. It came out of our house browser's
   script runner, so the DOM + events + timers are all real, not
   stubs.
2. For game tooling this is nice: a tiny embedded JS you can drive
   headless in your build loop — `make check` proves dom/fetch/events
   without a window. CPU-budgeted so a misbehaving script never
   freezes your tools.
3. JP-friendly note coming when we ship docs/ローカライズ. Today:
   Duktape-based, so old-school JS — no `=>`, no `let`, use
   `var`/`const`. Roadmap: `require`/`fs`/`process`, layout, mac/win.

---

## Notes

- Tweets are meant as *starting points*, not a paste-list. Read each
  outlet's `00-INDEX.md` for channel state before posting (`RTH`
  trolls a bit; `HARNECIENT` is cold/investor).
- Every tweet stays inside the Facts lock. Anything beyond it — public
  repo link, pricing, team names, founder photo — waits for founder
  approval (see `../STRATEGY/marketing.md`).
- The two headline values that make this story: **"we did not embed a
  browser"** and **"installable/agent-drivable/CPU-safe"**. Everything
  else is texture.