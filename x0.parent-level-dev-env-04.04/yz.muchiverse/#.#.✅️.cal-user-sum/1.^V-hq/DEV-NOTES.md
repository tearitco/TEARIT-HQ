# Dev Notes (casual)

Running scratchpad for future-feature ideas, half-formed plans, and "we
should talk about this eventually" stuff — deliberately NOT tracked in
INDEX.md and not held to its documentation standards. Add freely, prune
freely. Once something here actually gets designed/built, it graduates to
a real doc (and INDEX.md), and its entry here can be deleted or marked done.

---

## Piececraft: HQ-style metadata menu, replacing the blocking pre-setup screen

**2026-08-29.** Idea, not designed yet: piececraft-xyz's current setup
screen (Victory/Map/Combat options → `CONFIRM_START`, see
`HANDOFF_NEXT_SESSION.md`) is a blocking, modal, pre-game screen — you
have to get through it before you see anything, which makes debugging
awkward (can't peek at/adjust level metadata without restarting the whole
flow).

The idea: build a real HQ-style menu (same convention as db-hq/events-hq/
Settings - `khtpm_entity_menu_render.c`'s merged-renderer pattern, or its
own dedicated equivalent) that opens FROM piececraft's own native running
screen, non-blocking - lets you view/edit level metadata live instead of
only at a gated pre-setup step. Probably replaces the pre-setup screen
entirely rather than living alongside it, but that's not decided.

Open questions (not yet discussed):
- Does this reuse the merged renderer (`khtpm_entity_menu_render.c`) as a
  9th mode, or does piececraft get its own small standalone HQ window
  (matching civ-txt/tactics-txt's own real conventions, since piececraft
  is a clone of civ-txt's P1 skeleton, not a khtpm-family app)?
- What metadata is actually editable live vs. read-only (game_id/turn
  counter probably shouldn't be hand-edited mid-game; map_scale/victory
  condition probably should be, for debug purposes)?
- Does this block on piececraft's own clone-verification work finishing
  first (see HANDOFF_NEXT_SESSION.md - the P1 clone was never confirmed
  to build/run), or can it be prototyped in parallel?

Not started. Revisit once piececraft's clone phase is actually verified -
building a debug menu for an unverified base is premature.

---
