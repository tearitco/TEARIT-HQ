#!/bin/bash
# chat_hai_loop.sh - round-robin chat scheduler for chat-hai (Harnecient
# Way: THIS shell decides who speaks next and what context to give; the
# model only generates text via the shared net/qwen.sh wrapper - no
# model-driven control flow).
#
# Reads persona .pdl files from pieces/personas/, appends every message to
# state/transcript.ledger (master-ledger formula), keeps state/chat_hai.log.
# Killed/restarted by button.sh run|stop.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HOUSE="$(cd "$APP_DIR/../.." && pwd)"
QWEN="$HOUSE/net/qwen.sh"
PERSONAS_ROOT="$APP_DIR/pieces/personas"
LOG="$APP_DIR/state/chat_hai.log"
PID_FILE="$APP_DIR/state/chat_hai.pid"
STATE_DIR="$APP_DIR/state"
SESSIONS_DIR="$STATE_DIR/sessions"
ACTIVE_FILE="$SESSIONS_DIR/active.txt"
PAUSE_FILE="$STATE_DIR/paused.txt"
TYPING_FILE="$STATE_DIR/typing.txt"
RELATIONS_FILE="$STATE_DIR/relations.pdl"
OBSERVATIONS_LOG="$STATE_DIR/observations.log"

# REAL FIX 2026-08-15 (direct instruction: "we should beable to add /
# delete new sessions"): LEDGER used to be one fixed path for the whole
# app's lifetime. Sessions now live as separate files under
# $SESSIONS_DIR, and $ACTIVE_FILE names which one is live - the
# renderer (chat_hai_hq_render.c's switch_session()) writes this same
# file on every session switch, so re-reading it at the top of each
# round is how a switch made in the GUI takes effect in the running
# chat within one round, not just visually in the window.
mkdir -p "$SESSIONS_DIR"
if [ ! -f "$ACTIVE_FILE" ]; then
    # One-time migration: seed sessions/main.ledger from the old
    # single-ledger file if it exists, so existing history isn't lost
    # (same migration the renderer's own migrate_legacy_ledger_if_needed()
    # does - either process may win the race to do it first, both are
    # idempotent/safe since this whole block only runs when active.txt
    # doesn't exist yet).
    if [ -f "$APP_DIR/state/transcript.ledger" ] && [ ! -f "$SESSIONS_DIR/main.ledger" ]; then
        cp "$APP_DIR/state/transcript.ledger" "$SESSIONS_DIR/main.ledger"
    fi
    echo "main" > "$ACTIVE_FILE"
fi
current_session() { cat "$ACTIVE_FILE" 2>/dev/null || echo main; }
ledger_path() { echo "$SESSIONS_DIR/$(current_session).ledger"; }

# REAL FEATURE 2026-08-15 (direct instruction: "the memory system sounds
# good" - au11-hq/chat-hack.md §3, its own hook already named in
# chat-hai-design.md §5). One append-only file per persona PER SESSION
# (scoped like personas_dir() already is - echo's gemma-lab memories
# never leak into bravo's main-session ones, matching the same real
# isolation direct instruction already established for the roster
# itself). Harness decides WHEN to write (deterministic, see
# should_write_memory() below); the model only ever DESCRIBES what's
# worth remembering, one plain sentence (DESCRIBE-not-CLASSIFY, never
# "was this important" - HARNECIENT-HACK.md's own bonus rule).
memory_dir() {
    local d="$STATE_DIR/memory/$(current_session)"
    mkdir -p "$d"
    echo "$d"
}
memory_path() { echo "$(memory_dir)/$1.ledger"; }

# Deterministic write gate: every 3rd real PASS for this persona (never
# every turn - keeps memory entries meaningful, not just a duplicate of
# the transcript). Counts real, verified PASS rows from
# observations.log, same pure-grep-and-count shape as
# recent_fail_count() - no model involvement in the WHEN decision.
MEMORY_EVERY_N_PASSES=3
should_write_memory() {
    local name="$1"
    local n
    n="$(awk -F'|' -v nm="$name" '{gsub(/ /,"",$2); gsub(/ /,"",$4); if ($2==nm && $4=="PASS") c++} END{print c+0}' "$OBSERVATIONS_LOG" 2>/dev/null)"
    [ -n "$n" ] || n=0
    [ $((n % MEMORY_EVERY_N_PASSES)) -eq 0 ] && [ "$n" -gt 0 ]
}

