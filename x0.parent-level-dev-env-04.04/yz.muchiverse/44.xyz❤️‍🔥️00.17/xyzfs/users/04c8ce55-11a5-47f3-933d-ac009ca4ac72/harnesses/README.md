# Test Harnesses — claude-0001

## Quick Start
```bash
cd <house_root>
HOUSE="$PWD" bash xyzfs/users/04c8ce55-11a5-47f3-933d-ac009ca4ac72/harnesses/test_events_e2e.sh
```

## Available Tests

### `test_events_e2e.sh`
End-to-end event system test — relay-only (no direct CLI/binary calls used as "the test", per house
rule in `au11-hq/TESTING_STRATEGY.md`).

Run all 4 sub-tests, or name specific ones:
```bash
HOUSE="$PWD" bash test_events_e2e.sh                              # all
HOUSE="$PWD" bash test_events_e2e.sh entity_event common_event     # just these two
```

**Sub-tests:**
- `entity_event` — Change Gold on a live entity (`m8_redhorned`), triggered via the real production
  path (`RUN_METHOD:Play` relay injection into `interact_relay.txt`), same as a real right-click.
- `entity_multitrigger` — proves multi-page/multi-trigger dispatch: two pages with different
  triggers on the same entity, confirms each trigger fires ONLY its own matching page, confirms an
  unmatched trigger fails cleanly with no side effects.
- `common_event` — session-level "common event" using the exact same package format as an entity
  event, just rooted at `sessions/<id>/common_events/` instead of inside an entity. Proves the
  runtime needs zero code duplicated between local and session-level events.
- `common_event_gui` — proves `event-ez` (normally launched against an entity) is fully reusable,
  UNMODIFIED, against a session's `common_events/event_pkg` — this IS the "db-ez" GUI, not a
  separate tool. Launches the real widget, injects real keypresses (same k3 method event-ez's own
  HOW2 guide documents), reads the real rendered frame.

**Expected runtime:** ~10 seconds for all 4 tests.
**Prerequisites:** livedesk taskbar running with `m8_redhorned` on the active desk (session `s4`,
desk `desk_01`, house account `jb`, uuid `0a9558a7-7c74-4358-833c-2d5b21edc421`). If entity/session
paths change, update the fixed paths near the top of the script.

**Output:** `results/<timestamp>/` — `summary.md` (PASS/FAIL), `log.txt` (full timestamped log),
plus real frame/inventory snapshots for each test (screenshots-equivalent proof, PM-scannable).

**If a test fails on `entity_event` specifically:** check the entity process is alive first
(`ps aux | grep pals/m8_redhorned`) — a long-running process can stop responding to relay actions
after heavy testing (see `au11-hq/EVENTS_RUNTIME.md`'s "Real Bug #4"), fixed by restarting it, not a
code bug.

## Reference
Full design/bug-fix history for everything this harness tests:
`#.#.✅️.cal-user-sum/au11-hq/EVENTS_RUNTIME.md`
