# house-compaction.md — proposal only, NOTHING edited yet

**Date:** 2026-08-24. Written after live-testing the palettes handoff, then auditing
the real TPMOS/wraith-alpha standard against livedesk/khtpm's current practice, per
direct instruction. This is a proposal for your approval — no docs or code touched.

---

## Part 1 — the receipt drift finding (the thing you asked me to check first)

**Question asked:** does `khtpm_hq_render`'s dump path have frame history, even if it's
missing the receipt?

**Answer: no, on both counts, and it's a real drift, not a documentation gap.**

- Read `khtpm_hq_render.c`'s `dump_frame_png()` (~line 1424, called from `'p'` key and
  `--dump-and-exit`). It does exactly one thing: `XGetImage` the offscreen pixmap, encode
  PNG, write it. No receipt write. No frame-history append. Confirmed live: ran
  `--dump-and-exit` on `palettes-emojis.chtpm`, got a fresh PNG, zero `.receipt.txt`
  anywhere near it.
- Searched the whole khtpm/livedesk-taskbar tree for anything resembling TPMOS's
  `session_frame_history.txt` append-log model — found nothing. The taskbar's OWN strip
  parser has `#.desktop/khtpm_strip_frame_history.txt` (so that half of the house DOES
  have a frame-history log), but the `khtpm_hq_render` family (db-hq, palettes,
  events-hq, chat-hai, entity-menu — all one shared binary per `khtpm-merge-how2.md`)
  has **neither** a frame-history log **nor** a receipt. This is the bigger, shared
  renderer used by the most actively-developed apps in the house, and it has the
  weakest audit trail of the three input/render families.

**Root cause of the confusion in au11-hq's docs:** the au11-hq "receipt" convention
(`TESTING_STRATEGY.md`'s 2026-08-12 addendum, `dump_frame_png()`'s own comment in
`khtpm_ai_cell_render.c`) describes a single flat `key=value` file
(`ok/w/h/timestamp/app-state`). That pattern is real and does exist in `ai-cell`
(`khtpm_ai_cell_render.c`) and `db-hq`'s *design intent* — but it was **never actually
ported into `khtpm_hq_render.c`** when db-hq/palettes/events-hq/chat-hai got merged into
that shared binary. So the au11-hq docs aren't wrong about the intended pattern — the
*code* drifted away from what its own docs already asked for. This predates today.

**What the REAL standard actually is (from TPMOS bible §13.7 + live wraith-alpha
session files), which is richer than au11-hq's flat receipt:**

wraith-alpha's `wraith_rgb_daemon.c` writes, every frame, into `session/rgb/`:
- `current_frame.receipt.pdl` — full semantic dump: viewport/cell/glyph geometry,
  render checksum (`render_checksum_fnv1a64`), object_count, focused_object_id,
  mouse position + hit-test offsets, THEN one `OBJECT | NNNN | ...` line per rendered
  element with full pixel bounds (`px_x0/y0/x1/y1`), clip rect, hit rect, focus rect,
  colors, label, and action — genuinely enough to reconstruct/verify the whole frame
  headlessly, no pixels needed.
- `gl_display.receipt.pdl`, `gl_input.receipt.pdl` — companion receipts for the
  display/input halves specifically.
- `wraith_project_scan.receipt.pdl` — a *different kind* of receipt (project-discovery
  audit, not per-frame render audit) — worth knowing receipts aren't only a rendering
  concept in this house, they're a general "prove what happened without re-deriving it"
  pattern.

This is confirmed **live and current**, not aspirational — I read the actual file,
timestamps are real, `object_count=56` lines are genuinely present.

**Gap, stated plainly:** `khtpm_hq_render.c` has ~0% of this. It has a PNG dump and
nothing else. No object list, no checksum, no focus/mouse state, no frame-history log.
Anyone auditing a palettes/db-hq/events-hq/chat-hai bug today is reduced to reading raw
pixels — exactly the "trust but verify" failure mode `TESTING_STRATEGY.md` already warns
about elsewhere in the house.

