# 🤝 Grok handoff — 2026-09-02 (fresh, replaces the 2026-08-28/29 doc)

> **UPDATE 2026-09-03 — how you're actually reading this.** The
> "real, live, human-supervised chat channel" §1 of the roadmap doc
> describes as something to *build* is now BUILT and working:
> **`&.hq-apps/co-lab-hai/`** (taskbar → h-ai cell → "Co-lab-h-ai").
> Read `&.hq-apps/co-lab-hai/USER-FAQ.md` for the real onboarding
> contract (one shell command, no API key). If you're reading this
> handoff INSIDE that room right now, you're already onboarded — post
> your own real questions there and the owner will approve them into
> view. Real, live-verified features: approve/reject gate, multi-agent
> tested (4 separate real agents joined cold), word-wrap, sessions
> (start fresh without losing history), and `@agent`/`@everyone`
> addressing with real per-agent visibility filtering (an
> `@`-addressed message is genuinely absent from other agents' own
> feed files, not just hidden in the UI). Known, disclosed gaps: no
> dropdown "Menu" yet (plain "Dir" button only), no sidebar scroll
> region (fine for ~6 agents, the owner's own stated ceiling).
>
> **The old `GROK-RENDER-INPUT-REFACTOR-HANDOFF.md` is archived**, not
deleted — `legacy-root/GROK-RENDER-INPUT-REFACTOR-HANDOFF-2026-08-28-
to-29-ARCHIVED.md`, real historical record if you need it, but its own
target file no longer exists under that name (see §1) and its own
open items are either resolved or superseded by the plan below. Start
here, not there.

## 1. What changed since that doc (read this before anything else)

- **`khtpm_entity_menu_render.c` was renamed AND merged.** It's now
  `khtpm_core_render.c` (`*.monads/*.livedesk-taskbar/ops/`), and as
  of 2026-09-01 it also absorbed the taskbar strip parser's own duty
  (`khtpm_strip_parser.c` folded in verbatim). If you see the old name
  anywhere, it's stale — this is the same file, just renamed/expanded.
- **`CENTROID_GOLD_STD.md`** (`44.xyz.01.00/CENTROID_GOLD_STD.md`,
  adopted 2026-08-31) is now the mandatory architecture for anything
  new — read it in full before touching the renderer. The short
  version: one real parsed `.chtpm`+CSS Elem tree, business logic in a
  separate manager process, and **never** a new `g_is_<project>` flag/
  branch in the shared renderer — a new mode/app is data-driven
  (classes, attributes), not a new hardcoded branch. The old handoff's
  own claim/release protocol is still the right idea (don't both edit
  the shared file at once) — just apply it to the new filename.
- **The whole house moved.** New root:
  `/home/no/Desktop/github/work/NNEST-12.00/`. Internal folders
  renamed too: `44.xyz❤️‍🔥️00.17` → `44.xyz.01.00`. Full detail in
  `09-appendix/S1_HOUSE_PATH_MIGRATION.md` if you land on an old path
  reference anywhere.
- **Docs are now a book.** This file lives in
  `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/`. Start at
  `!.HQ-IQ-BOOK/README.md` for the chapter map.
- **Real feature work landed since Aug 29** that the old doc predates
  entirely: a full network-browser app (real back/forward/tabs/
  bookmarks, media→sprite conversion, a sandboxed Duktape JS-eval op —
  see `02-architecture/HTML-MEDIA-AND-SCRIPTING.md`), a real install/
  uninstall CLI pipeline (`x0.parent-level-dev-env-04.04/xyz-installer-
  dev/`), and a long-term security review (`07-install-and-ship/
  SECURITY.md`).
- **Testing convention unchanged, still real and still the bar**:
  relay-file injection (decimal `KEY_PRESSED: <n>`/`MOUSE_EVENT: ...`
  lines into `#.desktop/entity_menu_history/<pid>.txt`), a real
  screenshot via `dump_frame_png_op.+x <hex_window_id> <out.png>`,
  cross-checked against real backing state files — never claim
  something works without both. See `06-testing/` for the full
  reference.

## 2. The real work queued for you, in order

Full detail in `FORWARD-ROADMAP-2026-09-02.md` (same directory's
parent, `08-roadmap/`) — read that document in full, this is just the
short version and the actual sequence:

1. **Status graph first, before any code**: a real, grounded (code-
   checked) map of what's currently built in events/db-hq/palettes/
   plugins (RPG-Maker parity work), what you're about to change, and
   how — reviewed by the owner before you start. This is the real
   first deliverable, not a formality.
2. Once a real, live, human-supervised Sonnet/Grok/owner chat channel
   exists (being built, see roadmap doc §1) — media-studio → khtpm
   port, then the other network apps (pal-chain, pal-forum, pal-chat-
   irc) → khtpm port, same migration pattern as the already-done
   network-browser conversion.
3. A settings pass: taskbar/context/window font size + corner
   "roundout" — real, generic, not per-app.
4. db-hq RPG-Maker parity continuation: the 13-tab audit
   (`sep-1-grok.md`), tile-placement tools (rectangle select/delete),
   events depth extended to entities + player + plugins (not just
   Common Events).
5. Image editor + AI (Stable Diffusion / text-to-image) roadmap —
   scoped for real once 2-4 have real progress, not before.

## 3. Working conventions, carried forward from the old doc (still real)

- **Claim/release protocol**: post a real claim before editing a
  shared file both of you might touch, release when done, same
  discipline the old doc used successfully for weeks.
- **Append, don't rewrite**, in any shared collaboration doc — this
  house's standing convention for exactly this kind of file.
- **Real verification only** — a clean compile is not evidence; a
  screenshot you actually looked at, or a real file-level check, is.

## Cross-references
- `FORWARD-ROADMAP-2026-09-02.md` — the full task sequence.
- `legacy-root/GROK-RENDER-INPUT-REFACTOR-HANDOFF-2026-08-28-to-29-
  ARCHIVED.md` — real prior history, superseded not deleted.
- `CENTROID_GOLD_STD.md`, `06-testing/`, `09-appendix/
  S1_HOUSE_PATH_MIGRATION.md` — the standing references this doc
  assumes you'll actually go read, not just take on faith.
