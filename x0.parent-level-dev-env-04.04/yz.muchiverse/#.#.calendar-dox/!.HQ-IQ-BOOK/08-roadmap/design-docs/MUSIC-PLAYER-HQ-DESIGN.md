# music-player-hq — iTunes-inspired X11-HQ music player toy

**Status: DESIGN ONLY, not implemented.** Direct request 2026-09-05:
"another toy we want mpe player, with visualizer section (later will
also play music videos, player with its own 'chrome & playheads' but
first lets focus on the music player). populated from dir in .pal list
(and later from file-explorer 'ADD' 2 library like itunes, play lists
etc, very itunes inspired). its x11-hq app ofc".

Real reference implementations on the external drive (legacy, prior
art - NOT code to port line-for-line, but the architecture is exactly
this house's own manager/template split and worth reading):
- `/media/no/b7ced73c-.../home/jbez/Music/0.mu.playr.APP🫕️📻️i15/` -
  `1.mp3_player_cli]+c2.c` (FFmpeg + PulseAudio engine, ~21KB),
  `2.mp3_player_gui]b1]srch]g9.c` (GLUT GUI + search, ~20KB),
  `0.find_mp3s]ALL]fixed]b1.c` (nftw scan), `!.play.sh` (3-binary
  launcher). Has a working FFT visualizer in C.
- `/media/no/b7ced73c-.../home/jbez/Music/mp3gui-69-player-2-tpm_1.0/`
  - `yingyang-cli.c` / `yingyang-gui.c` / `yingyang-find.c` +
    `yingyang-plan.txt` (a clean architecture writeup - see it for the
    full lib list). Same decoupled CLI/GUI design.

## The architecture is already ours

Both legacy apps are: a **backend CLI process** that decodes+plays
audio and writes plain-text state, and a **frontend** that reads that
state and writes commands back. That IS the house's real manager +
static-template convention (taskbar-settings / File Explorer / pdl-read
/ csv-hq all do exactly this). So the port is:

- **Frontend** = a static `music-player-hq-pal.xhtpm` + CSS, rendered
  by the shared `khtpm_core_render.+x` (a `<module>` launches the
  backend). **ASCII / Xft only - NO OpenGL/GLUT** (direct: "only ascii
  for now. we will do gl later in gl-os"). The GLUT visualizer modes
  from the legacy code are a real v2/GL concern, not v1.
- **Backend** = `music_player_manager.c` under `ops/` - genuinely new
  C (audio), but a self-contained manager, not renderer C. Publishes
  `music_player_ui.txt`; polls `music_player_action.txt`
  (seq/cmd, same convention every other manager here uses).

## v1 scope (build this first)

### Library source
- A `.pal`-listed directory (like the legacy `config.txt`'s single
  path line). `music_player_manager.c` reads a library-root path from
  the app's own `library.pdl` (`root=/abs/path` - or a small list of
  roots), then `nftw()`-scans it for audio files (`.mp3` first;
  `.m4a`/`.flac`/`.ogg`/`.wav` are free once the engine is FFmpeg-
  based - see Engine below), skipping AppleDouble `._*` files
  (verbatim from `yingyang-find.c`). Result: an in-memory track list
  the manager publishes.
- **NOT v1**: File Explorer "ADD to library", real iTunes-style
  playlists, per-track metadata (artist/album/art via ID3/FFmpeg
  tags). All real v2 (see below).

### Audio engine — decision needed, two real options

1. **`mpg123 -R` (remote-control mode) as a child process** -
   `mpg123` is already installed. `-R` reads line commands on stdin
   (`LOAD <path>`, `PAUSE`, `JUMP <frames|+Ns>`, `STOP`, `SILENCE`)
   and writes `@F <cur> <left> <cur_s> <left_s>` frame/time lines +
   `@S`/`@P` state lines on stdout. The manager just pipes to/from it.
   **Pro**: v1 playback in a day, no decode code, robust. **Con**:
   `.mp3` only (no `.m4a`/`.flac`), and NO raw sample access → no real
   FFT visualizer (v1 visualizer would be a placeholder / idle
   animation).
2. **FFmpeg decode + PulseAudio out, own thread** (what both legacy
   apps do; all `-dev` libs present). A background thread
   `avformat_open_input` → `avcodec` decode → `swr_convert` to S16 →
   `pa_simple_write`. **Pro**: any format, and you get the raw PCM
   buffer → real FFT for the visualizer. **Con**: real, careful C
   (threading, seek, EOF, format negotiation) - a week, not a day.

**Recommendation**: v1 with `mpg123 -R` (fast, real, deliverable -
matches "first lets focus on the music player"), and treat the FFmpeg
engine as the v2 upgrade that unlocks the real visualizer + more
formats. The manager's OWN command/status file protocol stays
identical across the swap - only its private "how do I actually make
sound" changes.

### `music_player_ui.txt` (published state)
```
state=PLAYING|PAUSED|STOPPED
track_path=/abs/path/to/song.mp3
track_name=song            (basename, no ext, until real tags exist)
track_index=16
n_tracks=512
pos_sec=92.7
dur_sec=230.6
pos_pct=40                 (for a text/ASCII progress bar width)
shuffle=0|1
volume=80
track_0_name=... track_1_name=...   (a display-capped window of the
                                     list, same pattern csv-hq's
                                     cell_R_C / pdl-read's doc_N_title
                                     already use - do NOT publish all
                                     512 every tick)
list_offset=0             (which track index track_0_name maps to, for
                          a scrolling window)
```

### `music_player_action.txt` verbs (seq/cmd)
`PLAY` (resume / start current), `PAUSE`, `STOP`, `NEXT`, `PREV`,
`PLAY_INDEX:<n>` (click a track), `SEEK:<sec>` or `SEEK_PCT:<0-100>`,
`SHUFFLE_TOGGLE`, `VOLUME:<0-100>`, `RESCAN` (re-nftw the library
root). A new `dispatch()` prefix `MUS_*` in `khtpm_core_render.c` -
same shape as `PDL_*` / `CSVH_*` (write the action file; for verbs
that need a live `<cli_io>`/slider value, read it off the tree like
`FE_SAVEAS` / `CSVH_SETCELL` already do).

### Template shape (`music-player-hq-pal.xhtpm`)
- Persistent `<tabbar>` under chrome: `Library` / `Rescan` / (later
  `Playlists`, `Search`). Menu-bar-style action tabs (NO `target_id=`,
  so they don't scope-confine nav - the `<tab>` fix from 2026-09-05).
- `<sidebar>`: the scrolling track list - `<scrolllist>` +
  `<repeat count="${n_tracks_shown}" bind="t">` of `<item id="t${t.#}"
  action="MUS_PLAY_INDEX:${t.#}" label="${t.name}"/>`. The currently-
  playing row gets a data-driven `class="playing"` (manager publishes
  `t_N_playing_class`).
- `<panel>`: the "now playing" area -
  - `<text label="${track_name}"/>`, `<text label="${pos_sec} / ${dur_sec}"/>`
  - a **playhead / scrubber**. The house has no `<slider>` element
    yet. v1: an ASCII progress bar (a `<text>` or a thin `<item>`
    whose width tracks `${pos_pct}`), plus Left/Right = `SEEK ∓5s`
    while the panel row is nav-focused, and clicking somewhere on the
    bar → `MUS_SEEK_PCT` computed from the click x. A real draggable
    `<slider>` (thumb + track + drag via the same pointer-polling the
    text_area mouse-drag work will add) is a shared element worth
    building once - flag it, don't block v1 on it.
  - transport row: `<item action="MUS_PREV" label="|<"/>`,
    `MUS_PLAY`/`MUS_PAUSE` (one toggle button, label from `${state}`),
    `MUS_NEXT` label `">|"`, `MUS_SHUFFLE_TOGGLE` (data-driven active
    class), a `<cli_io id="vol">` or +/- buttons for volume.
  - **visualizer section**: a reserved box (a `<canvas>` element -
    `kh_draw_canvas()` already exists - or, ASCII v1, a `<repeat>` of
    thin `<item>`s whose per-bar height/`bg=` the manager publishes
    from... nothing real in v1 (mpg123 gives no samples), so v1 shows
    an idle placeholder ("visualizer - GL soon") or a cheap
    pos-driven sweep. Real bars land with the FFmpeg engine + an
    `fft.c` writing `player_viz.dat`-style magnitudes the template
    reads.

### toy.pdl
`@.apps/music-player-hq/` with `title | music-player-hq`,
`launch | button.sh` - same as every other toy this session. Scanned
live by the taskbar toys menu (already covers `@.apps/`).

## v2 (after v1 plays songs from a .pal dir)

- **FFmpeg engine** (per Engine option 2) → all formats + raw PCM.
- **Real FFT visualizer**: engine thread runs an FFT on each decoded
  buffer, writes N bar magnitudes to `music_player_viz.dat` (small,
  fast-rewrite, marker-debounced like every other state file here);
  the template's visualizer box reads it. Modes (bars / waveform /
  spectrum / particles) are the legacy GLUT code's real feature -
  ASCII bars first, GL modes when gl-os lands.
- **File Explorer "ADD to library"**: a tab launches the shared File
  Explorer widget; the manager polls its published pick (same
  `poll_file_explorer_pick` pattern pdl-read/text-edit-hq use) and
  appends the picked file/dir to `library.pdl` + rescans.
- **Playlists** (iTunes-style): `playlists/<name>.pdl` (an ordered
  list of track paths); a `Playlists` tab switches which list the
  sidebar shows; drag-to-reorder is the same pointer-polling work.
- **Metadata**: FFmpeg (`av_dict`) or an ID3 read for
  artist/album/title/track#/duration; a real library table (sortable
  columns) instead of a flat path list. Album art → `<canvas>`.
- **Search / filter**: a `<cli_io>` in the tabbar; the manager
  publishes a filtered `track_N_name` window (reactive, same as the
  legacy GUI's search).
- A shared **`<slider>` element** for the scrubber + volume (thumb,
  track, click-to-set, drag-to-scrub) - built once, reused by any app.

## v3 (later, explicitly deferred)

Music **videos** with the player's "own chrome & playheads" - a real
video surface (FFmpeg video decode → a `<canvas>` frame blit, or a
separate GL path), its own transport chrome, fullscreen. Its own
design doc when it's next.

## Open questions to settle before starting

1. Engine: `mpg123 -R` for v1 (recommended) vs. go straight to the
   FFmpeg engine. Speed vs. "visualizer works in v1".
2. Library root config: one path, or a list, in `library.pdl`? (List
   is barely more work and matches "ADD" later.)
3. Visualizer box in v1: idle placeholder, or a cheap non-FFT
   pos-driven animation, or omit the box until v2?
4. Scrubber in v1: ASCII bar + arrow-seek + click-to-seek (no drag),
   OR build the real `<slider>` now (it's wanted for volume too, and
   the text_area mouse-drag work needs the same pointer-polling).
