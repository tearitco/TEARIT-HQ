# 🖼️➡️⚙️ walk-off-au5.md — event-ez, session pause 2026-08-05

> **Superseded for current status:** house-root **`walk-off-au6.md`** (2026-08-06) — KEY:7 Clear All, compose/ping CPU fix, 30fps caps, Change Gold dogfood PASS. Keep this file for 08-05 history.

Read `&.widgits/event-editor/visual-event-compiler-pal.md` first (the design), then this (the current real build state). A machine crash paused testing here — recovered cleanly, code survived, one live re-verification was interrupted mid-flight.

## What's real right now (all files in `ops/`)

`ops/ez_compose_frame.c` and `ops/ez_menu_input.c` were substantially rewritten this session — the OLD flat Chase/Flee/Wander/Idle/Target/Speed/Command model (pet-behavior demo) is **gone**, replaced by a real, growing, RPG-Maker-style command list per page:

- **Gallery** (`event_ez.chtpm`) — unchanged shape, real page list, "always one more empty slot."
- **Page screen** (`event_ez_page_N.chtpm`) — real Trigger field + Save Trigger (writes `condition.pdl` only now), a real `-- COMMANDS --` list read from `event.ir.pdl`'s own `NODE` rows, and a `[+] Add Command` button.
- **Command Picker** (`event_ez_page_N_cmdpick.chtpm`) — real, currently lists only "Change Gold" (Show Choices not listed — not built, no dead button).
- **Change Gold parameter screen** (`event_ez_page_N_cmd_change_gold.chtpm`) — one `cli_io` amount field, `KEY:6` Save.

**`KEY:6` (Save Change Gold)** is the real compiler entry point: appends a `NODE | id=N type=change_gold | amount=X` row to `event.ir.pdl`, then **fully regenerates `event.pal` from scratch** by reading every `NODE` row back out — genuine "IR is truth, .pal is always a compiled artifact" semantics, never hand-patched.

## Two real bugs found ONLY by running the compiled output (not by reading it)

Both are written up in full in `!.HOUSE_STDS.md` §H (new section this session) — summarized here:

1. **`prisc+x`'s real `exec` opcode supports exactly one literal argument.** `mr_change_gold.+x` needs two (entity dir, amount) — the naive `exec <op> <arg1> <arg2>` compiled line silently never ran (no error at all). Fixed: `KEY:6` now also generates a real, tiny wrapper shell script (`page_dir/cmd_<id>.sh`, `chmod 755`) with both args baked in; `event.pal`'s own line is just `exec <wrapper>` (zero extra literal args, fully supported).
2. **`${command_list_rows}` was one render behind** on first navigation into a fresh page — a real race between `chtpm_parser_pal`'s own synchronous href-commit reparse and `ez_compose_frame.+x` (a separate process) computing the fresh value slightly later. Fixed: `compose_gallery()` now precomputes **every reachable page's own** `command_list_rows_N` (page-numbered key, not a shared one) at Gallery-compose time — by the time a page is actually clicked into, `gui_state.txt` already has the right value from the last Gallery visit. `write_page_layout()`'s own template references `${command_list_rows_N}` (N baked in per-file), not a generic shared var.

**Also fixed, real but smaller**: `ez_compose_frame.c`'s own `gui_state.txt` write was non-atomic (`fopen(...,"w")` truncate-in-place) — a genuine torn-read race against `chtpm_parser_pal`'s own concurrent reads. Now write-to-`.tmp`-then-`rename()`, matching every other shared state file in this house.

## Where testing was interrupted

Mid-way through re-running the full authored flow (Gallery → Page 1 → Add Command → Change Gold → type `10` → Save) against `m8_redhorned`'s real `event_pkg`, to confirm the wrapper-script-fixed compiler output actually executes via `prisc+x`. **On-disk state right now**: `@.apps/MUCHI_RANCHER/entities/m8_redhorned/event_pkg/pages/page_1/` has a real, single `Change Gold: amount=10` command in both `event.ir.pdl` and `event.pal` (compiled with the wrapper-script fix, entity-dir-not-event_pkg-dir fix, both applied). **Not yet confirmed**: literally running that `event.pal` and checking `inventory.txt` gets `qolq=10` for real. Do that first before building anything further — see MUCHI_RANCHER's own walk-off for the exact commands.

## Known, real, deliberate gaps (not bugs)

- **No "load existing saved command back into the form"** — Save always appends a new command; there's no edit-in-place yet. Real, not accidental.
- **`ez_trigger` field typing tested flaky with the k3 test tool specifically for the underscore character** (`tp_test_send_key.+x` doesn't hold Shift, so `plus`/`underscore` keysyms type the UNSHIFTED glyph on a US layout instead — `=` instead of `+`, `-` instead of `_`). Not a real product bug, a real test-tool limitation — use `underscore`/`plus` keysym names (not literal shell chars) and expect them to need `BackSpace` correction, or accept the dash/equals variant for throwaway test values.
- **`Show Choices` is fully designed, not built** — real branching command list, `objects.pdl`-based popup injected into the monster's own running window via a new `SHOW_PAGE` relay command (not built in `tp_desktop_window.c` yet). See `visual-event-compiler-pal.md` §7 for the confirmed design (all 4 open questions there are answered by direct user response, captured in that doc).

## CPU/process safety — mandatory before any future test here

See `!.HOUSE_STDS.md` §H.5.4 in full. Short version: every `gl_mirror` window this stack opens shares the literal title `"mutaclsym RGB mirror"` — multiple stale sessions are trivially easy to accumulate (a crash, a backgrounded-and-disowned launch whose trap cleanup never fires) and cause real, confusing "wrong navigation" symptoms that are actually just k3 injection hitting the wrong window. Always:
```
ps aux | grep -E "chtpm_parser_pal|gl_mirror|prisc" | grep -v grep   # empty before launch
xwininfo -root -tree 2>/dev/null | grep -c "mutaclsym RGB mirror"   # 0 before, 1 after
```
and `timeout <N>`-wrap every launch. A real crash this session very likely came from accumulated stray processes across repeated test cycles - be disciplined about killing + re-verifying after every single cycle, not just at the end.