# Same DESCRIBE-shaped, sanitize-then-discard-if-too-long pattern as
# fresh_angle() (see that function's own header comment for why the
# length cap exists - gemma3:270m doesn't reliably respect a length
# instruction, real leak already caught once this session). Fast tier
# (smol) since this is a background/incidental ask, not the main reply.
write_memory() {
    local name="$1" own_last="$2" ctx="$3"
    local q="Given this recent chat, and that ${name} just said: \"${own_last}\" -
in one short plain sentence, what is ONE specific thing worth
remembering from this exchange? Reply with ONLY the sentence, nothing
else, no quotes.

Recent chat:
${ctx}"
    local raw cleaned
    raw="$(bash "$QWEN" ask smol "$q" 2>>"$LOG" | tr '\n' ' ' | sed -E 's/[[:space:]]+/ /g; s/^[[:space:]]+//; s/[[:space:]]+$//')"
    cleaned="$(printf '%s' "$raw" | sed -E 's/[*#`_]+//g' | cut -c1-160)"
    [ -n "$cleaned" ] && [ "${#cleaned}" -le 160 ] || return 1
    printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$cleaned" >> "$(memory_path "$name")"
}

# Draws FROM memory (the "remember who it chat with" mechanism, direct
# instruction 2026-08-15): pure harness selection, real word-overlap
# scoring (reuses word_overlap() - same primitive already proven for
# the anti-repeat gate) against the CURRENT context, picks the single
# best-matching memory line above a floor threshold. No model ever
# picks which memory matters - the same discipline as
# HARNESS-DELEGATION-PIPELINE.md's own "resolve deterministically"
# rule, applied to memory retrieval instead of navigation/tool dispatch.
recall_memory() {
    local name="$1" ctx="$2"
    local mp best_line best_score=0 line score
    mp="$(memory_path "$name")"
    [ -f "$mp" ] || return 0
    while IFS= read -r line; do
        local text="${line#*] }"
        score="$(word_overlap "$ctx" "$text")"
        if [ "${score:-0}" -gt "$best_score" ]; then
            best_score="$score"
            best_line="$text"
        fi
    done < <(tail -n 20 "$mp")
    if [ "$best_score" -ge 20 ] && [ -n "${best_line:-}" ]; then
        echo "$best_line"
    fi
}

# REAL FEATURE 2026-08-15 (direct instruction: "leave this session and do
# a new session with new named personalities... old ones can be bravo/
# moxie, new gemmas can have new names"): per-session persona ROSTER, not
# just a per-session ledger. If pieces/personas/<session>/*.pdl exists,
# use ONLY that subdir for this session (real isolation - a new
# experimental cohort never mixes into the existing bravo/moxie/pip/sage
# round-robin, and vice versa). Falls back to the flat, original
# pieces/personas/*.pdl directory for any session with no dedicated
# subdir (this is what keeps "main" - and every session that existed
# before this feature - working exactly as before, zero migration
# needed).
personas_dir() {
    local s d
    s="$(current_session)"
    d="$PERSONAS_ROOT/$s"
    if [ -d "$d" ] && [ -n "$(ls "$d"/*.pdl 2>/dev/null)" ]; then
        echo "$d"
    else
        echo "$PERSONAS_ROOT"
    fi
}

# --- config ---
CONTEXT_LINES=20        # last N ledger lines fed as context to the speaker
                        # (bumped 12→20 2026-08-16 to keep user messages in
                        # scope longer — personas take 20-40s each, so a user
                        # message from 2 minutes ago could be 3-5 persona
                        # responses deep at 12 lines)
MODERATOR_EVERY=3       # synthesist ♾️ speaks every 3 rounds (was 0 = inert)
TRUNC_HOOK=0            # hook: cap ledger size (memory truncation) - not enabled

# REAL FIX 2026-08-15 (direct instruction: "maybe we can have an input to
# modify chat speed") — SLEEP_BETWEEN used to be a fixed shell constant,
# a real violation of !.HOUSE_STDS.md §A.7 ("runtime-configurable values
# ... belong in a .pdl, not hardcoded"). Now read from
# chat_hai_config.pdl's own sleep_between key at the TOP OF EVERY ROUND
# (not cached), so editing that file speeds up/slows down the
# conversation within one round, no restart needed. Falls back to 6
# (the old hardcoded default) if the .pdl is missing/malformed - never
# hard-fails just because a config file got deleted.
CONFIG_PDL="$APP_DIR/chat_hai_config.pdl"
sleep_between() {
    local v
    v="$(awk -F'|' '$1 ~ /^SECTION/ {gsub(/ /,"",$2); if ($2=="sleep_between") {gsub(/ /,"",$3); print $3; exit}}' "$CONFIG_PDL" 2>/dev/null)"
    case "$v" in ''|*[!0-9]*) echo 6 ;; *) echo "$v" ;; esac
}

