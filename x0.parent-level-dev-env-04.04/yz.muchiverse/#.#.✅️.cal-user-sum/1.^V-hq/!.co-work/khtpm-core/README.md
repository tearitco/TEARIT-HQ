# 🧰 khtpm-core — the real, working parser/draw files

These are copied straight from the working house (not simplified/rewritten),
so they're real and battle-tested — but they're the **generic core only**,
not a full runnable app. You (or your agent) still write:
- the `main()` / X11 window-open loop for your own program,
- your own `.chtpm` file describing your window,
- your own `dispatch_onclick()` for what your buttons actually do,
- optionally, your own manager process (see `c-house-onboard-agent-prompt.md`).

## 📄 What's in here

- **`khtpm_css_parser.c` / `.h`** — parses a `.chtpm` file's tags/attributes
  into a tree of `Elem` structs, plus a tiny CSS-like stylesheet parser for
  colors/fonts/spacing. Compile this as its own `.c` file (normal
  compile+link), `#include "khtpm_css_parser.h"` where you need it.

- **`khtpm_render_core.c`** — the `Elem` struct itself, plus
  `hit_test()`/`find_by_tag()`/`find_by_id()`. ⚠️ **Different include
  style on purpose**: this one is meant to be **text-included directly**
  (`#include "khtpm_render_core.c"` — the actual `.c` file, not a header)
  into your main render file, the same way the original house does it.
  This house's own real convention has **zero custom `.h` files for
  shared structs** — only system headers. If that feels unusual, that's
  intentional, not a mistake on our end — see `!.HOUSE_STDS.md` in the
  main house docs if you want the full reasoning, or just do it your own
  way, this isn't load-bearing for whether your project works.

- **`khtpm_draw_core.c`** — actually draws an `Elem` tree to an X11
  drawable (`draw_elem()`, generic keyboard-nav badge rendering, etc).

- **`stb_image_write.h`** — single-header PNG writer, used for the
  "dump current frame to a PNG" debug/testing feature (see doc 2's
  testing section — very useful for headless verification).

- **`example-dashboard.chtpm`** — a REAL, currently-in-use `.chtpm` file
  from this house (a database-style editor window: tabbar across the
  top, sidebar list on the left, detail panel on the right). Read this
  to see the real tag shapes in context, not just the trimmed snippet in
  `c-htpm-agent-onboard-prompt.md`.

## 🧑‍🍳 How to actually use these

1. Write your own `.chtpm` (or copy/trim `example-dashboard.chtpm`).
2. Write a small C program: open an X11 window, parse your `.chtpm` with
   `khtpm_css_parser.c`'s loader, build the `Elem` tree.
3. Each draw/redraw: walk the tree and call `draw_elem()` from
   `khtpm_draw_core.c` on each node (`khtpm_render_core.c`'s
   `find_by_tag`/`find_by_id` help you find specific nodes to update).
4. On a real click or a keyboard-nav activation, call `hit_test()` (or
   use the nav index the draw core already tracks), then run YOUR OWN
   `dispatch_onclick(elem->onclick)` — a plain string switch/if-chain you
   write for whatever `onClick="..."` values you invented in your
   `.chtpm`.
5. Add the text-relay input file + text-state-dump feature from doc 2's
   testing section as you build — much easier to bolt on early than
   retrofit later.

If you get stuck on the X11 window-open/event-loop boilerplate
specifically (not the chtpm/khtpm part), that's normal, ordinary Xlib
code — any general C agent can write that part from scratch, it's not
house-specific.

Good luck! 🛠️✨
