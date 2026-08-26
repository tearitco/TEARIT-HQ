🎪 EVENTS-HQ & DB-HQ: THE HUMAN-READABLE NUANCE GUIDE 🎪
=========================================================
(A friendly map through the sharp edges. Written 2026-08-25 after building
and live-verifying the H6/H7/H8 batch — every 🐛 below is a REAL bug that
was actually hit, not a hypothetical.)

---

## 🗺️ 1. THE BIG PICTURE — who is who

📦 **One binary to rule them all**: `khtpm_entity_menu_render.c`
(under `*.monads/*.livedesk-taskbar/ops/`) is the SAME compiled program
for db-hq, events-hq, stats-hq, bookmarks, palettes, chat-hai, and
taskbar-settings. It figures out which app it's being at startup by
reading the `<window class="...">` attribute off the `.chtpm` file it's
handed. So when you're hunting a bug, you're always in this ONE file —
just look for the `g_is_events_hq` / `g_is_db_hq` / `g_is_chat_hai` /
`g_is_stats_hq` flags to find the right branch. 🔀

🧠 **Shell + Manager, always two processes**:
- The **shell** (the binary above) draws the window, handles clicks/keys.
- The **manager** (`khtpm_events_hq_manager.c` for events-hq,
  `khtpm_hq_manager.c` for db-hq) owns the REAL business logic — file
  scanning, compiling, writing state. The shell launches it itself via a
  real `fork()+execv()` reading a `<module src="..."/>` tag in the
  `.chtpm` file. Nobody launches the manager directly.