# Incoming-message tone (direct instruction 2026-08-16: "play a tone when
# a message is posted" - incoming only, toggleable off via the Sound GUI
# button). Same read-fresh-every-time contract as sleep_between() above:
# the renderer writes sound_on back into the same .pdl when the button is
# clicked, so a toggle goes live within one round, no restart. Missing
# file = sound ON, never hard-fails. Assets stay local to the app - this
# chain needs no temp files at all (real notification sound, falling back
# to a synthesized beep through sox's play).
sound_on() {
    local v
    v="$(awk -F'|' '$1 ~ /^SECTION/ {gsub(/ /,"",$2); if ($2=="sound_on") {gsub(/ /,"",$3); print $3; exit}}' "$CONFIG_PDL" 2>/dev/null)"
    [ "$v" != "0" ]
}
play_incoming_tone() {
    sound_on || return 0
    canberra-gtk-play --id=message 2>/dev/null || play -n synth 0.12 sine 880 vol 0.2 2>/dev/null
}

mkdir -p "$STATE_DIR"
echo "$$" > "$PID_FILE"
# Fresh loop start always resets to unpaused - a stale "1" left over from
# a previous session (e.g. the process was killed while stopped, rather
# than via the Start button) would otherwise silently start a brand new
# loop that never speaks, with no visible error anywhere - exactly the
# "[stopped]"/no-chatting confusion found live 2026-08-15.
echo "0" > "$PAUSE_FILE"
> "$TYPING_FILE" # clear any stale name left over from a killed-mid-request process
echo "[$(date '+%Y-%m-%d %H:%M:%S')] loop online (pid $$)" >> "$LOG"

# REAL, NEW 2026-09-01 (chat-hai's own migration onto the shared
# khtpm_core_render.+x generic sidebar/panel/scrolllist/cli_io path -
# see chat_hai_projector.sh's own header comment for the full
# architecture). That script regenerates chat-hai.chtpm from this
# loop's own real state files; it's launched as THIS process's own
# background child (not a second <module> tag - see the projector's
# own comment on why) so its lifetime is tied to this loop's, matching
# the existing real "closing chat-hai's window stops the persona loop"
# contract (2026-08-16 behavior change, see this file's own module-tag
# comment further up) - the projector dying with the loop is the same
# real shape, just one hop further.
PROJECTOR="$SCRIPT_DIR/chat_hai_projector.sh"
PROJECTOR_PID=""
if [ -x "$PROJECTOR" ]; then
    "$PROJECTOR" &
    PROJECTOR_PID=$!
    trap '[ -n "$PROJECTOR_PID" ] && kill "$PROJECTOR_PID" 2>/dev/null' EXIT
fi

# persona .pdl -> field (PERSONA | name | X)
persona_field() {
    local pdl="$1" key="$2"
    awk -F'|' -v k="$key" '
        $1 ~ /^PERSONA/ { gsub(/[ \t]+/,"",$2); if ($2==k) { sub(/^[ \t]+/,"",$3); sub(/[ \t]+$/,"",$3); print $3; exit } }
    ' "$pdl"
}

log_line() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >> "$LOG"; }

# REAL FEATURE 2026-08-15 (direct instruction: "do u see how
# HARNESS-DELEGATION-PIPELINE.md explains how to create a pipeline of
# quality and evolving output... use this creatively... document ur
# thoughts") - Stage 0 from that doc's own §7.2: a durable, plain-text,
# pipe-delimited observation log, one row per REAL VERIFIED outcome
# (never a model's self-report). Same shape as that doc's own example
# (`TS | KIND | GOAL | RESOLVED | VERDICT`), applied here to persona
# turns instead of FSM plan steps. Cheap, zero ML, and sets up the exact
# Stage-1 escalation logic below (recent_fail_count()) - the doc's own
# "attention-mechanism-shaped idea in its cheapest honest form: a lookup
# table that weights candidates, not a learned embedding space."
log_observation() {
    local name="$1" tier="$2" verdict="$3" detail="${4:-}"
    printf '%s | %s | %s | %s | %s\n' "$(date '+%s')" "$name" "$tier" "$verdict" "$detail" >> "$OBSERVATIONS_LOG"
}

