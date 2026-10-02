# wsr-pal — docs

Working documentation for `WSR_PAL-PREFERED/`. Read these before touching the
tree; each one exists because something in it cost real time to learn.

| Doc | What it is |
|---|---|
| [DRIVING.md](DRIVING.md) | **Start here.** How to drive the game from the relay, per mode. The two-relay trap. |
| [PROGRESS.md](PROGRESS.md) | Dated log of verified work, with the evidence for each claim. |
| [ROADMAP.md](ROADMAP.md) | What is done, what is next, in dependency order. |
| [KNOWN-ISSUES.md](KNOWN-ISSUES.md) | Open defects, with file:line and how each was confirmed. |

The top-level `../WSR_PAL-README.md` (one level up in `yz.muchiverse/`) is the
map across all three trees — preferred, deprecated, and the pristine original.
This `docs/` is the detail for *this* tree only.

## House rules that apply here

- **Linux is canonical; Windows is only wrapped.** Never refactor POSIX logic
  "clean" — add a narrow `#ifdef _WIN32` branch and leave the Linux path
  byte-identical. Every Windows fix in `PROGRESS.md` did exactly this.
- **Relay first, `xdotool` last.** Agent input goes through the same relay a
  human's keystrokes use. See `DRIVING.md`.
- **The house skill `khtpm-house-standards` is mandatory reading** before
  touching anything `.chtpm`, manager/renderer, or taskbar-launch related. It
  also warns that nav numbers are *global across concurrently-open khtpm
  windows* and must be read out of a live frame, never hardcoded.
- **`.chtpm` layouts live at `pieces/chtpm/layouts/`**, but the **live
  `piece.pdl` is under `projects/wsr-pal/pieces/<piece_id>/`**. A stale copy
  also exists at `pieces/<piece_id>/piece.pdl` and it disagrees. Always confirm
  which one the code reads before concluding something is unwired.

## Two directories, one project — this is the single most dangerous trap

| Path | Status |
|---|---|
| `WSR_PAL-PREFERED/` | working tree, all work lands here |
| `MSR-DEPRACATED/` | legacy k32, reference only |
| `44.xyz.01.00/014.wsr-pal💸️📌️+2/` | pristine original, the revert target |

Within *this* tree there is a second, older copy of several files
(`pieces/*.pdl`, `pieces/*/state.txt`) that the live code does **not** read.
When a file "exists but is ignored", check the path the code actually opens.
