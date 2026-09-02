#!/bin/bash
# run_plan.sh — the FSM/verification layer for Harnecient delegation
# (au11-hq/HARNESS-DELEGATION-PIPELINE.md §3.1's EXECUTE/VERIFIED loop).
#
# What this actually does, real not aspirational: drives h-ai (open-hai)
# via its EXISTING, unmodified relay + detect_tool() machinery - this
# file adds ZERO new capability to open-hai itself, it only sequences
# and VERIFIES calls to what already works. Each step's real output is
# captured to its own file so later steps' assertions can reference
# EARLIER steps' actual results - this is the "verification layer"
# itself: nothing is ever scored by the model saying "done," only by
# grepping a real output file (house law, see au11-hq/HARNECIENT-HACK.md).
#
# Usage: run_plan.sh <plan-file>
#
# Plan file format (pipe-delimited, one step per line, '#' = comment):
#   STEP | <task text to type into h-ai's composer> | <assertion> [| APPROVE|DENY]
#   NAVIGATE | <goal, plain English, describing WHAT to reach> | <assertion>
#   INCLUDE | <path to another .plan file, relative to this plan's dir>
#   BRANCH | <condition, references prior STEPn_OUT> | <plan if true> | <plan if false>
#   LOOP_UNTIL | <max iterations> | <path to a sub-plan to repeat>
#
# Composition primitives (added 2026-08-13, direct instruction: "branching,
# sub-plans and loop/until sound good"), all built on ONE mechanism -
# run_plan_file(), the same engine that runs a top-level plan, called
# recursively:
#   - INCLUDE runs a sub-plan and takes its overall PASS/FAIL as this
#     step's verdict directly - the reusable-subroutine primitive.
#   - BRANCH evaluates a shell condition (referencing earlier STEPn_OUT
#     files) and INCLUDEs one of two sub-plans depending on the result -
#     the model never picks the branch, the condition is always a real
#     verified check on real prior output, same discipline as
#     everything else here.
#   - LOOP_UNTIL re-runs a sub-plan (fresh run_plan_file() call each
#     time, own artifacts each time) until it PASSes or a hard
#     iteration cap is hit - the "write code, verify it compiles,
#     retry with the compiler error fed back" shape self-coding needs,
#     without inventing a second retry mechanism (MAX_RETRIES's
#     Harnecient-suggested-phrasing retry already exists for STEP-level
#     "try different words" - LOOP_UNTIL is for STEP-level "try again
#     with a different apporach based on what genuinely failed",
#     structured as its own sub-plan so the retry logic can itself use
#     STEP/NAVIGATE/APPROVE, not just a phrasing tweak).
# Recursion depth is capped (MAX_INCLUDE_DEPTH joint, default 5) - real
# protection against a runaway/self-including plan, not just a comment.
#
# The assertion command for STEP/NAVIGATE runs with STEP1_OUT,
# STEP2_OUT, ... exported (paths to each prior step's captured output,
# in order, SCOPED TO THE CURRENT run_plan_file() invocation - a
# sub-plan run via INCLUDE/BRANCH/LOOP_UNTIL gets its OWN STEP1_OUT.. -
# exit 0 = pass, nonzero = fail (aborts the remaining plan). NAVIGATE
# steps additionally export STEPn_PRE/STEPn_TARGET/STEPn_LABEL - see
# HARNESS-DELEGATION-PIPELINE.md §6 for why post-action verification is
# mandatory, not optional, for navigation.
#
# Safety scope (real, not aspirational):
#  - STEP (delegate): read-only tools pre-execute automatically.
#    Mutating tools (write/edit/cmd) FAIL CLOSED unless the plan line's
#    4th field is literally "APPROVE" or "DENY" - see §8 of the design
#    doc for the real bug this fixed (an approval banner was briefly
#    indistinguishable from a real tool result before this existed).
#  - NAVIGATE: resolves the model's plain-text reply against REAL
#    current delegation-safe labels (nav_intent_to_index.sh) and FAILS
#    CLOSED (no dispatch at all) on no match - never sends a guessed
#    digit.
#  - MAX_RETRIES (STEP-level phrasing retry) and LOOP_UNTIL (sub-plan
#    retry) are BOTH opt-in (0 iterations / no LOOP_UNTIL used by
#    default) - a plan doesn't accidentally loop forever or retry
#    silently unless it explicitly asks to.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../.." && pwd)"
PLAN_FILE="${1:-}"

if [ -z "$PLAN_FILE" ] || [ ! -f "$PLAN_FILE" ]; then
    echo "usage: run_plan.sh <plan-file>" >&2
    exit 1
fi