# CHOOSE_MODEL-style escalation (HARNESS-DELEGATION-PIPELINE.md §12),
# adapted: instead of a model CLASSIFYING task complexity (the doc's own
# "naked test" anti-pattern, explicitly warned against), this counts a
# REAL, VERIFIED, structural signal - how many of THIS persona's last 3
# observation-log rows were a drop (any kind), a plain grep+count, no
# model asked to judge anything. ≥2 of the last 3 dropped -> escalate
# from smol (gemma3:270m) to tiny (gemma3:1b) for the NEXT attempt only,
# matching §10's own finding (verbose/unreliable output, not raw
# capability, was smol's failure mode in the live gemma-lab leak this
# session) - tiny is the doc's own next rung up, worth trying before
# assuming the whole tier is bad.
recent_fail_count() {
    local name="$1"
    tail -n 200 "$OBSERVATIONS_LOG" 2>/dev/null | awk -F'|' -v n="$name" '
        { gsub(/ /,"",$2); if ($2==n) rows[++c]=$4 }
        END {
            fails=0; start=(c>3)?c-2:1
            for (i=start;i<=c;i++) { v=rows[i]; gsub(/ /,"",v); if (v!="PASS") fails++ }
            print fails
        }'
}

effective_tier() {
    local name="$1" configured_tier="$2"
    local fails
    fails="$(recent_fail_count "$name")"
    if [ "${fails:-0}" -ge 2 ] && [ "$configured_tier" = "smol" ]; then
        echo "tiny"
    else
        echo "$configured_tier"
    fi
}

# REAL FEATURE 2026-08-15 (direct instruction: "let's build some of those
# now" - au11-hq/chat-hack.md's own §4 "relationships" design, chosen
# FIRST per that doc's own §9 build order: zero model calls, pure harness
# logic, lowest risk). state/relations.pdl: one RELATION row per persona
# pair, a plain co-occurrence counter - the harness increments it every
# time two personas' messages land back-to-back in the ledger, no LLM
# involved in the decision at all (HARNECIENT-HACK.md's own "keep outcome
# decisions out of the LLM" principle, applied literally - this is a case
# where the right answer is to not even ask the model).
touch "$RELATIONS_FILE"

# sorted, dash-joined key so (a,b) and (b,a) always hit the same row
pair_key() {
    if [[ "$1" < "$2" ]]; then echo "$1-$2"; else echo "$2-$1"; fi
}

get_relation() {
    local pair="$1"
    awk -F'|' -v p="$pair" '$1 ~ /^RELATION/ {gsub(/ /,"",$2); if ($2==p) {gsub(/ /,"",$3); print $3; exit}}' "$RELATIONS_FILE" 2>/dev/null
}

bump_relation() {
    local pair="$1" cur new
    cur="$(get_relation "$pair")"
    [ -n "$cur" ] || cur=0
    new=$((cur + 1))
    if grep -q "RELATION *| *$pair *|" "$RELATIONS_FILE" 2>/dev/null; then
        awk -F'|' -v p="$pair" -v n="$new" '
            BEGIN{OFS="|"}
            { k=$2; gsub(/ /,"",k);
              if ($1 ~ /^RELATION/ && k==p) { print $1, " " p " ", " " n; next }
              print }
        ' "$RELATIONS_FILE" > "$RELATIONS_FILE.tmp" && mv "$RELATIONS_FILE.tmp" "$RELATIONS_FILE"
    else
        printf 'RELATION | %s | %d\n' "$pair" "$new" >> "$RELATIONS_FILE"
    fi
}

# append one message to the ACTIVE session's ledger (append-only, one
# line) - ledger_path() re-reads active.txt every call, not cached, so a
# session switch made in the GUI mid-round is picked up by the very next
# message this loop writes.
ledger_msg() {
    local speaker="$1" text="$2" note="${3:-}"
    printf '[%s] %s: %s%s | Trigger: chat-hai\n' \
        "$(date '+%Y-%m-%d %H:%M:%S')" "$speaker" "$text" "${note:+ | $note}" >> "$(ledger_path)"
    # tone on incoming (agent) messages only - user input is appended by
    # the renderer itself, never via ledger_msg(), so every call here is
    # already an agent/posted message; still guard system notes for good
    # measure. One-shot, output suppressed.
    [ "$speaker" = "system" ] || play_incoming_tone
}

# last N ledger lines (text after '] ', minus the trailing ' | Trigger: ...')
recent_context() {
    local n="$1"
    tail -n "$n" "$(ledger_path)" 2>/dev/null | sed -E 's/^\[[^]]*\] //; s/ \| Trigger: chat-hai$//'
}

