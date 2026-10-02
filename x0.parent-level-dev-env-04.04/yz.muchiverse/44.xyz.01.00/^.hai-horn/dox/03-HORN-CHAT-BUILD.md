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
| `ops/horn_chat_backend.c` | the LLM transport, provider-agnostic. Pure transport: no history, no layout |
| `ops/horn_publish.c` | project history → `view.txt` / `state.txt` + pulse the frame |
| `ops/horn_completions.c` | `@` path completion |
| `ops/horn_tool_exec.c` | the tool allowlist gate + `list_dir`/`read_file`/`grep_files` |
| `tools/horn_tools.json` | tool definitions + the name→op map. Adding a tool = edit this + drop in an op |
| `system/keyboard_input.c`, `system/renderer.c` | local copies (no canonical version exists to compile in place) |

### Tools

HORN can call tools, and it is the reason this matters: HORN is the model
that will help build HALO, and an IRL harness comparing two models needs the
tool to fire *consistently* or the comparison is measuring noise.

**`tools/horn_tools.json` is the entire security boundary.** A tool the
model asks for that is not in the `ops` map is refused, never dispatched —
a hallucinated tool name must not become a process launch. Adding a tool
needs no C: drop a binary in `ops/+x/`, name it in the map. That is the
seam HALO's Concept Bank ops will go through.

Three read-only tools ship now: `list_dir`, `read_file`, `grep_files`.
Nothing built in writes, deletes or executes; a mutating tool has to be a
deliberate edit.

The loop:

```
horn_turn seeds convo.json (system + user)
  -> horn_chat_backend  --(exit 10, tool_calls.json)-->  horn_turn
  -> horn_tool_exec runs each call, appends {"role":"tool",...}
  -> horn_chat_backend again ... -> exit 0, final text
```

The transport never executes anything and `horn_tool_exec` is the only
place a tool name is resolved, so there is exactly one file to audit.

**`!` forces a tool call.** Measured: with `tool_choice:"auto"` a model
fires 8–15 times out of 10 depending on provider; forced is 10/10. Forcing
on *every* request cannot terminate, though — `required` also applies after
a tool result — so `!` forces only the first call of the turn and the rest
of the loop goes back to `auto`.

### Answering an approval prompt

Per the house J2 testing guide
(`#.#.calendar-dox/1.^V-hq/_.0.aigent-testing-k9.txt`):

1. `Esc` — digits only work in **nav mode**. While the composer is *active*
   a digit is just a character, so you are typing `3` into your message.
   `Tab` does not move focus; arrow keys and nav numbers do.
2. The nav **number** — `2` for APPROVE, `3` for DENY.
3. `Enter` — activates the focused button, whose `onClick="KEY:1"` /
   `KEY:0` injects the answer into the relay for the pal loop.

Two rules from that guide that matter when scripting this:

- **Nav indices are not fixed.** Read the number off the live frame
  (`[ ] 3. [  [0] DENY this tool call]`) immediately before pressing it.
- **Never run two instances.** Concurrent parsers each poll the same
  relay with their own cursor and race each other for the same key; that
  alone accounts for most "flaky, unreproducible" results. Kill all, confirm
  zero, launch one, confirm one.

### Providers

`ops/horn_chat_backend.c` is one file with a provider table. OpenRouter and
Poolside both expose an OpenAI-compatible `POST /chat/completions` with a
Bearer token and a `choices[0].message.content` reply, so they share one
request path, one reply parser and one payload writer; what differs —
endpoint, key file, model ladder, whether thinking defaults on — is table
data, not a second 300-line copy.

**Provider order is fallback order.** Poolside is first, because its key is
independent of OpenRouter's 50/day free tier. Within a provider the model
ladder is walked until one answers; a quota failure breaks out of that
provider immediately, since the limit is account-wide and every model in it
shares the bucket.

| provider | endpoint | key file | models |
| --- | --- | --- | --- |
| poolside | `inference.poolside.ai/v1/chat/completions` | `raw_poolside.txt` | `laguna-s-2.1`, `laguna-xs-2.1` |
| openrouter | `openrouter.ai/api/v1/chat/completions` | `openrouter_api_key.txt` | 3 free slugs |

A provider with no key is skipped with a note on stderr, not silently —
that silence is what hid a real misconfiguration for a while.

Poolside-hosted inference enables thinking by default, which prepends a
reasoning block to the reply; its requests carry
`chat_template_kwargs.enable_thinking=false`.

**Exit codes** (callers depend on the distinction):
`0` replied · `1` bad usage or no key at all · `2` reachable but silent ·
`3` every provider rate/quota limited — an account state, not a code fault.

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

### 4. `grep_files` let the model find its own question

Searching the project root hit `chats/HORN_SESSIONS/transcript.txt`, which
contains the model's previous output — including the question it had just
been asked. So "find where MAX_TOOL_ROUNDS is defined" matched the question
text before the real source, and the model confidently reported a line
number that was really its own prompt echoed back. `chats/` and `pieces/`
are now refused by name, with the reason given.

The same pass fixed `grep_files` treating a file path as a directory,
which reported "no matches in 0 files" for a symbol that was definitely in
the file it was pointed at.

### 5. The tool loop could not see its own results

The transport re-seeded `convo.json` on every call, so within one user turn
each round wiped the tool results. The model saw `system + user + a fresh
tool call` every time, had no memory, and re-issued the same call until the
round cap — a hang that burned six live requests per turn. Conversation
seeding moved to `horn_turn`, which owns that policy; the transport only
seeds when running standalone.

Two more from the same build, both caught only by actually running it:

- `post_chat` returned exit 10 without setting `*tools_out`, so `main` read
  it as "no reply and no tools", printed "groq unavailable" and walked the
  entire ladder — the tool call was captured correctly and then discarded
  by the code meant to act on it.
- `arguments` is a JSON string *containing* escaped JSON. A reader that
  stopped at the first `"` returned `{\`, so every argument silently
  vanished and `list_dir` answered with the project root every time.

And a warning worth keeping: the first version of the suite reported
"no leaked processes" while 30 orphaned renderer/parser pairs were running,
because the check reused the teardown's own broken pattern. A teardown
assertion that shares the teardown's pattern cannot detect teardown failure.

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

- **Key files are per-tree, and this repo has a worktree.** The key lives at
  `<tree>/&.widgits/open-hai/state/<name>_api_key.txt`, so a git worktree has
  its own copy of that directory. A key pasted into the main checkout is
  invisible to a worktree, and vice versa. `HORN_POOLSIDE_KEY` /
  `HORN_API_KEY` override the file if you want one key for both.
- **The model ladder rots.** Two of the three slugs inherited from
  entity-cli's version went paid-only in a week. A slug retired from a free
  tier returns HTTP 200 with an error *body*, not an HTTP error — so it
  shows up as "no content", not a crash. Refresh with
  `curl -s https://openrouter.ai/api/v1/models` and
  `curl -s https://inference.poolside.ai/v1/models -H "Authorization: Bearer $KEY"`.
- **Two provider keys were committed in plaintext** and pushed to origin
  before 2026-10-01. Untracked now, but untracking does not unpublish —
  they still need rotating.
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