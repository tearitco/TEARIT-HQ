# media-daw-hq — house-spec DAW (HOW2_DAW pass-2)

Source: `103.media-studio/103.daw/`. Page-level `<canvas>` + tabbars +
pchq chrome (`_` `!` `X`). Nested canvas-in-panel is not laid out.

## Works now
- Demo 4 tracks (Drums/Bass/Keys/Lead) with MIDI-ish clip blocks
- Transport: rew / stop / play / rec / cycle / BPM +/-
- Arrangement lanes + playhead + piano roll for the selected track
- Track list (click select, backspace mute), Mixer toggle → fader chips
- File-backed actions only. No renderer verbs. Visual playhead; no WAV/VST yet.

## Test
```
bash 44.xyz.01.00/@.apps/media-daw-hq/button.sh run
dump_frame_png_op.+x 0xWINDOW /tmp/daw.png
```