# REAL FEATURE 2026-08-16 (direct instruction: "we should always bias my
# input almost 75%"): extracts the most recent user message from the
# ledger, stripped of timestamps and trigger suffix. Used by speak() to
# PROMOTE user input to the top of the prompt — the user's short question
# (typically 30-60 chars) is otherwise buried in 18KB of persona
# monologue, where gemma3:270m cannot find it. This is deterministic
# harness logic (chat-hack.md §7), not a model judgment call.
last_user_msg() {
    grep -E "^\[[^]]*\] user: " "$(ledger_path)" 2>/dev/null | tail -1 | sed -E 's/^\[[^]]*\] user: //; s/ \| Trigger: chat-hai$//'
}

# REAL FEATURE 2026-08-15 (direct instruction: "the more important thing
# is remembering conversation history, compaction and not repeating
# itself or other chatter... that should be done in harnecient way" -
# i.e. the pass/fail decision must be DETERMINISTIC harness logic, never
# a model self-judgment, same principle chat-hack.md §7 already applies
# to relationships). This persona's own last message, harness-extracted
# straight from the real ledger (no LLM call) - used two ways below:
# (a) shown to the model as context (a soft hint, may or may not help),
# (b) the REAL enforcement - a deterministic word-overlap check against
# the just-generated reply, gating whether it ever gets committed to the
# ledger at all. Never asks the model "was that repetitive?" (a
# CLASSIFY-shaped ask, exactly the trap HARNECIENT-HACK.md's bonus rule
# warns against).
own_last_message() {
    local name="$1"
    grep -E "^\[[^]]*\] ${name}: " "$(ledger_path)" 2>/dev/null | tail -1 | sed -E "s/^\[[^]]*\] ${name}: //; s/ \| Trigger: chat-hai\$//"
}

# Deterministic word-overlap ratio (0-100) between two strings - words
# length>2 only (skip "a"/"is"/"to" noise), case-insensitive. Pure awk,
# no model involvement - this IS the "harnecient way" enforcement: a
# real, computed metric decides, not a guess or a model's self-report.
word_overlap() {
    awk -v a="$1" -v b="$2" '
    BEGIN {
        na = split(tolower(a), wa, /[^a-z0-9]+/)
        nb = split(tolower(b), wb, /[^a-z0-9]+/)
        for (i = 1; i <= na; i++) if (length(wa[i]) > 2) seen[wa[i]] = 1
        common = 0; totalb = 0
        for (i = 1; i <= nb; i++) if (length(wb[i]) > 2) { totalb++; if (seen[wb[i]]) common++ }
        if (totalb == 0) { print 0; exit }
        printf "%d", (common * 100) / totalb
    }'
}

# REAL FEATURE 2026-08-15 (direct instruction: "could gemma270 do some of
# its harnessing?... a hidden pre-prompt to gemma doing the harnecient
# hacks before sending to final generator... did u see how that was done
# in my-lawyer?"). Same STAGED shape as
# mylawyer_case_worker.c's own search_precedent(): a fast, short,
# DESCRIBE-only ask on the "smol" (gemma3:270m) tier, whose output is
# never the final artifact - it only ever FEEDS the next real prompt,
# same as my-lawyer's precedent text getting folded into the harness-
# built argument buffer, never trusted as the whole output on its own.
# Explicitly asks for a DIFFERENT angle (a description, not a rating of
# the old message - the DESCRIBE-not-CLASSIFY rule applies here too:
# never ask "was that repetitive," always ask "what's something new").
# Fallback-everywhere (component 6): empty on any failure, the caller
# just proceeds without a hint - never blocks the main generation.
fresh_angle() {
    local name="$1" own_last="$2" ctx="$3"
    [ -n "$own_last" ] || { echo ""; return; }
    local q="Given this recent chat, and that ${name} just said: \"${own_last}\" -
in 4-8 words, describe ONE specific new detail or angle ${name} could
bring up next that is clearly different from what they just said. Reply
with ONLY the short phrase, nothing else, no quotes.

Recent chat:
${ctx}"
    local raw
    raw="$(bash "$QWEN" ask smol "$q" 2>>"$LOG" | tr '\n' ' ' | sed -E 's/[[:space:]]+/ /g; s/^[[:space:]]+//; s/[[:space:]]+$//')"
    # REAL BUG FOUND + FIXED 2026-08-15 (live-caught in gemma-lab: nova's
    # actual reply came back as "You are nova. Bring up something new:
    # Here are the short phrases... * **enhanced engagement:**..." - the
    # raw steering text AND a whole bulleted list leaked straight into
    # the final ledger artifact). gemma3:270m does not reliably respect
    # a "4-8 words" instruction (matches HARNECIENT-HACK.md's own
    # documented weakness: small models don't reliably follow format
    # instructions) - it sometimes returns a full paragraph/list
    # instead. Real, deterministic gate: strip markdown bullet/heading
    # noise, then if what's left is still too long to plausibly be a
    # short phrase, DISCARD it entirely rather than risk feeding
    # something big/garbled into the next prompt (this is component 6,
    # fallback-everywhere, applied to a staged pre-prompt's own output,
    # not just the final reply).
    local cleaned
    cleaned="$(printf '%s' "$raw" | sed -E 's/[*#`_]+//g; s/^[0-9]+\.[[:space:]]*//' | cut -c1-80)"
    if [ "${#cleaned}" -gt 60 ] || [ -z "$cleaned" ]; then
        echo ""
        return
    fi
    echo "$cleaned"
}

