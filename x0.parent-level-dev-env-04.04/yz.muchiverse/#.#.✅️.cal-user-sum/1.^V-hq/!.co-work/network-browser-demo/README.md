# 🌐 network-browser-demo — the real house renderer, runnable standalone

This is a real, working, end-to-end proof of the pattern documented in
`../khtpm-core/GENERIC-CAPABILITIES-PATTERN.md`. **Read that doc
first** — this folder is the proof that it's real, not just theory.

**Everything here is the REAL, unmodified house code** — not a
simplified stand-in. This is genuinely how the real network browser
runs in the real house today, byte-for-byte:

- **`*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`** — the
  actual, complete, unmodified shared renderer (~18,000 lines). It also
  serves 7 other, unrelated apps in the real house (a database editor,
  a chat window, a palette picker...) — none of those code paths are
  reachable from this demo's own real invocation shape
  (`<house_root> <chtpm_path>`, the generic default/sidebar+panel mode
  this app uses), but they're real, live code sitting in the same
  binary, same as in the actual house. This file is genuinely worth
  reading in full if you want to actually learn the real engine, not
  just a toy version of it.
- **`&.hq-apps/network/network_browser_manager.c`** — the real,
  unmodified business-logic manager. Fetches a real URL (`curl`), does
  simple real HTML title/text/link extraction, republishes its own
  live `.chtpm` projection every tick. Zero X11, zero rendering
  knowledge — a real, standalone, independently-testable binary.
- **`&.hq-apps/network/button.sh`** — the real, unmodified launcher.
  Same file the real house's own taskbar "Browser" row runs.
- **`&.hq-apps/network/network-browser-hq.chtpm.bootstrap`** — the
  real, authored `.chtpm` seed. Its only real job is the `<module>`
  tag; the manager overwrites everything else within ~300ms.
- **`&.hq-apps/network/ops/nb_write_go.sh`** — the real dispatch script
  the address bar / links actually run.

## Run it

```sh
sh run.sh
```

Type a real URL into the address bar (click it or press Enter/a digit
to arm it, type, Enter to commit) and watch it actually fetch and
render. Click a real link in the results to navigate again. This is a
genuinely live network fetch, not a mock.

## What build.sh does

`khtpm_core_render.c` depends on 4 shared source files that live in
`../khtpm-core/` in this same repo (`khtpm_css_parser.c/.h`,
`khtpm_render_core.c`, `khtpm_draw_core.c`) plus `stb_image_write.h` —
`build_core_render.sh` copies them in at build time (the real house's
own "copy, don't hand-fork" convention — this package never uses a
runtime shared include path, ever). `build.sh` runs that, then builds
the real manager. Two real op binaries the house's own build script
also builds here (`swatch_picker_manager.+x`, `dump_frame_png_op.+x`)
are left out — genuinely unrelated to running this one app.

## A real house-vs-demo difference worth knowing

The real house always already has a `#.desktop` directory at its own
root — a fresh standalone checkout doesn't. Without it, the manager's
every `fopen("w")` for its own state/request/status/projection files
fails completely silently (no crash, no error — it just never writes
anything), and the window renders nothing but its own title bar. `run.sh`
does the one real `mkdir -p "#.desktop"` needed to match the real
house's own environment — that's the only real difference between this
demo and the actual production setup.

## The real bug this demo's build caught, worth knowing

While packaging this app, a `write_chtpm_projection()` action string
was found pointing at `nb_write_go.sh` directly under
`&.hq-apps/network/`, when the script has always actually lived at
`ops/nb_write_go.sh`. Since the dispatch runs through a real shell
command with stderr redirected to `/dev/null`, this failed completely
silently — the address bar and every content link did nothing in the
real house, with zero visible symptom, until this was found and fixed
(already applied to `network_browser_manager.c` here, and pushed to
the real house repo).
