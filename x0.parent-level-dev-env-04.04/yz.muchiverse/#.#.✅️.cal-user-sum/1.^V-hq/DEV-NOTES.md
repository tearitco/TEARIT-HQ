# Dev Notes (casual)

Running scratchpad for future-feature ideas, half-formed plans, and "we
should talk about this eventually" stuff — deliberately NOT tracked in
INDEX.md and not held to its documentation standards. Add freely, prune
freely. Once something here actually gets designed/built, it graduates to
a real doc (and INDEX.md), and its entry here can be deleted or marked done.

---

## ✅ DONE (2026-08-30) - Piececraft: HQ-style metadata menu / khtpm board window

**Graduated** - this idea from 2026-08-29 turned into real, built,
live-verified work. See:
- `PIECECRAFT-HQ-BOARD-KHTPM-CONVERSION-2026-08-30.md` - the real khtpm
  board window (`run_pchq_board_mode()` in `khtpm_entity_menu_render.c`),
  true engine-parity nav/interact, real chrome styling, real perf fixes.
- `CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` §5-8 -
  the follow-on File(level)/Desk(map) menu design, still IN PROGRESS
  (design confirmed, code not started as of this entry - see that doc's
  own §8 real next steps).
- `PCHQ-BOARD-HISTORY-INJECTION-CHECKLIST-2026-08-30.md` - the real
  local test recipe for this window's own render/relay pipeline.

Original open questions resolved: reused the merged renderer
(`khtpm_entity_menu_render.c`) as a new mode, NOT a standalone window -
piececraft-hq's board content itself stays on the legacy
`chtpm_parser_pal` engine (absolute parity), only the window chrome is
khtpm. Metadata-editing scope narrowed to File/Desk (level/map
selection), not a general live-edit-everything panel.

---

## ✅ DONE (2026-08-30) - Taskbar File/Desk menus: C-hardcoded → real PDL-driven

Closed the `TASKBAR-MENU-ARCHITECTURE.md` standing-debt item for these
two specific cells: `livedesk_build_file_menu()`/`_desk_menu()` now read
`file_menu_N_label/_cmd` / `desk_menu_action_N_label/_cmd` rows from
`#.desktop/livedesk_taskbar.pdl`, matching the already-correct
`livedesk_build_hq_menu()`/`_palettes_menu()` pattern. Other cells
(user/player/db/pals/toys/clock/ai) remain unconverted - same debt,
not touched this pass.

**Two real bugs found + fixed live during this work** (see git log
`a2b7e8df`): (1) `livedesk_switch_desk()`'s outgoing-desk snapshot could
silently wipe a real, populated desk `.pdl` with zero rows if the live
entity registry was transiently empty at switch time - real data loss,
happened live, recovered from git, now guarded against. (2)
`run_khtpm_strip.sh`'s restart had a race (fixed 1s sleep not always
enough for the old manager to actually exit) that could leave the
parser running with NO manager - looked like "entities in toolbar but
not on screen." Fixed to poll for real death instead of guessing.

---

## Taskbar: search input + page-scroller navs above the toolbar

**2026-08-30.** Idea, not designed yet, recorded for later per direct
request: add a "search input" field, plus "page scroller" navs, to the
bottom toolbar - positioned slightly above the toolbar, roughly centered
horizontally. Real, existing precedent worth checking first when this gets
picked up: the house's own `<cli_io>` input mechanism (`!.HOUSE_STDS.md`
§K.3 item 5, `onclick="input:<file>|<post cmd>"`) is likely the right real
building block for the search field itself, matching how this session's
own Settings-window opacity control and bookmarks' New+ field already
work - not a new input mechanism to invent. What "search" actually
searches (open windows? entities? commands?) and what the page-scroller
paginates are both undecided. Not started.

---
