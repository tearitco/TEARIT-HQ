# pal-chat-irc Feature List (044.pal-chat-irc👥️+2)

Live-updated as work happens this session. Verified against files on disk, not
against older docs' claims. See `quick-summar-j26.txt` for the fuller architecture
comparison this was distilled from, and `arch-re&test.txt` (once written) for the
master-ledger change + test evidence referenced below.

## DONE (confirmed on disk)

### Runtime / process architecture
- [x] `system/orchestrator.c` - 3-layer cascading kill, PAL_LAYOUT env var, fork/exec launches, compiled binary present
- [x] `system/renderer.c` - single-marker + skip-if-unchanged (flicker fix, no double-render)
- [x] `system/keyboard_input.c` - Ctrl+C (ETX) only quit, no 'q' branch
- [x] `system/chtpm_parser_pal.c` - 'q'/'Q' quit branch removed (confirmed absent)
- [x] `pieces/os/kill_all.sh` - present, executable, session-scoped 3-layer kill
- [x] `button.sh` - `--pal` flag, compiles orchestrator, session-isolated `run` action
      (creates `pieces/sessions/<ts>-<pid>/` with symlinks back to shared system/ops/pal/
      chtpm/users/rooms, so concurrent terminals of the same program don't collide) -
      built session-isolated from day one per its own header comment, not retrofitted
- [x] `project.pdl`, `debug.txt` present (added in the j26 pass)
- [x] No cruft found (cleanest project of the family per convert-report-j26.md)

### P2P networking
- [x] `ops/palnet_peer.c` - own_kind=irc_node, seek_kind=irc_node, full mesh, launched
      automatically by `button.sh run`
- [x] Port auto-allocation: `bind_with_retry()` + `base_port_for_kind()` in palnet_peer.c -
      each "kind" gets its own base port, retries base_port+1/+2/... on collision, so
      multiple concurrent sessions never fight over the same port. Confirmed present,
      shared logic across 044/pal-chain/pal-forum.
- [x] `ops/chat_inbox_watcher.c` - persistent daemon, tails `net/inbox.txt`, applies
      remote MSG lines to local `rooms/<room>/messages.txt`, dedups via `already_has_line()`,
      triggers re-render via `chat_compose_frame.+x` shellout (real bug fixed 2026-07-20:
      render trigger must NOT be gated on the dedup check, see file's own header comment)
- [x] Auto-launched alongside renderer/chtpm_parser_pal/palnet_peer by `button.sh run`

### Chat features (user-facing)
- [x] Login screen -> create user (`ops/chat_create_user.c`) / switch user (`ops/chat_switch_user.c`)
- [x] Room list screen, join-or-create-by-name (`pieces/chtpm/layouts/room_list.chtpm`,
      `ops/chat_menu_input.c`'s `write_room_choices()`)
- [x] Room screen: post message (`ops/chat_post_message.c`), last 12 messages shown
      (`ops/chat_compose_frame.c`'s `show_recent_messages()`)
- [x] Multi-room support (rooms created implicitly on first message)
- [x] Session isolation verified with real evidence: 2 real prior `pieces/sessions/<id>/`
      dirs present on disk from actual past runs

### Testing
- [x] Project has its own `TESTING_GUIDE.txt` (session setup + 3-layer test stack +
      key injection format) - adapted from 1.TPMOS methodology, predates and is
      compatible with `#.haiku+/tpmos-re-dox/_.0.aigent-testing-k3.txt`
- [x] Smoke-tested via pty (per convert-report-j26.md): login screen renders correctly,
      "(Ctrl+C to quit)" hint shown, 'q' confirmed harmless

## IN PROGRESS (this session - master ledger architecture change)

- [ ] `data/master_ledger.txt` - new global append-only source of truth (mirrors
      `101.ledger-player-npc-simple+3`'s `data/master_ledger.txt`), shared/symlinked
      into every session like `rooms/` and `users/` already are
- [ ] Dual-write: `ops/chat_post_message.c` and `ops/chat_inbox_watcher.c` append every
      MSG line to `data/master_ledger.txt` in addition to the existing per-room
      `rooms/<room>/messages.txt` write
- [ ] `ops/chat_replay_ledger.c` - new standalone recovery/audit op, replays
      `data/master_ledger.txt` in full and rebuilds every room's `messages.txt` from
      scratch (real replay capability, not just a second copy)
- [ ] `button.sh` wiring: mkdir/symlink `data/`, compile the new op
- [ ] One-time backfill: merge existing `rooms/*/messages.txt` content (test-1, t1,
      muchi-test, guest rooms already have real history) into `data/master_ledger.txt`
      so it isn't orphaned from the new source of truth
- [ ] Compile clean
- [ ] Live multi-session/multi-port test: two sessions, confirm distinct ports,
      confirm message propagation A->B, confirm ledger has no duplicate lines, confirm
      `chat_replay_ledger.+x --rebuild` reproduces identical room content
- [ ] Write design + test evidence to `arch-re&test.txt`

## NOT DONE / KNOWN GAPS (not in scope for this pass unless stated otherwise)

- [ ] `system/chtpm_parser.c` (true C-mode parser) does not exist - only the `_pal`
      variant. By design per `!.!..j25-conversion-task.md` ("C mode is future - both
      modes currently run the same PAL loop via orchestrator"). Not a bug.
- [ ] Rooms list UI is a flat list of buttons (`${room_choices}` in `room_list.chtpm`,
      written by `write_room_choices()` in `ops/chat_menu_input.c` ~line 210), not a
      collapsible "+/- dropdown" as directly requested. CHTPM has no existing
      dropdown/select tag anywhere in this tree (grepped, zero hits) - would need a new
      widget or a toggle-visibility affordance built from the existing
      `visibility="${...}"` pattern. Deferred - user paused this to prioritize the
      master-ledger change instead.
- [ ] `pc-irc-trace-j26.md` (IO/functionality-flow trace doc, mapped to code) - was
      the originally planned next step, explicitly SKIPPED per direct instruction in
      favor of the master-ledger architecture change instead. Not written, not needed
      right now.
- [ ] No signature/authenticity check on P2P-received messages (v1 gap, documented in
      `chat_inbox_watcher.c`'s own header comment, matches pal-chain's/pal-forum's same
      gap). Not being addressed this pass.
- [ ] No root-dir `FRAME_REPORT_<timestamp>_<topic>.txt` produced yet for 044 per the
      `_.0.aigent-testing-k3.txt` "Mandatory Post-Test Report Gate" convention - the
      project's own `TESTING_GUIDE.txt` covers similar ground but the formal root-report
      file hasn't been generated. Will be produced as part of the live test step above.
- [ ] `041.pal-chain` hang (from `convert-report-j26.md`, still unresolved) and
      `041.pal-forum` (not started at all) - both explicitly deferred until after 044's
      ledger work + rooms-dropdown fix are done, per the user's own stated sequencing
      in this conversation.
