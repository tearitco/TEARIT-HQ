# 🏠️ House-Style C — Onboarding Prompt for Your Agent 🤖️

👋 Hey! This is a copy-paste prompt for whatever coding agent you're using
(Claude, GPT, whatever). It explains the "house style" of C we use — small,
boring, real programs instead of one giant clever one. Paste the whole
thing in when you start a new project and want it built this way.

---

## 📋 PASTE THIS TO YOUR AGENT 👇

> We're building this project in **"house style C"** — a small set of
> real conventions, not a framework. Read the rules below and follow them.
> If you're ever unsure whether something needs a fancier tool than
> described here (especially the "optional UI layer" section at the
> bottom), **ask the user to check with the original house first** rather
> than guessing or inventing your own new pattern. 🙏

---

### 1️⃣ One job per binary ("ops")

🔧 Don't write one big program that does everything. Write lots of small
C programs ("ops"), each one doing ONE real thing:

- `change_gold.c` → reads/writes a gold value. That's it.
- `show_message.c` → prints/pops up a message. That's it.
- Compile each to its own binary (`ops/+x/change_gold.+x`, etc).

✅ **Why:** small ops are easy to test alone from a terminal, easy to
reason about, easy to replace later. 🧩

---

### 2️⃣ One manager, everyone else reads 📖

🎛️ If several things need to share state (a game loop, a UI, a background
process), pick **ONE process** to be the manager:

```c
while (running) {
    check_for_changes();   // scan input/state
    do_the_real_work();    // the ONE thing this manager owns
    usleep(16667);         // ~60Hz is a totally normal tick rate
}
```

- The manager is the **only** thing that writes the shared state file.
- Every other process just **reads** that file. Never two writers. 🚫✍️✍️

✅ **Why:** race conditions between two writers are a real, painful bug
class. One writer + many readers sidesteps the whole problem.

---

### 3️⃣ Storage: plain text first, always 📄

😌 You do **NOT** need a database or a fancy format to start. Two real
shapes, pick whichever fits:

**Flat key=value** (simplest — use this by default):
```
quest_started=1
gold_bonus=50
```
One line per value. Easy to `grep`/`sed`/hand-edit. Great for
switches/flags/counters.

**Pipe-table ("PDL")** — for a few rows of structured data:
```
SECTION      | KEY                | VALUE
----------------------------------------
BOOKMARK     | favorite-page      | /some/path
BOOKMARK     | another-one        | /other/path
```
Use this when flat key=value would need repeated/nested info.

🎯 **Rule of thumb:** if you can express it as `.txt`, don't reach for
anything heavier. A real house-style project can run its ENTIRE state
on flat text files. No SQL, no JSON parser dependency, no drama.

---

### 4️⃣ Data-driven > hardcoded 🗃️

🚫 Don't hardcode a list of "types" or "commands" in C arrays if new ones
will be added later. Put the list in a text file (a "registry") and have
your C program **read** it at runtime:

```
COMMAND change_gold
  LABEL Change Gold
  FIELD1 Amount:
  PARAMS amount
  TEMPLATE change_gold.+x {amount}
END
```

Adding a new command = editing this file. Zero recompiles. ✨

---

### 5️⃣ Test like it's real, not like it's a demo 🧪

😤 The #1 mistake: "it compiled, so it works." **A clean compile is NOT
evidence anything works.** Always:

1. Actually **run** the thing.
2. Actually **read the output file** it was supposed to write.
3. Only THEN say it's done.

🖱️ **Avoid real mouse-clicks/screenshots for testing when you can help
it.** If your program reads input from stdin or a file, write a tiny
test script that FEEDS it input and reads back what it produced. That's
faster, more reliable, and an agent can run it without a real screen. 🤖

📸 Screenshots/real clicks are a legitimate LAST resort — only reach for
them when a text-based test genuinely can't answer the question (e.g.
"does this pixel actually render"). Don't jump there first out of
convenience.

---

### 6️⃣ 🖼️ Optional: a real UI layer (only if you actually need one)

You **do not need this to start.** Plain text + a terminal, or a simple
native window, is a completely legitimate whole project. 🙌

If your project grows into needing a real GUI window with clickable
buttons, tabs, keyboard navigation, etc — there's a real, proven pattern
for that too (a tiny custom XML-ish layout format + a generic renderer +
auto-numbered keyboard nav for every clickable thing). It's genuinely
useful once you're there, but it's also a bigger commitment and easy to
reach for too early.

⚠️ **If your agent starts feeling like it needs this** (a real window,
multiple panels, lots of buttons) — **have it ask you to check with the
original house first**, rather than it inventing its own UI framework
from scratch. There's a real spec for it and some hard-won lessons about
how NOT to build it (raw hand-positioned pixel coordinates instead of a
real layout = a whole class of bugs you don't want to rediscover). 😅

---

## 🎁 TL;DR for your friend (you!) 🎁

- 🧱 Lots of tiny C programs, not one big one.
- 📝 One writer, everyone else reads.
- 📄 Flat `.txt` files for storage — no database needed.
- 🗂️ Put "types"/"commands" in a data file, not hardcoded in C.
- 🧪 Prove it works by reading the real output, not by trusting the compiler.
- 🖼️ Skip the fancy UI layer until you actually need it — and ask first
  when you think you do.

Good luck, have fun! 🚀💛
