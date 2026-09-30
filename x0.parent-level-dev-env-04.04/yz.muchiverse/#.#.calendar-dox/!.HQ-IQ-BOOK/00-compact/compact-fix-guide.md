# 🧷 compact-fix-guide.md — per-file pointer index (not a duplicate of the pitfalls/bounty logs)

**Why this exists, and why it's not the same as `03-pitfalls/
HOUSE_CODE_PITFALLS.md` or `04-bugs/bug_bounty.md`:** those two are
organized **chronologically, by incident** — great for "have I seen
this exact symptom before," slow for "I'm about to edit
`layout_dock_bar()` in `khtpm_core_render.c`, what should I already
know before I start." This file is organized **by real file/code
area** instead: one section per file, each a short list of pointers
(doc + line/anchor) into the real, existing writeups — never restating
their content here. Keep entries to 1-3 lines: file/line + one phrase
of what it's about + which doc has the full story.

**How to use it:** before editing a file listed below, skim its
section first — 30 seconds, not a research pass. If a real new lesson
comes out of your own session's work, add ONE line here (pointer only)
and put the actual writeup in `bug_bounty.md`/`HOUSE_CODE_PITFALLS.md`
as usual — this file must never grow its own duplicate prose.

**Not exhaustive.** Only files that have already generated a real,
written lesson are listed. A file's absence here means nothing —
check `bug_bounty.md`/`HOUSE_CODE_PITFALLS.md` directly for anything
newer than this doc's own edits.

---

## `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`

The shared, generic Elem/CSS renderer — the single most-touched file
in the house. Read `khtpm-house-standards` skill in full before any
edit here; it exists BECAUSE of this file specifically.

- **`layout_dock_bar()`'s dropdown-child stacking** — `stack_n` used to
  reset by sibling-adjacency, broken by incremental reparse's
  non-contiguous ordering (real root cause: `bug_bounty.md`, search
  "stack_n"). Fixed with a per-`target_id`-keyed counter.
- **`kh_get_var()` / `kh_substitute_vars()`** — template `${VAR}`
  builtins (`HOUSE`/`PKG`/`PID`/`WIN_X`/`WIN_Y`/…) are substituted
  ONCE at `parse_chtpm()` time, which runs BEFORE most of `main()`'s
  later argv/mode-detection code. A var whose value is set later than
  the parse call will silently read stale/default data (real incident:
  `bug_bounty.md`, "WIN_X/WIN_Y ordering bug", commit `d3792bbf`) —
  before adding a new builtin, check it's assigned *before*
  `g_window = parse_chtpm(g_chtpm_path)` in `main()`, not after.
- **A new `layout_*` branch** — grep for a sibling shape first
  (`class="swatch"`, `layout_scroll_region`,
  `layout_fixed_rows_and_scrolllist`) before writing one; a new branch
  MUST clip (never translate) off-screen rows and MUST NOT add its own
  key handling. Full incident + rules: `khtpm-house-standards` skill,
  "Adding a *layout* branch" section; `HOUSE_CODE_PITFALLS.md` #14.
- **A new `g_is_<project>` branch, a per-project dispatch case, or a
  hand-built `Elem` tree** — don't. `02-architecture/
  CENTROID_GOLD_STD.md` items 2/7; `khtpm-house-standards` skill.
- **Element identity across reparse / `<cli_io>` `content=` vs
  `label=`** — `content=` seeds the editable buffer, `label=` is
  display-only; mixing them up breaks Backspace and can double-render
  text. Full rule: `khtpm-house-standards` skill, "Element identity
  across reparse" section.
- **Scoped nav / `[^]`/`[>]` badges** — read `09-appendix/HANDOFF-
  scope-nav-and-chtpm-port.md` §9 before touching
  `kh_apply_scope_confine`/`kh_elem_in_scope`/nav badges.
- **A window's own on-screen X error / silent disappearance** —
  `HOUSE_CODE_PITFALLS.md` #23 (no custom X error handler = any
  protocol error silently `exit(1)`s the process, looks like a crash
  with no message).

## `&.widgits/_shared-lib/khtpm_draw_core.c`

Shared, text-included draw routines (edit the SHARED copy here, not a
per-app `ops/` copy — those are overwritten on build; running windows
need a relaunch to pick up a rebuild).

- **`draw_elem()`'s badge-position branch** (~line 1723, "sprite tile:
  badge drawn ABOVE the row") — excludes `dock-cell` rows (2026-09-15)
  and `dropdown-child` rows (2026-09-23, `tax_robot`'s missing badge,
  `bug_bounty.md`) for zero-vertical-gap packing. A NEW zero-gap row
  class will hit the same bug if not added to this same exclusion list.
- **`hq_sprite()`** — returns NULL silently if a pal's
  `sprite.csv` doesn't exist; a non-empty `e->sprite` field does NOT
  guarantee a real icon draws. Don't assume "field is set" == "image
  will render."

## `_.monads/_.livedesk-taskbar/ops/khtpm_taskbar_manager.c` + `khtpm_taskbar_manager_main.c`

The taskbar manager (business logic) + its `main()`/event loop.

- **`livedesk_build_pals_menu()`** (~line 3471) — a fixed-size array
  cap (`KTB_LIVEDESK_DYN_MAX`, `khtpm_taskbar_manager.h`) can silently
  crowd out a trailing row (real incident: Cancel row missing,
  `bug_bounty.md` commit `4584cc25`) if the real item count can exceed
  it. Reserve trailing-row slots up front, don't append-and-hope.
  `hi_N_sprite` is unconditionally derived from the pal name regardless
  of whether that dir actually has a `sprite.csv` — see
  `khtpm_draw_core.c`'s `hq_sprite()` note above.
- **`notes-<cell>` rows** (~line 4805) — a real, intentional feature
  (auto-appended dev-notes row per header-cell menu), not a bug. Don't
  re-diagnose it as a ghost/ ophaned row (`bug_bounty.md` has the full
  "retraction" writeup from when this happened once already).
- **`ktb_init()`/`livedesk_spawn_desk()`** (~line 2474) and
  `ktb_find_live_pid_for_pal()` (~line 2768) — a per-entity `/proc`
  scan here runs BEFORE the manager publishes `strip_ui.txt`, so it's
  on the taskbar's own visible-appearance critical path. Fixed once
  already (snapshot-once-per-pass, not per-entity) — don't reintroduce
  a per-entity re-scan.
- **`khtpm_taskbar_manager_main.c`'s `dispatch_code()`** — bounds
  checks on focus-index round-trips with the renderer have drifted out
  of sync with the renderer's own synthetic slots (pager buttons) more
  than once; the two clamps live in two different files/functions
  (`bug_bounty.md`, "dock focus... never reaches the last 2 pager
  slots") — if you touch one, check the other.
- **Runtime instance files under `xyzfs/users/*/.../menu.chtpm` vs the
  source templates under `#.desktop/entities/*/menu.chtpm`** — these
  are generated 1:1 by `meta_to_menu_chtpm.py` from `meta.pdl`, but
  existing files are NOT auto-regenerated (delete + rerun the converter
  to refresh). A fix to the source template does NOT propagate to
  already-generated live instances — both need editing (real incident:
  the Cli-io inline-field fix touched 18 source templates AND 38 live
  instances separately, `bug_bounty.md`/`12.calendar/2026-09-24/
  notes.md`).

## `_.monads/_.livedesk-taskbar/ops/khtpm_entity.c`

The OLDER, still-live, hand-rolled popup renderer for entities with no
`menu.chtpm` yet (its own X11 event loop, `open_context_menu()`, real
`CLI_IO`/`GOTO:`/`STATE:` action dispatch built in). Do NOT port its
patterns into `khtpm_core_render.c` — that would duplicate a legacy
bespoke-popup anti-pattern into the generic renderer this house is
actively migrating away from (see `meta_to_menu_chtpm.py`'s own header
comment, "ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md").

- **A tile/entity-mode window with no `dpy`/`cmap`/`screen` globals
  set** — `alloc_pixel()`/`redraw()` called from here crash or no-op;
  use `tp_hex_pixel()` instead (house memory:
  `khtpm-tp_main-globals-footgun`).
- **`XGrabKeyboard`/focus** — real incidents of a stuck grab from one
  process killing keyboard input for every OTHER window too
  (`HOUSE_CODE_PITFALLS.md` #24, `bug_bounty.md` dpy/grab entries).

## `&.widgits/entity-cli/` (`open_entity_act.sh`, `ops/entity_cli_commit.sh`, `ops/act_row.sh`)

- **`open_entity_act.sh`** launches a brand-new `khtpm_core_render.+x`
  process for the "Act" menu — by design (its dynamic `skills.pdl`
  content is real justification for a separate window, per a
  2026-09-24 direct decision), but it must receive the calling
  window's real `${WIN_X}`/`${WIN_Y}` as argv or it opens in the wrong
  place (`bug_bounty.md`, commits `fa246921`/`d3792bbf`).
- **`ops/act_row.sh`**'s `move|use)` case is a literal no-op
  (`echo "$cmd recorded"`) — real, tracked, NOT fixed yet
  (`bug_bounty.md`, 2026-09-24 entry). `attack` has real logic
  (`apply_range.sh`) and is the template to copy when this gets built.
- **`ops/entity_cli_commit.sh`** takes its target entity directory
  from `$pkg` (argv[1] = the RENDERER's own `g_package_dir`) — this
  only works correctly when the `<cli_io>` field is embedded directly
  in that entity's own window (current, correct state as of
  `d1e630bf`/`f379e233`). If you ever see this script running against
  the WRONG entity, check what window it's actually embedded in first.

## `&.widgits/tile-picker/ops/khtpm_show_choices.c`

Standalone choice-picker binary, spawned by scripts like book-stack's
`dispatch.sh`, not part of the `khtpm_core_render.c` family directly
(though it launches that same binary internally for its own popup).

- **Position** comes from `desktop_pos.txt` (the entity's SAVED tile
  position, reference px) by default — only an approximation of
  wherever the window that triggered the picker actually is. Prefer
  `KHTPM_WIN_X`/`KHTPM_WIN_Y` env vars (already real screen px, no
  `kps_ref_to_screen()` conversion) when the caller can provide them
  — see commit `b0187294` for the pattern (env vars survive an
  `execl()` chain with zero explicit passing needed at each hop).

## `08-roadmap/design-docs/DUSTOPIA-HACK.md` (not code — the "Addendum 2026-09-23" ladder)

Referenced from multiple sessions' work; keep its own inline
`**Met**`/`**Not met**` markers up to date rather than trusting a
summary of it written even a day earlier — a fix elsewhere in the
house (e.g. `send_window_key`, commit `ff456637`) satisfied step 2
silently, and the doc's own text didn't say so until manually updated
2026-09-24. Always open the doc itself and check its OWN latest dated
section before treating a step as still open.

---

## See also (already covers cross-file / whole-house material — don't duplicate here)

- `!.HQ-IQ-BOOK-MAP🗺️v0.1.md` — the real book-wide index, chapter by
  chapter, including this `00-compact/` directory's own file list.
- `01-orientation/GLOSSARY.md` — house terms, alphabetical.
- `03-pitfalls/HOUSE_CODE_PITFALLS.md` — numbered, chronological,
  the FULL writeup for anything only pointed to here.
- `04-bugs/bug_bounty.md` — hard-to-pin/recurring bugs, newest at top.
