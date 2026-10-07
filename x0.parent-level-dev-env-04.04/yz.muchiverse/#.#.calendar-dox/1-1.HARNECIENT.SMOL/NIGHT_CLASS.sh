#!/bin/bash
# NIGHT_CLASS.sh — deliver the NIGHT 32 materials to an external reviewer.
#
# Two artifacts, deliberately different in genre, and the reviewer needs
# both:
#
#   NIGHT_32_THE_MACHINE_THAT_CAN_WRITE.txt   the NIGHT CLASS SCRIPT.
#       A scripted dialogue in the house's established format - two
#       students (MAXINE, RAHWEH) and TOMO teaching - covering what
#       changed and what is still open. This is what the mp3 narrates.
#
#   EXTERNAL_REVIEW_3_DAYS_5_BRANCHES.txt    the VANILLA REVIEW REPORT.
#       Plain technical briefing: branch-by-branch diffs, line counts, the
#       security model in reviewable detail, open risks. No dramaturgy.
#
# An earlier version narrated the REPORT to mp3 and called it a night
# class. That was wrong twice: it was the wrong genre, and the "review" a
# reviewer receives should not require an audio player.
#
# Usage:
#   bash NIGHT_CLASS.sh [output_dir]
#
# Defaults to the timemachine review directory if it exists, else $PWD.

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"

CLASS_TXT="${NIGHT_CLASS_FILE:-$HERE/NIGHT_32_THE_MACHINE_THAT_CAN_WRITE.txt}"
REVIEW_TXT="${NIGHT_CLASS_REVIEW:-$HERE/EXTERNAL_REVIEW_3_DAYS_5_BRANCHES.txt}"
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
echo "  script : $(basename "$CLASS_TXT")"
echo "  review : $(basename "$REVIEW_TXT")"
echo "  audio  : $(basename "${MP3:-<none found>}")"
echo "  dest   : $DEST"

missing=0
for f in "$CLASS_TXT" "$REVIEW_TXT"; do
    if [ ! -f "$f" ]; then
        echo "  MISSING: $f" >&2
        missing=1
        continue
    fi
    cp "$f" "$DEST/" || exit 1
    echo "  copied $(basename "$f")"
done
[ "$missing" -eq 0 ] || exit 1

if [ -n "${MP3:-}" ] && [ -f "${MP3}" ]; then
    cp "$MP3" "$DEST/" || exit 1
    echo "  copied $(basename "$MP3") ($(du -h "$MP3" | cut -f1))"
else
    echo "  WARNING: no mp3. Regenerate with:"
    echo "           python3 $HERE/convert_night32_to_audio.py"
fi

cat <<EOF

=== for the reviewer ===

  START HERE:  EXTERNAL_REVIEW_3_DAYS_5_BRANCHES.txt
               (plain briefing; no audio needed)

  THEN:        NIGHT_32_THE_MACHINE_THAT_CAN_WRITE.txt
               or its mp3 - the same territory as a night class dialogue,
               including the open questions the briefing does not resolve.

  CODE:        git clone $REPO
               cd TEARIT-HQ && git checkout $BRANCH

  $BRANCH consolidates every branch that still held unmerged work
  (attrition, kilo-wsr-pal, kilo, opencode-fix, opencode-win32):
  52 commits onto claude.

=== READ THIS BEFORE YOU TRUST ANY "all tests passed" ===

1. A green summary in this repo has previously meant "the suite skipped the
   part that mattered". Check what EXECUTED, not the exit code.

2. The repo is PUBLIC and contains live API keys - in a release asset and
   in git history since 5a4164334. Treat every key in this tree as
   compromised.

3. Highest-value read: ops/horn_tool_exec.c - the tool allowlist gate and
   the bwrap sandbox. A model in that tree can write files and run shell.

4. Four house CPU-safety rules were broken by the code under review, fixed
   only after being pointed at the missing documents. Report section 4.

5. The orphan fix (PR_SET_PDEATHSIG) is UNVERIFIED. Reproduce it before
   relying on it.

EOF