OPEN_HAI_DIR="$HOUSE/&.widgits/open-hai"
RELAY="$HOUSE/#.desktop/open_hai_agent_relay.txt"
AUDIT="$OPEN_HAI_DIR/pieces/audit"
RECEIPT="$AUDIT/open-hai-frame.png.receipt.txt"
NAV_LABELS="$AUDIT/open-hai-frame.png.nav-labels.txt"
SESSIONS="$OPEN_HAI_DIR/sessions"
RESOLVER="$SCRIPT_DIR/nav_intent_to_index.sh"

# Stage 0 observation log (HARNESS-DELEGATION-PIPELINE.md §7.2). One
# shared log across ALL nested run_plan_file() invocations too - a
# sub-plan's steps are just as real as a top-level plan's.
OBS_LOG="$SCRIPT_DIR/observations.log"
log_observation() {
    printf '%s | %s | %s | %s | %s | %s\n' "$(date +%s)" "$1" "$2" "$3" "$4" "$5" >> "$OBS_LOG"
}

OLLAMA_URL="${OLLAMA_URL:-http://10.0.0.144:11434}"

# ---- tunable "joints" (tunables.conf) ----
NAV_MODEL="gemma3:1b"
# Real fix 2026-08-13 (Stage A pilot #1): 15 x 0.5s = 7.5s was sized
# for near-instant read-only tool calls, real too short for a full
# script generation from a larger model (stable-code:latest genuinely
# took closer to a minute for a real bash script) - the SAME poll loop
# now also correctly waits for +2 messages (see delegate_step()'s own
# fix note), so it needs real headroom to reach that target, not just
# "any change."
POLL_TRIES=90
POLL_INTERVAL_S=1
NAV_SETTLE_S=0.5
RELAY_CODE_DELAY_S=0.2
LAUNCH_SETTLE_S=1
DUMP_SETTLE_S=1
MAX_RETRIES=0
MAX_INCLUDE_DEPTH=5

# --- model-choice joints (2026-08-13, direct instruction: "can even
# call 2 gemma 2 choose joint? get it?" - the model-selection decision
# is itself made by asking a cheap Harnecient model, same DESCRIBE-
# then-deterministically-resolve pattern as nav_intent_to_index.sh,
# not a new architecture. Informed by a REAL 4-attempt live comparison
# (HARNESS-DELEGATION-PIPELINE.md §10): stable-code:latest failed 3/3
# strict self-coding checks (reliably pads output with prose/markdown
# fences); gemma3:1b passed 1/1, clean, zero pollution. ---
SIMPLE_TASK_MODEL="gemma3:1b"
COMPLEX_TASK_MODEL="stable-code:latest"
MODEL_CHOOSER_MODEL="gemma3:270m"  # classification is itself a simple task - use the cheapest model for it
COMPLEXITY_THRESHOLD=2  # complexity_signal_count() (commas + "and"s in the model's DESCRIBE-style reply) at or above this -> COMPLEX_TASK_MODEL. First-pass value from 4 real trials, not extensively tuned - a real Stage 1 candidate.
G_MODELS_ORDER=("stable-code:latest" "gemma3:1b" "gemma3:270m" "llama3-groq-tool-use:8b" "llama2:latest")  # MUST match g_models[] in khtpm_open_hai_render.c exactly - order is how cycle_model() advances

TUNABLES_FILE="$SCRIPT_DIR/tunables.conf"
if [ -f "$TUNABLES_FILE" ]; then
    # shellcheck disable=SC1090
    source "$TUNABLES_FILE"
fi

log() { echo "[fsm] $*" >&2; }

# ---- open-hai process lifecycle (button.sh's own single-instance guard
# - PITFALL 72) ----
ensure_open_hai() {
    log "ensuring single open-hai instance via button.sh..."
    sh "$OPEN_HAI_DIR/button.sh" "$HOUSE" || { echo "ensure_open_hai: launch failed" >&2; exit 1; }
    sleep "$LAUNCH_SETTLE_S"
    n="$(pgrep -f 'khtpm_open_hai_render\.\+x' | grep -c . || true)"
    if [ "$n" != "1" ]; then
        echo "ensure_open_hai: expected 1 process, found $n - aborting" >&2
        exit 1
    fi
}

# ---- receipt polling ----
dump_receipt() {
    echo 112 >> "$RELAY"
    sleep "$DUMP_SETTLE_S"
}
receipt_field() {
    # preceding-space lookbehind so e.g. "nav=" doesn't also match
    # inside "n_nav=" (real bug hit and fixed 2026-08-13, §6).
    grep -oP "(?<= )$1=\K\S+" "$RECEIPT" 2>/dev/null | head -1
}

