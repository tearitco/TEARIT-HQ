# Entities as gas and as liquid: lending an ability, or becoming part of another entity (idea + options)

Status: IDEA captured 2026-10-07 by claude; **owner is not sure how to arrange it yet**, so this lists the shapes that fit the house and what each costs. Nothing is built or decided.
Owner's words: "entities can be used as gas (gets the ability of the entity) or liquidated into other entities, so it becomes a permanent part, based on its weights, associations, word banks, skills etc. I'm not sure how to arrange this yet, but I wanted to say something like that."

## 1. The two verbs
- **Gas (inhale): temporary.** A host entity gains the *ability* of a source entity for a while (its skills, some of its behavior, word-bank associations at reduced weight). It wears off. The source may or may not be consumed.
- **Liquid (liquidate / infuse): permanent.** The source entity becomes a **permanent part** of the host: some of its weights, skills and word-bank associations are merged into the host for good, and the source is gone as an independent entity ("liquidated into").
(Third state for completeness, not asked: **solid** = the entity as it is. "Liquidated" can also mean the finance sense, turning an entity into value/coins; recorded as an open question, section 6.)

## 2. What an entity is made of (so there is something to transfer)
From the existing designs: **private numbers** in the entity (variables, scores, `needs.pdl`, school record, inventory, phone) and **shared hidden-layer sets it points at** (bank set = behavior with signatures; schools). Transfer works on three lawful layers:
1. **Skills / classes / parameters** (RPG Maker primitives: `change_skill`, `change_class`, `change_parameter`, states). Easiest, most visible.
2. **Word-bank associations and scores** (the entity's `zz.wordbank/{words,scores,vars}.txt`): aliases and weights, optionally the source's bank pointer.
3. **Behavior variables** (what the entity binds to a behavior's parameters).
**Identity is never transferred.** The host keeps its own `entity_uid` and phone. Copying an identity would repeat the bug already found and fixed in `desk_copy_op` (a copied PAL hash duplicating an entity's identity).

## 3. Option A: gas as a state with a duration (smallest, fits today)
Inhaling = the host gets a **state** ("Inspired by <source>") for N clock ticks (a **schedule row** on the clock, expiry by the ledger cursor), and that state applies **temporary overlays**: skill rows with an `expires=` tick, extra aliases with weight x factor. When the state ends, the overlay rows are removed; nothing permanent changed, so it is **trivially reversible and safe**. Overlay strength = a function of the source's weights and a tunable (`gas_factor`, e.g. 0.3). The source can be a **consumable item** ("essence" in the host's inventory) instead of a live entity: packing an entity into an item is the same pattern as inventory items, which also makes it tradeable (auction item kind "entity").

## 4. Option B: liquid as a merge proposal (permanent, goes through review)
Liquidating = compute a **merge plan** (a dry run) and apply it only after review. The plan is a list of candidate EDITs on the host: raise skill levels toward the source's (bounded: `host += alpha * (source - host)`, capped), add the source's strongest word-bank aliases at a reduced weight, add skills the host lacks at a reduced level. **It follows the attrition law**: the merge is a *proposal* routed through the validator and a human review file (no auto-promotion; the halo auto-promote gap stays the owner's decision), never an unreviewed rewrite. The source entity is **not deleted**: it is **tombstoned** (archived with a ledger row `LIQUIDATED | source | into=host | plan_hash | ts`), so the act is **auditable and reversible** (restore from a checkpoint with `game_snapshot_op`, or re-split by replaying the plan backwards because the plan stores before/after values). Cost: needs the word-bank and school designs built first (they are designs today), and a rule for what happens to the source's own dependents (its phone contacts, schools).

## 5. Option C: both, with essence as the common currency (my suggestion, not decided)
One conversion makes both verbs the same machinery: **entity -> essence** (a record of its skills, bank associations and weights, derived from the entity), and **essence -> host** in two modes, **gas** (temporary overlay, Option A) or **liquid** (permanent merge proposal, Option B). Essence is an item (an inventory folder), so it can be carried, stored, traded and, later, sold on the auction screen, and a **recipe** (Canvas-Craft) can combine essences (cooking an ability). The "weights and associations" decide how much transfers: a source with a strong, well-scored skill transfers more of it than a weak one; associations with the host's own word bank (shared synonyms) transfer more easily (similar entities blend better; dissimilar ones transfer less), which is a real, countable rule rather than a model's judgment.

## 6. Rules and risks (apply to every option)
- **Consent and ownership:** you may only gas/liquidate entities you own; another user's entity needs their permission (user data rules; their desk data is theirs).
- **Death stays off** (needs design): liquidating is not death; the tombstone keeps the source recoverable.
- **No identity or secret copy:** never copy `entity_uid`, phone or wallet; essence carries skills/bank rows only.
- **Bounded:** caps per liquidation and per host (so one entity cannot become everything), tunables in a `.pdl`, audit rows for every change.
- **Never silently:** every effect is a ledger row; the calendar/history window can show "inhaled on day N, wore off on day M".
- **Models do not decide transfers** (law from NIGHT 26): the amount comes from counted weights and a formula; a model may only describe.
- **Cost:** merging banks and skills is data work, not process work, so cheap; the review step is the slow part by design.

## 7. Open questions for the owner
1. Is "liquidated" the **merge** meaning (becomes a permanent part) only, or also the **finance** meaning (entity converted to coins/value)? Both can coexist (an entity sold off vs absorbed).
2. Does the source disappear (tombstoned) when gassed, or is gas a copy of ability while the source stays? (Option A as written: source stays unless it is a consumable essence item.)
3. Should absorbing need a **review** every time, or only above a threshold amount?
4. Entities as **tradeable essence items** on the auction screen: wanted?
5. Should weights/associations make dissimilar entities refuse to blend, or just transfer less (my suggestion: transfer less)?

## 8. Where it would be built (not started)
After needs/word banks/school are real: a pure `essence_of(entity)` derivation, a `gas_apply` event page (state + overlay + expiry), a `liquidate_plan` dry-run op, and the review/apply path; each with a pal harness and rehearsal on scratch entities in alpha.
