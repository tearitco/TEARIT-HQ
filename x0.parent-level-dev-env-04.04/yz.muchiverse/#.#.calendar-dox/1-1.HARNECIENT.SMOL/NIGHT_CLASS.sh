#!/bin/bash
# NIGHT_CLASS.sh — deliver a night class to an external reviewer.
#
# The night class is a NIGHT_nn_*.txt briefing plus its narration. This
# script is the reproducible path from "the txt exists" to "a reviewer has
# everything, including the code", and it is deliberately standalone so the
# reviewer can run it without this house's layout knowledge.
#
# What it does:
#   1. locate the night class text and its mp3
#   2. copy both next to this script
#   3. report which branch carries the code and how to get it
#   4. print the reviewer's own self-check, so they are told in advance
#      what "green" has meant in this repo before they read a summary
#
# Usage:
#   bash NIGHT_CLASS.sh [output_dir]
#
# Defaults to ../../../../XO/11.timemachine-git-repo/ if it exists,
# otherwise the current directory.

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
CLASS="${NIGHT_CLASS_FILE:-$HERE/NIGHT_32_EXTERNAL_REVIEW_3_DAYS_5_BRANCHES.txt}"
AUDIO_DIR="${NIGHT_CLASS_AUDIO:-$HERE/audio-book}"
MP3="$(ls "$AUDIO_DIR"/NIGHT_32*.mp3 2>/dev/null | head -1)"
BRANCH="${NIGHT_CLASS_BRANCH:-claude-kilo}"
REPO="${NIGHT_CLASS_REPO:-https://github.com/tearitco/TEARIT-HQ.git}"

DEST="${1:-}"
if [ -z "$DEST" ]; then
    GUESS="/home/no/Desktop/github/work/XO/11.timemachine-git-repo"
    DEST="$([ -d "$GUESS" ] && echo "$GUESS" || echo "$PWD")"
fi
mkdir -p "$DEST" || exit 1

echo "=== night class delivery ==="
echo "  script : $(basename "$CLASS")"
echo "  audio  : ${MP3:-<none found>}"
echo "  dest   : $DEST"

[ -f "$CLASS" ] || { echo "MISSING: $CLASS" >&2; exit 1; }

cp "$CLASS" "$DEST/" || exit 1
echo "  copied script"

if [ -n "${MP3:-}" ] && [ -f "$MP3" ]; then
    cp "$MP3" "$DEST/" || exit 1
    echo "  copied audio ($(du -h "$MP3" | cut -f1))"
else
    echo "  WARNING: no mp3 found - regenerate with:"
    echo "           python3 $HERE/convert_night32_to_audio.py"
fi

cat <<EOF

=== for the reviewer: getting the code ===

  git clone $REPO
  cd TEARIT-HQ
  git checkout $BRANCH

  $BRANCH consolidates every branch that held unmerged work:
  attrition (HORN_CHAT), kilo-wsr-pal (civ/economy), kilo,
  opencode-fix, opencode-win32. 52 commits onto claude.

=== READ THIS BEFORE YOU TRUST ANY "all tests passed" ===

1. A green summary in this repo has previously meant "the suite skipped the
   part that mattered". Check what EXECUTED, not the exit code.

2. The repo is PUBLIC and contains live API keys - in a release asset and
   in git history since 5a4164334. Treat every key in this tree as
   compromised.

3. The highest-value read is ops/horn_tool_exec.c: the tool allowlist gate
   and the bwrap sandbox. A model here can write files and run shell
   commands.

4. Four house CPU-safety rules were broken by the code under review and
   were fixed only after the reviewer pointed at the missing documents.
   The report's section 4 lists them.

5. The orphan fix (PR_SET_PDEATHSIG) is UNVERIFIED. Reproduce it before
   relying on it.

EOF