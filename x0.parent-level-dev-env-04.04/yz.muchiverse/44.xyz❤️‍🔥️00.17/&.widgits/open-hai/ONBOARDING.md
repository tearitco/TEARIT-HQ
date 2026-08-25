# h-ai onboarding — pick up here

Renamed 2026-08-12: "ai" → "h-ai" everywhere going forward (header
cell label, menu row, window title text). The directory/binary/file
names are still `open-hai`/`khtpm_open_hai_render.c` — not renamed yet,
low-risk to do later, not done this session for time reasons.

Read `README.md` (this same dir) first — it covers build/run, file
layout, the input/relay/nav conventions, and (important) why you must
use the app's own PNG-dump+receipt mechanism instead of `xwd`/screen
capture for verification. This doc describes the current working state
and what comes next.

## Next priority: Harnecient Integration (2026-08-13+)

**Primary work**: Wire the Harnecient protocol into h-ai (see `au11-hq/HARNECIENT-H-AI-RELAY.md`
for the full design and `13.AUG.13-HAI-2do.txt` (`#.#.✅️.cal-user-sum/`) for the phase breakdown).

The 4-phase plan:
1. **Choosable model** — ✅ DONE 2026-08-13, relay-verified end-to-end (see below)
2. **Harnecient backend mode** — deterministic tool dispatch, persona-based prompting (4-6 hours) — NEXT
3. **Relay demo** — prove read/write/run works against taskbar state (1-2 hours)
4. **Lasting harness** — automated N/N proof of concept (3-4 hours)

End goal: h-ai autonomously writes and runs harnesses for itself, with Claude reviewing.

### Phase 1 (model selector) — DONE, real, relay-verified 2026-08-13

Sidebar "MODEL" nav item cycles through a 5-model whitelist
(`stable-code:latest`, `gemma3:1b`, `gemma3:270m`,
`llama3-groq-tool-use:8b`, `llama2:latest`) on Enter, persists the
choice to `sessions/model.txt`, reloads it correctly on restart.
Verified via a CLEAN relay test (single confirmed process, nav index
computed live from a `'p'`-key receipt immediately before jumping —
see README.md's nav-index warnings for why this matters): 3 sequential
cycles produced the correct model each time, and the choice survived a
full process restart.

**Getting this working correctly took ~2 hours of debugging, almost
none of it the feature code itself** — three stacked testing-
methodology bugs (confusing `--dump-and-exit`'s throwaway process for
the live one; a test loop that kept invalidating its own nav target;
and, the real root cause, FIVE concurrent processes racing on the same
relay file because `pkill` wasn't reliably matching this binary and
`button.sh` had no kill-before-launch guard) produced ~2 hours of
"flaky, unreproducible" results before any of it was root-caused.
**Read `_.0.aigent-testing-k9.txt` "SCOPE ADDENDUM 2026-08-13" (house
root) before touching this file's nav/relay testing again** — it has
the full incident writeup and the now-fixed procedure.

**Real fixes shipped as part of closing this out** (not just the
feature): `button.sh` now kills any existing `khtpm_open_hai_render`
instance (TERM→KILL escalation) before launching and confirms exactly
one PID survives; the binary itself now writes a pidfile
(`pieces/audit/open-hai.pid`) and handles `SIGTERM`/`SIGINT` with a
real clean shutdown instead of needing `kill -9`. The same
concurrent-instance class of bug was found to apply to `open_db_hq.sh`
and `events-hq/button.sh` too and got the same treatment (events-hq's
guard is scoped per-entity, since multiple simultaneous entity editors
are a legitimate use case there, unlike open-hai/db-hq which are
single-instance-per-house).

**A real, NOT-yet-fixed bug found along the way**: nav items beyond
index 9 are unreachable via relay digit-jump once several sessions
exist (`handle_key()`'s single-digit branch always wins before the
multi-digit accumulator can fire). Flagged in README.md and the code
itself; not blocking Phase 1's own success criteria (model item lands
comfortably within index 9 for normal session counts), but will bite a
future relay test on a long-lived instance with many saved chats.

## Future things worth doing (lower priority than Harnecient)

- Rename `open-hai`/`khtpm_open_hai_render.c` → `hai`/`khtpm_hai_render.c`
  (or similar) for real, everywhere (dir, binary, `#.desktop` relay
  filename, session storage path, build script, `button.sh`,
  `OPEN-HAI-GUI-DESIGN.md`'s own title) — not done this session, the
  taskbar-open bug was higher priority.
- Model switcher UI (currently hardcoded `stable-code:latest` in
  `g_model_name`).
- Real khtpm CSS engine styling (currently hand-rolled pixel layout).
- agent-45 as an alternate backend (`BACKEND_AGENT45_LEGACY` enum
  value exists, nothing implements it).
- Live tool-call feed pane (see design doc §7's resolved sidebar
  taxonomy answer for what this should look like).

## What's real and working right now (verified, not assumed)

- Managed X11 window, real focus, RGB compose→present.
- Real nav (`[N]` bracket badges, digit-jump, Enter-activates) across
  sidebar, scroll buttons, composer, and now a real close button
  (last nav index — **Escape no longer closes the window**, only
  disarms the composer; use the close button/nav item, matching every
  other khtpm window's own convention).
- Real relay injection (`#.desktop/open_hai_agent_relay.txt`).
- Real raw-Ollama backend, real disk-persisted chat history
  (deletable), real transcript scrolling.
- Real PNG-dump + receipt verification (`'p'` key or
  `--dump-and-exit`) — use this, not `xwd`/screenshots, once a real
  human might be using the same desktop concurrently.

Full history/rationale for all of the above: `OPEN-HAI-GUI-DESIGN.md`
(au11-hq, house root) §1-§11.
