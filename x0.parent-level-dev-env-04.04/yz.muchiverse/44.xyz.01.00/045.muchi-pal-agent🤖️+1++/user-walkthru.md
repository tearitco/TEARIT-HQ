# 🧑‍💻️ USER-WALKTHRU — do your own code/book with the agentic tools

> For a **human user** testing with their **own** code or book — no test harness,
> just type and watch. Proves what the harnesses prove, but with your stuff.
> Status: tools live + proven (H1 + H2 harnesses green, 2026-07-31).
> Companion docs: `2&3-jul31-sprint.md`, `#.haiku+/30.jul-30-handoff/5.tool-scaffold-gemma-agentic.md`.

---

## 🚀 1. Start it

```bash
cd 045.muchi-pal-agent🤖️+1
bash button.sh run
```

A chat UI opens. The header shows the model (default **gemma-lan**, or your last
chosen model). The line at the bottom tells you the **Session: pieces/sessions/…**
directory — that's where the files you create will live.

**Files you create land in your session dir.** The session dir is printed when
you start. Your created files (hello.py, book.txt, …) appear there alongside the
chat pieces. They're wiped on the next launch (fresh session each time) — so if
you want to keep something, copy it out.

---

## 🧰 2. The tool menu (type it in plain English, press Enter)

One **tool per turn**. The keywords are deterministic — no AI involved in the
decision. The result always lands in the chat as `[<tool> result]: …`.

| You want to… | Type this (works, proven) | Result looks like |
|---|---|---|
| 📋 list files | `list the files` or `list dir` | `[list_dir result]: …` file names |
| ✍️ write a file | `create file hello.py containing print('hello world')` | `[write_file result]: Written 21 bytes to hello.py` |
| ▶️ run a command | `run python3 hello.py` | `[exec_cmd result]: hello world` |
| 📖 read a file | `read file book.txt` | `[read_file result]: …file contents…` |
| ➕ append a line | `append to book.txt the line "It was a dark and stormy night."` | `[edit_file result]: Appended … bytes to book.txt` |
| 🔀 edit (replace) | `edit hello.py replace hello with hi` | `[edit_file result]: Successfully edited hello.py (1 replacement)` |
| 🔎 search | `search for dark in book.txt` | `[search_in_files result]: book.txt [Line 2]: …` |
| 🔊 speak | `speak hello` | TTS says it |
| 🔄 switch model | `/model iqabod-test` | header model changes |

---

## 🧪 3. Your own mini-project (10 minutes)

A whole "dev loop" for YOUR code, one tool per turn:

```
You:  create file hello.py containing print('hello world')
      [write_file result]: Written 21 bytes to hello.py

You:  run python3 hello.py
      [exec_cmd result]: hello world

You:  edit hello.py replace hello with hi
      [edit_file result]: Successfully edited hello.py (1 replacement)

You:  run python3 hello.py
      [exec_cmd result]: hi world

You:  read file hello.py
      [read_file result]: print('hi world')
```

Your book:

```
You:  create file book.txt containing Chapter 1: Hello
You:  append to book.txt the line "It was a dark and stormy night."
You:  read file book.txt
      [read_file result]: Chapter 1: Hello
      It was a dark and stormy night.
You:  search for dark in book.txt
      [search_in_files result]: book.txt [Line 2]: It was a dark and stormy night.
```

Each Enter = **one** tool. The result comes back before any model call
(pre-LLM, deterministic — that's the whole trick for a 270M model).

---

## 📏 4. Grammar — so it always works

- **write:** `create file <name> containing <content>` — also works with
  `with`, `that says`, `as`. Content = everything after that word. Quotes around
  the content are stripped.
- **run:** `run <command>` (needs `python3`, `bash`, etc. — anything a shell can do).
- **read:** `read file <name>` (also `open`/`cat`).
- **append:** `append to <name> the line <text>` — or `append <text> to <name>`.
- **edit:** `edit <name> replace <old> with <new>` (also `change`).
- **search:** `search for <word> in <file-or-dir>` (also `grep`).
- Missing a filename? You get a clean `Missing filename` result — no crash, no
  `TOOL:` garbage.

**Rules:** paths are session-scoped (your session dir). Outputs cap at ~4KB.
Keep it one tool per turn — gemma can't plan multi-step sequences; you're the
driver, like a human pair-programmer.

---

## 🎛️ 5. Optional: let gemma narrate the result

Default is result-only (no model call after a tool). Want gemma to comment on the
result ("nice, next?")? Edit
`pieces/world_01/session_01/chat/state.txt` → set `model_after_tool=yes`
and relaunch. It still never calls tools itself — it just talks about results.

---

## ❓ 6. If something looks wrong

| Symptom | Cause / fix |
|---|---|
| `hello world` never appears | check the Session dir you were in — files vanish on relaunch |
| tool not triggered | keyword not in the message — re-read section 2, keep it simple |
| `TOOL: …` garbage in chat | old bug, now fixed — if you ever see it again that's a regression |
| command did nothing | no such binary (`python3` vs `python`), or the file wasn't written yet |
| long `read`/`search` truncated | output cap at ~4KB by design |

Want the machine-grade proof instead? Run the harnesses:
`test-harn-same/scenarios/demo_tool_hello_python.sh` and
`test-harn-same/scenarios/demo_tool_edit_book.sh` — all KPIs green.
