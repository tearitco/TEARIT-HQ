# Event-retrofit pretest/compare baseline

Real "before" artifacts for `EVENT-MODULARITY-AND-BUILD-SPEED.md` §1's
🎬️/⚙️/🧩 retrofit, captured 2026-09-28 from `robot_chat_001` (a real
entity with a genuine `event_pkg/pages/page_1`) before any retrofit code
exists. Re-run `capture_baseline.sh <suffix>` after each retrofit step
and diff against these `_before` files — a diff means something changed,
which is either an intended new capability or a regression, never a
guess either way.

## Files

- `robot_chat_001_filelist_before.txt` — full file listing of the
  entity's own directory. A retrofit step that's supposed to be
  isolated (e.g. running `event_page_to_pal.+x` against a COPY, not the
  live entity) should produce zero diff here.
- `robot_chat_001_cmd1_output_before.txt` — functional trace: running
  `cmd_1.sh` directly. Baseline: exit 0, stdout
  `robot_chat_001: Hello. I am a small robot.`
- `robot_chat_001_ui_before.txt` — the live events-hq manager's
  published `ui.txt` (launched via `&.widgits/events-hq/button.sh`).
  Baseline: `n_pages=1`, one `ai_chat` command row.
- `robot_chat_001_pages_state_before.txt` — the manager's
  `pages.state.txt` roster. Baseline: `page_1`.

## What's already resolved going in (see EVENT-MODULARITY-AND-BUILD-SPEED.md)

- Copy-vs-link: copy now, template/delta-bank link later.
- Page numbering: per-🎬️ (each clacker starts its own `page_1`).
- Creation trigger: automatic, both directions (new page auto-creates
  ⚙️/🎬️; dropping a 🎬️/⚙️ onto a target auto-updates that target's live
  view).
- `cmd_N.sh` portability confirmed clean even for a REAL
  compiler-generated event (`common_events/greet_player`) - zero
  `pkg=`/`page=` reference, resolves via
  `ENT="${MUCHI_TARGET_ENT:-$PWD}"`. The existing `MUCHI_TARGET_ENT`
  override (2026-09-21) already covers "this pal lives inside another
  entity's inventory" - reuse it for dropped-in 🎬️/⚙️, don't invent a
  second mechanism.

## Compare protocol

1. Build the smallest retrofit slice (`event_page_to_pal.+x` standalone,
   not wired to anything live yet).
2. Run it against a **copy** of `robot_chat_001`'s page, not the live
   entity.
3. Re-run `capture_baseline.sh after1` against the STILL-LIVE, untouched
   `robot_chat_001` and diff against `_before` - must be byte-identical
   (proves the export tool is genuinely isolated, no accidental live
   mutation).
4. Only after that passes: wire the automatic-creation hook, re-run
   `capture_baseline.sh after2` - a diff here is now expected (a new 🎬️
   object should appear), but `ui.txt`'s existing fields for the
   original page must be unchanged, and `pages.state.txt` should gain
   an entry rather than losing or reordering the existing one.
5. Retroactive sweep last, and only ever tested against a disposable
   copy of the house tree first.
