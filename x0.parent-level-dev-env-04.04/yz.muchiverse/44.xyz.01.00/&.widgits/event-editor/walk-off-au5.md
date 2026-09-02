# 📜 walk-off-au5.md — event-editor (shared docs + shared engine fixes), session pause 2026-08-05

This dir holds the design docs shared across ALL event-editor variants (real CHTPM editor, event-mock, event-ez), plus this session added `visual-event-compiler-pal.md`. It also owns the `EVENT_SCRIPTING_PROGRESS_AND_GOALS.md`'s own "KNOWN BUG" entry that got resolved this session — read that update before assuming the Gallery↔Page nav is broken.

## Read in this order

1. `visual-event-compiler-pal.md` — **new this session**, the real RPG-Maker runtime model (page selection, trigger types), the real `pages/page_N/` data shape, the real `prisc+x` opcode-vocabulary discrepancy discovered this session, and the Show Choices/Change Gold design (§7, all open questions answered by direct user response — read it before designing Show Choices' own build, don't re-derive).
2. `EVENT_SCRIPTING_PROGRESS_AND_GOALS.md` — full chronological history. Its own **"KNOWN BUG" section is RESOLVED** as of this session (was previously marked unresolved) — see next section here for the short version.

## The Gallery↔Page href "bug" — resolved, was a test-hygiene issue, not a parser bug

A prior session reported digit-jump navigation landing on the wrong page and left real, prescribed debug instrumentation to root-cause it (never actually run). This session **ran that exact instrumentation for real** (env-gated `CHTPM_NAV_DEBUG=1`, now a permanent, harmless opt-in trace in `101.mutaclsym🧟‍♂️️+18.01/system/chtpm_parser_pal.c` — `do_jump()`/`parse_chtm()`/the href-commit branch all log to `/tmp/chtpm_nav_debug.log` when set) and captured real traces against real k3-injected sequences.

**Finding**: `do_jump()`/the href-commit logic traced CORRECTLY every single time, in every clean test. The confusing "wrong page" results came from **multiple stale event-ez sessions accumulating** (every `gl_mirror` window shares the literal title `"mutaclsym RGB mirror"`, so name-substring key injection silently hits whichever stale session's window happens to match first) — this was reproduced live, repeatedly, during this session's OWN testing before being identified. See `!.HOUSE_STDS.md` §H.5.4 for the full writeup and the mandatory pre-test hygiene checklist.

**Two REAL, separate bugs were found and fixed in the same investigation** (not the reported symptom, but real nonetheless — both in `event-ez`'s own `ez_compose_frame.c`, see that dir's own walk-off for detail):
1. A non-atomic `gui_state.txt` write race.
2. Cosmetic double/nested numbering (`ez_compose_frame.c` baked its own `"N. [...]"` into labels, colliding with `chtpm_parser_pal`'s own automatic `[ ]`/`N.` numbering).

**If you see "wrong page" behavior again**: before assuming the parser is broken, run the two hygiene checks in `!.HOUSE_STDS.md` §H.5.4 FIRST. It was never the parser, both times this was investigated.

## Real precedent worth knowing about (researched, not built)

`projects/wraith-alpha/ops/wraith_gl.c` has a genuinely different, real mouse-input model worth understanding before ever building a "richer" context-menu system: it mirrors an EXISTING terminal-cell-grid CHTPM screen and hit-tests clicks against a declarative `OBJECT | x | y | w | h | z | action` file — the GL window itself draws nothing, it's purely a coordinate mapper into real CHTPM state that already exists elsewhere. This is NOT what `tp_desktop_window.c`'s own KHTPM system does (that one hand-draws its own popup, it doesn't mirror a `.chtm` screen) — see `&.widgits/tile-picker/TILE_PICKER_DESIGN.md` §11 for the full comparison and why KHTPM is a deliberately different, smaller-scope shape.

## Not touched this session

- The real CHTPM editor variant (root of this dir, `ee_compose_frame.c`/`ee_menu_input.c`) — untouched. All the work this session was in `event-ez` and `tp_desktop_window.c`/KHTPM.
- event-mock (`gl_mock/`) — untouched, look/feel reference only per its own existing docs.
