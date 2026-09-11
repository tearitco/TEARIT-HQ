# cli_io / render-pipeline pitfalls — 2026-09-11 incident dossier

**Why this doc exists, as its own file, direct instruction**: "i think
u should also document, in pitfalls dir, a whole document dedicated to
all the bugs we fought thru today so we never have to do it again
blindly." This was a long, multi-round session (network-browser's
address bar "fixed" and reported unchanged 3 times in a row before the
real root cause was found) with a real, repeating pattern: relay-driven
tests kept passing while real hardware kept failing, and several
narrower fixes each solved a real symptom while leaving the actual
foundation problem in place. Read this whole doc, not just the
symptom that matches what you're looking at — several of these bugs
share one root cause and the earlier, narrower fixes for them are now
superseded.

**The actual root-cause fix, if you're chasing a cli_io/text_area
focus, content, or state-loss bug**: read `08-roadmap/design-docs/
CHTPM-INCREMENTAL-REPARSE-DESIGN.md` and `CENTROID_GOLD_STD.md` item 9
FIRST. `khtpm_core_render.c` used to destroy and rebuild its entire
`Elem` tree on every reparse; it now diffs and patches in place
(`khtpm_reparse_diff.c`). Most of the bugs below either ARE that
foundation problem, or were mistakes made while working around it
before the real fix existed.

---

## 1. cli_io losing keyboard focus/grab on reparse — the root cause

**Symptom**: a `<cli_io>` field (open-hai's composer, network-browser's
address bar) would stop taking keystrokes, or drop/garble keys, while
its window's manager was actively reparsing (streaming an AI response,
or any manager that rewrites its projection frequently).

**Real cause**: `reparse_chtpm_if_changed()` unconditionally did
`g_default_input_elem = NULL; kh_ungrab_kbd();` on EVERY reparse,
regardless of whether the armed field's content actually changed.
`<text_area>` got a narrower fix for this on 2026-09-08
(`kh_text_areas_reload()` restores content from a save file after
reparse); `<cli_io>` never did, so any window whose manager reparses
often (network-browser's ~300ms tick; any chat app while a response
streams) hit this constantly.

**Real fix (now superseded by §0 below, kept for history)**: a
narrower "capture the armed field's key before reparse, find it again
by key afterward, don't release the grab if it's still there" patch
was built, reverted, rebuilt, and reverted again TWICE before the
actual foundation fix (the reparse-diff engine) landed. If you find
yourself about to write ANOTHER version of "find this element again by
its key after reparse" for some element type this doesn't already
cover — stop. That pattern is retired. See §0.

## 0. THE root cause under all of the above: destroy-and-rebuild reparse

**Symptom class**: any state living on an `Elem*` — armed cli_io,
selection, scroll position, anything not written to disk — could be
silently lost or corrupted the moment ANY reparse happened, for ANY
reason, even one totally unrelated to that state.

**Real cause**: `reparse_chtpm_if_changed()` did
`g_n_elems = 0; parse_chtpm(...)` — a full, from-scratch rebuild of
the ENTIRE window's `Elem` tree on every real content change, no
matter how small. Every `Elem*` any code held across that call went
dangling. The house's fix pattern up to this point was reactive and
per-consumer: `kh_text_areas_reload()`, then `kh_cli_io_reload()`/
`kh_find_input_by_key()` — "capture a key before the rebuild, go find
the new element by that key afterward, copy state across." Real,
working, but a genuine bolt-on needed for every new stateful element
type, and — per §2 below — not even fully sufficient on its own.

**Real, final fix**: `khtpm_reparse_diff.c` (new, `&.widgits/
_shared-lib/`) — a real, keyed tree diff/patch. Matched elements
(same `target_id`/`id`) keep their OWN `Elem*` forever; only their
template fields (label, onclick, classes...) update, runtime state
(`input_buffer`/`cursor`/`text_area_buffer`/grid state) is explicitly
preserved. No more "go find it again" step, for any element type,
ever. Full design + rollout evidence: `CHTPM-INCREMENTAL-REPARSE-
DESIGN.md`. **If you're about to add a new `kh_*_reload()` function:
don't. Add one line to `kh_diff_apply_template()`'s preserve-list
instead.**

## 2. `content=` vs `label=` — a real, already-hit double-echo bug

