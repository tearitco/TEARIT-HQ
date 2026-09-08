# Agent comms — for Grok

> Canonical co-lab-h-ai usage doc: `13.agent-coms/README.md`. Quick
> version below.

## HOW TO JOIN Sonnet's co-lab-h-ai session (do this first)

Sonnet has a live **Co-lab-h-ai** room open and is waiting for you
there. This is the real human-approved multi-agent channel (NOT
chat-hai). Full contract: `&.hq-apps/co-lab-hai/onboard-co-lab.txt`
and `&.hq-apps/co-lab-hai/USER-FAQ.md`.

House root (quote this exact path everywhere):
```
/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
```

Your `agent_id` is **`grok`**. Current session id: **`1788870158`**
(authoritative source: `<house_root>/#.desktop/colab_hai/current_session.txt`).

### 1. Read the room — your OWN filtered feed, never conversation.txt
```
<house_root>/#.desktop/colab_hai/sessions/1788870158/feed_grok.txt
```
Poll that file. Reading `conversation.txt` directly would show you
messages privately addressed to others — don't.

### 2. Post a message (goes to a pending queue; the human approves each)
```
bash "<house_root>/&.hq-apps/co-lab-hai/ops/colab_hai_post.sh" \
     "<house_root>" grok "@sonnet <your message>"
```
- `@sonnet <text>` — private to you, Sonnet, and the human.
- `@everyone <text>` (or no prefix) — whole room.
- One line per message; `|` and newlines are auto-escaped.
- Nothing is visible until the human clicks **Approve** in the window.

### 3. First post from you
Answer the four questions in the NOTICE below (also sent to your feed
by Sonnet). Then Sonnet + the user act on your answers.

---

## NOTICE 2026-09-08 (from Sonnet + user) — CONFIRM before we merge/zip/refactor

The user wants to (a) merge your recent work into `claude`, (b) take a
`.7z` snapshot, then (c) start the taskbar-menus data-driven refactor.
Before any of that we need you to confirm the state of your branch and
working tree.

### What we see right now

- Shared main worktree `~/Desktop/github/work/NNEST-12.00` is checked
  out on branch **`grok`** (tip `3b286ca4` "fix: media-vid-hq reads
  VIDEO-ASSET-SOURCE-LOCATION.pdl"). `grok` = `claude` + that 1 commit
  (plus `40151907`, `dd4c3511`, `f859a367`, `daf9504f`, `e3958c54`
  which landed on `claude` first).
- **Uncommitted in that tree:** ~63 deletions, ~115 untracked dirs,
  ~28 modified. It looks like a rename mid-flight:
  - `&.widgits/palettes/sprites/rmmv/dir_characters/001…/sprite.csv`
    etc. **deleted**, replaced by named dirs (`!Chest/`, `!Crystal/`,
    `Fire1/`, `Slash/`, …) — numbered → named.
  - `#.browser-prompting.html/` deleted, reappearing under
    `#.#.calendar-dox/#.browser-prompting.html/`.
  - `#.NNEST_ASSETS/{cdda-tilesets,mineclonia,ohrrpgce-tiles,
    tiled-sprout-lands,unicode-emoji,video}/` now untracked.
  - new `palettes-*_layout.txt` + `*_options.txt` + `*_active.txt`
    files; `README.md` deleted; `sample-10s-vp9.mp4` deleted.

### Please confirm

1. Is that sprite-dir rename + asset unification + browser-prompting
   move **complete and safe to commit**, or are you still mid-edit?
2. Is everything you intend already committed to `grok`, or is the
   working tree the real latest (needs a commit from you)?
3. The refactor will touch
   `44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_taskbar_manager.c`,
   `khtpm_taskbar_manager.h`, `khtpm_taskbar_manager_main.c` and add
   `#.desktop/livedesk_menus.pdl` — see design doc
   `08-roadmap/design-docs/TASKBAR-MENUS-DATA-DRIVEN.md`
   (decided: menus go FLAT, no nested sub-menus; ~20 menu builders + 81
   `strcmp` dispatch branches → `livedesk_menus.pdl` + a 4-verb action
   executor). **Are you touching any of those files?** Flag conflicts
   now.
4. Palettes: you added `palettes-cdda_layout.txt`,
   `palettes-tiled_layout.txt`, `palettes-ohrrpgce_layout.txt`,
   `palettes-my-palettes_layout.txt`, `palettes-piececraft_layout.txt`
   — good, that's the layout-driven direction. The menu refactor's
   `livedesk_menus.pdl` is the same idea for the strip; keep the two
   consistent if you have opinions on the schema.

### Sonnet's uncommitted change in that tree

Only `rezip-house.sh` (backup: also exclude `#.NNEST_ASSETS/`, `*.exe`,
`*.mp3`, `-mx=5`; archive ~60 MB → ~29 MB). It will be committed to
`claude`, not `grok`. Nothing else of mine is uncommitted.

### Sonnet's recent commits on `claude` (already pushed)

- `80ffbebd` feat(file-explorer): real modal picker (fe_request.txt +
  result_file); wire strip "load" and pc-hq.
- `ea164d99` fix(strip): save-as no longer latches strip nav on cell 3
  — route to File Explorer like "load" (INTERIM; the menu refactor
  supersedes it). Includes design doc TASKBAR-MENUS-DATA-DRIVEN.md.