send_code() { echo "$1" >> "$RELAY"; sleep "$RELAY_CODE_DELAY_S"; }
send_text() {
    for c in $(echo -n "$1" | od -An -tu1); do send_code "$c"; done
}

goto_nav() {
    local idx="$1"
    for d in $(echo -n "$idx" | grep -o .); do
        send_code "$((d + 48))"
    done
}

find_approve_nav() {
    grep '|Approve: ' "$NAV_LABELS" 2>/dev/null | head -1 | cut -d'|' -f1
}
find_deny_nav() {
    grep '|Deny|' "$NAV_LABELS" 2>/dev/null | head -1 | cut -d'|' -f1
}
find_model_nav() {
    grep '|Model: ' "$NAV_LABELS" 2>/dev/null | head -1 | cut -d'|' -f1
}
# Reads the CURRENTLY ACTIVE model name from the live nav-labels dump
# (never from a stale sessions/model.txt read - the running process's
# own real state is the only trustworthy source, same discipline as
# every receipt/label read elsewhere in this file).
current_model_name() {
    grep '|Model: ' "$NAV_LABELS" 2>/dev/null | head -1 | sed 's/.*Model: //'
}

# ---- deterministic model switch: cycles open-hai's REAL, LIVE model
# selector via the same nav+Enter action a human clicking "Model"
# would use (cycle_model() in khtpm_open_hai_render.c) - never edits
# sessions/model.txt directly (that's only read at process STARTUP,
# an external edit wouldn't take effect on a running instance). Cycle
# count is computed from G_MODELS_ORDER, which MUST mirror the app's
# own g_models[] order - if they drift apart, this silently cycles to
# the WRONG model, so keep them in sync by hand until there's a better
# live source (a future dump_nav_labels()-style export of the model
# list itself would remove this hardcoded-order dependency entirely -
# not done, flagged as a real follow-up, not hidden). ----
switch_to_model() {
    local target="$1"
    dump_receipt
    local current
    current="$(current_model_name)"
    if [ "$current" = "$target" ]; then
        log "switch_to_model: already on $target, no cycling needed"
        return 0
    fi
    local cur_idx=-1 tgt_idx=-1 i=0
    for m in "${G_MODELS_ORDER[@]}"; do
        [ "$m" = "$current" ] && cur_idx=$i
        [ "$m" = "$target" ] && tgt_idx=$i
        i=$((i + 1))
    done
    if [ "$cur_idx" -lt 0 ] || [ "$tgt_idx" -lt 0 ]; then
        echo "switch_to_model: '$current' or '$target' not found in G_MODELS_ORDER - refusing to guess a cycle count" >&2
        return 1
    fi
    local n="${#G_MODELS_ORDER[@]}"
    local cycles=$(( (tgt_idx - cur_idx + n) % n ))
    local model_nav
    model_nav="$(find_model_nav)"
    if [ -z "$model_nav" ]; then
        echo "switch_to_model: no 'Model:' nav row found - unexpected state" >&2
        return 1
    fi
    log "switch_to_model: $current -> $target ($cycles cycle(s) on nav $model_nav)"
    for ((c = 0; c < cycles; c++)); do
        goto_nav "$model_nav"
        send_code 13
        sleep "$DUMP_SETTLE_S"
        dump_receipt
    done
    local final
    final="$(current_model_name)"
    if [ "$final" != "$target" ]; then
        echo "switch_to_model: FAILED - expected '$target', live state shows '$final' after $cycles cycles" >&2
        return 1
    fi
    log "switch_to_model: confirmed live on $target"
    return 0
}

# DESCRIBE, don't CLASSIFY (real fix 2026-08-13, direct correction:
# "is this using the harnecient hack or a naked test?"). The FIRST
# version of this function asked the model to directly output one of
# two enum tokens (SIMPLE/COMPLEX) - that IS the anti-pattern
# HARNECIENT-HACK.md exists to avoid, and it showed: unreliable across
# both gemma3:270m and gemma3:1b, multiple prompt rewrites (few-shot,
# explicit rubric, system pre-prompt), one attempt even returned a
# non-word ("SHORTEST"). Fixed by asking for a plain DESCRIPTION
# instead (same shape as nav_intent_to_index.sh's proven pattern) -
# the model never classifies anything, it just describes the task's
# shape in its own words, and complexity_signal_count() below resolves
# that description deterministically. Verified 4/4 correct across
# both classifier models on both test tasks before this was trusted.
query_ollama_for_task_description() {
    local task_desc="$1"
    local prompt="You are a plain-text assistant. Reply in ONE short sentence, no markdown, no lists.\n\nTask: \"${task_desc}\"\n\nIn one plain sentence, describe how many distinct steps or parts completing this task would involve."
    curl -s -m 30 -X POST "$OLLAMA_URL/api/generate" -H "Content-Type: application/json" \
        -d "$(python3 -c "import json,sys; print(json.dumps({'model':sys.argv[1],'prompt':sys.argv[2],'stream':False}))" "$MODEL_CHOOSER_MODEL" "$prompt")" \
        | python3 -c "import json,sys
try: print(json.load(sys.stdin)['response'].strip())
except Exception: print('')"
}

