# DRIVING — the relay protocol

Everything here was confirmed on Windows 2026-09-29 by driving the real UI and
diffing state. Where something is an assumption rather than a measurement, it
says so.

---

## The two relays

There are **two** input paths and they are not interchangeable.

| | renderer | relay file | line format | launched by |
|---|---|---|---|---|
| **default** | `chtpm_parser_pal` | `pieces/keyboard/history.txt` | `KEY_PRESSED: N` | `orchestrator.c:420-436` |
| **pal mode** | `prisc+x` + `pal/main_loop.pal` | `pieces/apps/player_app/interact_relay.txt` | bare int | not by `run`; by `-Pal` / `sim-key` |

The orchestrator **does not launch `prisc+x`**. If you see a `prisc+x` process
during a plain `button.ps1 run`, it is a leftover from an earlier standalone
test, not part of the run.

### Key code mapping

`KEY_PRESSED: N` where N is the decimal ASCII:

| N | key |
|---|---|
| 32-126 | the literal printable character |
| 8 | Backspace |
| 9 | Tab |
| 13 | Enter |
| 27 | Escape |
| 200 / 201 / 202 / 203 | Up / Down / Left / Right |
| 204 / 205 | PageUp / PageDown |

Arrow keys have no ASCII code, hence the reserved 200+ band.

---

## Default mode (what you almost certainly want)

    # start
    $env:NO_GL = "1"          # headless; omit to open the GL mirror
    .\button.ps1 run

    # read the CURRENT nav numbers - never assume 1..N in order
    Get-Content pieces\display\current_frame.txt

    # send a key
    Add-Content pieces\keyboard\history.txt "KEY_PRESSED: 49"    # '1'
    Add-Content pieces\keyboard\history.txt "KEY_PRESSED: 13"    # Enter

### Multi-digit nav jump

Send each digit separately, **at least 2 seconds apart**, and confirm the
accumulator before committing:

    Add-Content pieces\keyboard\history.txt "KEY_PRESSED: 50"    # '2'
    # frame now shows:  Active [^]: 2
    Add-Content pieces\keyboard\history.txt "KEY_PRESSED: 57"    # '9'
    # frame now shows:  Active [^]: 29
    Add-Content pieces\keyboard\history.txt "KEY_PRESSED: 13"    # Enter -> selects 29

At 1.2 s gaps the `2`,`9`,Enter sequence was silently dropped and the layout
never changed. **2 s works.** Always read `Active [^]:` to confirm where you
are.

The main menu numbers to **34**, and numbering is not sequential across
sections — the `--- Navigation ---` block restarts the visual grouping without
restarting the numbers.

### The request flow

    pieces/keyboard/history.txt          KEY_PRESSED: N     <- agent writes here
      -> chtpm_parser_pal resolves nav/digits/Enter
      -> pieces/apps/player_app/interact_relay.txt
      -> pal/main_loop_chtpm.pal (read_history) calls `wsr_menu_input <n>`
      -> wsr_menu_input dispatches the piece.pdl METHOD row
      -> compose_frame + hit_frame rewrite pieces/display/current_frame.txt

Human keystrokes reach the same place: `keyboard_input.c:209,220` and
`gl_mirror.c:220,221` write to **both** relays on purpose.

---

## Reading state (cheapest and most reliable)

`pieces/display/current_frame.txt` is plain text. Prefer it over decoding a PNG.

    Get-Content pieces\display\current_frame.txt          # what the player sees
    Get-Content pieces\display\current_layout.txt         # which .chtpm is live
    Get-Content projects\wsr-pal\pieces\player_you\state.txt

`renderer.c` only rewrites the frame when the content **changes**, so an
unchanged frame does not mean a dead app. Check the mtime before concluding it
froze:

    (Get-Item pieces\display\current_frame.txt).LastWriteTime

---

## Capture + injection in one step

    .\scripts\k3_frame_capture.ps1 -Topic my-topic -KeyCodes 49,13

Writes the K3 frame/RGB receipts and injects keys. Evidence paths it uses:
`current_frame.txt`, `frame_history.txt`, `rgb_frame.receipt.txt`,
`gl_display.receipt.txt`, `rgb_frame.raw`.

Note its own header lists `pieces/keyboard/history.txt` as the injection
target, which is the **default-mode** relay — correct for a normal `run`.

---

## Presenting the result

House convention, from a direct instruction dated 2026-08-25 ("presentations
being made when we are done of proof each major feature is working"):

    44.xyz.01.00/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/
      home/livedesk/pals/cursword/presentations/make_presentation_video.py

Layout:

    presentations/<feature>/
        snapshots/     01_*.png 02_*.png ...
        manifest.txt   <snapshot> | <seconds_to_hold> | <caption>
        REPRODUCE.md   plain-English steps to redo the test
        presentation.mp4        <- output

Captions are burned onto the frame and spoken via `edge_tts`; hold time is
`max(seconds, narration + 0.6s)` so audio never clips. Needs ffmpeg + Pillow.

`k3_frame_capture.ps1` produces the state side; this script assembles it. For
economy work the honest artifacts are the state-file diffs, not screenshots.

---

## Rules

1. **Append only.** Never truncate a relay file — they are cursor-based and the
   orchestrator truncates them itself on a clean launch (`orchestrator.c:400,402`).
2. **Read nav from a live frame.** Do not hardcode an index. Nav numbering is
   global across concurrently-open khtpm windows per the house skill.
3. **`xdotool` is a last resort.** Relay first, text state read second,
   `dump_rgb_png` third, synthetic mouse physics last.
4. **Assert on state, not on the frame's own claims.** The frame reported
   `Ran: Buy 10 shares` for an op that never ran. That class of bug is fixed
   for the equity rows, but diff `player_you/state.txt` and
   `transactions.txt` rather than trusting a success message.
