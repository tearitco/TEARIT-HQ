# 🖼️ chtpm/khtpm — Onboarding Prompt for Your Agent (the "fancy UI" layer) 🤖️✨

👋 Read `c-house-onboard-agent-prompt.md` FIRST — this is the follow-up
doc for when your agent says something like "this project might want a
real window/UI." This explains that real UI layer: **chtpm** (the layout
format) + **khtpm** (the C program that renders it). Paste this whole
doc to your agent when you're ready to go there.

📦 **The real parser/draw code is included too** — see `khtpm-core/` in
this same folder (real, working files copied straight from the house,
plus a README explaining how to actually wire them into your own
program, plus a real example `.chtpm` file). You don't have to write
the chtpm parser or the generic draw/nav code from scratch — that part's
already done and battle-tested.

---

## 📋 PASTE THIS TO YOUR AGENT 👇

> If we're adding a real GUI window to this project, we're doing it
> "house style": a small declarative layout file (**chtpm**) read by a
> generic renderer (**khtpm**), not a hand-rolled pixel-positioned UI.
> Read the whole doc below before writing any UI code — there's a real,
> hard-won lesson at the bottom about what goes wrong if you skip the
> layout file and hand-position things in C instead. If anything here
> is unclear or you think you need something bigger than described,
> **ask the user to check with the original house first.** 🙏

---

## 🐣 Step 0: do you even need this yet?

**No, probably not yet.** Stay in plain C + text files (see the other
doc) for as long as you possibly can. Reach for chtpm/khtpm only when
you have a REAL need for:
- a real window with more than one clickable thing in it, AND
- keyboard navigation between those things (not just mouse), AND
- the layout will change/grow over time (so hardcoding pixel positions
  in C would mean editing C every time).

If you just need "print some text" or "one button" — plain X11/whatever
your toolkit is, or even just a terminal program, is still fine. 😌

---

## 🧩 What chtpm actually is

A tiny, boring XML-ish file describing a window's CONTENTS, not its
pixels. You write WHAT is in the window; a generic C program (khtpm)
figures out WHERE to draw it and handles clicks/keyboard for you.

```xml
<!-- example: a real, working shape used in this house today -->
<window class="my-app-window">
  <!-- the manager process this window talks to (see doc 1, rule #2) -->
  <module src="ops/+x/my_app_manager.+x" />

  <tabbar class="tab-bar">
    <tab label="Home" class="active"/>
    <tab label="Settings"/>
  </tabbar>

  <sidebar id="sidebar" class="sidebar">
    <item label="(loading)"/>   <!-- real rows get filled in at runtime -->
  </sidebar>

  <panel id="content-panel" class="settings-block">
    <title class="block-title" label="Nothing selected"/>
    <text class="hint" label="(pick something from the sidebar)"/>
    <button label="Do The Thing" onClick="do-the-thing"/>
  </panel>
</window>
```

That's it. `<window>`, `<tabbar>`/`<tab>`, `<sidebar>`/`<item>`,
`<panel>`, `<title>`, `<text>`, `<button>` — a handful of tags, plus
`id=`/`class=` for styling hooks and `onClick=` for actions.

---

## ⚙️ What khtpm actually is

The ONE generic C renderer that reads a `.chtpm` file and:
1. Draws every element in the tree (tabbar across the top, sidebar down
   the left, panel filling the rest — real layout math, not something
   you write per-window).
2. **Auto-numbers EVERY element that has `onClick=` into keyboard nav**
   — this is the important part. The user (or a testing agent!) can
   press `1`, `2`, `3`... + Enter to activate ANY button, with zero
   per-button code from you. This numbering happens automatically from
   a single generic pass over the tree — you never hand-assign nav
   numbers.
3. Dispatches `onClick="whatever-you-wrote"` back to YOUR code (a
   switch/if-chain on that string) when something is activated — by
   mouse click OR by keyboard nav. Same code path either way.
