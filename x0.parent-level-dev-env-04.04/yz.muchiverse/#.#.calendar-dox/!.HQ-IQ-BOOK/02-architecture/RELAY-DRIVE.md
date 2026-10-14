# relay_drive: driving an x11-hq window like a person (2026-10-13)

Op: `&.widgits/_shared-lib/ops/relay_drive.c` -> `ops/+x/relay_drive.+x`. It writes the per-process input relay (`#.desktop/entity_menu_history/<pid>.txt`) that every `khtpm_core_render` window already polls (see the khtpm standards skill). Case files are `.pdl` rows, so a test is data. First user: img-hai (`&.hq-apps/img-hai/tests/`).

```
relay_drive <pid> key <enter|esc|bs|tab|up|down|left|right|pgup|pgdn|space|one char>
relay_drive <pid> type <text>            one key per character, 70 ms apart
relay_drive <pid> click <x> <y>          press + release (window pixels)
relay_drive <pid> tap <x> <y>            two clicks: focus, then activate
relay_drive findwin <w> <h>              the one unnamed X window of that size (khtpm windows have no name and no _NET_WM_PID)
relay_drive shot <winid> <out.png>       dump_frame_png_op on that window
relay_drive <pid> run <cases.pdl> [winid]
```
Case rows: `STEP | key|type|click|tap|sleep|shot|expect`, e.g. `STEP | expect | <file> | contains|absent | <text> | <timeout_s>`. Paths are relative to the house root. Exit 0 = all pass, 1 = a FAIL, 2 = cannot run. Verified once: `tests/img-hai-negative.pdl` (expects a state that never appears) exits 1 with FAIL lines.

## What I learned about the relay (observed on img-hai)
- Keys are **decimal codes**: `KEY_PRESSED: 112` is `p`; the literal line `KEY_PRESSED: p` is silently ignored.
- The house is **two-step click**: the first click on a row only focuses it, the second activates. One relayed click did nothing; two placed the tile. `tap` does both.
- An **armed cli_io swallows every key**, including `p`. Esc disarms. Typing digits then Enter jumps to the row with that number and activates it (`type 3`, `key enter`).
- **Prefer number-jump over pixel clicks**: a prompt line that wrapped to two lines moved "Draft" 30 px and a pixel click at the old spot hit "next model" instead. Read the numbers from a fresh screenshot; they also shift (placing a tile adds a numbered row; the tile list's scroll chips and the words field also take numbers: I assumed rotate was 29, the screenshot showed 29 was "move left", rotate was 33).
- Click coordinates are window-relative, the same as a screenshot of that window.
- A screenshot taken right after a state change can show the previous frame; check the state file first, then the picture.

## Not verified
Mouse wheel, drag, right click, modifier keys, and windows that are not khtpm windows. The driver is a C op with case data; it is not yet wrapped as a pal harness case.
