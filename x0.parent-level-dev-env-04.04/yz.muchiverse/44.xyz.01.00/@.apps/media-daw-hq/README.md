# media-daw-hq — conversion skeleton (2026-09-08)

## Source
`44.xyz.01.00/103.media-studio/103.daw/` — read `HOW2_DAW.md` (it has a
"pass 1 vs pass 2" table — pass 2 is the target: looks like Logic/
GarageBand, not a MIDI toy).

## Target layout (house spec — build in `media-daw-hq.xhtpm`)
- **Transport**: `|<  Stop  Play  Rec  Cycle` + LCD `bars.beats.ticks` + BPM
- **Bar ruler** with beat ticks + cycle highlight
- **Arrangement** (top): horizontal track lanes + colored MIDI region blocks
- **Track headers**: color chip, icon, M/S/R, mini fader ; `+ Track` button (`=` key)
- **Piano roll editor** underneath the arrangement
- **Mixer/inserts drawer** toggled by `B` (bottom strip)

## Skeleton status
Compiles, launches, shows in HQ toys, round-trips one action. Real
layout + audio engine: TODO(grok). Migration, not redesign — keep
every `HOW2_DAW.md` feature. Retire `103.media-studio/103.daw/` (leave
a pointer) at parity.
