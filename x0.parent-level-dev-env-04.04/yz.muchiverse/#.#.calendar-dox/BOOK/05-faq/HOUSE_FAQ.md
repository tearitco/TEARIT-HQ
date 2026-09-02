# HOUSE_FAQ — real answers to real questions

*Moved from `#.#.calendar-dox/1.^V-hq/HOUSE_FAQ.md`, 2026-09-02.
Architecture Q&As now also live in `02-architecture/` in fuller form
(this file keeps the FAQ shape); bug-shaped entries moved to
`04-bugs/BUG-LOG.md`. Append new Q&As at the bottom, dated — don't
rewrite old answers if they go stale, add a 🔄 CORRECTION note under
the original instead.*

============================================================
🧩️ ARCHITECTURE
============================================================

See `02-architecture/TWO-PARSER-FAMILIES.md` and
`02-architecture/INPUT-RELAY-PIPELINE.md` for the full answers to:
why there are 7+ khtpm "modes" instead of one generic engine, whether
each mode has its own layout parser, and the LayDoc vs Elem/CSS split.

============================================================
📁️ FILES / COMPLIANCE
============================================================

See `02-architecture/STATE-AND-PDL-CONVENTIONS.md` for: "if it's not
in a file, it's a lie," and what a "manager" process is / why a
feature can't just be C code embedded in the renderer.

============================================================
🎮️ NAV / INPUT
============================================================

See `02-architecture/INPUT-RELAY-PIPELINE.md` for: why `nav_index`
restarts per-window instead of continuing across windows, and how a
human click/keypress actually reaches the renderer.

============================================================
🖼️ ASSETS
============================================================

**Why do RPG Maker asset paths live in a `.pdl` file instead of being
hardcoded in C?** So the path can change (drive letter, OS, files
physically moved) without a C rewrite — see
`02-architecture/STATE-AND-PDL-CONVENTIONS.md`.

**Does mutaclysm-neo need the render/input refactor too?** No — it's
the reference implementation the refactor has been copying FROM (its
`pieces/keyboard/history.txt` format is what every hq window's input
capture now uses verbatim). It was never in scope because it's already
the standard being matched.

============================================================
🧟️ PROCESS / REGISTRY HYGIENE
============================================================

**The bottom bar says an entity is open, but it's nowhere on the real
desktop. What's going on?** Almost certainly a zombie PID —
`ktb_pid_alive()`'s `kill(pid,0)==0` check can't distinguish a real
running process from an exited-but-unreaped zombie. See
`04-bugs/BUG-LOG.md` for the confirmed incident and the quick fix
(`ps aux | grep <old-binary-name>`, `kill -TERM` any stray).

============================================================
🚚️ PATHS / HOUSE LOCATION
============================================================

**Why does this path look different from an older doc/handoff I'm
reading (`NNEST-11.17`, `44.xyz❤️‍🔥️00.17`, `#.#.✅️.cal-user-sum`)?**
The whole house moved 2026-09-01 — see `09-appendix/
S1_HOUSE_PATH_MIGRATION.md` for full detail, and this book's own root
`README.md` for the quick-reference table. A literal old path inside
runtime-generated content (a stale `action=` in a live `.chtpm`, an
old relay/log line) self-heals on the next write from `g_house_root`;
a literal old path hardcoded in actual source is a real bug (the
migration's own audit found and fixed the handful that existed).
