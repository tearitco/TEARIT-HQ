# 21 — Drive, document, fix

The standing loop for any agent that opens a board, looks at a frame,
writes what is wrong, and fixes only what the owner has named. This is
not a handoff. A new agent starts here.

- `HOW2-DRIVE-DOC-FIX.md` — the procedure, and the long-range stop
  for every run: RPG Maker MV editing (place, event page, expected
  result, effect), then play that event, then the same edit and play
  on mcl-hq, cdda-hq, df-hq, and any other book. Drive until a real
  failure blocks the next step.
- `OPEN.md` — bugs already seen, with the file that draws them. Research
  starts here. Do not launch a board to re-read a row that already has
  a frame.
- `PLAY-SENSE.md` — first-frame feel: xelector cell, camera, In,
  debug text, empty pane. Add a row when a new kind of thing shows up.
  Write it before it is named. Fix it only after it is named.
- `CHROME-COMPARE.md` — LyfeDesk `strip_btn_N` beside the edit-board
  tab, one row each, including the clock and the pid cell. Fill it
  before saying the toolbar matches. A sentence that says "follow
  LyfeDesk" is not that check.
- `DONE.md` — rows already driven, documented, and fixed. Read this
  before a launch. Do not re-drive a row that has a proof.
- `PROOF-2026-10-29.md` — the hashes and sessions for those rows.
- `TESTING-BOARD-BEHAVIOUR-DETERMINISTICALLY.md` — when a live board will not hold still: make it a verb, test the real binary in a scratch house with fixed cells, add negatives and a mutation run; the traps (one key per line, read neighbouring code first, session binary copies).
- `HARNESS-REUSE.md` — what harnesses exist, the gaps the drive loop hits, rules for new checks.
- The closed-row check is a pal harness: `cd '&.widgits/_shared-lib/harness'; prisc editor_skip.pal` (source, always valid) and `editor_skip_live.pal` (state files, only while a board is open). It does not open a board.
- `ONBOARD-CODEX.md` — Codex-specific re-onboarding from the 2026-10-30
  editor drive: intended screen roles, party/footer distinctions, verified
  new-page edit/play, projector rebuild, and branch handoff.

Grok's personal reonboard stays `13.agent-coms/GROK/ONBOARD-GROK.md`.
The old path `13.agent-coms/HOW2-DRIVE-DOC-FIX.md` is a pointer.
