#!/bin/bash
# chat_hai_projector.sh — REAL, GENERATED PROJECTION writer for chat-hai,
# 2026-09-01. Same real architecture as open-hai's own
# khtpm_open_hai_manager.c write_chtpm_projection() (see that file's own
# header comment), just in shell instead of C, since chat-hai already
# has a real, working shell-based backend (chat_hai_loop.sh) and needs
# no new binary.
#
# Regenerates chat-hai.chtpm every ~300ms from chat_hai_loop.sh's own
# real state files (state/sessions/*.ledger, state/sessions/active.txt,
# state/paused.txt, state/typing.txt, chat_hai_config.pdl) using ONLY
# generic tags (<sidebar>/<panel>/<scrolllist>/<item>/<cli_io>) - the
# shared khtpm_core_render.+x picks this up via its own
# reparse_chtpm_if_changed(). DO NOT hand-edit chat-hai.chtpm - it is
# overwritten every tick.
#
# REAL FIX 2026-09-01 (the actual mechanism that gets chat-hai off its
# own ~4000 lines of g_is_chat_hai-hardcoded renderer code, found live:
# `grep -n 'g_is_chat_hai = 1' khtpm_core_render.c` shows it's set ONLY
# when `<window class="chat-window">` is present): this projection's
# own <window> tag carries NO class at all, so g_is_chat_hai never
# becomes 1 and the shared renderer falls through to the SAME generic
# default/popup sidebar+panel path open-hai/network-browser already
# use - zero khtpm_core_render.c changes needed for this migration. The
# old chai_* functions become real, harmless dead code (not deleted
# yet - a later, separate cleanup pass, not required for this to work).
#
# Launched as chat_hai_loop.sh's own background child (see that
# script's own startup section) - killed via the same trap that stops
# the main loop, so both real processes share one lifetime, same
# contract chat-hai already had before this migration.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HOUSE="$(cd "$APP_DIR/../.." && pwd)"
STATE_DIR="$APP_DIR/state"
SESSIONS_DIR="$STATE_DIR/sessions"
ACTIVE_FILE="$SESSIONS_DIR/active.txt"
PAUSE_FILE="$STATE_DIR/paused.txt"
TYPING_FILE="$STATE_DIR/typing.txt"
CONFIG_PDL="$APP_DIR/chat_hai_config.pdl"
CHTPM_OUT="$APP_DIR/chat-hai.chtpm"
OPS_DIR="$SCRIPT_DIR"

mkdir -p "$SESSIONS_DIR"

xml_escape() {
    printf '%s' "$1" | sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' -e 's/"/\&quot;/g'
}

sq() {
    # single-quote-escape for embedding inside a shell action= string
    printf '%s' "$1" | sed "s/'/'\\\\''/g"
}

current_session() { cat "$ACTIVE_FILE" 2>/dev/null || echo main; }

sleep_between() {
    v="$(awk -F'|' '$1 ~ /^SECTION/ {gsub(/ /,"",$2); if ($2=="sleep_between") {gsub(/ /,"",$3); print $3; exit}}' "$CONFIG_PDL" 2>/dev/null)"
    [ -n "$v" ] && [ "$v" -gt 0 ] 2>/dev/null && echo "$v" || echo 6
}

is_paused() {
    v="$(cat "$PAUSE_FILE" 2>/dev/null)"
    [ "$v" = "1" ] && echo 1 || echo 0
}

LAST_PROJECTION=""

