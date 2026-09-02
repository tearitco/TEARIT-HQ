# Muchi DAW — Phase 1 pass 2 (test card)

## What changed vs pass 1 (critical)

Pass 1 looked like a MIDI toy, not Logic/GB/Pro Tools:

| Miss | Pass 2 |
|---|---|
| Piano roll = whole UI | **Tracks/Arrangement on top**; **piano roll editor underneath** |
| Tracks = vertical list only | **Horizontal lanes** + colored **MIDI region** blocks |
| No add track | **`+ Track` button** and **`=` key** |
| Plugins ate the right third forever | **Mixer/inserts drawer** toggle **`B`** (bottom strip) |
| Weak transport | **|<  Stop  Play  Rec  Cycle** + LCD bars.beats.ticks + BPM |
| No ruler | **Bar ruler** with beat ticks + cycle highlight |
| No track chrome | **Color chip, icon, M/S/R, mini fader** per header |

## Launch
```bash
cd 103.media-studio/103.daw
sh button.sh r
```
Stop: Esc or `sh button.sh kill`

## Pass 3 — File menu + left channel strip
- **File** (top-left): New / Open-Load / Save / Save As / Quit  
  Shortcuts: **Ctrl+N**, **Ctrl+O**, **Ctrl+S** (or **s** save)
- **Left CHANNEL strip** always shows the **active track**: M/S/R, volume fader (click), pan, EQ (on + low/high bars), Reverb, Distortion inserts

## Try
1. **Space** — play; playhead across arrangement + piano roll.  
2. Click **File → Save**, then change notes, **File → Open / Load** to restore.  
3. Select track 2 — left strip switches to Bass (fader / EQ / FX).  
4. **=** or **+ Track** — new lane.  
5. **a–l / wetyu** — musical typing.  
6. **R** record · **B** bottom mixer · **s** save.

## Still MVP (honest)
Software synth only; no audio clips/WAV; no automation lanes; no real VST; fixed window size; no CHTPM shell yet.