**Symptom**: a `<cli_io>` field either (a) looked fully populated but
Backspace did nothing to it, or (b) after a fix attempt, visibly
doubled its own text on screen (e.g. "www.4chan.org/b/www.4chan.org/b/").

**Real cause**: `label=` only ever sets `e->label` (a DISPLAY value —
for cli_io, drawn as a literal prefix before the editable
`input_buffer`, same mechanism as chat-hai's `"&gt; "` prompt).
Nothing seeds `input_buffer` (what typing/Backspace actually edit)
from `label=`. A manager that projects a field's real content via
`label=` (network-browser's address bar showing the loaded URL) looks
populated but has an EMPTY editable buffer underneath — Backspace has
nothing to delete. The first fix attempt seeded `input_buffer` FROM
`label=` directly — this reintroduced the doubling, because label is
STILL also drawn as the prefix, so the same text showed twice.

**Real fix**: `content=` is the real seed attribute (mirrors
`text_area`'s own pre-existing `content="${...}"` pattern) — it seeds
`input_buffer` directly and is never also drawn. Use `content=` for a
field's real editable value; `label=` stays a short prefix or empty.
This is now a permanent rule: `CENTROID_GOLD_STD.md` item 9.

## 3. Manager echoing live-typed state back fights the user's own edits

**Symptom**: an address bar (or any cli_io whose manager also
publishes ITS content) would snap back / fight backspacing while the
user typed, especially on a window whose manager reparses frequently.

**Real cause**: `network_browser_manager.c`'s `write_ui_projection()`
derived `addr_label` from `g_current_url` (the loaded page) — nothing
to do with what the user is actively typing. Any OTHER unrelated field
changing (status, tab count, a spinner) in the same ~300ms tick
re-triggered a reparse, which re-seeded the field from the stale full
URL.

**Real fix (now moot given §0/§2 above, kept for history — do not
resurrect this pattern)**: an early fix made the manager echo back the
live-typed value from `cli_io_state.txt`. This was real and worked,
but became unnecessary (and actively conflicted with §2's fix) once
`content=` + the reparse-diff engine made a manager-side echo hack
redundant. **The final, correct state**: `addr_label` is purely
`g_current_url`, exactly like it was before any of this investigation
started — the diff engine is what actually protects the field now,
not a manager-side echo.

## 4. `parse_element()` crash on `elem_new()` pool exhaustion

**Symptom**: the whole renderer process segfaults loading a large real
page (found on 4chan's `/b/`, ~250+ real content rows).

**Real cause**: `g_pool[MAX_ELEMS]` (1024) is a hard cap; each
network-browser content row allocates 4 `Elem`s (one per candidate
kind — title/text/link/media — even though only one `show=`s per row).
A big enough page exhausts the pool; `elem_new()` returns `NULL` (its
own long-documented contract: "guarded call sites just skip adding
content") but `parse_element()`, the ONE call site every element in
the house ultimately funnels through, never actually checked — a bare
`e->parent = parent` on a `NULL e`.

**Real fix**: `if (!e) return p + strlen(p);` — stop parsing at the
exact point of exhaustion (same "ran out of string" contract this
function's own closing-tag branch already used), truncating the tree
silently instead of crashing. Root-cause, not network-browser-specific
— any window's page was one large-enough load away from this same
crash.

## 5. No select-all — "can't clear the search bar, have to open a new tab"

**Symptom**: no way to quickly clear a cli_io/text_area's content in
one motion; had to Backspace every character individually.

**Real cause**: `activate_focused()` arms a field with the cursor at
the END and the selection COLLAPSED — never select-all-on-focus like a
real browser's address bar — and `default_cli_io_handle_key()` had no
key bound to select the whole buffer at all.

**Real fix**: Ctrl+A (ASCII 1) selects `[0, strlen(buf))`, same pattern
as the pre-existing Ctrl+C/X/V handling. Generic, every cli_io/
text_area in the house.

## 6. Relay-driven testing can mask real-hardware-only bugs

**Real, confirmed pattern, not hypothetical**: a scripted relay test
(`entity_menu_history/<pid>.txt`, `MOUSE_EVENT`/`KEY_PRESSED` lines —
this house's own preferred testing method, ranked above `xdotool`)
passed CLEANLY for the cli_io reparse-focus bug 3 separate times this
session. The user's real hardware showed the exact same failure
unchanged, every time, until the actual root cause (§0) was found.
This is the SAME shape as the already-documented `override_redirect`
masking (`09-appendix/pc-hq-bugs.md` Bug 2 — `xdotool` synthetic input
showed success while real Mutter-routed focus failed). See memory
`relay-testing-may-mask-real-focus-bugs` for the standing caution: a
passing relay test is necessary but NOT sufficient proof for a focus/
grab fix. Get real-hardware confirmation before declaring one fixed,
every time — this is now a hard rule for this bug class specifically,
not just general good practice.

## 7. Driving a window with the relay WHILE a human tests it live = corrupted results

**Symptom**: real-time "doubling" of typed input, garbled address-bar
content, during a live test.

**Real cause**: the agent was sending `MOUSE_EVENT`/`KEY_PRESSED` lines
to the SAME window's relay file the user was simultaneously typing
into by hand. Both input streams landed in the same field and
interleaved/merged — nothing wrong with the code, a pure test-
methodology collision.

**Standing rule**: once the user says they're testing something live,
STOP sending relay input to that exact process until they report back.
Don't dual-drive the same window from two input sources at once.

## 8. Stale/reused `entity_menu_history/<pid>.txt` content

**Symptom**: a fresh process launch showed keystrokes/clicks in its
relay log that nobody (agent or user) sent in the current test window
— traced to real prior content (e.g. a genuine `www.youtube.com` visit
+ Enter, twice) already sitting in the file before the new process's
own polling cursor caught up.

**Real cause**: PID reuse / a leftover file from an earlier process
that happened to share the same PID number, combined with the
relay's own cursor-from-current-file-size-on-first-poll behavior not
being a hard guarantee against reading genuinely old content if the
timing is unlucky.

**Real fix / standing practice**: `truncate -s 0` (or delete) a
target process's own `entity_menu_history/<pid>.txt` explicitly before
a scripted test, don't assume a fresh PID means a fresh file.

## 9. Flex layout frozen at its first-ever size (network-browser fullscreen)

**Symptom**: toggling fullscreen grew the window's outer chrome/border
correctly, but the sidebar/content/console panels stayed pinned at
their original small size — a small box inside a big one.

**Real cause**: `khtpm_render_core.c`'s `css_layout_pass()` (the real
flexbox engine) fell back to an element's OWN previous `w`/`h` if
already nonzero, instead of the fresh `avail_w`/`avail_h` the caller
just computed — so a flex container's size was only ever set once
(its first-ever layout pass, when `w`/`h` started at 0) and every
later relayout silently kept the stale value. Only hit network-browser
because it's the only window in the house using the top-level `<page
class="...">` flex-row layout at all (though `canvas-craft` uses the
same pattern and was flagged as needing the same check — not yet
independently verified).

**Real fix**: always use `avail_w`/`avail_h` when there's no explicit
CSS size — never fall back to a remembered stale value. A real flex
engine recomputes every pass; it doesn't cache.

## 10. History list had no delete — feature gap, not a bug, fixed same session

network-browser's sidebar History rows had no `backspace_action`
(every other deletable list row in the house — open-hai's sessions —
already has this generic capability) and no "clear all." Added
`h_%d_del_action`/`nb_write_delhist.sh` (`delhist:<display_idx>`) and
a `clear_hist_action`/`nb_write_clearhist.sh` (`clearhist:`) request
verb, same shape as the existing bookmark/go verbs in
`network_browser_manager.c`'s `handle_request()`.

---

## Related

- `08-roadmap/design-docs/CHTPM-INCREMENTAL-REPARSE-DESIGN.md` — the
  real fix for §0/§1.
- `CENTROID_GOLD_STD.md` item 9 — the permanent house rule this whole
  incident produced.
- `04-bugs/BUG-LOG.md` — "network-browser address bar" entry, the
  blow-by-blow incident trail before the root cause was found.
- `09-appendix/pc-hq-bugs.md` Bug 2 — the earlier, separate
  `override_redirect` focus bug this session's own investigation
  initially (wrongly) suspected was the same cause; it wasn't, but the
  "synthetic input masks real bugs" lesson from it is what led to §6.
- Memory `relay-testing-may-mask-real-focus-bugs`.
