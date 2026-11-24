# hai: results versus desire (stakeholder report, 2026-11-23)

For the harnecient stakeholders. The narrated deck (English voice, Japanese subtitles, narrator face) is in the owner's XO folder `31.Nov/nov-22/13.hai-results-vs-desire-stakeholder-deck-en-voice-ja-subs/` (media stays out of the repo, per the presentations standard). The running record behind every number is the lab notebook `XO/%.HARNECIENT/%.calendar/nov-22/4.lab-notebook-router-and-training-bank-2026-11-22.md`; the facts that live in the house are in `17.ai/META-STRATEGY-ATTRITION-HARNESS-TOOL-USE-AND-CHEAP-BUILDING-2026-11-22.md` sections 8b to 8d.

Labels: **RAN** = run and observed in this session. **ESTIMATE** = not measured. **GAP** = wanted, not built. **DOC** = written down, not proven.

## 1. The desire (the owner's words, 2026-11-23)
"hey, how are u? what should i call u? do u like this "house" <- keyword, have u tried "driving it", whats ur "confidence" <- see all these keywords? they should do something, even trigger a sub convo or side window asking me to explain; also there should be something i can use to indicate "words more deserving of attention" while we get strapping." Earlier asks in the same line: a chat that learns phrases, concepts and tool calls, modifies its own weights, runs its own attention-like step, and is a useful, versatile tool that saves tokens over time. Update the same day: it does not have to be a second window; inline is fine.

## 2. What the owner's own example does today (RAN, in the real hai-cli window, photographed)
| You type | hai answers | Confidence | Why |
|---|---|---|---|
| hey , how are u ? | the greeting reply `r_greet` | 21% | matched the word "hey" |
| what should i call u ? | the help reply `r_help` | 15% | matched "what" |
| do u like this house | "I don't know that yet. Did you mean ..." | 0% | no known word |
| have u tried driving it | same fallback | 0% | no known word |
| whats ur confidence | same fallback | 0% | no known word |
hai knows 5 concepts and about 25 keywords. "house", "driving" and "confidence" are not among them, and the fallback does not ask what they mean. That is the gap the owner described.

## 3. Results versus desire
| Desire | Result today | Evidence |
|---|---|---|
| Key words are recognized | PARTIAL: 5 concepts, about 25 words, whole-word matching | `chat/data/concepts.pdl` |
| A key word triggers a sub-conversation asking me to explain | **BUILT 2026-11-23, inline in the same chat**: an unknown content word is asked about (`I do not know "house" yet. What does it mean to you?`), the typed answer is stored as a ledger row and quoted back later | case `hai_teach` 42 checks; relay injection into the real window, photographed |
| I can mark words that deserve more attention | **BUILT**: `/mark word`: asked about first, counts double (confidence capped at 100) | `hai_teach`; photographed: 50% to 83% (graded) to 100% (marked) |
| Explanations are remembered and change later behavior | **BUILT**: `/teach`, `/alias`, `/say`, `/forget`, `/pack`, the automatic ask and `/grade`: every lever is a ledger row, replay rebuilds it, a stray file teaches nothing | `hai_teach` (8 mutants red), `hai_chat` 17, `hai_learn` 16, `hai_send` 13 |
| It learns phrases, concepts, tool calls, its own weights | PARTIAL: it adjusts concept weights from grades and shows a confidence per reply; replies are fixed and never rewritten (by design); phrases, concepts and tool calls are not learned | `hai_confidence` 11, `hai_calibration` 6 |
| Confidence is honest | PARTIAL: confidence is shown and a calibration check exists; with 2 grades in a real session it says nothing yet (ECE 0.585 on 2 grades) | `hai_chat calib` |
| It is a usable window | BUILT: opens from the H-AI menu (`hai-cli`, row 12), you type inside it, `/grade N` grades; tested by relay injection (16 of 16) | hai book 21, `window/tests/` |
| A useful tool that saves tokens | OPEN: a separate internal router for the agent's repeated jobs exists; the saving is an ESTIMATE (ratio 0.29, range 0.18 to 0.55) and real token use is not measured | META-STRATEGY 8b |

