#!/bin/bash
# game_slot_verify.sh - proves game_slot_op (save-game / load-game slots) on a SCRATCH entity tree; never touches real user data. PASS/FAIL lines + VERDICT; exit 0 = PASS.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; OP="$HERE/+x/game_slot_op.+x"; [ -x "$OP" ] || { echo "build first: sh build_phone_ensure_op.sh" >&2; exit 2; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT; fail=0
ck() { if [ "$2" = ok ]; then echo "PASS|$1"; else echo "FAIL|$1|$3"; fail=1; fi; }
P="$T/pals"; SG="$T/savegames"; mkdir -p "$P/ent_a/inventory" "$P/ent_b"
printf 'alpha\n' > "$P/ent_a/pal.pdl"; printf 'beta\n' > "$P/ent_b/pal.pdl"; printf 'hp=10\n' > "$P/ent_a/inventory/stats.txt"
sumtree() { (cd "$P" && find . -type f | sort | xargs sha256sum | sha256sum | cut -c1-16); }
BEFORE="$(sumtree)"
out="$("$OP" "$SG" "$P" save 3 --now-ms 1790000000000)"; rc=$?
[ "$rc" = 0 ] && echo "$out" | grep -q 'saved slot 3: 3 files' && ck save-reports-files ok || ck save-reports-files bad "rc=$rc out=$out"
[ -f "$SG/slot_03/manifest.txt" ] && [ "$(wc -l < "$SG/slot_03/manifest.txt")" = 3 ] && grep -q '^SLOT | files | 3' "$SG/slot_03/meta.pdl" && ck manifest-and-meta-written ok || ck manifest-and-meta-written bad "slot_03 files missing"
grep -q '^1790000000000|save|3|files=3|manifest=' "$SG/ledger.txt" && ck ledger-line-appended ok || ck ledger-line-appended bad "$(cat "$SG/ledger.txt" 2>&1)"
[ "$(sumtree)" = "$BEFORE" ] && ck save-changes-no-entity-data ok || ck save-changes-no-entity-data bad "entity tree differs after save"
"$OP" "$SG" "$P" save 3 --now-ms 1790000000001 >/dev/null; [ "$(wc -l < "$SG/ledger.txt")" = 2 ] && ck resave-appends-not-rewrites ok || ck resave-appends-not-rewrites bad "ledger lines: $(wc -l < "$SG/ledger.txt")"
out="$("$OP" "$SG" "$P" load 3 --now-ms 1790000000002)"
echo "$out" | grep -q 'same=3 changed=0 missing=0 new=0' && ck load-identical-tree-all-same ok || ck load-identical-tree-all-same bad "$out"
# change one file, delete one, add one, then load: 1 same, 1 changed, 1 missing, 1 new
printf 'hp=9\n' > "$P/ent_a/inventory/stats.txt"; rm "$P/ent_b/pal.pdl"; printf 'new\n' > "$P/ent_b/created.txt"
AFTER="$(sumtree)"
out="$("$OP" "$SG" "$P" load 3 --now-ms 1790000000003)"
echo "$out" | grep -q 'same=1 changed=1 missing=1 new=1' && ck load-counts-changed-missing-new ok || ck load-counts-changed-missing-new bad "$out"
grep -q '^changed|ent_a/inventory/stats.txt' "$SG/last_load.txt" && grep -q '^missing|ent_b/pal.pdl' "$SG/last_load.txt" && grep -q '^new|ent_b/created.txt' "$SG/last_load.txt" && ck last-load-lists-paths ok || ck last-load-lists-paths bad "$(cat "$SG/last_load.txt")"
[ "$(sumtree)" = "$AFTER" ] && ck load-restores-nothing ok || ck load-restores-nothing bad "load modified the entity tree"
[ "$(cat "$SG/loaded_slot.txt")" = 3 ] && grep -q '|load|3|same=1|changed=1|missing=1|new=1' "$SG/ledger.txt" && ck loaded-slot-and-ledger ok || ck loaded-slot-and-ledger bad "loaded_slot/ledger wrong"
"$OP" "$SG" "$P" load 7 >/dev/null 2>"$T/err"; rc=$?; [ "$rc" = 1 ] && grep -q 'slot 7 is empty' "$T/err" && ck load-empty-slot-fails-cleanly ok || ck load-empty-slot-fails-cleanly bad "rc=$rc $(cat "$T/err")"
"$OP" "$SG" "$P" save 17 >/dev/null 2>&1; r1=$?; "$OP" "$SG" "$P" save 0 >/dev/null 2>&1; r2=$?; "$OP" "$SG" "$P" wipe 1 >/dev/null 2>&1; r3=$?
[ "$r1" = 2 ] && [ "$r2" = 2 ] && [ "$r3" = 2 ] && ck bad-slot-or-verb-rejected ok || ck bad-slot-or-verb-rejected bad "rc $r1 $r2 $r3"
"$OP" "$SG" "$T/nope" save 1 >/dev/null 2>&1; [ $? = 1 ] && [ ! -d "$SG/slot_01" ] && ck missing-pals-dir-writes-nothing ok || ck missing-pals-dir-writes-nothing bad "slot_01 created for a missing pals dir"
echo "VERDICT|$([ "$fail" = 0 ] && echo PASS || echo FAIL)"; exit "$fail"
