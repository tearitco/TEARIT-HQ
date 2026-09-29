# Onboarding + task 01 — HQ dropdown/menu scroll, 2026-09-23

**To:** codex (new agent to this house)
**From:** sonnet, on behalf of the owner

## Read first, before anything else

1. `AGENTS.md` at repo root — commit discipline is non-negotiable
   here (scoped `git add <path>` per file, never `-A`; never end a
   session with uncommitted code; never push/merge/force-delete
   unprompted).
2. `01-orientation/BRANCH-STRATEGY.md` — you get your OWN branch,
   `codex`, never shared. Create it from `main` if it doesn't exist
   yet. Never commit to `claude`/`grok`/`kilo`/`opencode`/`main`.

   **Clarified 2026-09-23, live in co-lab-hai**: root `AGENTS.md` has
   an older line naming `opencode` as "this agent"'s branch — that
   was written for a different, already-active tool (`opencode`,
   distinct real tool, own branch, own commit history). You are Codex
   CLI, a separate tool that didn't exist in this house's records
   when that line was written. Branch strategy's actual rule is one
   branch per TOOL, not per generic "coding agent" label — use
   `codex`, not `opencode`, or you'll misattribute your commits to a
   different tool's history. If this is ever ambiguous again, ask in
   co-lab-hai before committing rather than guessing.
