#!/bin/bash
# scripts/halo_offline_test.sh - deterministic offline proof of halo_chat_validate (no API key, no network, no repo files touched).
# Copies the Concept Bank data to a scratch house, feeds candidate EDIT records to ops/+x/halo_chat_validate.+x and checks the verdicts and
# that the bank changes ONLY when a promotion is allowed. Usage: bash scripts/halo_offline_test.sh   (build first: sh scripts/build.sh)
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE_SRC="$(cd "$HERE/.." && pwd)"
BIN="$HERE/ops/+x/halo_chat_validate.+x"
[ -x "$BIN" ] || { echo "build first: sh scripts/build.sh"; exit 2; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
mkdir -p "$T/house/&.widgits/concept-bank"; cp -r "$HOUSE_SRC/&.widgits/concept-bank/data" "$T/house/&.widgits/concept-bank/data"
SPOKE="$T/house/&.widgits/concept-bank/data/spokes/gravity_constant.pdl"
fail=0
run() { # name tier record expect_substring expect_bank_changed(0|1)
    local name="$1" tier="$2" rec="$3" want="$4" changed="$5" ent="$T/ent_$1" before after out
    mkdir -p "$ent"; echo "strength_of_training.max_tier: $tier" > "$ent/learning_limits.pdl"; printf '%s\n' "$rec" > "$ent/pending_review.txt"
    before="$(sha256sum "$SPOKE" | cut -c1-16)"
    out="$("$BIN" "$ent" "$T/house" 2>&1)"; after="$(sha256sum "$SPOKE" | cut -c1-16)"
    local ok=1; [[ "$out" == *"$want"* ]] || ok=0
    if [ "$changed" = 1 ] && [ "$before" = "$after" ]; then ok=0; fi
    if [ "$changed" = 0 ] && [ "$before" != "$after" ]; then ok=0; fi
    [ $ok = 1 ] && echo "PASS  $name -> $out" || { echo "FAIL  $name -> $out (bank changed: $([ "$before" != "$after" ] && echo yes || echo no), wanted '$want')"; fail=1; }
}
REC='[t] EDIT|id=e1|proposer=halo_chat|status=candidate|type=spoke_weight_delta|target=gravity_constant|slot=force|delta=0.2|reason="offline test"'
run valid_elementary   elementary_hs "$REC" "QUEUED_FOR_REVIEW" 0
run valid_master_phd_policy_off master_phd "$REC" "QUEUED_FOR_REVIEW" 0          # tier alone never promotes: the house policy line is missing = off
# turn the HOUSE policy on (AUTO_PROMOTE | enabled=true) and give the ledger evidence for candidates e2 (25 rewards) and e3 (19 rewards: one short of N>=20)
mkdir -p "$T/house/^.hai-horn" "$T/house/&.widgits/concept-bank/promotion_ledger"
printf 'AUTO_PROMOTE | enabled=true | gate=owner_explicit\n' > "$T/house/^.hai-horn/learning_limits.pdl"
printf 'EDIT | id=e2 | type=spoke_weight_delta | target=gravity_constant | slot=force | delta=0.2000 | reason="x" | proposer=halo_chat | reward=25 | punish=0 | replayed=1\nEDIT | id=e3 | type=spoke_weight_delta | target=gravity_constant | slot=force | delta=0.2000 | reason="x" | proposer=halo_chat | reward=19 | punish=0 | replayed=1\n' > "$T/house/&.widgits/concept-bank/promotion_ledger/ledger.txt"
run policy_on_no_evidence  master_phd "$REC" "score-0.50<0.90" 0                                # e1 has no evidence: this run registers it at 0/0, and 0/0 scores 0.50
run policy_on_n_too_low    master_phd "${REC/id=e1/id=e3}" "n-19<20" 0
run policy_on_tier_low     elementary_hs "${REC/id=e1/id=e2}" "tier-too-low" 0
run policy_on_evidence_ok  master_phd "${REC/id=e1/id=e2}" "PROMOTED" 1
grep -q 'PROMOTED | id=e2 .*before=.*after=' "$T/house/&.widgits/concept-bank/promotion_ledger/promoted.txt" && echo "PASS  audit row written (before/after)" || { echo "FAIL  no audit row"; fail=1; }
run delta_out_of_range elementary_hs "${REC/delta=0.2/delta=0.9}" "REJECT delta" 0
run unknown_target     master_phd    "${REC/gravity_constant/no_such_spoke}" "REJECT target" 0
run unknown_slot       master_phd    "${REC/slot=force/slot=no_such_master}" "REJECT slot" 0
echo "spoke after the promotion:"; grep -i 'weight\|force' "$SPOKE" | head -3
[ $fail = 0 ] && echo "ALL PASS" || echo "FAILED"
exit $fail
