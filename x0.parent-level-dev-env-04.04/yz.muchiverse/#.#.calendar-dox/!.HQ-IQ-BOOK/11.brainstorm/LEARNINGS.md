# 11.brainstorm — durable learnings from archived dated dirs

Condensed from `2026-09-05/` and `2026-09-14/`, both moved to
`#.Zarchive-2-trash/11.brainstorm/` (see each chapter's own
`00-INDEX.md` history for why: routine day-scoped brainstorming, not
itself durable reference once the real decisions below are captured).
This file holds only what's still worth finding later — not a
chronological diary.

## Font size / UI scale (2026-09-05 brainstorm — still not scheduled)

`#.desktop/hq_ui.pdl` ships a documented `font_scale` key
(0.5–3.0, default 1.25) that **nothing reads**. `khtpm_core_render.c`'s
`scaled()` — the single choke point every font/layout-box size call
site should funnel through — is a plain identity function today
(`return base_px;`). The `g_dbhq_font_scale` variable its own nearby
comment describes does not exist in the file (same class of stale-
comment drift this session found for `click_two_step`). If this ever
gets scheduled: wiring the existing PDL key into `scaled()` is the
lowest-risk option (reuses already-half-built infra); the real scope
risk is that "surrounding sizes" means auditing every raw-literal size
constant (`CHROME_H`, `DOCK_BAR_H`, `POPUP_ROW_H`, badge/chip padding,
sprite caps, default window size) since not all of them route through
`scaled()` today. No decision was made on the UI control shape
(reuse the opacity-style `+`/`-` buttons vs. a real slider primitive).

## File Explorer widget — BUILT (2026-09-05)

Real, shared `&.widgits/file-explorer/` widget landed same day (manager
+ static `.xhtpm`/CSS, LOAD-mode browsing, real X11-verified). Backs
`pdl-read` and is the intended Save/Load path for any future `toys`
text editor. Two bugs worth remembering if this code is touched again:
a `vars=` path-doubling bug, and `<scrolllist>` requiring a real
`<sidebar>`+`<panel>` pair to lay out at all (a bare top-level one sits
at 0×0 forever). A real reference UX (tpmos's `agy-text-editor` loader
→ editor → FILE MENU → Save-As file browser, with SEARCH/FILE fields,
suggestions, and human-readable directory sizes) was captured live and
is the shape to match or deliberately deviate from if the `toys` text
editor ever gets built — it hasn't been yet, only scoped.

## `102.agy-txt` legacy-launcher bug — FIXED (2026-09-05)

Root cause of `./system/renderer: not found`-style failures: a
`button.sh` refactor removed the old symlink-into-session approach
(switched to `PRISC_PROJECT_ROOT` for *data* files) but left the
*binary* exec lines as relative paths, which broke once the script's
own `cd "$SESSION_DIR"` moved the cwd away from `$SCRIPT_DIR`. Fixed by
making those exec lines absolute. Same bug also hit
`102.editor-📄️00.00`; both fixed and verified. General lesson: a
"legacy binary" framing doesn't fit every launcher failure — check
whether the binary is actually fine before reaching for a
legacy/compat marker.

## Play Mode / entity movement harness (2026-09-14 brainstorm — not started)

Real design decided, not yet built: a new Common Event "move" command
(Wander — generalizes the existing hardcoded `chicken` NPC behavior;
Pathfind-to-entity) plus a distinct Play Mode UI (tactics-RPG-style
context menu: Move/Inventory/Ops/Stats/STOP) toggled from the taskbar,
an entity button, or a new pc-hq "▶ Play" button (doesn't exist yet).
`xelector` (player cursor) and `chicken` (NPC wander) are the two real
existing movement implementations meant to become the shared "entity
harness" backing code, not be reinvented. Two open design questions
flagged, still unanswered as of the brainstorm: does the tactics-range
move overlay need new pc-hq-side rendering (almost certainly, since
pc-hq is a separate engine from the khtpm desktop family), and is
"move" a real Common Event *command* or a built-in verb outside the
event-authoring system? This work was explicitly gated on the
trigger-layer track landing first — the trigger layer closed
2026-09-14 (see `12.calendar/LEARNINGS.md`), so this is now genuinely
unblocked but still not started as of 2026-09-17.

## Scratch/Blueprints visual scripting — after, not before, other work

Real fossil found: the Scratch block-palette rendering view existed
once (built 2026-08-29) and was deliberately deleted when per-project
mode blocks got pulled out of the shared renderer — only a comment
survives (`publish_scratch_blocks()` in
`khtpm_events_hq_manager.c` still emits real data for exactly one
instruction shape, but nothing renders it). Deliberately scoped as
lower priority than the trigger layer (a real missing capability) since
Scratch is polish on an authoring path — the plain Scripting/command-
list tab — that already works. If revived: use the real Scratch 3.0
PNG asset pack (not SVG — this house's rendering stack has no SVG
rasterizer anywhere, but does have a proven `stbi_load()` PNG pipeline
via `palettes_manager.c`); borrow Scratch's visual grammar, keep the
house's own RPG-Maker-style event vocabulary. Whether Scratch- and
Blueprints-style skins share one graph/data model or are two
independent renderers is still an explicitly open, unresolved question
— settle it before building past a skeleton.
