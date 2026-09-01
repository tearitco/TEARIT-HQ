# 🪤 HOUSE_CODE_PITFALLS.md

Real, live-confirmed problems and their real fixes/diagnostic paths.
Not theory — every entry below was actually hit, actually diagnosed
with evidence, and actually fixed (or explicitly still-open) in this
house. Read this BEFORE re-diagnosing something that "looks broken" —
several of these produce symptoms that look exactly like a code
regression but aren't.

---

## 1. A relaunched process can still be running the OLD binary

**Symptom:** you edit code, rebuild, relaunch — and the fix doesn't
seem to take effect, or an old bug you already fixed "comes back."

**Real cause:** `pkill -f <pattern>` is not reliable against this
house's own emoji-laden, star-globbed paths (`44.xyz❤️‍🔥️00.17/`,
`*.monads/*.livedesk-taskbar/`, etc.) — it silently fails to match in
some shells/environments, confirmed live more than once this session.
A `pkill` call that reports success (or reports nothing, which looks
like success) can leave the old process running untouched. The next
`nohup ... &` you run then launches a SECOND instance alongside the
still-alive first one, and whichever one you happen to be testing
against might be the stale one.

**Real fix / procedure:**
1. Never trust a single `pkill -f` call. After it, run
   `ps aux | grep <binary> | grep -v grep` and confirm **zero** results.
2. If anything remains, `kill -9 <exact PID>` each one individually.
3. Only THEN relaunch, and re-confirm exactly one process exists before
   testing anything.
4. Better: use the real house reset command below instead of manual
   kills at all.

## 2. The real way to restart the taskbar/entities is `button.sh reset`, not manual kills

**Location:** `$.crypts/button.sh` (house root).

`bash $.crypts/button.sh reset` does the whole job correctly in one
shot: kills every known taskbar/entity process by a real, maintained
pattern (`khtpm_strip_parser`, `khtpm_taskbar_manager_main`,
`khtpm_hq_render`, `tp_desktop_window_rgb`, etc. — see its own
`KHTPM_PAT`), rebuilds the khtpm binaries fresh, then relaunches
everything via `crypt_autostart.+x` against `autostart.pdl`. Manual
`pkill`+`nohup` sequences are how pitfall #1 above happens — prefer
this real command instead of reinventing the kill/relaunch cycle by
hand.

## 3. Runtime state files under `#.desktop/*.txt` are NOT reset by a source rollback

**Symptom:** `git reset --hard` to a known-clean commit, rebuild, and
something *still* behaves like the old, buggy code — e.g. an entity
spawns at a bizarre screen position, or a feature that was supposedly
removed still seems "on."

**Real cause:** `#.desktop/*.txt` (camera mode/state, one-map-active
flags, armed flags, etc.) are runtime **data**, not source — `git
reset --hard` only touches tracked source files. A file like
`desktop_camera_mode.txt` or `desktop_camera_state.txt` left non-zero
from an earlier testing session survives untouched across the reset
and gets re-read by the freshly-rebuilt-but-clean binary, producing
behavior that looks exactly like a live regression.

**Real fix / procedure:** when a feature is torn out or reverted,
explicitly reset any `#.desktop/*.txt` files it wrote to their real
default values (usually `0` or `1`, matching that file's own
`load_*()` fallback) as part of the same cleanup — don't assume a
source rollback also cleans runtime state.

## 4. External screenshot capture (`scrot`/PIL) is unreliable for verification

**Symptom:** a screenshot shows a window as black/empty/missing
content that you know should be there (or vice versa — shows stale
content that should have changed).

**Real cause:** `scrot`/external `XGetImage`-on-root capture is a
snapshot of whatever the window manager/compositor has actually
composited at that instant — it can race with a window's own redraw,
get occluded by something else briefly on top of it, or simply be read
one frame too early/late. This house's own `!.HOUSE_STDS.md`-adjacent
testing guide already documents this exact failure mode for a
DIFFERENT reason (concurrent real human use) — it applies just as much
to plain timing races with no human involved.

**Real fix / procedure, in order of preference:**
1. **Text state** — read the app's own frame-history/state file
   directly (e.g. `#.desktop/khtpm_strip_frame_history.txt`, a debug
   dump command) instead of a picture. Cheapest, fastest, least
   ambiguous.
2. **Direct window image** — `XGetImage`/python-xlib's
   `window.get_image()` on the SPECIFIC window ID you care about (not
   the root window), which reads that window's own real content
   regardless of what else is on screen. Still can race a redraw in
   flight, but immune to occlusion.
