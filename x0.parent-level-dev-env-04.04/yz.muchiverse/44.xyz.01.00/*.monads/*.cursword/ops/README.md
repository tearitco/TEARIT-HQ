# cursword/ops — the onboarding FSM

`cursword_fsm.c` → `+x/cursword_fsm.+x <house_root> [--auto]`

## What it does (v1, 2026-09-03)

For a **guest** (no `00.login-signup/current_login.txt`), on launch:

1. Narrates a greeting to `entities/cursword/say_log.txt`.
2. Drives the real taskbar the same way the test harnesses do —
   appends `4002` to `#.desktop/strip_history.txt` (open the USER
   cell), then decimal key codes to `#.desktop/livedesk_agent_relay.txt`
   (Enter to activate "New User…", Enter to arm the text field).
3. **Walk-to-the-door**: stops there and narrates *"type a username…"*.
   The human types their own username + display name. The FSM watches
   `#.desktop/strip_state.txt` (`cliio_op`: `new-user-id` →
   `new-user-name`) and narrates each transition, then watches
   `current_login.txt` for the finished account and points at avatar
   creation.
4. `--auto` instead types throwaway `guest_<ts>` / "New Player"
   credentials itself (hands-off demo / store test).

Exits immediately (state `IDLE`) if someone is already signed in.

## States

`OFFER → OPEN_USER → NEW_USER → ARM_TYPING → WATCH_ID → WATCH_NAME →
WATCH_DONE → DONE → IDLE`  (or `ERROR` if the taskbar can't be
reached). Current state is mirrored to `entities/cursword/cursword_fsm.state`;
a trace goes to `cursword_fsm.log`.

## The three future hooks (stubbed, fallback already wired)

| function | v1 | later |
|---|---|---|
| `cursword_say(intent, fallback)` | writes `fallback` to `say_log.txt` + `tts_queue.txt` | build a prompt from `intent`, call the local **gemma** harness (`POST /api/chat`, no schema), use its plain-text reply if non-empty within a short timeout, else `fallback` — the Harnecient pattern. Call sites never change. |
| `cursword_tts(line)` | appends to `tts_queue.txt` | piper / espeak speaks new lines |
| `cursword_stt_poll(out)` | reads+clears `entities/cursword/stt_in.txt` (lets a human/test simulate a spoken reply) | real speech-to-text |

## Test

`sh run_cursword_fsm.sh [house_root] [--auto]` — builds, clears
`say_log.txt`, launches. Watch `say_log.txt` and `cursword_fsm.state`.
Verified end to end 2026-09-03 against a fresh (guest) install: menu
opened, field armed, narrated hand-off, saw both cli_io stages,
account created (`users/tester1/`), narrated welcome, exited clean.

## Not yet wired

- The manager does not launch this yet — run it by hand /
  `run_cursword_fsm.sh` for now. Manager auto-launch (next to
  `livedesk_ensure_cursword()`) is a small additive follow-up.
- The signup cli_io itself renders as a cramped one-line field in the
  top strip — a real UX gap, tracked for the settings/polish pass
  (`FORWARD-ROADMAP §4`, `CURSWORD-SOUL-VISION §4`). The FSM drives it
  correctly regardless of how it looks.