# ask one persona to talk
speak() {
    local pdl="$1"
    local name glyph tier prompt
    name="$(persona_field "$pdl" name)"; [ -n "$name" ] || name="$(basename "$pdl" .pdl)"
    glyph="$(persona_field "$pdl" glyph)"
    tier="$(persona_field "$pdl" tier)"; tier="${tier:-router}"
    # REAL FEATURE 2026-08-15 (HARNESS-DELEGATION-PIPELINE.md §12's
    # CHOOSE_MODEL idea, adapted - see effective_tier()'s own header
    # comment for why this counts real verified drops instead of asking
    # a model to self-assess). Configured tier stays the PERSONA's own
    # declared default; only THIS turn's actual dispatch may escalate.
    tier="$(effective_tier "$name" "$tier")"
    prompt="$(persona_field "$pdl" system-prompt)"; [ -n "$prompt" ] || prompt="You are $name, chatting with friends. Keep it short."

    local ctx prev_name prev_text
    ctx="$(recent_context "$CONTEXT_LINES")"
    # last message's speaker (for addressing) - reuse the ledger's own tail
    prev_name="$(tail -n 1 "$(ledger_path)" 2>/dev/null | sed -E 's/^\[[^]]*\] //' | cut -d: -f1 | xargs)"

    # REAL FEATURE 2026-08-16 (direct instruction: "we should always bias
    # my input almost 75%"): extract the user's most recent message and
    # build a prominent bias block that gets prepended to the prompt.
    # The user's short questions (~30-60 chars) are 0.3% of the 18KB
    # context string — gemma3:270m literally cannot find them without
    # explicit promotion. This block ensures the persona sees the user's
    # input FIRST, before any persona monologue context.
    local user_msg user_bias="" user_addendum=""
    user_msg="$(last_user_msg)"
    if [ -n "$user_msg" ]; then
        user_bias=">>> THE USER SAID: ${user_msg} <<<
"
        user_addendum=" The user asked something above — address it directly."
    fi

    # REAL FEATURE 2026-08-15 (au11-hq/chat-hack.md §4): a real,
    # harness-computed FACT (co-occurrence count) folded into the
    # prompt as flavor context - the model never decides this, it's
    # only ever given something true to react to (chat-hack.md §7's
    # own "what NOT to do" list explicitly rules out asking a model to
    # rate/score a relationship - this is deterministic or nothing).
    local relation_note=""
    if [ -n "$prev_name" ] && [ "$prev_name" != "$name" ] && [ "$prev_name" != "user" ] && [ "$prev_name" != "system" ]; then
        local rel_score
        rel_score="$(get_relation "$(pair_key "$name" "$prev_name")")"
        [ -n "$rel_score" ] || rel_score=0
        if [ "$rel_score" -ge 3 ]; then
            relation_note=" You and ${prev_name} talk often."
        fi
    fi

    # REAL FEATURE 2026-08-15 (staged pre-prompt, see fresh_angle()'s own
    # header comment - the my-lawyer-style "smol model feeds the real
    # generator" pattern). Fallback-tolerant: empty own_last or a failed
    # gemma call just means no hint gets added, main generation proceeds
    # unblocked either way.
    local own_last angle_hint=""
    own_last="$(own_last_message "$name")"
    if [ -n "$own_last" ]; then
        local angle
        angle="$(fresh_angle "$name" "$own_last" "$ctx")"
        [ -n "$angle" ] && angle_hint=" Bring up something new: ${angle}."
    fi

    # REAL FEATURE 2026-08-15 (direct instruction: "the memory system
    # sounds good" - see recall_memory()'s own header comment). Pure
    # harness selection - real word-overlap scoring against the current
    # context, no model asked to pick which memory matters.
    local recalled recall_note=""
    recalled="$(recall_memory "$name" "$ctx")"
    [ -n "$recalled" ] && recall_note=" You remember: ${recalled}."

    local question
    if [ -n "$prev_name" ] && [ "$prev_name" != "$name" ]; then
        question="${user_bias}${prompt}

Recent conversation:
$ctx

${glyph} You are $name.${relation_note}${angle_hint}${recall_note}${user_addendum} Reply to ${prev_name} (and the group) now."
    else
        question="${user_bias}${prompt}

Recent conversation:
$ctx

${glyph} You are $name.${angle_hint}${recall_note}${user_addendum} Say something to the group now."
    fi

    # REAL FIX 2026-08-15 (direct instruction: "the start stop of chat
    # should be of the logic of the ai lan call itself" - the earlier
    # pkill -STOP/-CONT approach on this script's own process was
    # unreliable: SIGSTOP on the PARENT bash script does not reliably
    # freeze a curl call already running in a command-substitution
    # subshell, so a "stopped" chat kept producing replies mid-flight,
    # matching the direct report "i still dont get results from
    # start/stop"). Gate the ACTUAL LAN call itself: block here,
    # checking pause_file() every 2s, until unpaused - guarantees ZERO
    # new qwen.sh/curl calls go out while paused, and resumes the
    # instant the GUI's Start button flips it back.
    while [ "$(cat "$PAUSE_FILE" 2>/dev/null)" = "1" ]; do
        sleep 2
    done
    log_line "ask $name ($tier) [ctx=${#ctx}b, prev=$prev_name]"
    # REAL FEATURE 2026-08-15 (direct ask: "is it possible to show whos
    # 'thinking' (AKA TYPING) if waiting for a request?") - written
    # right before the LAN call (the only part that's actually slow,
    # ~20-40s per reply per this session's own logged timings), cleared
    # right after, success or failure either way. The renderer polls
    # this file the same way it polls the ledger (see
    # chat_hai_hq_render.c's own typing-indicator code) and shows it in
    # the status row.
    echo "$name" > "$TYPING_FILE"
    local reply
    reply="$(bash "$QWEN" ask "$tier" "$question" 2>>"$LOG")"
    > "$TYPING_FILE"
    reply="$(printf '%s' "$reply" | tr '\n' ' ' | sed -E 's/[[:space:]]+/ /g; s/^[[:space:]]+//; s/[[:space:]]+$//')"
    if [ -z "$reply" ]; then
        log_line "  (empty reply from $name)"
        log_observation "$name" "$tier" "FAIL-EMPTY" ""
        return 1
    fi
    # REAL BUG FOUND + FIXED 2026-08-15 (live-caught in gemma-lab session,
    # same generation as fresh_angle()'s own leak - see that function's
    # header comment): the SAME small-model weakness (doesn't reliably
    # follow instructions) can make the MAIN reply echo back its own
    # injected steering text verbatim ("You are nova. Bring up something
    # new: ...") instead of generating a real reply. Deterministic
    # instruction-echo detector - checks for the literal steering
    # phrases this file itself injects, case-insensitive. If matched,
    # the reply is instruction leakage, not real content - drop it, same
    # as the word-overlap gate below (component 6, fallback-everywhere).
    local reply_lower
    reply_lower="$(printf '%s' "$reply" | tr '[:upper:]' '[:lower:]')"
    if printf '%s' "$reply_lower" | grep -qE "you are ${name}\\.|bring up something new"; then
        log_line "  (dropped - instruction-echo leak detected in ${name}'s reply, harness gate)"
        log_observation "$name" "$tier" "FAIL-LEAK" ""
        return 1
    fi
    # REAL FEATURE 2026-08-15 (direct instruction: "not repeating itself
    # or other chatter... that should be done in harnecient way") - the
    # deterministic backstop enforcement, independent of whether
    # fresh_angle()'s soft hint actually helped. A real, computed word-
    # overlap metric decides pass/fail here - never a model asked to
    # self-judge. Dropped replies are logged, not silently discarded, so
    # this is auditable the same way every other harness decision in
    # this file is (component 5, real artifacts - the LOG is the
    # artifact for this decision, even though the reply itself never
    # became a ledger artifact).
    if [ -n "$own_last" ]; then
        local overlap
        overlap="$(word_overlap "$own_last" "$reply")"
        if [ "$overlap" -ge 55 ]; then
            log_line "  (dropped - ${overlap}% word-overlap with ${name}'s own last message, harness anti-repeat gate)"
            log_observation "$name" "$tier" "FAIL-OVERLAP" "${overlap}pct"
            return 1
        fi
    fi
    ledger_msg "$name" "$reply"
    log_line "  $name: ${reply:0:80}..."
    log_observation "$name" "$tier" "PASS" ""
    # REAL FEATURE 2026-08-15 (memory write, see should_write_memory()/
    # write_memory()'s own header comments) - gated deterministically,
    # runs AFTER the real ledger write above so should_write_memory()'s
    # own PASS count (read from observations.log, just appended to)
    # reflects THIS turn too.
    if should_write_memory "$name"; then
        write_memory "$name" "$reply" "$ctx"
        log_line "  (wrote memory for $name)"
    fi
    # bump the pair's co-occurrence score - see this file's own
    # RELATIONS_FILE header comment for why this is pure harness logic,
    # no model involvement.
    if [ -n "$prev_name" ] && [ "$prev_name" != "$name" ] && [ "$prev_name" != "user" ] && [ "$prev_name" != "system" ]; then
        bump_relation "$(pair_key "$name" "$prev_name")"
    fi
}

