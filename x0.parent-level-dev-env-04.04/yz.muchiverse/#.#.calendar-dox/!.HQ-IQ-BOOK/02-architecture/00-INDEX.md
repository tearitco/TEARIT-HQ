# 02 — Architecture

- `CENTROID_GOLD_STD.md` — **the current gold-standard rendering
  architecture.** Read this before touching any khtpm-family
  renderer/manager code. Moved verbatim (`git mv`) from
  `44.xyz.01.00/CENTROID_GOLD_STD.md`.
- `XHTPM-PARSER-REFERENCE.md` — **complete xhtpm/khtpm feature
  inventory**: every tag/attribute, the `${var}`/`<repeat>`/`show=`
  template pipeline, `<module>` projector contract, every reserved
  `onclick` verb, the 4 layout modes, nav/focus model, CSS subset, a
  hard-limits table (buffer sizes + overflow symptoms), and a
  feature-gap matrix vs. tpmos chtpm. Does NOT cover performance/
  optimization — see `RENDERER-MODULARITY-AND-PERF-AUDIT.md` for that.
- `RENDERER-MODULARITY-AND-PERF-AUDIT.md` — audit of
  `khtpm_core_render.c` + the shared-lib files against
  `CENTROID_GOLD_STD.md`'s own rules AND a plain readability/reuse bar
  (not just the letter of the no-per-app-C rule): remaining `g_is_*`
  globals, copy-pasted logic, per-pixel/per-tick performance waste, and
  a concrete "could this convert to the ops/manager+projector pattern
  events-hq and pchq-board already prove out" assessment for `tp_main()`
  (TILE MODE). Audit only, nothing fixed as part of it.
- `X11-HQ-APP-DESIGN-WISDOMS.md` — **"how it actually feels to build
  one."** §0 points at the runnable copy-paste skeleton
  (`44.xyz.01.00/&.hq-apps/_template-hq/`) and the five non-obvious
  must-haves (sidebar+panel ⇒ chrome + taskbar entry; `database-window`
  ⇒ persistent; `<repeat>` bind name = key prefix; `<module>` exec
  contract; the `&`-path / `sh -c` menu-row gotcha). Then the deeper
  layer: delegating the manager to a fresh agent, cross-app comms via
  published state, `dispatch()` verb blocks, buffer limits.
- `RENDERING-ORIENTATION.md` — how the khtpm merged binary actually
  works today (the "current, not-yet-ideal" companion to the gold
  standard).
- `TWO-PARSER-FAMILIES.md` — why chtpm_parser_pal and khtpm both exist,
  at a glance, and current migration posture.
- `INPUT-RELAY-PIPELINE.md` — how a click/keypress reaches the
  renderer, the relay files, nav_index/Tab-cycle design, LayDoc vs
  Elem/CSS.
- `STATE-AND-PDL-CONVENTIONS.md` — file-based state discipline, what a
  "manager" process is, PDL conventions, asset-path conventions.
- `LEGACY-GL-PIPELINE.md` — the older chtpm_parser_pal → GL mirror
  pipeline, for projects still on that path.
- `HTML-MEDIA-AND-SCRIPTING.md` — should a browser-type app get a
  separate HTML-specific parser, new img/video tag types, or reuse
  the existing generic `sprite=` mechanism? (Reuse it — no new parser,
  no new tags.) Also covers where Duktape JS execution should live
  (manager/op-side only, never the shared renderer) and cross-refs
  `07-install-and-ship/SECURITY.md` for the sandbox questions that
  raises.

- `reference/TPMOS-DIAMOND-render-chain.md` — **the "DIAMOND
  standard"**: a line-by-line dissection of the original 1.TPMOS render
  chain (`chtpm_parser.c` + `renderer.c`) — two processes, two
  append-only **size-growth** markers, one frame file, 60Hz both loops,
  input handlers write the marker and never call the composer. GOLD §8
  is the rule distilled from this; this is the worked example with real
  file+line citations and a gap table vs. khtpm's current `cli`
  terminal mirror. Read before reworking any marker / terminal-mirror
  path.

Condensed from: `44.xyz.01.00/CENTROID_GOLD_STD.md` (moved),
`#.#.calendar-dox/1.^V-hq/SKILLS.md` §2, `HOUSE_FAQ.md` (Architecture/
Files-Compliance/Nav-Input/Assets sections), `44.xyz.01.00/
!.HOUSE_STDS.md` §A.7/§B.
