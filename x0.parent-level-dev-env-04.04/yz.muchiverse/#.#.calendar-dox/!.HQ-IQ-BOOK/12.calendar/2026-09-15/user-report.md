# 📋 Event-Trigger Build-Out — Status Report for the User

## ✅ What's DONE (fully closed, proven live, no fake "should work")

🎮 The **whole point** of this track: make something happen automatically
in a game world, with **zero** manual button-pressing. That's now real.

- 🚶 Player walks onto a tile in piececraft-hq (`x=6, y=5` in `cdda_sample`)
- 📜 A real ledger line gets appended automatically (`touched_npc|x:6,y:5`)
- 👀 A persistent watcher daemon (`pc_trigger_watcher.c`) sees it instantly
- 🔔 It fires the matching Common Event on its own — **no editor, no ▶️ Play
  button, no human touching anything**
- 💬 A real "Show Text" popup genuinely appeared, live, proven with a full
  debug trace

Before this, **the only way ANYTHING ever ran was a human clicking Play**.
Every `trigger=player-touch` label sitting in the event data was 100%
decorative — nothing ever read it. Now something does. 🎉

🐛 Two real bugs got found and fixed along the way, not swept under the rug:
1. 📄 `mr_show_text.+x` needs a real **file path**, not inline text — fixed
2. 👻 An "it fired twice!" scare turned out to be a stray leftover process
   from an earlier test, not a real logic bug — found it, killed it, re-proved
   clean single-fire behavior

## 🧩 Is there bulk work left? Short answer: not on the trigger mechanism itself.

The trigger layer is **closed**. What's left is real, but smaller/scoped:

1. 🎯 **No per-entity scoping yet** — right now there's only ONE
   `player-touch` common event in the whole house, so it doesn't matter
   which one fires. The moment a SECOND one exists, this breaks (it'll fire
   ALL matching-trigger events, not just the right one). 🚩 Flagged, not
   fixed — real, contained work when it's actually needed.
2. 🖱️ **No menu button yet** for loading a specific map
   (`CONFIRM_START_MAP`) — works today only via the relay/inbox testing
   convention, not a real clickable UI element yet.
3. 🪤 **One pre-existing bug flagged elsewhere, not touched**:
   `m8_redhorned`'s own `cmd_4.sh` has the exact same "literal text instead
   of a file path" bug that got fixed for the new event. Just a note for
   later.

## 🚀 The REAL bulk work: everything downstream, genuinely 0% started

This whole trigger-layer effort existed to **unblock** one specific thing:
`PLAY-MODE-ENTITY-HARNESS-DESIGN.md` — and that doc says, in its own words:
**"Not started. Waiting on the trigger-layer track to actually begin
first."** 🕐 That wait is over now — but nothing has been built yet.

That doc's own real build order (don't skip steps 🪜):
1. 🐾 "Wander" — smaller, provable-first half of NPC "move"
2. 🏃 Enough of "move" to drive one real automated trigger test
3. 🎭 The FULLER Play Mode UI, later, separately: context menu,
   tactics-range overlay, Inventory/Ops/Stats

⚠️ Two real, load-bearing DESIGN QUESTIONS are flagged to settle **before**
any of this gets coded (per house rule: ask first on a real design fork,
don't just start typing):
- 🎨 Who owns rendering for a moving NPC?
- 🛠️ Should "move" be a Common-Event scripted command, or a built-in verb?

**Bottom line**: the hard, foundational plumbing (the trigger mechanism
itself) is done and proven. The actual game-feature work everyone's been
waiting to build on top of it — NPCs that move, that you can walk up to
and trigger stuff with — hasn't started. 🏗️