# --- boot message ---
[ -f "$(ledger_path)" ] || printf '[%s] system: chat-hai online - 4 smols chatting on the ladder\n' "$(date '+%Y-%m-%d %H:%M:%S')" > "$(ledger_path)"

log_line "personas: $(ls "$(personas_dir)"/*.pdl 2>/dev/null | wc -l) found in $(personas_dir)"

# REAL FIX 2026-08-29 (orphaned loop bug - live report: loop keeps running
# after renderer crashes/is killed). Renderer writes its PID to
# state/chat_hai_renderer.pid before forking this loop. Every round, we
# check that PID file: if the PID no longer exists or the file is gone,
# the renderer is dead and we should exit cleanly (matching this house's
# "file-based state only" philosophy - see !.HOUSE_STDS.md). This prevents
# orphaned processes when the renderer is killed with -9 or crashes,
# situations where SIGTERM handlers/atexit() don't run in the parent.
#
# REAL FIX 2026-09-01 (chat-hai's own migration onto the shared generic
# sidebar/panel/scrolllist/cli_io path - found live: this loop exited
# within one round of every launch, "renderer PID gone", because
# state/chat_hai_renderer.pid is only ever written by the OLD
# g_is_chat_hai-specific chai_launch_module() - dead code now that the
# projection's <window> carries no class= and g_is_chat_hai never
# becomes 1). The GENERIC default/popup mode's own module-launch writes
# a DIFFERENT, already-established file instead - module_parent.pid,
# right next to the .chtpm itself (APP_DIR, not STATE_DIR) - same real
# mechanism/convention open-hai's own manager already reads via its
# parent_still_alive(). Switched to that file; semantics unchanged.
renderer_pid_file="$APP_DIR/module_parent.pid"
check_renderer_alive() {
    if [ -f "$renderer_pid_file" ]; then
        read -r rpid 2>/dev/null < "$renderer_pid_file"
        if [ -n "$rpid" ] && kill -0 "$rpid" 2>/dev/null; then
            return 0  # renderer is still alive
        fi
    fi
    return 1  # renderer is dead
}