## 4. What the lab measured (RAN; these numbers are about the TASK ROUTER, a sibling of hai that uses the same whole-word matching idea; they are not measurements of the chat)
- Pre-registered hypotheses, one small experiment each, two teacher models (llama3 8B on debil, qwen2.5-coder 7B on the Mac), 286 teacher-written phrasings.
- **Wording is the wall.** The router is right on 90% of 40 human-written goals and on about 53% of teacher-written phrasings. Fixes tuned on one teacher gained 15 to 18 points on that teacher's own phrasing and 1 to 4 points on a different teacher's: it memorizes wording.
- **Confidence can be made honest (CONFIRMED on fresh data).** A softmax top-probability at temperature 0.12 was calibrated on two datasets independently (error 0.05 to 0.08) and on a third, never fitted (0.072); items rated 0.8 or above were 90 to 95% right. A candidate for hai too; it needs an owner yes.
- **Not usable (falsified or dropped):** mined synonym rows (precision falls), stemming (+1.7 points), a 20-way model judge (about 50% right on known labels), a yes/no model judge (80%, two judges both saying yes 81% with 82% precision: triage only, never an accept gate).
- **Confirmed but confounded:** adding plain-English descriptions as extra matching text (+6.6 points) mostly echoes the prompt the teachers were given.
- **Nothing from the lab is in the tracked banks.**

## 5. What the results mean for the desire
The sub-conversation is not a side feature; it is the missing data source. The lab found that independent, human-originated phrasing is the bottleneck, and that more model-written text does not carry across wording. A hai that asks "what do you mean by house?" and stores your answer collects exactly that, inside the ledger, with the harness able to test it.

## 6. Plan: sprint 4, "teach loop v0" (PROPOSAL, hypothesis written before building)
1. `/mark <word> [high|normal]` stores an attention row in the ledger.
2. A marked word, or any input with no match at all, moves hai to a state that asks "What do you mean by <word>?" inline (no second window).
3. The typed answer is stored as a definition row (word, explanation, time) and the word becomes a keyword of a new learned concept whose reply quotes the definition; replay from the ledger rebuilds all of it.
4. Confidence uses the attention weight.
5. Tests: a relay-injection script typing into the real window, and an engine case with C and JS parity.
**H12:** after teaching N words, rephrased uses of the taught words are recognized (top concept right; the calibration bin of 80% or more is right at least 80% of the time) and untaught words still fall back. Falsified otherwise, and the result is recorded either way.

## 7. What we need from stakeholders
1. About 20 sentences in the owner's or an engineer's own words (five minutes): "things you would actually say to get one of these jobs done". This is the decision-grade test data.
2. A yes or no on adopting the calibrated confidence (softmax top-probability, T = 0.12, gate 0.80) in place of the current gap rule.
3. Token usage per session, or a hook that exposes it: today every saving is an ESTIMATE.
4. Which words to teach first (the owner named house, driving it, confidence).

## 8. Limits, said plainly
Nothing in the chat learns from explanation yet. The numbers in section 4 are about the router, not the chat. The Japanese subtitles in the deck are mine and were not reviewed by a native speaker. The 0.45 fire margin, the calibration temperature and the prior are fitted to small data. One hidden-window incident: my hai-cli window had been hidden (unmapped), so it was relaunched with its launcher before the photographs.

## 9. Where things are
Window and tests: hai repo `window/`, `book/21-hai-cli-in-the-h-ai-menu.md`; menu row and launcher: house `#.desktop/livedesk_taskbar.pdl`, `#.desktop/livedesk_launchers.pdl`, `&.hq-apps/hai-cli/`; lab data and scripts: `/home/no/staging/lab-router-2026-11-22/` (outside the repo); engineer pipeline guide: XO `%.HARNECIENT/%.calendar/nov-22/3.pipeline-for-engineers-...md`.