# Deterministic resolver: counts real structural-complexity signals in
# the model's plain-text description (commas = enumerated parts, " and
# " = joined parts) - a genuinely complex task naturally gets described
# with more enumerated pieces, a genuinely simple one doesn't, without
# ever asking the model to self-report a category. Threshold (2) and
# the signal set are a first-pass heuristic, not tuned beyond the 4
# real trials that validated this approach - a real Stage 1 candidate
# (HARNESS-DELEGATION-PIPELINE.md §7.3) once more data exists.
complexity_signal_count() {
    local desc="$1"
    local commas and_count
    commas="$(echo -n "$desc" | tr -cd ',' | wc -c)"
    and_count="$(echo "$desc" | grep -oi ' and ' | wc -l)"
    echo $((commas + and_count))
}

delegate_step() {
    local task="$1" approve_flag="${2:-}"
    dump_receipt
    local n_nav baseline_msgs
    n_nav="$(receipt_field n_nav)"
    baseline_msgs="$(receipt_field n_msgs)"
    local composer_nav=$((n_nav - 1))

    goto_nav "$composer_nav"
    send_code 13          # arm
    send_text "$task"
    send_code 13          # submit
    sleep "$DUMP_SETTLE_S"

    # Real bug found and fixed 2026-08-13 (Stage A pilot #1, EVENTS-PAL-
    # BUILDOUT-PLAN.md §6): submit_composer() persists the USER's own
    # message SYNCHRONOUSLY (n_msgs +1 immediately), then the real
    # reply (chat completion OR a tool result - both go through
    # check_pending()'s async fork/curl-child polling in
    # khtpm_open_hai_render.c) lands LATER, asynchronously, as a
    # SEPARATE +1. The original poll here stopped on ANY change from
    # baseline - correct by accident for fast paths (a synchronous
    # tool_pending banner adds both +1s together, read-only tools often
    # complete inside one poll interval) but WRONG for a genuinely slow
    # generation (a full script from stable-code:latest): the poll
    # exited the instant the user-message-only +1 landed, and
    # capture_last_output() then grabbed the STALE last real assistant
    # line (the session's own startup greeting) instead of waiting for
    # the actual reply - a real, confirmed false-negative this pass.
    # Fixed: always wait for +2 (user message AND the real response),
    # not just "something changed."
    local target_msgs=$((baseline_msgs + 2))
    local tries=0 cur_msgs="$baseline_msgs"
    while [ "$cur_msgs" -lt "$target_msgs" ] && [ "$tries" -lt "$POLL_TRIES" ]; do
        sleep "$POLL_INTERVAL_S"
        dump_receipt
        cur_msgs="$(receipt_field n_msgs)"
        tries=$((tries + 1))
    done
    if [ "$cur_msgs" -lt "$target_msgs" ]; then
        echo "TIMEOUT waiting for tool result (got n_msgs=$cur_msgs, needed >=$target_msgs)" >&2
        return 1
    fi

    local tool_pending
    tool_pending="$(receipt_field tool_pending)"
    if [ "$tool_pending" = "1" ]; then
        if [ "$approve_flag" != "APPROVE" ] && [ "$approve_flag" != "DENY" ]; then
            echo "delegate_step: mutating tool awaiting approval, plan did not request APPROVE or DENY - failing closed, nothing approved or denied" >&2
            return 1
        fi
        local action_nav
        if [ "$approve_flag" = "APPROVE" ]; then
            action_nav="$(find_approve_nav)"
        else
            action_nav="$(find_deny_nav)"
        fi
        if [ -z "$action_nav" ]; then
            echo "delegate_step: tool_pending=1 but no '$approve_flag' nav row found - unexpected state" >&2
            return 1
        fi
        log "delegate_step: sending $approve_flag for pending tool (nav $action_nav)"
        goto_nav "$action_nav"
        send_code 13
        sleep "$DUMP_SETTLE_S"

        local action_baseline="$cur_msgs"
        tries=0
        while [ "$cur_msgs" = "$action_baseline" ] && [ "$tries" -lt "$POLL_TRIES" ]; do
            sleep "$POLL_INTERVAL_S"
            dump_receipt
            cur_msgs="$(receipt_field n_msgs)"
            tries=$((tries + 1))
        done
        if [ "$cur_msgs" = "$action_baseline" ]; then
            echo "TIMEOUT waiting for $approve_flag's real result" >&2
            return 1
        fi
    fi
    return 0
}