- They talk ONLY through files in `pkg_dir/.hq_manager/` (events-hq) or
  `#.desktop/` (db-hq, since it's single-instance). Shell writes
  `action.txt`, manager reads it, does the real work, writes back a
  `*.state.txt`, shell polls and redraws. 📨↔️📨

🧍 **events-hq is multi-instance, db-hq is not**. You can have events-hq
open on `ava` AND `m8_redhorned` at the same time — that's intended, not
a bug. db-hq is one-window-per-house, so its launcher (`open_db_hq.sh`)
kills any prior instance before starting a new one. events-hq's launcher
(`button.sh`) only kills a PRIOR instance on the SAME entity — never a
different one. Know which rule applies before you `pkill`! 🔫

---

## ⌨️ 2. THE TESTING LIFELINE — relay/history files

🚫 **Never click with a real mouse to test.** Every one of these windows
polls its OWN text file every ~150ms and treats each line as a fake
keypress. This is the ONLY sanctioned way to test — direct CLI calls or
manual XTest injection are last resorts, not the default.

📁 **Where the file lives** (as of 2026-08-25, renamed from
`*_agent_relay.txt`):
```
#.desktop/db_hq_history.txt
#.desktop/events_hq_history.txt      ← but see per-entity note below!
#.desktop/stats_hq_history.txt
#.desktop/chat_hai_history.txt
#.desktop/entity_menu_history.txt
#.desktop/taskbar_settings_history.txt
```

⚠️⚠️ **BIGGEST GOTCHA**: events-hq is multi-instance, but the history
file name above is the SAME regardless of which entity's window you
mean! In practice each running events-hq process reads
`#.desktop/events_hq_history.txt` — so if you have TWO events-hq windows
open at once (ava + m8_redhorned), writing to that file drives **both of
them** simultaneously. If you're testing one entity, make sure no other
events-hq window is open, or you'll get cross-talk that looks like a
random flaky bug. 🐛👻

🔢 **The codes are ASCII DECIMAL, not the literal digit!** This one bit
ME during tonight's own verification pass. To send keypress `'2'` you
write the line `50` (its ASCII code), NOT the line `2`. `2` all by
itself means nothing to the dispatcher and is silently ignored — no
error, just... nothing happens, forever, looking exactly like a broken
feature. Cheat sheet:
```
13  = Enter          27 = Escape         8 = Backspace
48-57 = digits '0'-'9' (so '2' → 50, '7' → 55, etc.)
32-126 = any other printable ASCII character
200-205 = arrow keys / page up / page down (debug-only extension,
          these have no real ASCII code so they live outside 0-126)
```
🧮 Quick trick: in bash, `printf '%d\n' "'2"` gives you the ASCII code
for any character.

✏️ **`#`-prefixed lines are audit comments, not commands.** Since
2026-08-25 you can drop a note into any history file
(`# checked trigger-edit path, jbrooks 2026-08-25`) and it'll be safely
skipped (never dispatched) but still advance the read cursor. Great for
leaving breadcrumbs for the next person testing the same file.

🔁 **The file is append-only, never truncated.** Every one of these
readers is cursor-based (`g_history_cursor`/`g_relay_cursor` style) —
they remember how far they've read and only look at NEW bytes. You do
NOT need to clear the file before testing. (Fun fact: earlier tonight I
was manually truncating it before every test out of habit — totally
unnecessary, the mechanism handles it on its own.)

☠️ **Single-instance guard, every single time.** Before launching ANY
of these windows for a test, run
`ps aux | grep <binary-name> | grep -v grep` and confirm ZERO results.
A stray leftover process from your last test session will silently
race the new one on the exact same file, and you'll see "flaky,
unreproducible" behavior that isn't a code bug at all — it's two
processes fighting over one relay file. This has bitten multiple
sessions' worth of testing in this house. Confirm clean BEFORE and
AFTER, every time. 🧹

---

## 🎹 3. NAV / KEYBOARD ACCESSIBILITY

🔢 **Everything is digit-jump + Enter.** Every clickable row/button in
these windows gets a `[ n]` bracket badge showing its nav number.
Sending the ASCII code for that digit moves keyboard FOCUS there;
sending Enter (`13`) next ACTIVATES whatever has focus. Two separate
steps — a common mistake is sending them both in the same instant and
wondering why "only half worked."

🙈 **Nav order follows DRAW-CALL order, not file/textual order.** If
you're guessing which number a row will get by reading the source top
to bottom, you WILL get it wrong if that function is called out of
textual order inside `redraw()`. Always grep the actual call order
inside `redraw()`/`evhq_layout_pass()`, don't assume.

🎯 **Keyboard FOCUS ≠ page/tab SELECTION** — these are two different
state variables that both get drawn with an orange box, and it is very
easy to confuse them (I did, tonight, mid-verification)! Moving your
nav cursor ONTO a page tab does NOT switch which page's content is
showing — you have to actually press Enter on that tab to select it.
The "current page" is `g_evhq_current_page`; the "keyboard cursor
position" is `g_focus_nav`. Don't assume the tab your cursor is
hovering is the one whose Trigger/Commands panel you're looking at.

♿ **House standard: no UI element without a keyboard mirror.** Any new
button/row MUST get a real nav badge and be dispatchable via digit-jump
— this isn't optional polish, it's `!.HOUSE_STDS.md` §K.6. If you build
a mouse-only click handler, you're not done yet.

---

## 🐣 4. EVENTS-HQ SPECIFIC NUANCES

📄 **A "page" is a directory**: `event_pkg/pages/page_N/` containing
exactly two files:
- `condition.pdl` — has one `COND | trigger | <value>` line
- `event.ir.pdl` — the actual command list (change_gold, show_text, etc)
Both get compiled into `event.pal` by `compile_page()`.

🚨🚨 **THE ON-CLICK VS ON_CLICK TRAP** (a real bug I found and fixed
tonight, 2026-08-25): the trigger value must be written with a HYPHEN
— `on-click`, `on-interact` — NOT an underscore. `play_event.sh`
compares the trigger with an EXACT string match, ZERO normalization
anywhere in the pipeline. A page whose `condition.pdl` says `on_click`
(underscore) will NEVER run, from ANY trigger, EVER, with NO error
message anywhere. It just sits there looking exactly like a working
page. If you ever add code that writes a new `condition.pdl`, triple
check you used a hyphen. 🔗 vs ⬜

🏆 **Highest-numbered MATCHING page wins.** `play_event.sh` scans every
`page_N` whose trigger matches the one being fired, and runs ONLY the
highest-numbered match — same rule RPG Maker MV uses. This means: if
you create a bunch of test pages that all default to `on-click` and
forget to give them real content or different triggers, your newest
EMPTY test page will silently shadow your real page-1 event and nothing
will appear to happen when you hit Play. (Ask me how I know — I hit
this literally tonight verifying the Play button itself, and had to
delete my own test pages to get a clean signal.) 🫥

📝 **New pages default to trigger `on-click`** (after tonight's fix).
Rename it via the Trigger field if you want a different one.

🖊️ **Trigger editing reuses the Add Command picker's OWN keystroke
mechanism** — there is deliberately only ONE text-entry code path in
this file for events-hq mode, shared by both features. If you're
adding a THIRD editable text field someday, reuse this same
accumulate-then-commit-on-Enter pattern rather than inventing a new one
— that's an explicit house guardrail, not just a style preference.

🕹️ **Trigger field flow**: nav to it → Enter arms edit mode (buffer
starts EMPTY, doesn't pre-fill/append) → type the new value → Enter
commits (rewrites `condition.pdl`, keeps everything else in the file)
→ or Escape cancels with no write.

▶️ **The Play button runs the CURRENTLY SELECTED page's real event**,
using the exact same `play_event.sh` invocation an entity's own
right-click "Play" row uses. It is fire-and-forget (`&` backgrounded) —
the manager doesn't wait for it or report success/failure back to the
UI, so if nothing seems to happen, check the actual state file on disk
(gold.txt, inventory.txt, whatever your event touches) rather than
staring at the window for a confirmation that doesn't exist.

---

## 🗄️ 5. DB-HQ SPECIFIC NUANCES

🪟 **Single window per house, always.** Unlike events-hq, only one
db-hq can be open at a time — its launcher force-kills any prior
instance (shell AND manager) before starting fresh.

🧩 **Sidebar + panel, not tabs.** Real, working nav uses
`find_by_tag(window, "sidebar")` / `find_by_tag(window, "panel")` — this
is the SAME shape stats-hq was migrated onto (its old bash-XML-scraped
tabbar version genuinely never worked; its tabs never matched anything).
If you're adding a new -hq-style window, copy this sidebar shape, not
a tabbar.

🖱️ **Generic onClick dispatch, not hardcoded label matching.** The
newer, house-preferred pattern is a real `onClick="livedesk:foo:bar"`
string per element plus a generic branch in `activate_elem()` — NOT
matching against a hardcoded `g_events[]`-style C array by label text.
The label-matching style still exists in older code (Common Events) but
is considered legacy; don't copy it for new features.

📎 **Palettes/bookmarks were fully rebuilt as real managers** this
session — replacing bash scripts that hand-`printf`'d `.chtpm` XML.
`khtpm_hq_render.c` (the old standalone db-hq binary) is now DELETED,
not just deprecated — if you find a doc still pointing at it, that doc
is stale (already caught and fixed twice in `!.HOUSE_STDS.md`).

📊 **Palette grid columns/scroll are layout-derived, not hardcoded.**
`dbhq_pal_cols_for()` reads the REAL CSS tile width/gap/window-width via
`css_compute_style()` — if the grid looks wrong, check the CSS, not a
magic number in the C file (there isn't one anymore).

---

## 🧯 6. WHEN SOMETHING "DOESN'T WORK" — a checklist before you panic

1. 🔢 Did you send the ASCII code, not the literal digit? (See §2.)
2. 👯 Is there a STRAY process from a previous test still alive, racing
   this one? (`ps aux | grep <binary> | grep -v grep`)
3. 🎯 Are you confusing keyboard FOCUS with page/tab SELECTION? (§3)
4. 🫥 For events-hq: is a HIGHER-numbered page with the same trigger
   silently shadowing the one you're testing? (§4)
5. 🔗 For a newly-created page: is the trigger `on-click` (hyphen) and
   not `on_click` (underscore)? (§4)
6. 📸 Did you verify with a REAL pixel dump (`'p'` key → receipt/PNG),
   not just a state-file diff? A state file can update correctly while
   the actual rendered window is frozen/stale — these are proven to be
   able to diverge in this house.
7. 🧑‍🤝‍🧑 If an agent (including a Haiku subagent) reported "verified
   live" — check HOW it verified. Writing straight to `action.txt`
   proves the MANAGER works. It does NOT prove the actual UI button
   dispatch exists. (This exact gap was found and fixed tonight — see
   HAIKU_TASKS.md's 2026-08-25 merge note.)

---

🎉 That's the guide! When you hit a NEW nuance, add it here rather than
letting it live only in your head (or in an agent's one-off chat log).