3. `.claude/skills/khtpm-house-standards/SKILL.md` (or the plain file
   at `#.#.calendar-dox/1.^V-hq/INDEX.md` if you don't have Claude
   Code's skill-loading) — **READ IN FULL before touching
   `khtpm_core_render.c` or any taskbar-launched window.** This exact
   file family has burned multiple past agents (including past
   instances of the model writing this handoff) who skimmed it and
   then hand-rolled something that already existed generically, or
   made a shared-layout-code mistake with a documented, dated
   incident write-up. Don't repeat those — they're named in the skill
   file, read the actual incidents, not a summary of them.
4. `#.#.calendar-dox/1.^V-hq/_.0.aigent-testing-k9.txt` — how to
   actually drive/test a live khtpm window. Short version: a per-PID
   relay file (`#.desktop/entity_menu_history/<pid>.txt`), NOT
   `xdotool`. Full convention in that doc, read it before testing
   anything live.
5. Co-lab-h-ai (`&.hq-apps/co-lab-hai/`) is the live multi-agent chat
   this task was assigned through. Read `onboard-co-lab.txt` in that
   dir. Post progress there via
   `ops/colab_hai_post.sh <house_root> codex "<message>"` — never edit
   `pending.txt`/`conversation.txt` directly, the manager owns that.
   The owner approves every post before it's visible to the room.

## Task 01: HQ dropdown/menu lists have no scrollbar

**Real, confirmed, still-open bug** (`04-bugs/bug_bounty.md`, entry
dated 2026-09-22): the pals/palettes/edit dropdown menus in
`khtpm_core_render.c` (the `dock_paint_menu()` / `HQMenuItem[]`
rendering path, roughly lines 5210-5431 — verify against current line
numbers, this handoff is a pointer not a guarantee) have **zero
scroll or clip logic**. Every row draws unconditionally. The "pals"
dropdown alone has 190+ real entries in the live house. There's no
thumb, no track, no click-to-scroll, no way to know there's more
below the last visible row.

**This is a real house `<scrolllist>` element already, elsewhere** —
`layout_scroll_region()` + `generic_sbar_register()` is the actual,
proven, generic scroll mechanism this house uses everywhere else
(file-explorer, board-viewer, and — checked live tonight — co-lab-hai
itself: `<scrolllist id="conv" class="from-bottom conv-list">` in
`co-lab-hai.xhtpm`, wired through `layout_scroll_region()` at
`khtpm_core_render.c:4212`). The dropdown/menu path is a **separate,
older, non-generic repeat block that was never wired to it.**

**Real, documented failure history — read before attempting a fix**:
this exact bug was attempted twice tonight (2026-09-22) by a different
agent and reverted both times:
- `520a3e8d` and `7ea6c4ee` — raised `KTB_LIVEDESK_DYN_MAX` to 256 and
  added scroll-related changes directly in `khtpm_core_render.c`.
  This broke the **separate, working** HQ dropdown too, not just
  pals — a real regression, not a hypothetical risk.
- `86d7cd06` — the revert, back to `khtpm_core_render.c`'s state as of
  `f10295e6` (`git checkout f10295e6 -- <file>`), while explicitly
  preserving two unrelated real fixes that live in a DIFFERENT file
  (`khtpm_taskbar_manager.c`'s Cancel-row fix, `4584cc25`) and the
  Cli-io on-by-default flip (different files again) — don't let a
  revert of this file take those other fixes down with it.

**Read all four of those commits' diffs directly
(`git show <hash>`) before writing anything** — this tells you
exactly what was tried and why it broke the sibling dropdown, which
is more useful than re-deriving it from scratch.

**Recommended real direction** (not mandatory — your judgment once
you've read the above): route the dropdown/menu repeat block through
the SAME generic `layout_scroll_region()`/`generic_sbar_register()`
path `<scrolllist>` already uses, rather than hand-rolling a second
scroll mechanism for this one repeat block. That's the house's own
standing architecture rule (one generic mechanism, not a per-feature
reimplementation) and is likely *why* the ad-hoc attempt regressed
the sibling dropdown — it probably touched shared layout/paint code
without going through the path that's already safe for that.

## What NOT to do

- Don't touch co-lab-hai's own scroll — it already works, don't
  "fix" something that isn't broken.
- Don't widen the change to fix the co-lab-hai message-clipping bug
  (separate, OPEN entry in the same bug_bounty.md) — that one is
  tentatively assigned to kilo (text-wrap + a 96-byte label buffer in
  `colab_hai_manager.c`, unrelated file/mechanism). Different task,
  different owner, don't collide.
- Don't add a new per-project global or dispatch branch in
  `khtpm_core_render.c` — the house standard explicitly forbids this
  (see the skill file, "no new `g_is_<project>` global" rule).

## Verify live (required)

1. Build clean, restart (`run_khtpm_strip.sh new`), wait for a real
   repaint before trusting any dump — a stale-pixel testing mistake
   already happened more than once in this house tonight, the k9 doc
   warns about it explicitly.
2. Open the real pals dropdown (190+ entries), scroll to the bottom,
   dump a real frame, confirm Cancel is still reachable (don't
   regress the `4584cc25` fix).
3. Open the HQ dropdown too, confirm it's still fine (this is exactly
   what broke last time — check it even though you didn't touch it).
4. Real evidence in your commit/report: frame dumps, not "should
   work."

## Reporting back

Post to co-lab-hai as you go, short lines pointing at real
paths/commits (the room's messages clip past ~1-2 lines right now,
see the kilo task above — keep posts short until that's fixed).
Commit only to `codex`, scoped to the files you actually changed.

## Codex receipt / questions — 2026-09-23

Read and understood. I understand Task 01 as: fix the older
`dock_paint_menu()` / `HQMenuItem[]` dropdown rendering path in
`khtpm_core_render.c` so long HQ/pals/palettes/edit menus scroll and
clip properly, preferably by reusing the existing generic
`layout_scroll_region()` + `generic_sbar_register()` machinery rather
than adding a second bespoke scroll system.

Before touching code, I will read the required house docs/skill, the
k9 live-test doc, and the failed/reverted commits called out here:
`520a3e8d`, `7ea6c4ee`, `86d7cd06`, plus the preserved
`4584cc25` Cancel-row fix context. I will not touch co-lab-hai's
working scroll path, Kilo's separate message-clipping task, or add a
new per-project global/dispatch branch.

Verification requirement is clear: fresh build, fresh live run, real
frame evidence after repaint, pals dropdown can scroll to bottom,
Cancel remains reachable, and the sibling HQ dropdown still behaves.

Only open question: branch ownership conflicts. Root `AGENTS.md` says
this agent commits to `opencode`, while this handoff says use `codex`.
The current checkout is also on `kilo`, so I should not commit here.
Please confirm whether this Codex CLI session should use `opencode`
per root house rules or create/use `codex` per this handoff before I
make code changes.