## 11. Update 2026-11-23 (later): the gaps above were built the same day
The owner asked for the chat above the input, for the chat to learn ("otherwise it's a dud"), and for many different levers. All RAN, in the real window: the transcript shows hai's reply text above the typing box; unknown words are asked about; seven teaching levers (ask, `/teach`, `/alias`, `/say`, `/mark`, `/forget`, `/pack`) plus `/grade`, each one a ledger row; a starter pack that taught the owner's three words (house, driving, confidence) in one command; every turn writes a `SAID` row of what hai said. Details and the lever table: hai repo `book/22-teaching-hai-the-levers-2026-11-23.md`. Three narrated mp4s were recorded (stakeholder report and two tutorials; XO `31.Nov/nov-22/13.*` to `15.*` and `%.HARNECIENT/hai-user-dox/hai-videos-2026-11-23/`).
Measured (lab H12): after the starter pack, 20 of 20 differently worded sentences using a taught word were answered from it and 0 of 20 unrelated sentences were; but only 1 of 8 inflected forms (houses, driven, harnesses ...) were recognised: recognition is whole-word matching, the same lexical wall as the router. Confidence for a taught word is 50% when fresh, 83% after a grade of 4, 100% when marked.
Still open: inflections; the web engine has no teach loop; teach logic is C, not event pages (D92 stopgap); low-confidence replies are asserted not asked; tool-call lever `/act` not built; the 20 owner sentences are still the decision-grade test.

## 12. Update 2026-11-23 (night): agents, sessions, personalities, colours
The owner's end goal (different pets/agents with their own sessions, corpuses and personalities) is now real in the hai-cli window, RAN and photographed: an **agent is a self-contained folder** (its own corpus, voice, rules, packs); each agent has **its own sessions** (the left sidebar lists Agents, then the active agent's Sessions, like `chat-hai`); a **personality is a data preset** applied once at birth (`curious`, `terse`, `scholar`); a new session starts empty but **inherits what the person taught**; the chat is coloured (you blue, hai green, levers muted). Case `hai_agents` 33 checks (7 mutants red), every other hai case unchanged. Details, the bugs the real window found (a double-fired click truncated a session ledger) and the **design, not built,** for binding an agent to a house pet by entity uid: hai repo `book/23-agents-sessions-and-personalities-2026-11-23.md`. Tutorial videos: 1 teach it your words, 2 more levers and starter packs, 3 agents, sessions and personalities (XO `31.Nov/nov-22/14.*` to `16.*` and `%.HARNECIENT/hai-user-dox/hai-videos-2026-11-23/`).
Open: entity binding; shared knowledge across sessions (today a new session inherits a snapshot); lever confirmations are not in the agent's voice; inflections; the web engine.

## 13. Update 2026-11-23 (late): layout like rpg-edit, the Bank, "why", and the answer about a studio
- **Layout (RAN, photographed):** three columns, authored as a flex page like `canvas-craft`: Sessions on the left, the chat or the **Bank** in the centre, Agents with **New +** (plain, curious, terse, scholar) on the right; a bottom bar `left / right / bank / why`; each side has a hide button, and the left sidebar folds to a thin rail with a `[+]` restore (the sidebar element itself can never be removed: dropping it blanked the whole window).
- **Not a black box (RAN):** every answer records a `WHY` row and the window shows it as a gray line under the answer; the **Bank view** lists what the agent knows (built-in meanings, taught words with `[marked]`, aliases, answers, trust); `hai_chat why` and `hai_chat bank` dump the same data as text for tutorials. Case `hai_teach` 54 checks (12 for this).
- **Studio, checked:** Data Studio (`data-hq`) sniffs hai's data files but cannot read the mixed-row ledger; `concept-bank-hq` shows the house bank, not hai's. A hai exporter for Data Studio is the right hook and is DESIGN, not built (hai book 24 section 4).
- **A test-runner trap found on the way (RAN):** `EXPECT_FILE` with three fields is silently ignored by `harness_case_op` (use `EXPECT_EXISTS`); six of my checks were no-ops until I compared check rows with PASS lines. The harness README has other agents' uncommitted edits, so the one-line warning was NOT added there; it is in hai book 24 section 6 and should be copied to the README by whoever owns it.
- **Videos (RAN, verified):** stakeholder report, tutorials 1 to 3 and the explainer "how hai answers"; XO `31.Nov/nov-23/5.hai-videos-2026-11-23` and `%.HARNECIENT/hai-user-dox/hai-videos-2026-11-23/`. Japanese lines are mine, not reviewed by a native speaker.
