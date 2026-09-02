# 🌐 network-browser-demo — the real house pattern, runnable standalone

This is a real, working, from-scratch demo of the pattern documented in
`../khtpm-core/GENERIC-CAPABILITIES-PATTERN.md`. **Read that doc first**
— this folder is the proof that it's real, not just theory.

## What's real here

- **`&.hq-apps/network/network_browser_manager.c`** — the ACTUAL,
  unmodified business-logic manager from the working house. Fetches a
  real URL (`curl`), does simple real HTML title/text/link extraction,
  republishes its own live `.chtpm` projection every tick. Zero X11,
  zero rendering knowledge — a real, standalone, independently-testable
  binary (see doc 1's own "ops" rule).
- **`khtpm_generic_host.c`** — a real, compact reimplementation of the
  house's own generic renderer capabilities (live `.chtpm` re-parse, a
  real armed `<cli_io>` text field, generic click/action dispatch, a
  real `<module>` child-process launch). The REAL house's own shared
  renderer is ~18,000 lines and also serves 7 unrelated apps — too much
  to hand you as a first example. This file is the same real mechanism,
  trimmed to just what THIS app needs, with each real behavior ported
  faithfully (see its own comments for exactly where each piece came
  from) rather than reinvented.
- **`&.hq-apps/network/network-browser-hq.chtpm.bootstrap`** — the real,
  authored `.chtpm` seed. Its only real job is the `<module>` tag; the
  manager overwrites everything else within about 300ms of starting.
- **`&.hq-apps/network/ops/nb_write_go.sh`** — the real dispatch script
  the address bar / links actually run.

## Run it

```sh
sh run.sh
```

Type a real URL into the address bar (click it or press Enter to arm,
type, Enter to commit) and watch it actually fetch and render. Click a
real link in the results to navigate again. This is a genuinely live
network fetch, not a mock.

## The real bug this demo's own build caught, worth knowing

While building `khtpm_generic_host.c`, an early version called
`signal(SIGCHLD, SIG_IGN)` in `main()` for "cheap reaping." This is
inherited across `exec()` — the manager child (which uses `system()` to
run `curl`) got it too, and `system()`'s internal `waitpid()` can't
retrieve a real exit status when `SIGCHLD` is ignored (the kernel
auto-reaps the child first). Every real fetch silently reported
`rc=-1`, even though `curl` itself worked perfectly when run directly.
**Signal disposition survives `exec()`; don't set `SIGCHLD` to
`SIG_IGN` in a process that will `exec()` into something that shells
out.** See the fix's own comment in `khtpm_generic_host.c`'s `main()`
for the full diagnosis.

## What this demo deliberately leaves out

- **Real CSS-driven colors** — this demo hardcodes its own small
  palette in C rather than wiring up `css_compute_style()`. The real
  house version does load real CSS; left out here to keep the demo's
  own draw code short enough to read start to finish in one sitting.
- **The real flex layout engine** (`css_layout_pass()` in
  `khtpm_render_core.c`) — real, but explicitly flagged upstream as not
  yet proven on a live app. This demo uses simple, explicit fixed-row
  positions instead, same real convention the house's own
  `network-browser-hq.css` header comment describes for this exact app.
- **Sprites/emoji/DPI scaling** — real house-wide features the real
  renderer has; irrelevant to what this one app needs to demonstrate.

None of these omissions change the real lesson: one shared, generic
renderer, a real separate manager, and two real generic capabilities
(live re-parse + armed text input) — zero code in the renderer that
knows what a "network browser" is.