query_ollama_for_retry_suggestion() {
    local original_task="$1" assertion="$2"
    local prompt="You are a plain-text assistant helping retry a failed request. Reply in ONE short sentence, no markdown.\n\nThe original request was: \"${original_task}\"\n\nIt did not produce the expected result. Suggest ONE alternative way to phrase the SAME request, more specific or clearer, in one short plain sentence. Do not explain, just give the alternative phrasing."
    curl -s -m 30 -X POST "$OLLAMA_URL/api/generate" -H "Content-Type: application/json" \
        -d "$(python3 -c "import json,sys; print(json.dumps({'model':sys.argv[1],'prompt':sys.argv[2],'stream':False}))" "$NAV_MODEL" "$prompt")" \
        | python3 -c "import json,sys
try: print(json.load(sys.stdin)['response'].strip())
except Exception: print('')"
}

query_ollama_for_nav_intent() {
    local goal="$1" labels_csv="$2"
    local prompt="You are a plain-text assistant. Reply in ONE short sentence, no markdown.\n\nThe following menu items exist: ${labels_csv}.\n\nGoal: ${goal}.\n\nIn one short plain sentence, name EXACTLY which menu item (using its exact name from the list) you want to interact with."
    curl -s -m 30 -X POST "$OLLAMA_URL/api/generate" -H "Content-Type: application/json" \
        -d "$(python3 -c "import json,sys; print(json.dumps({'model':sys.argv[1],'prompt':sys.argv[2],'stream':False}))" "$NAV_MODEL" "$prompt")" \
        | python3 -c "import json,sys
try: print(json.load(sys.stdin)['response'].strip())
except Exception: print('')"
}

navigate_step() {
    local goal="$1" pre_file="$2" post_file="$3" target_file="$4" label_file="$5"
    dump_receipt
    cp "$RECEIPT" "$pre_file" 2>/dev/null

    if [ ! -f "$NAV_LABELS" ]; then
        echo "navigate_step: no nav-labels.txt - is open-hai's binary the 2026-08-13+ build with dump_nav_labels()?" >&2
        return 1
    fi
    local labels_csv delegate_labels=()
    while IFS='|' read -r idx display safe; do
        delegate_labels+=("$safe")
    done < "$NAV_LABELS"
    labels_csv="$(IFS=,; echo "${delegate_labels[*]}")"

    local reply
    reply="$(query_ollama_for_nav_intent "$goal" "$labels_csv")"
    log "navigate: goal='$goal' model_reply='$reply'"
    if [ -z "$reply" ]; then
        echo "navigate_step: empty model reply" >&2
        return 1
    fi

    local resolved_idx
    resolved_idx="$("$RESOLVER" "$reply" "${delegate_labels[@]}")"
    if [ -z "$resolved_idx" ]; then
        echo "navigate_step: FAILED CLOSED - model reply '$reply' matched no real label, no dispatch attempted" >&2
        return 1
    fi
    echo "$resolved_idx" > "$target_file"
    echo "${delegate_labels[$((resolved_idx - 1))]}" > "$label_file"
    log "navigate: resolved to index $resolved_idx (${delegate_labels[$((resolved_idx - 1))]})"

    goto_nav "$resolved_idx"
    send_code 13    # activate
    sleep "$NAV_SETTLE_S"
    dump_receipt
    cp "$RECEIPT" "$post_file" 2>/dev/null
    return 0
}

capture_last_output() {
    local out_file="$1"
    local newest
    newest="$(ls -t "$SESSIONS" 2>/dev/null | grep -v '\.txt$' | head -1)"
    local transcript="$SESSIONS/$newest/transcript.txt"
    if [ ! -f "$transcript" ]; then
        echo "capture_last_output: no transcript found" >&2
        return 1
    fi
    grep '^A|' "$transcript" | tail -1 | sed 's/^A|//; s/\\n/\n/g' > "$out_file"
}

