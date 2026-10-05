#!/bin/sh
# rc_open_with_backend.sh <entity_dir> <house_root> <backend>
#
# REAL FIX 2026-09-30, direct live report ("chat-api, chat-bank were
# meant to open the same open-hai style layout robot-chat was using but
# have the openrouter-api or pipeline bank backend, instead it's opening
# a real linux terminal window, very out of character") - Chat-api/
# Chat-bank's meta.pdl rows used to `exec gnome-terminal` around a
# throwaway shell loop script. This house never opens a bare terminal
# for a chat surface; every one is a real khtpm window
# (khtpm_core_render.c against a .xhtpm template). robot-chat.xhtpm/
# button.sh already IS that real window - the only real gap was a way
# to tell rc_check_request.c which backend to call (see that file's own
# 2026-09-30 header). This script is that: write the selector, then
# launch the SAME real window button.sh already builds.
#
# <backend> is one of gemma|openrouter|bank (rc_check_request.c's own
# real switch); absent/invalid falls back to gemma there, unchanged
# from the original single "Chat" button's behavior.
set -e
ENT="${1:?usage: rc_open_with_backend.sh <entity_dir> <house_root> <backend>}"
HOUSE="${2:?usage: rc_open_with_backend.sh <entity_dir> <house_root> <backend>}"
BACKEND="${3:?usage: rc_open_with_backend.sh <entity_dir> <house_root> <backend>}"
HERE="$(cd "$(dirname "$0")" && pwd)"

mkdir -p "$ENT/.hq_manager"
printf '%s\n' "$BACKEND" > "$ENT/.hq_manager/chat_backend.txt"

exec sh "$HERE/button.sh" "$ENT" "$HOUSE"
