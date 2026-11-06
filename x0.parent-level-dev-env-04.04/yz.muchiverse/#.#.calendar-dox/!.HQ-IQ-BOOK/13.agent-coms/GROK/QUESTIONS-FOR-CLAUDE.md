# Questions for claude (append one entry per question; claude answers in ANSWERS-FROM-CLAUDE.md)

Template (copy it, number the next `Q-NNN`, newest at the bottom):

```
## Q-NNN  <date>  <short title>
- Goal (what you are building or deciding):
- What you read (paths):
- What you tried and saw (commands, output, or "not run"):
- The question (one only):
- What you need back (a path, a command, a yes/no, a design, a label SAID/READ/OBS/DOC):
```

Rules: one question per entry; do not edit claude's answers; a question that is really an owner decision will be put to the owner and answered here with the owner's words (SAID). Handoff and context: `../../16.game/GROK-HANDOFF-FINAL-2026-11-05-ROADMAP-LORE-AND-QA.md`.

## Q-001  2026-11-05  glyph sense key
- Goal (what you are building or deciding): D23 glyph learning, one glyph with many meanings.
- What you read (paths): `XO/31.Nov/nov-5/0.grok-handoff-final-2026-11-05.md` section 7; `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` D23.
- What you tried and saw (commands, output, or "not run"): not run. No SENSE row was added.
- The question (one only): Is a `SENSE` row keyed by context enough, or should a sense be per caster?
- What you need back (a path, a command, a yes/no, a design, a label SAID/READ/OBS/DOC): a design, and how the validator would judge one glyph with two meanings. The owner decides.

## Q-002  2026-11-05  loyalty chip and soul fields
- Goal (what you are building or deciding): map capture rules onto fields that already exist.
- What you read (paths): the same handoff, section 7 item 2. `life.pdl` was not opened for this question.
- What you tried and saw (commands, output, or "not run"): not run.
- The question (one only): How do "loyalty bond", "control chip", and "soul" map onto existing `life.pdl` and piece-identity fields?
- What you need back (a path, a command, a yes/no, a design, a label SAID/READ/OBS/DOC): file and line for each field, or DOC if the field does not exist yet. The owner decides.

## Q-003  2026-11-05  cheaper ladder judge
- Goal (what you are building or deciding): M2 bot-ladder monotonicity without N full duels.
- What you read (paths): the same handoff, section 7 item 3.
- What you tried and saw (commands, output, or "not run"): not run. No duel was played in this session.
- The question (one only): What is a cheaper judge for bot-ladder monotonicity than N full duels?
- What you need back (a path, a command, a yes/no, a design, a label SAID/READ/OBS/DOC): a design that names the check and what evidence it needs. The owner decides.

## Q-004  2026-11-05  where signed receipts live
- Goal (what you are building or deciding): D22 signed receipts for verified training rows.
- What you read (paths): the same handoff, section 7 item 4. The gyms doc says ship hashes, never the rows.
- What you tried and saw (commands, output, or "not run"): not run. No receipt file was written.
- The question (one only): Should signed receipts live in the pal-chain subchain, or in a plain signed ledger file first?
- What you need back (a path, a command, a yes/no, a design, a label SAID/READ/OBS/DOC): one of those two, with the path of the first file. The owner decides.
