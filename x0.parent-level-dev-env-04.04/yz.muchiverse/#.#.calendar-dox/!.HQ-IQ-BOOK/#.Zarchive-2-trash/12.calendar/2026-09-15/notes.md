# 2026-09-15

## What happened

- **Closed the "bottom-bar click jumps to next nav" bug** (carried
  over from 2026-09-14's dock-bar migration work). Real root cause was
  NOT in the renderer's click handling (proved live with a temporary
  debug log at the hit-test) - it was a stale cross-process constant:
  `khtpm_taskbar_manager.h`'s `KTB_STRIP_N_CELLS` was hardcoded `15`,
  one behind the header template's real 16 cells, so the manager's own
  focus-echo decode landed one tab ahead of every real click and wrote
  that wrong value back over the correct one on the next reparse.
  Fixed at the root (one real macro, `KTB_STRIP_N_CELLS_MAX`, every
  duplicate literal replaced with it) rather than patched at the
  symptom. Full writeup: `03-pitfalls/HOUSE_CODE_PITFALLS.md` #22,
  `04-bugs/bug_bounty.md`'s own closed entry. Verified live against
  the exact xdotool reproduction that failed twice before.
- Also, while chasing it: reordered the dock event loop (drain/dispatch
  pending X events before `hq_idle_tick()`'s reparse work) - not the
  actual cause, but real, harmless hardening, kept.
- Small cosmetic follow-up, direct report: the bottom bar's `+`/`-`
  row pager nudged left and given more gap between the two buttons.
- Committed + pushed (`688bde76`, `claude` and `main` both updated).

## Architecture question raised, worth carrying forward

Direct question this session: is the parser/renderer pipeline clean,
modular, efficient - or a monolith? Should it move toward more ops/
pipe-based IPC or shared memory instead, especially with an eye toward
letting USERS build their own GUI windows/layouts from events later?

Honest read, from what this session actually touched:

- **The process split is already real and good**: each window/entity
  is its own process (`khtpm_core_render.+x`, many modes folded into
  one binary per the house's "no linking, verbatim fold-in" rule), the
  taskbar manager is a genuinely separate binary/process from the
  renderer. That boundary is real, not cosmetic.
- **`khtpm_core_render.c` itself is a monolith by deliberate policy**,
  not oversight - many modes (tile, strip/dock, db-hq, popup, entity-
  menu...) folded into one file/binary specifically to avoid dynamic
  linking/plugin complexity (`CENTROID_GOLD_STD.md`'s own rule). Real
  trade-off: it buys "zero linking, one build, one process image" at
  the cost of a very large single file that's hard to hold in your
  head all at once. Given how many multi-day bugs this session alone
  traced through it, the file's SIZE is a real, growing cost even if
  the "no dynamic linking" policy itself is still sound.
- **The actual weak point today's bug exposed is the IPC PROTOCOL
  between processes, not the process architecture.** Cross-process
  communication here is bare integer "code ranges"
  (`KSC_TAB_BASE 2000`, `KSC_SHORTCUT_BASE 3000`, `KSC_SET_FOCUS_BASE
  6000`, etc.) that BOTH sides must independently hardcode the
  boundaries of. That's exactly what drifted. A self-describing
  contract (named events, or the receiver asking the sender for a
  count instead of assuming one) would have made this whole bug class
  structurally impossible, not just today's specific instance of it.
- **Recommendation, not yet acted on**: don't reach for pipes/shared
  memory for this - the current flat-text relay/vars-file convention
  is simple, greppable, debuggable by hand mid-session (this whole
  session's own methodology depended on being able to `cat`/append to
  a relay file directly), and matches the house's existing philosophy.
  The real fix for "let users build GUI windows/layouts from events
  later" is a **schema/contract layer**, not a transport change: give
  every numeric-code-range convention (TAB/SHORTCUT/HQ_ITEM/SET_FOCUS/
  etc.) one real, shared definition both manager and renderer compile
  against (already true here - they share `khtpm_taskbar_manager.h` -
  the gap was that the COUNT they derive positions from wasn't
  actually flowing from one source, just the header was), and prefer
  the receiver deriving a count from live data over either side
  assuming a constant. If/when user-authored windows become real, the
  event vocabulary they get should be a small number of generic,
  already-proven primitives (the same `<item>`/`ACTIVATE`/nav-index
  convention every existing window already uses) rather than a new
  bespoke protocol per feature - that's the same lesson
  `khtpm-house-standards` already encodes for rendering, just applied
  to the IPC layer too.

## Next up

- Back to game mechanics (DSR / the games roadmap - `08-roadmap/
  design-docs/TEST-GAMES-ROADMAP.md`).
- Soon: AI + networking pipeline work (h-ai-lab, network browser) -
  no concrete plan yet, flagged here so it's on the radar for
  `11.brainstorm/` before it gets scheduled for real.
- Dock Bar Migration Phase 2/3 (generic scrollbar replacing the `+`/
  `-` pager, then deleting the now-dead hardcoded constants) still not
  started - see `08-roadmap/design-docs/DOCK-BAR-GENERIC-LAYOUT-
  MIGRATION.md`.
