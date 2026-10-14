# Hotsprings: voice mode for the house (design v2, 2026-10-14)

Updates the 2026-09-28 design in `08-roadmap/design-docs/AI-PUSH-ROADMAP-AND-NUANCES.md` (addendum "hotsprings") and keeps its decisions; what changed: speech out is **offline Kokoro, not edge-tts** (edge-tts has been refused with HTTP 403 since 2026-10-12), speech in is the **one listener** of `LISTENER-STT-DESIGN.md`, and the reserved ideas are now one coherent mechanism. **Not built.** The only built piece is an inert ♨️ button, `<item id="hotsprings" label="♨️" action="void"/>`, in `&.widgits/open-hai/open-hai.xhtpm` (line 67, with the original design in a comment above it).

## 1. What it is
Voice as an input/output **mode**, not a second data model.
- **Input:** a ♨️ toggle swaps the text composer for a "recording" indicator while the owner talks. Sends on an explicit Send button or on a real sustained pause (the VAD hangover, never a fixed timer that could cut someone off mid-thought).
- **Output:** the reply is spoken only (no live text bubble during a voice exchange), through the house voices (`pet_tts_local.py` path: Kokoro, effects, accents, pronunciation map).
- **Data:** every exchange is written to the same timestamped session files a text chat uses (`chat_history.txt` shape, ROBOT-CHAT-BLUEPRINT.md sections 2.1/2.3), so a voice session can always be read as a transcript afterward. Plus the audio-side rows: `LISTEN`, `HEAR`, `SAY` (see the listener and voice docs).
- **Two entry points, both kept:** the ♨️ button (discoverable, mouse) and **Space-to-arm-voice** on any `<cli_io>` (power-user path; Enter keeps arming for text; Space only means "voice" at the single unarmed-to-armed moment, once armed for text it types a space as today). One shared STT pipeline behind both: the second never gets its own recogniser.

## 2. States (one machine, in the manager that owns the chat window)
```
idle --(♨️ press | Space on an unarmed cli_io)--> LISTENING   mic open, indicator on, LISTEN row opened
LISTENING --(VAD: speech then hangover | Send)--> TRANSCRIBING   utterance -> E0 whisper (later E1/E2) -> HEAR row
TRANSCRIBING --(text)--> SENDING   same request path a typed message uses; model call as today
SENDING --(reply text)--> SPEAKING   TTS (Kokoro) wav cached per phrase; playing; mic closed while speaking (no self-hearing)
SPEAKING --(done)--> LISTENING (if still in voice mode) | idle (if toggled off)
any state --(Esc | ♨️ again | 60 s of silence)--> idle   mic closed, LISTEN row closed
```
Failure states shown honestly: `no mic`, `listener not built`, `transcribe failed` (text of the error in the status line, never silent).

## 3. Cursword and the keyboard (the open question, stated plainly)
The desktop "cursword" has an **armed** state that takes a display-wide keyboard grab on purpose (owner asked for stingy focus, 2026-08-30); the owner suspects the armed state itself may be a bug and asked to be reminded (memory `revisit-armed-cursword`; undecided; claude has not touched it). Voice arming must **not** depend on cursword's grab: it uses the cli_io's own arm flow (the unarmed-to-armed second key). I read the docs, not the renderer code, for this; before building: read how `<cli_io>` arming and the second-click convention are implemented in `khtpm_core_render.c` and check that a Space branch at the arm moment does not collide with armed-cursword key routing. A voice session must never leave a keyboard grab behind.

## 4. Rails and safety
- **Explicit arming only; no always-on mic, no wake word in v1.** Indicator visible whenever the mic is open. Recordings stay private (listener doc section 2); `LISTEN` rows are the audit of when the mic was open.
- **Voice commands** (later, E1 spotter) run only inside an armed session; destructive verbs need a confirm step; the command bank is data (`commands.pdl`), edited by candidate edits, promoted by a human other than the proposer.
- **No auto-send of dictated text to a model without the pause or Send** (a mis-heard sentence is cheap to fix before sending, not after).

## 5. Build order and proofs
1. Listener L0-L1 (capture, VAD, E0, `HEAR`). 2. **Hotsprings v0 inside voice-fx first** (a "listen" button, file mode for tests): proves capture-to-text without touching open-hai. 3. open-hai ♨️ wired: replace the `void` action with the request verb; relay case: file-mode utterance -> transcript in the chat history -> reply spoken (cached wav exists, `SAY` row) -> **negative case: a silent file sends nothing**. 4. Space-to-arm-voice after the cursword/cli_io check in section 3.
Proof requires: a screenshot of the LISTENING indicator, the `HEAR`/`SAY`/`LISTEN` rows, and a real spoken test by the owner (the relay cannot speak): record what was heard vs measured.

## 6. Open questions
(1) Is the 60 s idle timeout right? (2) Voice for every cli_io, or opt-in per field (`voice="1"`)? Numeric and filename fields should not take dictation. (3) Reply voice: one house voice, or the voice of whichever entity (pet) the chat belongs to? (4) Barge-in: may the owner interrupt a spoken reply by speaking? (v1 says no: mic closed while speaking.) (5) Languages.
