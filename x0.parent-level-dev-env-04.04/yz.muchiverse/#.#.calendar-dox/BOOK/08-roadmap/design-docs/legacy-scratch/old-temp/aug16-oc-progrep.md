# aug16-oc-progrep — opencode session progress report (2026-08-16)

## Goal
Complete and live-verify the per-app "message posted" notification tone
(open-hai + chat-hai) with a GUI toggle. Both apps now built, relaunched,
and tone-tested end-to-end.

## What was verified live this session
- **open-hai incoming-message tone**: Settings submenu (topbar "18
  Settings", Sound row). Toggle off→on and on→off via relay (Enter on the
  Sound row) — `state/settings.pdl` flips, label shows `Sound: on/off`,
  Escape closes the panel (n_nav 23→22). Tone: sound ON → `canberra-gtk-play`
  PID caught while the AI reply lands; sound OFF → zero hits while the
  reply still posts.
- **chat-hai incoming-message tone**: `chat_hai_loop.sh` `ledger_msg()`
  plays a tone for non-system (persona) messages. Sound ON → the loop
  process spawned `canberra-gtk-play` (parent PID = the loop); sound OFF →
  zero tone hits while persona messages still post.
- **chat-hai Sound button driven by real GUI clicks** (new reusable
  `pieces/audit/xclick` C helper, XSendEvent button-1 at window-relative
  coords): clicked the button — pdl `sound_on` 1→0 then 0→1, and the
  frame history now carries a `sound=N` field for scriptable checks.

## Bug found and fixed mid-session
- **Wrong house-root launch silently degrades chat-hai.** Both hq renderers
  take the APP DIR (`44.xyz.01.00`) as argv[1], NOT the yz.muchiverse
  house root. Wrong root → no config/ledger/`#.desktop/hq_ui.pdl`
  (`font_scale` missing → window shrinks 350→280px) and the `<module>`
  loop fails to spawn. The livedesk taskbar already passes the app dir;
  relaunched manually with the correct arg, verified window 350x1509 +
  self-spawned loop.
- Reconfirmed the `pgrep -f`/`pkill -f` self-match trap (a pattern in your
  own command string kills the command) — use the `[c]` bracket trick.

## State
- open-hai: render + manager running, `sound_on=1`.
- chat-hai: render + loop running, `sound_on=1`, frame history `sound=1`.
- Tone = incoming messages only (AI reply / persona message); the user's
  own sent messages are silent. Sound = `canberra-gtk-play --id=message`
  with a `play -n synth` fallback — zero audio files.
- Docs updated: `local-2do-15.txt`, `chat-hai-design.md`. Test logs and
  the `ai-cell-backup-pre-open-hai-rename` backup removed.

## Open item / next step
- Move chat-hai's Sound control out of the bottom control row into a
  Settings panel at the top of the window (requested right after this
  report), mirroring open-hai's topbar Settings submenu shape.