write_projection() {
    local session_ops="$OPS_DIR"
    local active; active="$(current_session)"
    local paused; paused="$(is_paused)"
    local speed; speed="$(sleep_between)"
    local typing; typing="$(cat "$TYPING_FILE" 2>/dev/null)"
    local status_raw status_esc
    if [ "$paused" = "1" ]; then
        status_raw="[stopped]"
    elif [ -n "$typing" ]; then
        status_raw="[running] $typing is typing..."
    else
        status_raw="[running]"
    fi
    status_esc="$(xml_escape "$status_raw")"

    local buf
    buf="<!-- chat-hai.chtpm - REAL, GENERATED PROJECTION.
     Written by chat_hai_projector.sh every real loop tick - DO NOT
     HAND-EDIT, changes are overwritten within ~300ms. No module-tag
     here on purpose - that only lives in chat-hai.chtpm.bootstrap;
     the shared renderer's own module-launch runs once at process
     startup (see khtpm_core_render.c's own default/popup main()), not
     on every reparse, same real convention open-hai's own
     write_chtpm_projection() already established (it never re-emits
     its bootstrap's module-tag either). This process (the
     projector) is ITSELF launched as chat_hai_loop.sh's own
     background child, not via a second module-tag - re-including
     one here would fork a SECOND chat_hai_loop.sh on every reparse. -->
<window label=\"chat-hai\">
  <page name=\"main\">
    <sidebar>
      <text id=\"sidebar-header\" label=\"Sessions\"/>
      <item id=\"new\" label=\"+ New Session\" action=\"'$session_ops/ch_item.sh' 'NEW'\"/>
      <scrolllist>
"
    local n=0
    if [ -d "$SESSIONS_DIR" ]; then
        while IFS= read -r ledger; do
            [ -z "$ledger" ] && continue
            local name; name="$(basename "$ledger" .ledger)"
            local is_active_prefix="  "
            [ "$name" = "$active" ] && is_active_prefix="> "
            local label_esc; label_esc="$(xml_escape "${is_active_prefix}${name}")"
            local name_sq; name_sq="$(sq "$name")"
            buf="$buf      <item id=\"s$n\" label=\"$label_esc\" action=\"'$session_ops/ch_item.sh' 'SWITCH|$name_sq'\" backspace_action=\"'$session_ops/ch_item.sh' 'DELETE|$name_sq'\"/>
"
            n=$((n + 1))
        done < <(find "$SESSIONS_DIR" -maxdepth 1 -name "*.ledger" 2>/dev/null | sort)
    fi
    buf="$buf      </scrolllist>
    </sidebar>
    <panel>
      <text id=\"status\" label=\"$status_esc\"/>
      <item id=\"pause\" label=\"$([ "$paused" = "1" ] && echo Start || echo Stop)\" action=\"'$session_ops/ch_item.sh' 'PAUSE'\"/>
      <item id=\"speed\" label=\"Speed: ${speed}s (click to cycle)\" action=\"'$session_ops/ch_item.sh' 'SPEED'\"/>
      <scrolllist>
"
    local ledger_path="$SESSIONS_DIR/$active.ledger"
    if [ -f "$ledger_path" ]; then
        local m=0
        while IFS= read -r line; do
            [ -z "$line" ] && continue
            # real line shape: "[YYYY-MM-DD HH:MM:SS] speaker: text | Trigger: ..."
            local rest="${line#*] }"
            local speaker="${rest%%:*}"
            local text="${rest#*: }"
            text="${text% | Trigger:*}"
            # REAL, NEW 2026-09-01 (direct instruction: "dont forget
            # their different css colors, since telling them apart is
            # hard otherwise") - one real class PER SPEAKER
            # (msg-<speaker>), not just a flat msg-user/msg-hai split -
            # this house's chat-hai has ~12 real personas
            # (pieces/personas/*.pdl) speaking in the same feed; a
            # single "AI" color made them indistinguishable. Slugified
            # (lowercase, non-alnum stripped) so a stray character in a
            # future persona name can't break the class= attribute.
            local slug; slug="$(printf '%s' "$speaker" | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z0-9')"
            local cls="msg-${slug:-other}"
            local row_esc; row_esc="$(xml_escape "${speaker}: ${text}")"
            buf="$buf        <text id=\"msg$m\" class=\"$cls\" label=\"$row_esc\"/>
"
            m=$((m + 1))
        done < <(tail -n 60 "$ledger_path" 2>/dev/null)
    fi
    buf="$buf      </scrolllist>
      <cli_io id=\"composer\" target_id=\"composer\" rows=\"3\" label=\"&gt; \" action=\"'$session_ops/ch_send.sh'\"/>
    </panel>
  </page>
</window>
"
    if [ "$buf" = "$LAST_PROJECTION" ]; then
        return
    fi
    LAST_PROJECTION="$buf"
    printf '%s' "$buf" > "$CHTPM_OUT.tmp"
    mv "$CHTPM_OUT.tmp" "$CHTPM_OUT"
}

trap 'exit 0' TERM INT

while true; do
    write_projection
    sleep 0.3
done