4. Talks to your manager process (see doc 1, rule #2) through plain
   text files — the window reads state to display, writes an "action"
   file when the user does something, the manager reacts.

You do NOT write your own drawing code, your own click-hit-testing, or
your own keyboard-nav-number-tracking. That's khtpm's job. Your job is
(a) the `.chtpm` file describing what's in the window, and (b) a
`dispatch_onclick(const char *action)` function that does the real work
for each `onClick=` string you invented.

---

## 🪜 Gradual adoption path (pick your step, don't jump to the end)

**Step 1 — one static window, no manager yet.** A `.chtpm` with a few
buttons, `onClick=` values that just run a small op directly. Good for
"I want a real window with 3 buttons" and nothing more.

**Step 2 — add a manager.** Once the window needs to show LIVE data
(a list that grows, a value that changes), add the one-manager-many-
readers pattern from doc 1: the manager scans/computes and writes a
state file; khtpm's renderer re-reads that file on a cheap timer and
updates the sidebar/panel content to match.

**Step 3 — sidebar + panel, multi-item.** Once you have a real LIST of
things (sessions, entities, files...) and want to click one to see its
detail, that's the `<sidebar><item>`/`<panel>` shape from the example
above — one manager publishes the list + the currently-selected item's
detail, the renderer just displays whatever the manager last wrote.

**Step 4 — real, editable sub-content.** If clicking something in the
panel needs to open ANOTHER small editable form (fields, a picker) —
give THAT its own small `.chtpm`-driven layout too, not hand-positioned
C. See the pitfall below for exactly why this matters.

You can stop at any step. Step 1 alone is a completely legitimate whole
feature.

---

## 💀 THE #1 PITFALL — do not hand-position pixels in C

This is the single most expensive mistake made building this exact
pattern (real incident, not hypothetical): a popup/sub-window was built
by hand-computing `x`/`y`/`w`/`h` pixel coordinates directly in C
instead of describing it in a `.chtpm` file. This looked fine at first
(mouse clicks worked, it drew correctly) but caused a WHOLE cascade of
real bugs later:

- 🖱️ Clicking things stopped working when the surrounding window's size
  changed, because the hand-computed coordinates silently drifted out
  of sync with reality.
- ⌨️ Keyboard navigation didn't work AT ALL on the hand-positioned
  content, because it was never registered into the generic
  auto-numbered nav system — only things declared as real elements
  with `onClick=` get that for free.
- 🧪 It couldn't be tested by an agent driving it through text-file
  input either, for the same reason — there was no real element for
  the test harness to point at.

**The fix, every time:** if it's clickable or navigable, it needs to be
a REAL element (with `onClick=`) in a REAL `.chtpm`-loaded layout —
never a raw `XDrawString`/manually-positioned rectangle that you also
hand-wire click detection for. If you catch yourself computing pixel
math for something interactive, stop and go back to a real layout file
instead.

---

## 🧪 Testing (same rule as doc 1, extra detail for windowed UIs)

Give your renderer a **text-file input relay**: it tails a plain file
(one line per keypress, decimal ASCII codes — `13`=Enter, `27`=Escape,
digits for nav-jump, `32`-`126`=typed characters), and dispatches each
line through the EXACT SAME code path a real keypress uses. This lets
an agent (or you) drive the whole window — navigate, click buttons,
type into fields — by appending lines to a text file. No screenshots,
no simulated mouse clicks, no real display needed.

Also give it a **cheap text state dump** (one relay code triggers "write
current state — what's focused, what's on screen — to a plain text
file"). Read THAT to verify a test worked, instead of decoding a
screenshot. Much faster, much more reliable, and it's how this exact
house catches real bugs (a button silently not wired to anything looks
identical to a working one in a screenshot, but shows up immediately
in a state dump).

Only fall back to a real screenshot/real click when you're testing
something a text dump genuinely can't answer (does this actually LOOK
right, pixel-wise) — not as your first move.

---

## 🎁 TL;DR for your friend (you!) 🎁

- 🐣 Don't reach for this until plain text+C genuinely isn't enough.
- 📄 `.chtpm` = WHAT's in the window (tags, `onClick=`). Never pixels.
- ⚙️ `khtpm` (generic renderer) = WHERE it goes + auto keyboard nav +
  dispatches your `onClick=` strings back to you.
- 🪜 Adopt gradually: static window → add a manager → sidebar+list →
  editable sub-forms, stop wherever your project actually needs.
- 💀 NEVER hand-position pixels for anything clickable — real element,
  real layout file, every time, or you'll rebuild this exact bug later.
- 🧪 Test by feeding text into a relay file + reading a text state dump,
  not by screenshotting or clicking for real.

Have fun building your window! 🪟✨
