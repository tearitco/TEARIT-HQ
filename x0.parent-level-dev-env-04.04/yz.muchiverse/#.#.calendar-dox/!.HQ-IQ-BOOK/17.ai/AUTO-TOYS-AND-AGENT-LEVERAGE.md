# How `auto` drives this week's toys, and how it multiplies agents like me (2026-10-14)

Reads on top of `02-AUTO-SYSTEM-DESIGN.md`. Everything here is a plan; where I say a thing exists I name the file, and where I have not examined something I say so.

## 1. The toys under `auto` (concrete loops, with the judge that keeps each honest)

### sfx-hai (cleanest first target: has a numeric judge)
- **Loop**: pick a recipe seed, `make`, read `sfx_make` checks (duration, peak, dominant frequency, DC), keep passes, vary one parameter (frequency, decay, sweep), repeat. A sub records which edits turned FAIL into PASS.
- **Unattended output**: a bank of N validated sound variants per theme (pickup, hit, menu click, alarm), each with its recipe row, for games and pets. Not "good-sounding" (nobody has heard them): **valid and distinct**. A distinctness check can be added numerically (compare dominant frequency and envelope), then the owner listens to a shortlist.
- **Owner gate**: choosing which sound ships (accept/promote).

### voice-fx (+ listener) — the closed loop that makes voice work measurable
- **Round trip (listener L4, planned)**: voice-fx `say` with knobs K, listener `hear` the resulting wav (file mode, no microphone), `wer` against the text. **Objective: keep intelligibility (WER) low while a knob (pitch/formant/accent) moves the voice away from the base.** `auto` runs a search over knobs overnight on debil/Mac and logs WER per setting. This is the pronunciation-bot idea made automatic: it finds words the engine mispronounces (house words: khtpm, tomom, brixel, colab), proposes `pron.pdl` rows, and re-tests them (S2 `pron_check` validates the rows; the map is empty on purpose until a person approves entries).
- **Hot-word learning loop**: every house word that `hear` gets wrong is a candidate for the hot-word prompt (measured today: 4.4% to 1.1% WER, leakage caveat). `auto` can maintain the hot-word list from `FIXED`/failed-word rows, evaluate on **held-out** sentences, and promote only on a held-out win.
- **Hard rails**: `auto` can run `listen_capture --file` (no mic); **arming the real microphone is owner-only, always** (rail 2). Recording consent questions from `LISTENER-STT-DESIGN.md` are still open.
- **Owner gate**: "does this voice sound right / like a different person" (ears), rename and ship of a voice.

### img-hai — a pipeline that is already half-`auto`
- It already has an unattended **batch queue** that both remote hosts pull from, two-judge verdicts, and candidate-edit bank tuning with overlay trial (`tune-1`: yes-rate 1/12 to 4/11, not promoted). `auto` adds the conductor: refill the queue when it empties (host load permitting), run `img_pixel` modes per part from `pixel_modes.pdl`, contact-sheet the results, **queue them for the owner's verdict**, and re-run tuning when verdicts arrive.
- **Owner gate (hard)**: Accept. The two-judge design exists because claude's yes is cheap; the owner's verdict is the label that matters. Surfacing the verdict queue (in `auto-hq`) so the owner can clear 30 verdicts in a minute is the most valuable `auto` feature for this toy.

### pets and games (tcg-rpgce, civ-rpgce, rpg-pet, dsr-rpg, ...)
- Pets are users/wallets in the design (pet-chain economy, ecosystem soul): `auto` per pet = a pet that acts on its own relay, the first honest version of "pets drive the relay". Start with a pet speaking a line on an event (Kokoro, offline) and a game-window smoke case run by the conductor as a nightly regression, not full behaviour.
- Grok's new boards have no relay smoke cases yet; writing one per board is the cheapest way to get them under `auto` and under regression.

