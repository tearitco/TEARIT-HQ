# 🤖 sep11-kilo-prompt.md — hand this whole file to Kilo, nothing else first

🚨 **Kilo: you have a SMALL context window. If you overflow it you STOP
WORKING entirely.** This file is written so you never need to read a
whole directory to get started. Read ONLY the exact files named below,
in order, and NOTHING else unless a step tells you to.

## 🛑 DO NOT read these (they will blow your context, guaranteed)

- ❌ `!.HQ-IQ-COMPACT🧭v0.1.md` in full — it's a whole-house onboarding
  doc for a human/high-context agent, not for you. Skip it entirely.
- ❌ Any `08-roadmap/OPEN-ITEMS.md` or `00-INDEX.md` — these are
  cross-reference indexes, not task instructions. Skip.
- ❌ `khtpm_core_render.c` (~16,000 lines) — you are NOT touching the
  shared renderer today. If a task ever asks you to, STOP and ask a
  human first.
- ❌ Any directory listing more than ~15 files — don't `ls` a big dir
  "just to look around." Every file you need is named explicitly below.
- ❌ The `1-1.HARNECIENT.SMOL/` lesson scripts (NIGHT_*/DAY_*.txt) —
  fun, not relevant to your job today, real context cost for zero task
  value.

## ✅ Your ONE real job today: the pc-hq event-trigger bridge

Read these 3 files, in this order, nothing else, before writing any code:

1. 🎯 `08-roadmap/design-docs/HARNECIENT-NIGHT-TRACK-HORIZON-ITEMS.md`
   — read ONLY item 1 ("pc-hq trigger layer"). That paragraph is your
   real task description. Stop reading after item 1 — don't read items
   2-6, they're not your job.
2. 📖 `02-architecture/CENTROID_GOLD_STD.md` — items **8, 9, and 10
   only** (repaint discipline / element identity survives reparse /
   never block window-open). Skip everything else in that file — it's
   long, most of it isn't relevant to this task.
3. 🧪 The testing section of `!.HQ-IQ-COMPACT🧭v0.1.md` — search that
   file for the word "relay" and read just that one section (the
   `entity_menu_history/<pid>.txt` testing convention). Don't read the
   rest of the file.

## 🧵 What "the smallest provable first version" means (do exactly this, nothing bigger)

1. Find where pc-hq's own game engine already knows, per tick, what
   tile the player's on / what it's touching (it renders this today,
   so the data exists somewhere real — go find it, don't guess).
2. Add ONE new append-only line write: when the player touches ONE
   specific NPC, append `touched_npc:<NAME>` to a new file,
   `pieces/display/board_events.txt` (same append-only, one-writer
   convention every marker file in this house already uses — read
   `prefer-marker-files-not-mtime` if that phrase means nothing to you
   yet, it's a real, short house rule).
3. Write ONE small, separate script/process (NOT inside the renderer,
   NOT inside the game engine — a genuinely separate file, same rule
   as everything else in this house: no linking, no editing a shared
   file to bolt on your feature) that tails `board_events.txt` and,
   for that ONE trigger string, fires ONE existing Common Event
   through events-hq's own real dispatch (already built, don't
   reinvent it — go find how events-hq fires an event today and reuse
   that exact call).
4. STOP THERE. One NPC, one trigger, one Common Event. Do not add a
   second NPC or a second trigger type until a human has confirmed
   step 1-3 actually works, live, with real evidence (not "should
   work" — see the testing section you read in step 3).

## 🧭 House standards, condensed to what actually matters for THIS task

- 🏠 Real, separate manager/watcher process — never edit the shared
  renderer (`khtpm_core_render.c`) or the game engine's own core loop
  to bolt this in.
- 📁 Append-only marker file, one writer, never mtime-based change
  detection.
- 🔍 Prove it with the relay (`entity_menu_history/<pid>.txt`), not
  "I think it works" — see step 3 above.
- 🌳 Commit ONLY to the `claude` branch. Never touch `main`/`opencode`/
  `grok` directly.
- 🚫 If you're about to `git reset --hard`, `pkill -f`, or touch a file
  outside `@.apps/piececraft-hq/` or `&.widgits/events-hq/` or
  `&.widgits/board-viewer/` — STOP and ask a human. That's outside
  today's scope.

## 🔮 One more thing, for later, not today

There's a real, separate plan for using a small local model (Gemma) to
help author attention weights and, eventually, an Inverse-RL process
that learns reward signals from this house's own agent/history logs —
see `08-roadmap/design-docs/HARNECIENT-NIGHT-TRACK-HORIZON-ITEMS.md`
items 4-6, and (once it exists) `1-1.HARNECIENT.SMOL/
NIGHT_08_GOAP_RL_AND_THE_DECISION_MODE.txt` /
`NIGHT_09_THE_FINAL_BOSS_ATTENTION_WEIGHTS_FROM_GEMMA.txt` /
`NIGHT_11_INVERSE_RL_AND_FAMOUS_LLM.txt` for the full reasoning.
**Do not start any of that today** — it's a real, separate, much
bigger effort. This note exists so you know it's coming and don't
reinvent a smaller version of it by accident while doing the
event-trigger task above.

## ✅ When you're done

Report back: what file you found the per-tick player-state data in,
the exact path of the new `board_events.txt`, the exact path of your
new watcher script, and the real relay-driven proof (what you sent,
what you saw) that one NPC touch fired one real Common Event. If you
can't get that far, report exactly where you got stuck — don't guess
past a blocker.
