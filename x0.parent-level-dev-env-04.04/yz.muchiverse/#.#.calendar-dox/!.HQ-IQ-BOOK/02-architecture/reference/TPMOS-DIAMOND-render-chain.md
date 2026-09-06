# TPMOS render chain — the "DIAMOND standard"

**What this is.** A line-by-line dissection of how the *original*
1.TPMOS renderer works, kept as the reference the khtpm house measures
its own `cli` / terminal-mirror path against. `CENTROID_GOLD_STD.md` §8
("Repaint discipline") is the *rule* distilled from this; this doc is
the *worked example* behind it, with real file+line citations so a
future reader checks the source, not a paraphrase.

Direct user framing (2026-09-06): this is the **DIAMOND standard** —
one tier above the GOLD doc, because GOLD is our restatement and this is
the thing itself, already shipped and proven.

## Where it lives

```
x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001/
  pieces/chtpm/plugins/chtpm_parser.c   ~3155 lines  — the PARSER/composer
  pieces/display/renderer.c             ~168 lines   — the terminal PRESENTER
  pieces/display/frame_changed.txt                   — input→compose marker
  pieces/display/renderer_pulse.txt                  — compose→present marker
  pieces/display/current_frame.txt                   — the one frame file
  pieces/keyboard/history.txt                        — append-only input log
  pieces/display/{state,layout}_changed.txt          — extra compose markers
```

In git status from the `44.xyz` house this tree shows as
`../1.TPMOS_c_+rmmp.0103.0001/...`. See memory
`tpmos-reference-location.md`.

## The chain, end to end

```
keypress
  → keyboard_input binary appends "KEY_PRESSED: <n>" to pieces/keyboard/history.txt
  → chtpm_parser.c main loop (60Hz) reads history.txt from a saved cursor
       (last_history_position = ftell(); never truncated; resync on shrink)
  → process_key(n) moves focus_index / digit_accum, calls export_active_index()
  → process_key() END: appends "K\n" to pieces/display/frame_changed.txt   ← THE marker
  → same loop iteration: stat(frame_changed.txt); st_size > last_size ? dirty = 1
  → if (dirty) compose_frame():
        load_vars(); parse_chtm(); render every element into one string
        → fopen("pieces/display/current_frame.txt", "w"); write it   ← ONE writer
        → append "P\n" to pieces/display/renderer_pulse.txt          ← 2nd marker
  → usleep(16667)                                                    ← 60 FPS
--------------------------------------------------------------------------
  renderer.c main loop (separate process, 60Hz):
  → stat(renderer_pulse.txt); st_size != last_marker_size ? render_display()
  → render_display(): read current_frame.txt, print with explicit \r\n,
       append to session history
  → usleep(16667)
```

Two processes, two markers, one frame file. The parser is the sole
compositor and sole writer of `current_frame.txt`; `renderer.c` never
touches termios and never writes the frame — that split is deliberate
(raw mode clears `OPOST`, which would staircase the parser's own
output; see `renderer.c` header + khtpm's `open_cli.sh` header for the
same reasoning ported).

## The five rules (verbatim doctrine from the source)

From `chtpm_parser.c` `main()` ~3024-3040:

> **RENDER TRIGGER — MARKER-DRIVEN, SINGLE SOURCE OF TRUTH**
> `compose_frame()` ONLY fires when `frame_changed.txt` grows.
> DO NOT add `dirty=1` from keyboard, view, or state changes.
> The marker file IS the throttle — it prevents redundant renders.
> Need a new render trigger? WRITE TO THE MARKER.
> DO NOT set `dirty=1` directly. That caused triple-rendering.

From `process_key()` end ~3006-3012:

> **NAV MARKER: For ALL layouts, write the marker so `compose_frame()`
> fires through the same single-trigger path. DO NOT set `dirty=1` from
> keyboard — this is the unified render path.**

Restated as rules:

1. **The marker is the clock.** A render happens iff an append-only
   marker file's **size grew** (`st.st_size > last_size` — strictly
   greater, monotonic). Never `mtime`. Never a hash. Never a bare
   `dirty=1`. Size-growth is immune to the sub-second-`mtime` collision
   that bit khtpm's first `cli` fix (two writes in one wall-clock
   second look identical to a seconds-resolution `st_mtime`).
2. **Input handlers write the marker; they do not call the composer.**
   `process_key()` / `handle_mouse()` mutate state and append one byte
   to `frame_changed.txt`. `compose_frame()` is called from exactly one
   place: the marker-growth check in the main loop.
3. **One writer per frame file.** `current_frame.txt` is written only
   by `compose_frame()`. `renderer.c` only reads it. (khtpm GOLD §8
   "One writer per frame file", tpmos PITFALL #17.)
4. **60Hz poll, both processes** (`usleep(16667)`). TPMOS BIBLE §3
   "Active Pulse Throttling" adds the idle rate `usleep(100000)`.
5. **Extra triggers are extra markers, not extra `dirty=1` paths.**
   `state_changed.txt` growth → reload vars + `parse_chtm()` **while
   preserving nav state** (only `initialize_focus()` if `focus_index`
   is now invalid); `layout_changed.txt` growth → swap layout +
   `initialize_focus()`. Each has its own `last_*_size`. The comment
   records *why*: mixing in direct `dirty=1` "caused triple-rendering
   (3 `compose_frame()` calls per keypress)".

## How khtpm's `cli` compares (2026-09-06)

khtpm folded its strip parser into `khtpm_core_render.c` on 2026-09-01
and the terminal mirror regressed. Current shape after the Sep-6 fixes:

| DIAMOND (TPMOS) | khtpm `cli` today |
|---|---|
| marker = **size growth** of `frame_changed.txt` | renderer watches `strip_state.txt` **nanosecond mtime**; presenter watches `strip_ascii_current_frame.txt` size+nsec mtime |
| input handler writes marker, composer called from one site | `dock_write_ascii_frame()` still called from `redraw()`, which has several callers (X events, idle tick, `dock_poll_strip_state`) — NOT yet the single-trigger discipline |
| 2 processes / 2 markers / 1 frame file | 3 processes (keyboard-relay, strip renderer, presenter), frame file written by the renderer only ✓ |
| 60Hz both loops | presenter 60Hz ✓; strip renderer dock tick 33ms (~30Hz) |
| nav state preserved across reparse | ✓ (`g_focus_nav` clamped, not reset) |

**Gap to close if `cli` is ever reworked toward DIAMOND:** move to a
strictly append-only, size-growth marker (a real `strip_pulse.txt`),
and make `dock_write_ascii_frame()` reachable from exactly one
marker-growth check instead of riding every `redraw()`. Until then the
nanosecond-mtime + 60Hz poll is the pragmatic equivalent and measures
~50-90ms keypress→mirror.

## See also

- `CENTROID_GOLD_STD.md` §8 — the rule form of all of the above.
- `reference/chtpm-render-dedup-guidance.md` — the view-dedup layer
  that sits in front of the marker for game/map layouts.
- `09-appendix/forensic-report-flicker.md` — the db-hq flicker
  incident that forced §8 to be written.
- memory `tpmos-reference-location.md`.