# ---- run_plan_file(): the ONE FSM engine, callable at top level and
# recursively (INCLUDE/BRANCH/LOOP_UNTIL). Returns 0=PASS, 1=FAIL.
# Prints its own RUN_DIR path to stdout on its LAST line so a caller
# (BRANCH/LOOP_UNTIL) can find its artifacts if needed - callers that
# don't care just check the exit code. ----
run_plan_file() {
    local plan_file="$1" depth="${2:-0}"

    if [ "$depth" -gt "$MAX_INCLUDE_DEPTH" ]; then
        echo "run_plan_file: MAX_INCLUDE_DEPTH ($MAX_INCLUDE_DEPTH) exceeded - refusing to recurse further (runaway INCLUDE/BRANCH/LOOP_UNTIL?)" >&2
        return 1
    fi
    if [ ! -f "$plan_file" ]; then
        echo "run_plan_file: plan not found: $plan_file" >&2
        return 1
    fi

    local ts run_dir summary
    ts="$(date +%Y%m%d-%H%M%S-%N)"
    run_dir="$SCRIPT_DIR/proof/run_${ts}"
    mkdir -p "$run_dir"
    summary="$run_dir/SUMMARY.txt"
    {
        echo "run_plan_file — $plan_file (depth $depth)"
        echo "started: $(date)"
        echo ""
    } > "$summary"

    local plan_dir
    plan_dir="$(cd "$(dirname "$plan_file")" && pwd)"

    local step_n=0 overall="PASS"
    while IFS='|' read -r kind task assertion; do
        kind="$(echo "$kind" | xargs)"
        [ -z "$kind" ] && continue
        case "$kind" in
            \#*) continue ;;
            STEP|NAVIGATE|INCLUDE|BRANCH|LOOP_UNTIL|SET_MODEL|CHOOSE_MODEL) ;;
            *) continue ;;
        esac
        task="$(echo "$task" | sed 's/^ *//; s/ *$//')"
        assertion="$(echo "$assertion" | sed 's/^ *//; s/ *$//')"
        step_n=$((step_n + 1))

        if [ "$kind" = "INCLUDE" ]; then
            local sub_plan="$task"
            [ "${sub_plan:0:1}" != "/" ] && sub_plan="$plan_dir/$sub_plan"
            log "step $step_n: include: $sub_plan (depth $((depth + 1)))"
            if run_plan_file "$sub_plan" "$((depth + 1))" > "$run_dir/step${step_n}.include.log" 2>&1; then
                echo "INCLUDE $step_n [$sub_plan]: PASS" >> "$summary"
                log_observation "INCLUDE" "$sub_plan" "-" "-" "PASS"
                log "step $step_n: PASS"
                continue
            else
                echo "INCLUDE $step_n [$sub_plan]: FAIL (see $run_dir/step${step_n}.include.log)" >> "$summary"
                log_observation "INCLUDE" "$sub_plan" "-" "-" "FAIL"
                log "step $step_n: FAIL"
                overall="FAIL"
                break
            fi
        fi

        if [ "$kind" = "SET_MODEL" ]; then
            # deterministic, explicit model switch - "task" holds the
            # target model name directly.
            if switch_to_model "$task"; then
                echo "SET_MODEL $step_n [$task]: PASS" >> "$summary"
                log_observation "SET_MODEL" "$task" "-" "-" "PASS"
                log "step $step_n: PASS"
                continue
            else
                echo "SET_MODEL $step_n [$task]: FAIL (see stderr above)" >> "$summary"
                log_observation "SET_MODEL" "$task" "-" "-" "FAIL"
                log "step $step_n: FAIL"
                overall="FAIL"
                break
            fi
        fi

        if [ "$kind" = "CHOOSE_MODEL" ]; then
            # Meta-delegation, direct instruction 2026-08-13 ("can even
            # call 2 gemma 2 choose joint? get it?" / "this is a good
            # idea imo and how it should be used"). REAL FIX same
            # session (direct correction: "is this using the harnecient
            # hack or a naked test?"): the first version asked the
            # classifier to output a raw SIMPLE/COMPLEX enum token
            # directly - the CLASSIFY anti-pattern HARNECIENT-HACK.md
            # warns against, and it was unreliable (multiple prompt
            # rewrites, one non-word reply). Fixed to DESCRIBE instead:
            # ask for a plain-text description of the task's shape (no
            # classification words at all), then resolve deterministically
            # via complexity_signal_count() (real comma/"and" counting on
            # the model's own words, not a self-reported category) -
            # verified 4/4 correct across two classifier models on two
            # real test tasks before being trusted here.
            local task_desc="$task"
            local description
            description="$(query_ollama_for_task_description "$task_desc")"
            local signals
            signals="$(complexity_signal_count "$description")"
            log "step $step_n: choose_model: task='$task_desc' model_description='$description' signal_count=$signals (threshold=$COMPLEXITY_THRESHOLD)"
            local chosen_model
            if [ "$signals" -ge "$COMPLEXITY_THRESHOLD" ]; then
                chosen_model="$COMPLEX_TASK_MODEL"
            else
                chosen_model="$SIMPLE_TASK_MODEL"
            fi
            log "step $step_n: choose_model: $signals signal(s) -> $chosen_model"
            if switch_to_model "$chosen_model"; then
                echo "CHOOSE_MODEL $step_n [$task_desc]: PASS (classified -> $chosen_model)" >> "$summary"
                log_observation "CHOOSE_MODEL" "$task_desc" "$chosen_model" "-" "PASS"
                log "step $step_n: PASS ($chosen_model)"
                continue
            else
                echo "CHOOSE_MODEL $step_n [$task_desc]: FAIL (classified -> $chosen_model, but switch failed, see stderr)" >> "$summary"
                log_observation "CHOOSE_MODEL" "$task_desc" "$chosen_model" "-" "FAIL-SWITCH"
                log "step $step_n: FAIL (switch to $chosen_model failed)"
                overall="FAIL"
                break
            fi
        fi

        if [ "$kind" = "BRANCH" ]; then
            # "task" is already the condition (field 2 of the plan
            # line, the outer `read -r kind task assertion` already
            # split it correctly) - "assertion" holds the REMAINING
            # "true-plan | false-plan" (read's leftover-field behavior
            # preserved that one pipe). Real bug found and fixed
            # 2026-08-13: originally tried to split THREE fields out of
            # assertion, double-counting the condition that was already
            # correctly in $task - always resolved to the false branch
            # regardless of the real condition.
            local cond="$task" true_plan false_plan
            IFS='|' read -r true_plan false_plan <<< "$assertion"
            true_plan="$(echo "$true_plan" | sed 's/^ *//; s/ *$//')"
            false_plan="$(echo "$false_plan" | sed 's/^ *//; s/ *$//')"
            [ "${true_plan:0:1}" != "/" ] && true_plan="$plan_dir/$true_plan"
            [ "${false_plan:0:1}" != "/" ] && false_plan="$plan_dir/$false_plan"

            log "step $step_n: branch: $cond"
            local chosen
            if eval "$cond"; then
                chosen="$true_plan"
                log "step $step_n: branch condition TRUE -> $chosen"
            else
                chosen="$false_plan"
                log "step $step_n: branch condition FALSE -> $chosen"
            fi
            if run_plan_file "$chosen" "$((depth + 1))" > "$run_dir/step${step_n}.branch.log" 2>&1; then
                echo "BRANCH $step_n [$cond -> $chosen]: PASS" >> "$summary"
                log_observation "BRANCH" "$cond -> $chosen" "-" "-" "PASS"
                log "step $step_n: PASS"
                continue
            else
                echo "BRANCH $step_n [$cond -> $chosen]: FAIL (see $run_dir/step${step_n}.branch.log)" >> "$summary"
                log_observation "BRANCH" "$cond -> $chosen" "-" "-" "FAIL"
                log "step $step_n: FAIL"
                overall="FAIL"
                break
            fi
        fi

        if [ "$kind" = "LOOP_UNTIL" ]; then
            local max_iters="$task" sub_plan="$assertion"
            [ "${sub_plan:0:1}" != "/" ] && sub_plan="$plan_dir/$sub_plan"
            local iter=0 loop_ok=0
            while [ "$iter" -lt "$max_iters" ]; do
                iter=$((iter + 1))
                log "step $step_n: loop_until: iteration $iter/$max_iters of $sub_plan"
                if run_plan_file "$sub_plan" "$((depth + 1))" > "$run_dir/step${step_n}.loop${iter}.log" 2>&1; then
                    loop_ok=1
                    log_observation "LOOP_UNTIL" "$sub_plan iter $iter" "-" "-" "PASS"
                    break
                else
                    log_observation "LOOP_UNTIL" "$sub_plan iter $iter" "-" "-" "FAIL"
                fi
            done
            if [ "$loop_ok" -eq 1 ]; then
                echo "LOOP_UNTIL $step_n [$sub_plan]: PASS (iteration $iter/$max_iters)" >> "$summary"
                log "step $step_n: PASS (iteration $iter/$max_iters)"
                continue
            else
                echo "LOOP_UNTIL $step_n [$sub_plan]: FAIL ($max_iters iterations exhausted, see $run_dir/step${step_n}.loop*.log)" >> "$summary"
                log "step $step_n: FAIL ($max_iters iterations exhausted)"
                overall="FAIL"
                break
            fi
        fi

        # A trailing "| APPROVE" or "| DENY" field (only meaningful for
        # STEP) opts INTO resolving a mutating tool's pending
        # approve/deny - explicit, per-step, fail-closed otherwise.
        local approve_flag=""
        case "$assertion" in
            *"| APPROVE") approve_flag="APPROVE"; assertion="${assertion% | APPROVE}" ;;
            *"|APPROVE") approve_flag="APPROVE"; assertion="${assertion%|APPROVE}" ;;
            *"| DENY") approve_flag="DENY"; assertion="${assertion% | DENY}" ;;
            *"|DENY") approve_flag="DENY"; assertion="${assertion%|DENY}" ;;
        esac

        local resolved_label="-" resolved_idx="-" out_file=""
        if [ "$kind" = "STEP" ]; then
            log "step $step_n: delegate: $task${approve_flag:+ ($approve_flag authorized)}"
            if ! delegate_step "$task" "$approve_flag"; then
                echo "STEP $step_n [$task]: FAIL (dispatch timeout, or a mutating tool needed APPROVE/DENY and didn't have it)" >> "$summary"
                log_observation "STEP" "$task" "-" "-" "FAIL-TIMEOUT-OR-UNAPPROVED"
                overall="FAIL"
                break
            fi
            out_file="$run_dir/step${step_n}.out"
            capture_last_output "$out_file"
            export "STEP${step_n}_OUT=$out_file"
        else
            log "step $step_n: navigate: $task"
            local pre_file="$run_dir/step${step_n}.pre" post_file="$run_dir/step${step_n}.out"
            local target_file="$run_dir/step${step_n}.target" label_file="$run_dir/step${step_n}.label"
            if ! navigate_step "$task" "$pre_file" "$post_file" "$target_file" "$label_file"; then
                echo "NAVIGATE $step_n [$task]: FAIL (no resolve or dispatch error - fail-closed, nothing was pressed)" >> "$summary"
                log_observation "NAVIGATE" "$task" "-" "-" "FAIL-CLOSED"
                overall="FAIL"
                break
            fi
            out_file="$post_file"
            resolved_idx="$(cat "$target_file" 2>/dev/null)"
            resolved_label="$(cat "$label_file" 2>/dev/null)"
            export "STEP${step_n}_OUT=$post_file"
            export "STEP${step_n}_PRE=$pre_file"
            export "STEP${step_n}_TARGET=$resolved_idx"
            export "STEP${step_n}_LABEL=$resolved_label"
        fi

        log "step $step_n: verifying: $assertion"
        local verified=0
        if eval "$assertion"; then
            verified=1
        fi

        local retry_n=0
        while [ "$verified" -eq 0 ] && [ "$kind" = "STEP" ] && [ "$retry_n" -lt "$MAX_RETRIES" ]; do
            retry_n=$((retry_n + 1))
            log "step $step_n: assertion failed, asking for retry suggestion ($retry_n/$MAX_RETRIES)"
            local suggestion
            suggestion="$(query_ollama_for_retry_suggestion "$task" "$assertion")"
            if [ -z "$suggestion" ]; then
                log "step $step_n: retry $retry_n: empty suggestion, stopping retries"
                log_observation "STEP-RETRY" "$task" "-" "-" "FAIL-EMPTY-SUGGESTION"
                break
            fi
            log "step $step_n: retry $retry_n: trying '$suggestion'"
            if ! delegate_step "$suggestion" "$approve_flag"; then
                log_observation "STEP-RETRY" "$suggestion" "-" "-" "FAIL-DISPATCH"
                continue
            fi
            out_file="$run_dir/step${step_n}.retry${retry_n}.out"
            capture_last_output "$out_file"
            export "STEP${step_n}_OUT=$out_file"
            if eval "$assertion"; then
                verified=1
                task="$suggestion (retry $retry_n of original: $task)"
                log_observation "STEP-RETRY" "$suggestion" "-" "-" "PASS"
            else
                log_observation "STEP-RETRY" "$suggestion" "-" "-" "FAIL-ASSERT"
            fi
        done

        if [ "$verified" -eq 1 ]; then
            echo "$kind $step_n [$task]: PASS (output: $out_file)" >> "$summary"
            log_observation "$kind" "$task" "$resolved_label" "$resolved_idx" "PASS"
            log "step $step_n: PASS"
        else
            echo "$kind $step_n [$task]: FAIL (assertion: $assertion; output: $out_file)" >> "$summary"
            log_observation "$kind" "$task" "$resolved_label" "$resolved_idx" "FAIL-ASSERT"
            log "step $step_n: FAIL"
            overall="FAIL"
            break
        fi
    done < "$plan_file"

    {
        echo ""
        echo "finished: $(date)"
        echo "overall: $overall"
    } >> "$summary"

    log "plan $plan_file (depth $depth) done - overall $overall - summary at $summary"
    echo "$run_dir"
    [ "$overall" = "PASS" ]
}

# ---- top-level entry ----
ensure_open_hai
run_dir_out="$(run_plan_file "$PLAN_FILE" 0)"
rc=$?
cat "$run_dir_out/SUMMARY.txt" 2>/dev/null
exit $rc
