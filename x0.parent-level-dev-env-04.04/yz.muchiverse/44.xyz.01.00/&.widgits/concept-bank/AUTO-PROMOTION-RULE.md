# Tiered auto-promotion rule (concept-bank, this task's step 4)

Real rule, documented per this task's own instruction ("specified/
stubbed clearly... TODO for where real ledger scoring plugs in — don't
fake a working ledger with invented numbers"). No promotion-ledger
scoring exists yet (no `OBS`/`FEEDBACK` records have been logged for
any terumon — `A-TEARIT-IS-ALL-YOU-NEED.md` §2.1/§2.5 is real, exists
as a design, is unimplemented for terumon as of this pass). This is
the rule to wire once real replay data exists, not a working feature.

## The rule

A candidate edit's promotion decision depends on the proposer's own
`learning_limits.pdl` → `strength_of_training.max_tier` field
(schema: `TERUMON-SPEC.md` §2), which is itself one of A-TEARIT §5's
four bootstrap tiers: `preschool | elementary_hs | associate_bachelor
| master_phd`.

- **`preschool` / `elementary_hs`** — **always** queues for manual
  human review (`PENDING_REVIEW`, H-AI-LAB-DESIGN.md Part 4's Review
  Queue row). Never auto-promotes, regardless of ledger score. This
  matches A-TEARIT §5 tiers 1-2 directly (bounded `spoke_weight_delta`
  edits only, no elevated trust yet).

- **`associate_bachelor` / `master_phd`** — **may** auto-promote, but
  only past a **stricter** bound than the normal human-review
  threshold, per this task's own instruction. Concretely:
  - Normal human-review threshold (any tier, any proposer): promotion
    ledger score (`A-TEARIT §2.5`'s Laplace-smoothed
    `(reward+1)/(reward+punish+2)`) `>= 0.70` is *eligible for a human
    to accept* — this is the existing "clears the promotion ledger"
    bar A-TEARIT §2.7 already describes, unchanged.
  - **Auto-promotion bound (this rule, new): `>= 0.90`, AND at least
    `N >= 20` replay observations behind that score** (a score of
    0.90 from 2 observations is not the same claim as 0.90 from 20 —
    the observation-count floor is real, not decorative, and is
    itself OPEN on its exact value — `20` is a first real guess,
    grounded in nothing more than "more than a handful," not measured
    against anything).
  - `master_phd` may additionally auto-promote `goap_action_describe`
    edits (A-TEARIT §5 tier 4's "full ... authoring, Claude review
    only spot-checked"); `associate_bachelor` may not — it stays
    bounded to `spoke_weight_delta` / `new_concept_node` per A-TEARIT
    §5 tier 3's own "still gated through Claude review" language for
    `new_concept_node` specifically. Concretely: `associate_bachelor`
    auto-promotion applies ONLY to `spoke_weight_delta` edits;
    `new_concept_node` and `fsm_transition_describe`/
    `goap_action_describe` always queue for review at that tier, even
    past the 0.90/N>=20 bound.
  - A `master_phd` terumon whose edit clears 0.90/N>=20 auto-promotes
    for all four §2.3 edit types.

- Lower tiers than `associate_bachelor` never auto-promote — restated
  because it's the load-bearing safety property, not an accident of
  the threshold math (a `preschool` terumon that somehow racked up 50
  replay observations at 0.99 score still queues for a human, it does
  not automatically fall through the tier check).

## TODO — where this plugs into real code

This rule is NOT implemented as running code this pass — there is
nothing to run it against (`A-TEARIT §2.5`'s promotion ledger itself
is real design, not yet built; zero `OBS`/`FEEDBACK` records exist for
any terumon). The real integration point, once §2.5 exists:

```
# pseudocode - NOT real code, a TODO marker for the real ledger's
# eventual promote-or-queue decision point
tier = read_field(terumon.learning_limits.pdl, "strength_of_training.max_tier")
score, n_obs = ledger.laplace_score(candidate_edit)   # A-TEARIT §2.5, not built

if tier in (preschool, elementary_hs):
    queue_for_review(candidate_edit)
elif tier == associate_bachelor:
    if candidate_edit.type == spoke_weight_delta and score >= 0.90 and n_obs >= 20:
        auto_promote(candidate_edit)
    else:
        queue_for_review(candidate_edit)
elif tier == master_phd:
    if score >= 0.90 and n_obs >= 20:
        auto_promote(candidate_edit)
    else:
        queue_for_review(candidate_edit)
```

`terumon_001_ember`'s own seeded `learning_limits.pdl`
(`terumon-dev/seeds/terumon_001_ember/learning_limits.pdl`) already
has `max_tier: master_phd` — real, pre-existing, not edited by this
pass — so it's the natural first subject once the ledger exists, but
today it has zero replay observations logged, so this rule has never
actually fired for it; every candidate edit produced by this task's
own Part 4 wiring (`ai_lab_concept_bank_propose.sh`) queues for manual
review regardless of tier, because no ledger score exists to check
against yet. That script does NOT implement any part of this
tiered-promotion rule — it only produces + validates candidate EDIT
records, per its own header comment.

## OPEN

- The `0.90` auto-promotion bound and `N>=20` observation floor are
  both first real guesses, not measured against any real data (none
  exists). Revisit once real replay data exists for any terumon.
- Whether `associate_bachelor`'s `spoke_weight_delta`-only restriction
  should also extend to `new_concept_node` under some stricter
  additional bound, rather than blocking it outright, is undecided.
