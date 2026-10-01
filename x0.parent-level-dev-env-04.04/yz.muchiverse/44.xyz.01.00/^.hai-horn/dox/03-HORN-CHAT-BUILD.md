# HORN_CHAT v0.1 — what was built, and why it looks like this

Status: **working, verified by a real OpenRouter round-trip.** See
`dox/03-HORN-CHAT-VERIFICATION.md` for the evidence and the exact state
of the last run.

---

## Run it

```bash
cd x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/^.hai-horn
./horn_chat.sh            # builds if needed, then runs the chat window
```

Inside the window:

| key | effect |
| --- | --- |
| `Enter` | focus the composer (it starts unfocused) |
| type + `Enter` | send the message to the model |
| `Esc` | leave the composer without clearing it (chtpm's own convention) |
| line starting `@` | path completion, not a message |
| `Ctrl+C` | quit |

Other entry points:

```bash
./horn_chat.sh send "What is 6 times 7?"   # one turn, no UI
./horn_chat.sh build                         # build only
./horn_chat.sh clean                         # drop session state
./horn_chat.sh kill                          # clear stragglers
bash scripts/e2e.sh                         # the test suite
```

---

## Architecture

The handoff's plan — copy `chtpm_parser.c`, write a `.pal` main loop — does
not match any real precedent in this house. What HORN_CHAT actually uses is
the same 3-process pal-native stack `014.wsr-pal💸️📌️+2` uses:

```
keyboard_input        raw termios -> keycodes
renderer              draws pieces/display/current_frame.txt
chtpm_parser_pal      parses layouts/horn_chat.chtpm
                      forks system/prisc+x pal/horn_main_loop.pal
```

| file | role |
| --- | --- |
| `layouts/horn_chat.chtpm` | the box, the composer, the `${…}` vars |
| `pal/horn_main_loop.pal` | dispatch loop — on Enter, run a turn. Nothing else |
| `ops/horn_turn.c` | read the prompt, dispatch (chat vs `@`), record, publish |
| `ops/horn_chat_openrouter.c` | the OpenRouter transport. Pure transport: no history, no layout |
| `ops/horn_publish.c` | project history → `view.txt` / `state.txt` + pulse the frame |
| `ops/horn_completions.c` | `@` path completion |
| `system/keyboard_input.c`, `system/renderer.c` | local copies (no canonical version exists to compile in place) |

`prisc+x.c` and `chtpm_parser_pal.c` are **compiled in place** from
`&.widgits/_shared-lib/system/` per the current convention
(`_shared-lib/README.md`, "SHARED-SOURCE-COMPILE-IN-PLACE.md", 2026-09-09).
They are never copied into this project — earlier copies drifted, which is
why the convention changed.

### Why the pal loop is so thin

The composer is a chtpm `<cli_io>`, which chtpm owns completely. It
accumulates keystrokes into its own buffer and renders them itself; the pal
loop never sees a character. On `Enter`, chtpm's own `process_key()` handler
saves the typed text to `gui_state.txt`, clears its buffer, and injects a
single raw key 13 into the interact relay.

So the only signal reaching the pal loop is that one 13. That is why it
reads the relay rather than the keyboard history: reading
`keyboard/history.txt` would see every character and race chtpm's own
buffer. This is the same conclusion the handoff reached about `@`
completion — prompt-level parsing is a harness concern, not a parser one.

---

## Three things that cost real debugging time

Recorded because each one is invisible in the code and would otherwise be
rediscovered.

### 1. `id="input_text"` on a cli_io breaks typing

A cli_io with `id="input_text"` live-syncs every keystroke into that
variable. chtpm's `compose_frame()` re-parses the entire layout whenever
`input_text` changes — so the layout is torn down and rebuilt *between
characters*, and the element's buffer is lost. Observed live: the box held
only the last character typed.

The layout uses `target_id="horn_prompt"` instead. The sync then goes to
that key, `input_text` never changes, and no re-parse happens.

### 2. A fresh pal loop replays every Enter ever pressed

The interact relay is append-only and is not cleared between sessions. A pal
loop starting its cursor at 0 re-reads the whole file and dispatches a turn
per stale 13 — against whatever is in `gui_state` right now. Observed live:
phantom turns in the transcript after a restart, duplicating history.

The loop now seeds its cursor with `read_pos` at startup.

### 3. `pkill -f` silently matched nothing

`pkill -f` takes an **extended regex**, so a pattern containing the binary
name reads the `+` in `prisc+x` as a quantifier (`prisc` + one-or-more `c` +
`x`) and matches nothing. The launcher's cleanup ran four kill layers of
which one was a no-op, and a test's "no leaked processes" assertion checked
the same wrong pattern — so it passed while 30 orphaned renderer/parser
pairs were still running. Both now match on plain substrings
(`horn_main_loop.pal`, `chtpm_parser_pal layouts`, `renderer$`).

The general lesson: a teardown assertion that reuses the teardown's own
pattern cannot detect teardown failure. It now prints the survivors.

---

## Design decisions taken

- **Chat history persists; it is not fed back as model context.** Each
  request carries only the current turn. Persistent context changes model
  behaviour turn over turn and makes the transcript disagree with what the
  model actually saw — which would poison the HORN-vs-HALO comparison the
  IRL harness exists to make. Revisit when IRL grading has a signal to
  attach it to.
- **`@` completion clears the composer.** chtpm's Enter handler clears the
  element buffer before the turn runs, and its gui_state sync deliberately
  skips the currently-active element, so there is no supported way to push
  text back into the box. Restoring the partial path would look like it
  worked while changing nothing on screen. v0.1 is not a completion popup.
- **HALO's validated edits go to a review file, with a flag to promote
  directly.** Matches NIGHT_30 and current house behaviour, and lets the
  IRL harness A/B the two policies later. (Sprint 2; not built yet.)
- **The model shown in the UI is the primary rung, not whichever rung
  answered.** The ladder exists so a rate-limited free slug does not break
  the turn; reporting the fallback would misreport which model is in use.

## Known limits

- **OpenRouter's free tier is 50 requests/day per key.** When it is spent,
  no model answers until the UTC day boundary. The transport reports this
  distinctly (exit 3) rather than as a generic failure, and `scripts/e2e.sh`
  skips the live assertions instead of reporting a false failure.
- **The model ladder rots.** Two of the three slugs inherited from
  entity-cli's version are now paid-only. A slug retired from the free tier
  returns HTTP 200 with an error *body*, not an HTTP error — so it shows up
  as "no content", not a crash. Re-verify with
  `curl -s https://openrouter.ai/api/v1/models` and add live free slugs.
- **The turn blocks the UI for up to 60s** across the ladder. The renderer
  and parser keep running, so the window stays live and the box is ready for
  the next message, but no second turn can be dispatched meanwhile.
- **The transcript view is capped at 400 lines** and older lines scroll off
  with a marker. Chat history on disk is uncapped.

## Not built (per the handoff's sprint plan)

- **Sprint 2, HALO_CHAT** — Concept Bank validation. The transport is
  already isolated from turn/publish precisely so this can interpose
  without touching it.
- **Sprint 3, IRL harness** — design doc only; genuinely undecided.