3. **The app's own frame/PNG dump**, if it has one (`dump_frame_png()`
   family) — reads its own offscreen buffer directly, immune to both
   occlusion AND compositor timing.
4. Plain `scrot`/external capture — last resort only.

Whichever method: **allow real settle time** after any edit+relaunch
before capturing evidence (see #5) — a capture taken too fast is
indistinguishable from a real bug until you retry with a longer wait.

## 5. "It's broken" immediately after an edit+reset can be a pure timing race, not a real bug

**Real, live example:** adding a submenu `<row>` to the taskbar's
"network" header button, then immediately checking the live rendered
header, appeared to make three OTHER header cells vanish entirely.
Reverting the change "fixed" it. This looked exactly like a real
structural parsing bug — a subagent even produced a plausible-sounding
theory for one (a 256-element array overflow) — but re-applying the
byte-identical change and checking again a bit later (after more
settle time) showed all cells rendering correctly, no revert needed.
The array-overflow theory was independently checked and disproven
(real element counts were ~29-83, nowhere near the claimed 256) — it
was a fabricated-sounding rationalization for a symptom that had a
much more mundane cause.

**Real fix / procedure:** after any edit + rebuild + `button.sh
reset`, wait at least 2-3 real seconds before capturing verification
evidence, and if a change "looks broken," **retest once cleanly with a
longer wait before concluding it's a real regression** — don't revert
and don't write up a root-cause theory based on a single fast check.

## 6. Relay files are keyed by package PATH, not PID — a stale command can be inherited by a brand-new process

**Real, live, confirmed bug:** `crypt_autostart.c`'s own
`quit_current_livedesk()` writes a plain `CLOSE\n` into every
registered entity's `interact_relay.txt` as a graceful-shutdown
attempt, before its own hard-kill sweep runs. If the OLD process for a
given pal got killed before it ever polled and consumed that line, the
stale `CLOSE` is still sitting in the file. The relay file itself is
named after the package's real directory PATH, not any particular
PID — so when a brand-new process for that SAME pal starts up moments
later, its own very first poll tick reads that leftover command and
dutifully closes itself. Symptom looked like "some entities randomly
fail to survive a reset" — confirmed via each pal's own `history.txt`
showing `WINDOW_OPEN` immediately followed by `INJECTED: CLOSE` a
second later.

**Real fix:** truncate a pal's `interact_relay.txt` immediately before
(re)spawning it — not a timing delay (which only narrows the race
window, it doesn't close it). This enforces the relay's own real,
already-existing "write once, consumed once" contract at the one point
a killed-before-consuming process could otherwise violate it. See
`livedesk_spawn_desk()` in `khtpm_taskbar_manager.c` for the real
implementation.

## 7. A static, hand-maintained entity list WILL drift from the real active desk

**Real, live, confirmed bug:** `$.crypts/autostart.pdl` had its own
separate, static list of `LAUNCH` rows for which entities to relaunch
on reset. The real active desk's own `.pdl` (e.g. `office.pdl`) is a
DIFFERENT file with the real, current entity list — nothing kept them
in sync, so a newly-placed tile that only existed in `office.pdl`
silently never survived a reset.

**Real fix:** don't maintain two separate lists. The manager's own
startup (`ktb_init()`) now reads whichever session/desk is genuinely
active and spawns its real entity list via the same proven
`livedesk_spawn_desk()` logic a live desk switch already uses —
`autostart.pdl` now only needs to launch the tool-bar itself. Any
future per-entity static list anywhere in this house should be treated
as a real drift risk unless something keeps it mechanically in sync
with the real source of truth.

## 8. An `.chtpm` ACTIVATE button's submenu content must be a real tag-tree CHILD, not a sibling

Already documented in `khtpm_strip_header.chtpm`'s own header comment
(STRUCTURE NOTE), worth repeating here since it's an easy trap: a
button that should have a working popup needs its `<row>${var}</row>`
content nested INSIDE it (`<button ...>` ... `</button>`), never as a
sibling after a self-closing `<button .../>`. `is_descendant()` walks
`parent_index` chains — a sibling row can never resolve as a
descendant of the button that's supposed to open it, and the popup
silently never maps (zero elements found, not a crash, not an error).

## 9. Delegated subagent findings need independent verification, especially confident-sounding root causes

**Real, live example:** a Haiku subagent, asked to root-cause the
"network menu breaks other cells" symptom above, confidently reported
a specific line number and a specific fix (bump `LAY_MAX_ELEMENTS`
from 256 to 512) with a plausible-sounding mechanism. It was checked
against real, previously-measured element counts (~29-83) and found to
be simply wrong — not malicious, just an ungrounded rationalization
that happened to sound structurally reasonable. The real cause (per #5
above) was a timing artifact, not a capacity bug at all.

**Real fix / procedure:** treat a subagent's root-cause claim as a
hypothesis, not a fact, especially when it includes a specific numeric
threshold or line number "explanation" — spend the 60 seconds to
verify the concrete evidence it cites (in this case: re-check the real
element count) before applying its suggested fix.

## 10. Prefer real relay-file injection over `xdotool`/screenshots for driving/testing a taskbar or khtpm window

Already the house's own documented standard (see
`_.0.aigent-testing-k9.txt`, sections "SCOPE ADDENDUM 2026-08-26" and
the two-relay-layer note for `khtpm_strip_parser`/
`khtpm_taskbar_manager`) — repeating the short version here since it's
easy to reach for `xdotool`/screenshots out of habit instead:

- `#.desktop/livedesk_agent_relay.txt` — parser-layer, real
  digit/Enter/Escape/printable ASCII, resolves clicks/keys the same
  way a real human input would (`#.desktop/harnesses/
  khtpm-livedesk-taskbar/nav.sh`, needs `HOUSE=<house_root>` set as an
  env var — it defaults to `$PWD` otherwise, a real, easy-to-hit
  footgun if you `cd` somewhere else first).
- `#.desktop/strip_history.txt` — manager-layer, already-resolved
  decimal action codes (`KSC_HQ_HEADER_BASE`+n for a header cell,
  `KSC_HQ_ITEM_BASE`+n for a submenu row) — use for isolating
  manager-side logic from parser rendering specifically.
- Only reach for `xdotool`/XTest/external screenshots when the above
  two are genuinely insufficient (e.g. real mouse-drag physics) — see
  the k9 doc's own explicit "order of preference," direct instruction:
  "u should always try that before using xdo tool."

## 11. Don't add a new mode's data-loading logic INSIDE the shared parser/renderer file - it creates reference drift

**Real, live example:** `khtpm_core_render.c`'s own
`dbhq_load_actors()` reads a real PDL data file
(`&.widgits/db-hq/data/actors.pdl`) directly from inside the shared,
"hard boundary" parser/renderer file - not hardcoded string literals
(the content itself is real, file-based, compliant with the house's
core rule), but the LOADING code lives in the same shared file every
other mode's rendering also lives in, instead of a separate manager
process. Compare to db-hq's own **Common Events** tab, which IS fully
split - a real, separate `khtpm_hq_manager.c` owns the scanning/
business logic and only talks to the shell through a real state file.

**Why this matters / the real risk:** every NEW mode that needs its
own data is tempted to add its own inline loader function to the SAME
shared file (`dbhq_load_actors()`, and possibly siblings for Classes/
Skills/Items/etc. - not yet fully audited). Each one ends up with its
own slightly different assumptions about file format/paths, all living
in one file nobody owns end-to-end - a real, compounding drift risk,
not a hypothetical one. It also makes the "hard boundary" file bigger
and riskier to touch with every mode added, the opposite of the
house's own stated goal for that file.

**Real fix / procedure:** when a new HQ-style window mode needs real
data, give it its own real, separate manager process (matching
`khtpm_hq_manager.c`'s own shape) that reads that mode's own real data
files and publishes a real, simple state file - the shared renderer
only ever reads that published state generically (via
`reusable_slot()`, per SKILLS.md §2's own house standard), never
parses the mode's own source data directly. Before writing a NEW
inline loader in the shared file, check whether the mode you're adding
should instead get its own manager, the same way you'd check `build_*.sh`
for a `cp` line before editing a file that might be a copy (see #1
above) - it's the same class of "check the real convention before
adding to the pile" discipline.

**Real follow-up, not yet done:** an audit pass across
`khtpm_core_render.c` (and other manager/ops files in this
house) to find every OTHER inline data-loading function that should
have been a separate manager from the start - `dbhq_load_actors()` is
the one already found; there may be siblings. See `au-31.md` for the
real, dated todo list this finding produced.

---

*Append new entries here as they're found — this file exists so the
next session doesn't re-discover the same mistake from scratch.*