**Recommendation (not yet acted on):** before the next round of palettes/T1-T6 work,
port a real `.receipt.pdl`-style dump into `khtpm_hq_render.c`'s `dump_frame_png()` —
doesn't need wraith's full pixel-bounds richness on day one, but at minimum: object
count, focused nav id, per-object label/nav/action, checksum, timestamp. This is a
code task, not a doc task — flagging it here because it changes what "verified" should
mean in `palettes-handoff-2026-08-24.md`'s T1/T3 (both currently claim verification
that, per this finding, wasn't actually receipted).

---

## Part 2 — is TPMOS/wraith-alpha genuinely a stronger scaffold for livedesk?

Yes, on the specific axis you asked about (audit trail / "if it's not in a file it's a
lie"), and I want to be precise about where the analogy holds and where it doesn't,
since livedesk is raw-Xlib, not a `chtpm_parser_pal` polling loop:

**Directly portable (same shape, just missing in livedesk):**
- Per-frame receipt files (Part 1, above) — most impactful because it's not built yet
  and is clearly wanted (au11-hq docs already gesture at it, they just never finished
  the port).
- "One Writer Rule" (TPMOS bible #87, pitfall #17/#36) — livedesk's own
  `TASKBAR-MENU-ARCHITECTURE.md`/relay docs already independently arrived at a
  single-dispatcher model (`khtpm_strip_parser.c`'s capture-only-writer +
  one-read-back-dispatcher, `taskbar-history-txt-migration-investigation.md`) — this one
  is NOT a gap, livedesk already converged on the TPMOS-correct shape on its own.
- Marker-file discipline over stat-polling — livedesk already does this
  (`frame_changed.txt`-equivalent markers per the relay docs) — also not a gap.

**Already diverged for a stated, documented reason (not drift, a real different
runtime):** livedesk/khtpm is raw-Xlib reading real X11 events, not a
`pieces/keyboard/history.txt`-polling `chtpm_parser_pal`. `TESTING_STRATEGY.md`'s own
"SCOPE" sections already say this correctly and tell you not to force the file-injection
testing method onto it. So: don't compact away the distinction between "wraith/TPMOS
pattern" and "raw-Xlib pattern" in the docs — it's load-bearing, confirmed by your own
prior sessions, not accidental duplication.

**Where I'd stop short of recommending a full port:** wraith-alpha's receipt is *very*
heavy (40+ fields per object, ancestor_chain strings, clip_chain strings). Given
`!.HOUSE_STDS.md`'s own "Anti-Overengineering Mandate" (TPMOS bible §10, which this
house also follows per HAIKU_TASKS/budget docs), I'd scope livedesk's receipt to what's
actually useful for the debugging livedesk agents actually do (nav id, label, action,
bounds, checksum) rather than reproducing every wraith field day one. Recommend, don't
mandate — this is a design call for you or whoever picks up the receipt-port task.

---

## Part 3 — compaction candidates (still not touched, awaiting your approval)

Scope: `#.#.✅️.cal-user-sum/1.^V-hq/` only so far (44 files). Have NOT yet read the
un-indexed files listed below line-by-line — flagging by name/size/INDEX-absence only.
Say the word and I'll open them before finalizing a compaction plan, or approve as-is
if you're fine trusting the heuristic.

1. **`khtpm-merge-how2.md` (172K, largest file in the dir).** INDEX.md itself says
   Stage 5 (the literal binary merge) is DONE. Most of the file is almost certainly the
   step-by-step merge trail (§5d.1–§5d.13+) that's now historical. Proposal: keep a
   ~1-2 page "CURRENT REAL STATUS" + architecture summary at the top (already exists per
   INDEX.md's description), archive the blow-by-blow merge log to a
   `khtpm-merge-how2.ARCHIVE.md` or fold into `legacy-shared-fix.md`'s existing
   archive pattern. Est. 80%+ size reduction of the *active* doc.

2. **`taskbar-keyboard-relay-and-terminal-render.md`.** INDEX.md already labels this
   "superseded in practice," kept only as historical/prerequisite reading, cross-linked
   from two newer docs. Proposal: shrink to a 1-paragraph "original finding + see X, Y
   instead" stub, since its content now lives in the two docs that superseded it.

3. **Un-indexed files — NOW READ, resolved into concrete actions:**

   | File | What it is | Proposed action |
   |---|---|---|
   | `15.clock-design.md` | Real, unbuilt design doc for taskbar cell 15 (2026-08-13) | Add routing row to INDEX.md Tier 3 — legit, just missing |
   | `CURSWORD-HQ-SPAWN.md` | DONE, relay-verified task record (2026-08-24) | Add routing row (historical/completed) — legit, just missing |
   | `todo-a12.txt` | Task tracker, "not started" status, references `db-0000.html` | Add routing row — legit, just missing |
   | `AU24-oc-handon.md` | Full 2026-08-24 handoff; most of its content (events proof, symlink-mirror removal, palettes categories) is now superseded same-day by `CURSWORD-HQ-SPAWN.md` + `palettes-handoff-2026-08-24.md` | Shrink to a short "superseded by X, Y — see those for current state" stub, don't delete (has some detail not fully duplicated, e.g. §4.x cursword spec source) |
   | `_.hai-LEARNINGS-a12.md` | ai-cell/tool-probing learnings, 2026-08-12 | Overlaps the ALREADY-INDEXED `&.widgits/open-hai/code-tools-harness/LEARNINGS.md` — needs a direct diff before deciding merge vs. delete; do NOT delete blind since not confirmed identical |
   | `session-au15.md` | Raw session/compaction transcript dump (literal "Assistant (Compaction...)" thinking blocks) — not authored documentation | **Delete** — this is debris, not a doc; if anything in it is load-bearing it should already be reflected in the real chat-hai docs (the khtpm_hq_render-not-bespoke-renderer lesson is already captured in CREATOR_AGENT.md) |
   | `#.house-docs.html` | 0 bytes, empty stub | **Delete** |
   | `x/` | Empty directory, no contents | **Delete** |
   | `db-0000.html`, `hai-desktop-gui.html`, `rpg-maker-database.html`, `rpgmaker-mv-event-editor.html` | Real HTML mockups (332-1158 lines), legitimately referenced by other docs as design/CSS-scope references | **Keep as-is** — these are design assets, not doc bloat; not adding INDEX.md rows since INDEX.md routes docs, not asset files (open to doing so if you want them discoverable) |

4. **`A15.chat-hack.md` — confirmed NOT a duplicate.** Only one file exists on disk;
   `chat-hack.md` (no prefix) does not exist. INDEX.md's Document Roles table link text
   is simply stale — a one-line typo fix (`chat-hack.md` → `A15.chat-hack.md`), not a
   file-consolidation task.

5. **`legacy-shared-fix.md` (84K) — NOT recommending compaction.** Its headline thread
   (12/12 consolidation) is done, but the doc's own status section says the 13-remaining
   GL-migration thread and mutaclysm's deferred camera work are both real, open, and
   actively referenced by other docs (`opencode-mutafix-pie.md`). Compacting this now
   would likely delete state someone will need mid-migration. Leave as-is.

**Not yet done, per your message:** the "even across entire house" sweep beyond
`1.^V-hq/` — I scoped this pass to the directory INDEX.md itself governs, since that's
where today's task started. Say if you want the broader house-wide pass now or after
you approve this batch.

---

## What I'm NOT doing without your go-ahead
- Not editing/archiving/deleting any file listed above yet.
- Not porting the receipt fix into `khtpm_hq_render.c` yet (Part 1's recommendation) —
  that's real code work, flagged for your decision on Sonnet vs. delegate vs. queue.
- Not opening the 11 un-indexed files yet (item 3 above).

Reply with which of Part 3's items to act on (or "all"), and whether you want the
receipt-port (Part 1) queued as a real task now, and I'll proceed.
