# media-vid-hq — conversion skeleton (2026-09-08)

## Source
`44.xyz.01.00/103.media-studio/103.vid-edit/` — read `HOW2_VIDEO.md`
(iMovie-shaped).

## Target layout (house spec — build in `media-vid-hq.xhtpm`)
- **File** menu row
- **Transport** + timecode LCD ; `Space` = play
- **Preview** (`<canvas>`, center) + **Inspector** (right)
- **Timeline** below: `V1 / V2 / A1 / A2` lanes, colored clip blocks,
  red playhead
- Demo project: 3 colored clips on V1/V2

## Skeleton status
Compiles, launches, shows in HQ toys, round-trips one action. Real
layout + playback/mux engine: TODO(grok). Keep every `HOW2_VIDEO.md`
feature. Retire `103.media-studio/103.vid-edit/` (leave a pointer) at
parity.