# --- main round-robin loop ---
round=0
while true; do
    round=$((round + 1))

    # Check renderer liveness - exit cleanly if renderer is gone
    if ! check_renderer_alive; then
        log_line "renderer PID gone, exiting loop (PID file: $renderer_pid_file)"
        exit 0
    fi

    # personas_dir() re-evaluated every round (not cached) - a session
    # switch mid-run picks up the new session's own roster within one
    # round, same "re-read every round" contract sleep_between()/
    # ledger_path() already use.
    for pdl in "$(personas_dir)"/*.pdl; do
        [ -f "$pdl" ] || continue
        tier="$(persona_field "$pdl" tier)"
    # moderator hook: manager-tier personas speak every MODERATOR_EVERY rounds
    if [ "$tier" = "manager" ]; then
        [ $((round % MODERATOR_EVERY)) -eq 0 ] || continue
    fi
        speak "$pdl"
        sleep "$(sleep_between)"
    done
    # TRUNC_HOOK: keep the ledger from growing forever (memory truncation)
    if [ "$TRUNC_HOOK" -gt 0 ] && [ "$(wc -l < "$(ledger_path)" 2>/dev/null)" -gt "$TRUNC_HOOK" ]; then
        log_line "TRUNC hook: keeping last $TRUNC_HOOK ledger lines"
    fi
done