## 2. What this gives agents like me (leverage, concretely)
1. **Eyes, hands, judges, memory as a service.** Today each agent re-derives how to find a window, press a button and tell if it worked. `auto` standardizes it: manifest = hands and expectations, `auto_observe` = eyes, harness checks = judges, ledger = memory. A new agent writes a manifest instead of improvising `xdotool` (the house's own k9 doc names that as the failure mode).
2. **A night shift.** Run every relay smoke case on debil/Mac overnight (sfx-hai 15/15, voice-fx 35/35 today, more as added). An agent starts the day with a **verified-green/red map of the house** instead of discovering regressions by accident. The 2026-10-14 reset+merge is exactly the kind of event after which this map is gold.
3. **Intents instead of pokes.** An agent files an intent ("make 20 validated pickup sounds", "retune the hot-word list on held-out set") into a queue; `auto` executes within budgets and gates; the agent reads the ledger. The agent's context is no longer the bottleneck: the work continues between sessions, which also answers the owner's point that uncommitted/unfinished agent work gets lost.
4. **Cheap workers under expensive supervisors.** T0/T1 do the mechanical 95%; a stronger model (or the owner) only sees the rows marked uncertain. This is the delegation flywheel (spec + locked harness + cheap workers) with a real runtime.
5. **Self-checking changes.** Every feature an agent ships carries its relay case; `auto` re-runs it after anyone's merge. A grok-type merge touching `khtpm_draw_core.c` then triggers the whole map automatically. (Today this was verified by hand: tree diff + string check, no on-screen check.)
6. **A standing ledger of what works.** Because every action is `ACT | ... | PASS/FAIL`, "never report a number you did not observe" becomes a property of the system: agents quote ledger rows.

## 3. Out-of-the-box ideas (each marked with what must be true first)
1. **The house as a training gym.** Each window is an environment: observation = state text/screenshot, actions = manifest verbs, reward = harness score. Cheap RL/bandit over verb choices needs no GPU (tabular or small tables in pal/C); the ledger is the replay buffer. Needs: A1-A3 working, rewards that cannot be gamed (negative-test every judge).
2. **Free demonstrations from the owner's own use.** The relay file records real human input too (`KEY_PRESSED`, `MOUSE_EVENT` lines per window pid; verified by reading `relay_drive.c` header and house standards doc, not tested in this session). Observing it offers macro proposals ("you did these 6 clicks 5 times"). **Requires explicit owner consent and a visible indicator**; proposals only, never actions (slice A8).
3. **Cost-aware cascade with decision caching** (section 5 of file 02): most steps repeat; hash the observation, reuse validated decisions; measure the fraction answered by cache/T0 vs LLM to prove the saving.
4. **Studio as a producer, scheduling by load.** The weak local machine is the constraint: the studio reads load average, keeps local work to coordination, and pushes drafts, STT and renders to debil/Mac (images and listener transcripts are already shaped for that). Idle hours become output.
5. **Voice as a control surface (hotsprings, later).** Listener L3 (command spotter over a small fixed vocabulary) + `auto` verbs: "stop" (always allowed; it only stops things), "play", "next". Only a closed vocabulary, only after the owner supplies their own clips and arms the microphone; pets reacting to speech is a separate question the owner has not answered.
6. **Cross-modal self-tests.** Generate speech (Kokoro), transcribe it (whisper), score it (`wer`): the whole audio chain checks itself without a person; the same idea for images (draft, judge, pixelise, compare palette) and sound (render, FFT, check).
7. **Auditable autonomy.** The `auto-hq` ledger tail plus a daily "what auto did" digest (rows grouped by window) written to the BOOK calendar folder. Trust grows from evidence the owner can skim.
8. **Self-healing within rails.** A watchdog verb "window X dead (pid file stale) -> relaunch via its own `button.sh run`" for windows the studio itself started; never touching windows it did not start and never the owner's positions. (Today the stale `renderer.pid` after the reset was noticed only by hand.)
9. **Irl hooks (later, owner-led).** The house has `phone_send_op` (a shared op for sending to phones; **I have not read it in this session**, check what it really does before promising anything) and an offline voice, so a digest or alarm could reach the owner. Anything that leaves the house (messages, purchases) is `owner_gate`.
10. **Agent-to-agent handshake.** Grok, kilo and claude share one checkout; a lock/lane file in the ledger ("window X is being driven by agent Y until Z") removes the class of collisions in AGENTS.md (shared index sweeps, shared `reset`).

## 4. What would make me (and the next agent) most effective on day one
- A manifest + relay case for **every** window we ship (rule: no new window without them; cost is an hour).
- The overnight green/red map (idea 2.2): needs only a loop over existing case files, no LLM, on debil/Mac. **If you do one thing before the full `auto`, do this: it is A1-A3 minus the brains and it already pays for itself.**
- The owner's verdict queue in one window: the scarcest resource is the owner's attention and ears; spend automation on making their yes/no cheap.
- Persist the missing state: img-hai canvas layout, voice-fx per-voice state is saved (`voices2.pdl`), others not checked.

## 5. Risks, stated plainly
- A 270M model as chooser will be wrong often; the design survives that (enumerated verbs, validity logging, T0 first) but do not oversell T1.
- The relay is not real hardware input; focus/grab bugs hide behind it.
- Shared checkout: a conductor writing to relay files or ledgers must never touch another agent's files or the shared git index.
- Subjective quality (looks right, sounds right) cannot be automated away; the verdict queue is the honest answer, not a model judging a model.
- Nothing in files 02/03 is built. The first deliverable is A0, which should take about a session and proves the off-switch before anything acts.
