#!/bin/sh
# pet_event.sh <verb> [arg] - the pet house verbs. State lives in PET_DIR (default ../state next to this app); every number comes from the .pdl files here.
# Verbs: new_pet [seed] | feed [item] | give <item> | sleep | wash | play | tick | evolve | status | stats. Each care verb ends with: grade EXP via
# &.widgits/concept-bank/ops/entity_grade, an evolve check, and a refresh of ui.txt for the layout window. Exit 0 on bad input (an empty verb never harms).
HERE="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
trap 'rm -f "$SHARED"/*.tmp.$$ "$PET"/*.tmp.$$ 2>/dev/null' EXIT   # per-run temp files (concurrent runs - the manager tick and a key press - must never share one)
SHARED="${PET_SHARED:-$HERE/state}"      # window-facing files (ui.txt, scene.raw, view.txt, party.txt, active.txt, world.st); each pet lives in $SHARED/pets/<id>/
DB="${PET_DB:-$HOUSE/&.widgits/db-hq/data}"     # the RPG Maker database (db-hq): the party is SYSTEM PetParty, every pet is an ACTOR row (class Pet, note: pet species N seed S)
ACTIVE="$(cat "$SHARED/active.txt" 2>/dev/null)"; [ -z "$ACTIVE" ] && ACTIVE="$(awk 'NR==1{print $1}' "$SHARED/party.txt" 2>/dev/null)"; ACTIVE="${ACTIVE:-a13}"
[ -n "${PET_DIR:-}" ] && ACTIVE="$(basename "$PET_DIR")"      # a run for one named pet (training, clock events) acts AS that pet
PET="${PET_DIR:-$SHARED/pets/$ACTIVE}"
GRADE="${ENTITY_GRADE:-$HOUSE/&.widgits/concept-bank/ops/+x/entity_grade.+x}"
GEN="${PET_GEN:-$HOUSE/@.apps/layout-studio/ops/+x/pet_gen.+x}"
VERB="${1:-}"; ARG="${2:-}"
V="$PET/variables.txt"

getv() { sed -n "s/^$1=//p" "$V" 2>/dev/null | head -1; }
setv() { # setv key value (clamped 0..100 for the care meters). Uses _sv so it never clobbers a caller's variable.
    _sv="$2"; case "$1" in hunger|energy|clean|happy) [ "$_sv" -lt 0 ] && _sv=0; [ "$_sv" -gt 100 ] && _sv=100;; esac
    if grep -q "^$1=" "$V" 2>/dev/null; then sed -i "s/^$1=.*/$1=$_sv/" "$V"; else printf '%s=%s\n' "$1" "$_sv" >> "$V"; fi
}
addv() { cur=$(getv "$1"); setv "$1" $(( ${cur:-0} + $2 )); }
pdlval() { # pdlval file rowkey name field -> value of field=N on the row "<kind> | <name> | ..."
    awk -F'|' -v n="$2" -v f="$3" '{g=$2; gsub(/^ +| +$/,"",g)} g==n {for(i=3;i<=NF;i++){x=$i; gsub(/^ +| +$/,"",x); if (index(x,f"=")==1) {sub(/^[^=]*=/,"",x); sub(/^\+/,"",x); print x; exit}}}' "$1"
}
need_pet() { [ -f "$V" ] && return 0; if [ -z "${PET_DIR:-}" ]; then ensure_party; else sh "$0" new_pet 1 >/dev/null; fi; }

W="$PET/weights.pdl"
getw() { sed -n "s/^$1=//p" "$W" 2>/dev/null | head -1; }
JT="${JOINT_TUNE:-$HOUSE/&.widgits/concept-bank/ops/+x/joint_tune.+x}"
feedback() { # feedback <valence +1|-1> <concept>: one graded row for the report card (entity_grade check reads it)
    printf 'FEEDBACK | valence=%s | concept=%s | layer=extracurricular | intensity=1\n' "$1" "$2" >> "$PET/obs_feedback_log.txt"
}
learn_pref() { # learn_pref <item> <valence>: the pet nudges how much it likes the item by +-1 (joint_tune, bounded, ledgered)
    [ -x "$JT" ] || return 0
    "$JT" apply "$HERE/joints.pdl" "$W" "$PET/tuning_ledger.txt" "pref_$1" "$2" pet "reaction to $1" >/dev/null 2>&1
    return 0
}
skill() { # skill <name>: EXP + level via entity_grade; refuses quietly when MP is short
    [ -x "$GRADE" ] && "$GRADE" use "$PET" "$HERE/skillbook.pdl" "$1" >/dev/null 2>&1
    return 0
}

self_care() { # the pet chooses its own care: a WEIGHTED choice among valid actions (needs x tunable priorities), never a model. Gated by the self_care skill.
    "$GRADE" use "$PET" "$HERE/skillbook.pdl" self_care >/dev/null 2>&1 || { printf '%s | self_care locked\n' "$(date '+%H:%M:%S')" >> "$PET/log.txt"; return 0; }
    h=$(getv hunger); e=$(getv energy); c=$(getv clean); p=$(getv happy)
    best=""; bs=$(getw self_care_min); bs=${bs:-50}; item=""
    food=$(awk -F'|' '/^ITEM/{n=$2;k=$3;gsub(/^ +| +$/,"",n);gsub(/^ +| +$/,"",k); if(k=="food")print n}' "$HERE/items.pdl" | while read it; do [ "$(inv_count "$it")" -gt 0 ] && echo "$(getw pref_$it) $it"; done | sort -rn | head -1 | cut -d' ' -f2)
    sc=$(( h * $(getw w_feed) )); [ -n "$food" ] && [ "$sc" -gt "$bs" ] && { bs=$sc; best=feed; item=$food; }
    sc=$(( (100 - e) * $(getw w_sleep) )); [ "$sc" -gt "$bs" ] && { bs=$sc; best=sleep; item=""; }
    sc=$(( (100 - c) * $(getw w_wash) )); have=$(inv_count soap); [ "${have:-0}" -gt 0 ] && [ "$sc" -gt "$bs" ] && { bs=$sc; best=wash; item=soap; }
    sc=$(( (100 - p) * $(getw w_play) )); [ "$e" -gt 20 ] && [ "$sc" -gt "$bs" ] && { bs=$sc; best=play; item=""; }
    [ -z "$best" ] && return 0
    printf '%s | auto | %s %s (score %s)\n' "$(date '+%H:%M:%S')" "$best" "$item" "$bs" >> "$PET/log.txt"
    if [ "$best" = wash ]; then sh "$0" give soap >/dev/null; elif [ "$best" = feed ]; then sh "$0" give "$item" >/dev/null; else sh "$0" "$best" >/dev/null; fi
    # learn the priority from how it went: the last feedback row's valence moves that action's weight by +-1 (joint_tune, bounded, ledgered)
    v=$(tail -1 "$PET/obs_feedback_log.txt" | sed -n 's/.*valence=\([-+0-9]*\).*/\1/p'); case "$best" in feed) wk=w_feed;; sleep) wk=w_sleep;; wash) wk=w_wash;; play) wk=w_play;; esac
    [ -x "$JT" ] && "$JT" apply "$HERE/joints.pdl" "$W" "$PET/tuning_ledger.txt" "$wk" "${v:-1}" pet "self care $best" >/dev/null 2>&1
    return 0
}

IOP="${INVENTORY_OP:-$HOUSE/&.widgits/entity-cli/ops/+x/inventory_op.+x}"
# The pet's inventory is the HOUSE inventory: <pet dir>/inventory/<item>/ (an item is a directory, slots are alphabetical, glyph.txt = its picture, inventory_slot.txt =
# selected slot; &.widgits/entity-cli/ops/inventory_op, khtpm_inventory.c). A stack is several directories (apple_1 apple_2 ...). Nothing is deleted: a used item is moved to used/.
inv_count() { ls -d "$PET/inventory/$1"_* 2>/dev/null | wc -l; }
inv_take_one() { f=$(ls -d "$PET/inventory/$1"_* 2>/dev/null | head -1); [ -n "$f" ] || return 1; mkdir -p "$PET/used"; mv "$f" "$PET/used/$(basename "$f")_$(date +%s)_$$"; }
inv_add() { # inv_add <item> [n]: the master (or the world) gives the pet n of an item
    it="$1"; n="${2:-1}"; kind=$(awk -F'|' -v n="$it" '/^ITEM/{g=$2; gsub(/^ +| +$/,"",g); if(g==n){k=$3; gsub(/^ +| +$/,"",k); print k}}' "$HERE/items.pdl"); [ -n "$kind" ] || return 1
    glyph=$(pdlval "$HERE/items.pdl" "$it" glyph); [ -n "$glyph" ] || glyph='?'
    c=$(getv inv_seq); c=${c:-0}
    while [ "$n" -gt 0 ]; do c=$((c + 1)); d="$PET/inventory/${it}_$c"; mkdir -p "$d"; printf 'entity_type=item\nname=%s\nkind=%s\n' "$it" "$kind" > "$d/state.txt"; printf '%s\n' "$glyph" > "$d/glyph.txt"; n=$((n - 1)); done
    setv inv_seq "$c"
}
LEXF="$PET/lexicon.pdl"; CHAT="$PET/chat.txt"
say() { printf 'PET: %s\n' "$1" >> "$CHAT"; }
mood() { p=$(getv happy); if [ "${p:-50}" -ge 40 ]; then echo happy; else echo sad; fi; }
reply() { # reply <verb>: the pet's fixed reaction text for the verb and mood
    t=$(awk -F'|' -v v="$1" -v m="$(mood)" '/^REPLY/{a=$2;b=$3;gsub(/^ +| +$/,"",a);gsub(/^ +| +$/,"",b); if(a==v&&b==m){x=$4; gsub(/^ +| +$/,"",x); print x; exit}}' "$HERE/replies.pdl")
    [ -n "$t" ] && say "$t"
}
expr() { printf '%s %s\n' "$1" "$(( $(date +%s) + ${2:-6} ))" > "$PET/expression.txt"; }
lex_best() { # lex_best <text>: best LEX row for the words in the text -> "verb|item|phrase|weight" (score = weight x words matched), empty if none scores
    printf '%s' "$1" | tr 'A-Z' 'a-z' | tr -c 'a-z0-9\n' ' ' | awk -v lf="$LEXF" 'BEGIN{FS="|"} {n=split($0,w," "); for(i=1;i<=n;i++) words[w[i]]=1}
        END { while ((getline line < lf) > 0) { if (line !~ /^LEX/) continue; split(line, c, "|"); ph=c[2]; vb=c[3]; it=c[4]; wt=c[5]; gsub(/^ +| +$/,"",ph); gsub(/^ +| +$/,"",vb); gsub(/^ +| +$/,"",it); wt+=0
            k=split(ph, pw, " "); hit=0; for(j=1;j<=k;j++) if (pw[j] in words) hit++; if (hit==0) continue; sc=wt*hit/k; if (sc>best) {best=sc; out=vb"|"it"|"ph"|"wt} } if (out!="") print out }'
}
lex_set() { # lex_set <phrase> <verb> <item> <delta> <initial>: add the word (initial weight) or move its weight by delta, bounded 0..10
    awk -F'|' -v ph="$1" -v vb="$2" -v it="$3" -v d="$4" -v ini="$5" 'BEGIN{OFS="|"} /^LEX/{p=$2;gsub(/^ +| +$/,"",p); if(p==ph){w=$5+0+d; if(w>10)w=10; if(w<0)w=0; oi=$4; gsub(/^ +| +$/,"",oi); if(it!="") oi=it; print "LEX | "ph" | "vb" | "oi" | "w; seen=1; next}} {print} END{if(!seen) print "LEX | "ph" | "vb" | "it" | "ini}' "$LEXF" > "$LEXF.tmp" && mv -f "$LEXF.tmp" "$LEXF"
    printf '%s | lex %s -> %s (%s) %s\n' "$(date '+%H:%M:%S')" "$1" "$2" "$3" "$4" >> "$PET/chat_ledger.txt"
}
do_chat() { # the master talks: the pet matches known words, does the thing, answers; unknown words are remembered so the master can teach them
    need_pet; text="$1"; [ -z "$text" ] && return 0
    printf 'YOU: %s\n' "$text" >> "$CHAT"; stat_train "$text"
    case "$text" in    # typed shortcuts: "teach <words> = <verb> [item]", "good", "bad"
        teach\ *=*) body=${text#teach }; ph=${body%%=*}; rest=${body#*=}; set -- $rest; do_teach "$ph" "$1" "$2"; return 0 ;;
        help|"help "*|"?"|"? "*) sh "$0" help "${text#* }" >/dev/null; return 0 ;;
        give\ *) set -- ${text#give }; gn=1; case "$1" in [0-9]*) gn="$1"; shift;; esac; gi="${1:-}"; gi="${gi%s}"; [ "$gn" -gt 100 ] 2>/dev/null && gn=100; if [ -n "$gi" ] && awk -F'|' -v n="$gi" '/^ITEM/{g=$2; gsub(/^ +| +$/,"",g); if(g==n) f=1} END{exit !f}' "$HERE/items.pdl"; then inv_add "$gi" "$gn" >/dev/null; say "thanks! $gn $gi"; else say "give what? (apple fish cake ball soap coin wood)"; fi; return 0 ;;
        good|"good pet"|"well done") do_judge 2; return 0 ;;
        bad|"bad pet"|no) do_judge -2; return 0 ;;
    esac
    hit=$(lex_best "$text")
    if [ -z "$hit" ]; then printf '%s\n' "$text" > "$PET/last_unknown.txt"; reply unknown; expr surprised 5; printf 'PET: I do not know "%s" yet\n' "$(printf '%s' "$text" | cut -c1-14)" >> "$CHAT"; printf 'PET: say: teach %s = play  (or help)\n' "$(printf '%s' "$text" | cut -c1-8)" >> "$CHAT"; return 0; fi
    vb=${hit%%|*}; rest=${hit#*|}; it=${rest%%|*}; rest=${rest#*|}; ph=${rest%%|*}
    printf '%s|%s\n' "$ph" "$vb" > "$PET/last_word.txt"
    case "$vb" in
        hello) reply hello; expr wave 6 ;;
        feed) sh "$0" give "${it:-apple}" >/dev/null; reply feed ;;
        sleep|wash|play) sh "$0" "$vb" >/dev/null; reply "$vb" ;;
        touch) sh "$0" touch >/dev/null ;;
        go) reply go; sh "$0" teleport "${it:-living}" 150 >/dev/null ;;      # taught: "go to the kitchen" = go living
        build) reply build; sh "$0" build_building "${it:-house}" >/dev/null ;;      # taught: "build a house" = build house
        climb) reply climb; sh "$0" climb "${it:-bed}" >/dev/null ;;       # taught: "get on the bed" = climb bed ("climb down" = get off)
    esac
}
do_teach() { # teach "<phrase>" <verb> [item]: the master teaches a word (starts weak, weight 3); teaching it again strengthens it
    need_pet; ph=$(printf '%s' "$1" | tr 'A-Z' 'a-z' | tr -c 'a-z0-9 \n' ' ' | tr -s ' ' | sed 's/^ //;s/ $//'); vb="$2"; it="${3:-}"
    case "$vb" in hello|feed|sleep|wash|play|touch|go|climb|build) ;; *) return 0;; esac
    [ -z "$ph" ] && return 0
    lex_set "$ph" "$vb" "$it" 1 3; printf 'YOU: (teaches "%s" = %s)\n' "$ph" "$vb" >> "$CHAT"; say "oh! $ph"; expr happy 5
}
do_judge() { # praise / scold: moves the weight of the last word the pet acted on, +-2, and its mood
    need_pet; [ -f "$PET/last_word.txt" ] || return 0; d="$1"; ph=$(cut -d'|' -f1 "$PET/last_word.txt"); vb=$(cut -d'|' -f2 "$PET/last_word.txt")
    lex_set "$ph" "$vb" "" "$d" 3; feedback "$( [ "$d" -gt 0 ] && echo +1 || echo -1 )" "${vb:-chat}"
    if [ "$d" -gt 0 ]; then addv happy 5; expr happy 6; say "^_^"; else addv happy -4; expr sad 6; say "T_T"; fi
}
do_touch() { # touched: head = pleased, belly = giggle; many touches in a row annoy it (valence -1)
    need_pet; part="${1:-head}"; now=$(date +%s); last=$(getv touch_t); n=$(getv touch_n); n=${n:-0}
    if [ -n "$last" ] && [ $((now - last)) -le 10 ]; then n=$((n + 1)); else n=1; fi
    setv touch_t "$now"; setv touch_n "$n"
    if [ "$n" -gt 5 ]; then addv happy -3; feedback -1 touch; expr sad 5; say "stop it!"; else addv happy 3; [ "$part" = belly ] && addv happy 1; feedback +1 touch; expr happy 5; reply touch; fi
}
rooms_file() { if [ -f "$SHARED/rooms_all.pdl" ]; then echo "$SHARED/rooms_all.pdl"; else echo "$HERE/rooms.pdl"; fi; }
merge_rooms() { # shipped rooms/home + what the pet built (state/built_*.pdl) -> state/*_all.pdl (readers prefer the merged files)
    cat "$HERE/rooms.pdl" "$SHARED/built_rooms.pdl" > "$SHARED/rooms_all.pdl" 2>/dev/null; cat "$HERE/home.pdl" "$SHARED/built_home.pdl" > "$SHARED/home_all.pdl" 2>/dev/null
}
# ---- phones: every pet is an entity with a house phone (own number, inbox, outbox) and its own tiny phone server (state/server) - the SAME ops the rest of the house uses
SRV="$SHARED/server"; PHE="${PHONE_ENSURE:-$HOUSE/&.widgits/_shared-lib/ops/+x/phone_ensure_op.+x}"; PHS="${PHONE_SEND:-$HOUSE/&.widgits/_shared-lib/ops/+x/phone_send_op.+x}"; PHR="${SERVER_ROUTE:-$HOUSE/&.widgits/_shared-lib/ops/+x/server_route_op.+x}"
ph_ensure() { # idempotent: a phone for every pet folder under state/pets; the index lives in state/server, not in the house server
    mkdir -p "$SRV"; [ -f "$SRV/tunables.conf" ] || printf 'route_max_msgs_per_min=60\nroute_batch_max=100\n' > "$SRV/tunables.conf"
    [ -x "$PHE" ] && "$PHE" "$HOUSE" --apply --pals-root "$SHARED/pets" --index "$SRV/phones.index" >/dev/null 2>&1; }
ph_number() { grep "^PHONE *| *number" "$SHARED/pets/$1/inventory/zz.phone/phone.pdl" 2>/dev/null | awk -F'|' '{gsub(/ /,"",$3); print $3}'; }
ph_name() { sed -n 's/^name_id=//p' "$SHARED/pets/$1/variables.txt" 2>/dev/null | head -1; }
contact_add() { # contact_add <owner id> <other id>: one CONTACT row (name, number, id, how much they like each other, when met); never twice
    f="$SHARED/pets/$1/contacts.pdl"; num=$(ph_number "$2"); [ -n "$num" ] || return 0; grep -q "| $num |" "$f" 2>/dev/null && return 0
    printf 'CONTACT | %s | %s | %s | like=5 | met=%s\n' "$(ph_name "$2")" "$num" "$2" "$(date +%F)" >> "$f"; }
ph_say() { # ph_say <from id> <to id> <text>: a phone.send "say" from the sender's own phone, then one router pass (delivery into the other pet's inbox)
    tn=$(ph_number "$2"); [ -n "$tn" ] && "$PHS" "$SHARED/pets/$1/inventory/zz.phone" "$tn" say "" "$3" 2>/dev/null || return 0; "$PHR" "$SRV" >/dev/null 2>&1
    printf 'ph >%s: %s\n' "$(ph_name "$2")" "$3" >> "$SHARED/pets/$1/chat.txt"; printf 'ph %s %s: %s\n' "$(ph_number "$1" | cut -c1-8)" "$(ph_name "$1")" "$3" >> "$SHARED/pets/$2/chat.txt"
    if [ "${PET_TRAIN:-0}" != 1 ] && [ "${PET_NO_VOICE:-0}" != 1 ] && [ "$2" = "$(cat "$SHARED/active.txt" 2>/dev/null)" ]; then      # the pet on screen reads a text it receives aloud, in the SENDER's voice (voices.pdl species voice)
        sv=$(sed -n 's/^species=//p' "$SHARED/pets/$1/variables.txt" 2>/dev/null | head -1); ( setsid sh "$HERE/ops/pet_voice.sh" "${sv:-0}" "$3" >/dev/null 2>&1 & ); fi; }      # phone chats show in the chat window: "ph >Name: text" sent, "ph <number> Name: text" received
# ---- town: buildings are rows (state/world/town_built.pdl); state/town_all.txt = world_map.txt with every building's footprint stamped on (pet_world and pet_scene read it)
TOWN="$SHARED/world"
town_merge() { # rebuild town_all.txt from the shipped map + the built buildings
    awk -v bf="$TOWN/town_built.pdl" 'BEGIN{ while ((getline l < bf) > 0) { if (l ~ /^BUILDING/) { split(l, a, "|"); nb++; bx[nb]=a[4]+0; by[nb]=a[5]+0; r=a[10]; gsub(/^ +| +$/, "", r); br[nb]=r } } }
        { line[NR]=$0 } END { for (i=1;i<=nb;i++) { n=split(br[i], rr, "/"); for (r=1;r<=n;r++) { row=by[i]+r; s=line[row]; for (c=1;c<=length(rr[r]);c++) s=substr(s,1,bx[i]+c-1) substr(rr[r],c,1) substr(s,bx[i]+c+1); line[row]=s } } for (i=1;i<=NR;i++) print line[i] }' "$HERE/world_map.txt" > "$SHARED/town_all.txt.tmp.$$" && mv "$SHARED/town_all.txt.tmp.$$" "$SHARED/town_all.txt"; }
land_fee() { # land_fee <w> <h>: coins for the land under a w x h footprint; rises as the explored land fills up (land.pdl)
    base=$(sed -n 's/^LAND.*base=\([0-9.]*\).*/\1/p' "$HERE/land.pdl" | head -1); k=$(sed -n 's/^LAND.*k=\([0-9.]*\).*/\1/p' "$HERE/land.pdl" | head -1)
    used=$(awk -F'|' '/^BUILDING/{u+=$6*$7} END{print u+0}' "$TOWN/town_built.pdl" 2>/dev/null); total=$(cat "$HERE/world_map.txt" | tr -cd '.,' | wc -c); total=$((total > 0 ? total : 1))
    awk -v b="${base:-2}" -v k="${k:-10}" -v u="$used" -v t="$total" -v w="$1" -v h="$2" 'BEGIN{ uf=u/t; p=b*(1+k*uf*uf)*w*h; f=int(p); if (f<p) f++; print f }'; }
town_site() { # town_site <w> <h>: first free grass footprint (x y) in the current town, scanning rows from the top; empty if none
    [ -f "$SHARED/town_all.txt" ] || town_merge; awk -v w="$1" -v h="$2" '{ line[NR]=$0 } END { for (y=2;y<=NR-h;y++) for (x=2;x<=length(line[y])-w;x++) { ok=1; for (r=0;r<h && ok;r++) for (c=0;c<w;c++) { ch=substr(line[y+r],x+c,1); if (ch!="." && ch!=",") { ok=0; break } } if (ok) { print x-1, y-1; exit } } }' "$SHARED/town_all.txt"; }
spend() { # spend <item> <n>: take n of an item from the pet's inventory (caller checked the count)
    s=0; while [ "$s" -lt "$2" ]; do inv_take_one "$1" || break; s=$((s + 1)); done; }
# ---- ai_step helpers (the pets that are not on screen live headless; ai.pdl holds every number)
aiv() { sed -n "s/^AI *| *$1 *| *//p" "$HERE/ai.pdl" | head -1; }
ai_log() { printf '%s|%s|%s\n' "${PET_AI_NOW:-$(date +%s)}" "$1" "$2" >> "$PET/ai_log.txt"; }
ai_clamp() { for k in hunger energy clean; do c=$(getv $k); [ "${c:-0}" -gt 100 ] && setv $k 100; [ "${c:-0}" -lt 0 ] && setv $k 0; done; }
ai_gather() { # ai_gather <item>: walk to the remembered (else nearest) source of an item, take from it when beside it. Returns 0 when it got something, 1 otherwise (moving, none left, stuck).
    it="$1"; WO="$HERE/ops/+x/pet_world.+x"; now="${PET_AI_NOW:-$(date +%s)}"; mkdir -p "$TOWN"; cool="$TOWN/cool.txt"; touch "$cool"
    chars=$(awk -F'|' -v i="$it" '/^SOURCE/{a=$3; gsub(/ /,"",a); c=$2; gsub(/ /,"",c); if (a==i) s=s c} END{print s}' "$HERE/ai.pdl"); [ -n "$chars" ] || return 1
    awk -v n="$now" -v i="$it" '$3==i && $4>n {print $1, $2}' "$cool" > "$PET/ai_excl.txt"
    tx=""; mem=$(awk -F'|' -v i="$it" '/^PLACE/{a=$2; gsub(/ /,"",a); if (a==i) {x=$3+0; y=$4+0; c=$5; gsub(/ /,"",c); n=$6+0; if (n>=b) {b=n; bx=x; by=y; bc=c}}} END{if (b>0) print bx, by, bc}' "$PET/places.txt" 2>/dev/null)
    if [ -n "$mem" ]; then read -r mx my mc <<EOF
$mem
EOF
        if grep -q "^$mx $my\$" "$PET/ai_excl.txt" 2>/dev/null; then mem=""; else tx=$mx; ty=$my; tc=$mc; fi; fi
    if [ -z "$tx" ]; then nr=$("$WO" nearest "$SHARED" "$ACTIVE" "$chars" "$PET/ai_excl.txt") || { ai_log "$ACTIVE" "none-found $it"; return 1; }; read -r tx ty _ tc <<EOF
$nr
EOF
    fi
    r=$("$WO" npcgo "$SHARED" "$ACTIVE" "$tx" "$ty")
    case "$r" in
        moved*) echo "go $it $tx $ty" > "$PET/ai_goal.txt"; return 1 ;;
        arrived) ;;
        *) rm -f "$PET/ai_goal.txt"; grep -v "^PLACE *| *$it *| *$tx *| *$ty " "$PET/places.txt" > "$PET/places.tmp" 2>/dev/null; mv -f "$PET/places.tmp" "$PET/places.txt" 2>/dev/null; ai_log "$ACTIVE" "stuck $it $tx,$ty"; return 1 ;;
    esac
    row=$(awk -F'|' -v c="$tc" -v i="$it" '/^SOURCE/{a=$3; gsub(/ /,"",a); k=$2; gsub(/ /,"",k); if (a==i && k==c) {n=$4+0; g=$5+0; print n, g; exit}}' "$HERE/ai.pdl"); read -r amt regrow <<EOF
$row
EOF
    inv_add "$it" "${amt:-1}" >/dev/null; grep -v "^$tx $ty $it " "$cool" > "$cool.tmp"; mv -f "$cool.tmp" "$cool"; echo "$tx $ty $it $((now + ${regrow:-120}))" >> "$cool"
    old=$(awk -F'|' -v i="$it" -v x="$tx" -v y="$ty" '/^PLACE/{a=$2; gsub(/ /,"",a); if (a==i && $3+0==x && $4+0==y) print $6+0}' "$PET/places.txt" 2>/dev/null | head -1); grep -v "^PLACE *| *$it *| *$tx *| *$ty " "$PET/places.txt" > "$PET/places.tmp" 2>/dev/null
    printf 'PLACE | %s | %s | %s | %s | %s\n' "$it" "$tx" "$ty" "$tc" "$(( ${old:-0} + 1 ))" >> "$PET/places.tmp"; mv -f "$PET/places.tmp" "$PET/places.txt"; ai_log "$ACTIVE" "gathered ${amt:-1} $it at $tx,$ty"; rm -f "$PET/ai_goal.txt"; return 0; }
# ---- stats (skills.pdl): Power / Magic / Defense / Intellect, trained by chatting
stat_exp() { getv "stat_$1_exp"; }
stat_level() { sl_per=$(awk -F'|' -v n="$1" '/^STAT/{a=$2; gsub(/ /,"",a); if (a==n) {print $3+0; exit}}' "$HERE/skills.pdl"); sl_exp=$(stat_exp "$1"); echo $(( 1 + ${sl_exp:-0} / ${sl_per:-10} )); }
stat_add() { # stat_add <stat> <exp>: give EXP; announces a level up in the chat
    [ "${2:-0}" -gt 0 ] 2>/dev/null || return 0; old=$(stat_level "$1"); addv "stat_$1_exp" "$2"; new=$(stat_level "$1")
    if [ "$new" -gt "$old" ]; then printf '%s: my %s is level %s!\n' "$(getv name_id)" "$1" "$new" >> "$CHAT"; printf '%s | stat | %s level %s\n' "$(date '+%H:%M:%S')" "$1" "$new" >> "$PET/log.txt"; fi; }
stat_train() { # stat_train <text>: every trigger word in the text trains its stat (a few words per message at most)
    cfg=$(sed -n 's/^CONFIG *| *//p' "$HERE/skills.pdl" | head -1); ep=$(printf '%s' "$cfg" | sed -n 's/.*exp_per=\([0-9]*\).*/\1/p'); mx=$(printf '%s' "$cfg" | sed -n 's/.*max_per_message=\([0-9]*\).*/\1/p')
    words=$(printf '%s' "$1" | tr 'A-Z' 'a-z' | tr -c 'a-z' ' ')
    awk -F'|' -v w="$words" -v mx="${mx:-3}" 'BEGIN{n=split(w, ws, " "); for(i=1;i<=n;i++) have[ws[i]]=1} /^STAT/{a=$2; gsub(/ /,"",a); c=0; m=split($4, tw, " "); for(j=1;j<=m;j++) if (tw[j] in have) c++; if (c>mx) c=mx; if (c>0) print a, c}' "$HERE/skills.pdl" | while read -r st c; do stat_add "$st" $(( c * ${ep:-2} )); done; }
# ---- economy: trades between pets (economy.pdl prices), stores, rent. Everything moves real item folders between bags; every trade is a ledger row (state/world/ledger.txt).
pinv_count() { ls -d "$1/inventory/$2"_* 2>/dev/null | wc -l; }
pinv_move() { # pinv_move <from_dir> <to_dir> <item> <n>: move up to n item folders from one bag to another; prints how many moved
    mv_n=0; mkdir -p "$2/inventory"; while [ "$mv_n" -lt "$4" ]; do f=$(ls -d "$1/inventory/$3"_* 2>/dev/null | head -1); [ -n "$f" ] || break; mv "$f" "$2/inventory/$3_m$$_${mv_n}_$(date +%N)"; mv_n=$((mv_n + 1)); done; echo "$mv_n"; }
pricev() { sed -n "s/^PRICE *| *$1 *| *//p" "$HERE/economy.pdl" | head -1 | tr -d ' '; }
econv() { sed -n "s/^ECON *| *$1 *| *//p" "$HERE/economy.pdl" | head -1 | tr -d ' '; }
deal() { # deal <buyer_dir> <seller_dir> <item> <n>: coins go one way, the items the other; prints ok or why not (no-price, seller-short, buyer-poor)
    dpr=$(pricev "$3"); [ -n "$dpr" ] || { echo no-price; return 1; }; dtot=$(( dpr * $4 ))
    [ "$(pinv_count "$2" "$3")" -ge "$4" ] || { echo seller-short; return 1; }
    [ "$(pinv_count "$1" coin)" -ge "$dtot" ] || { echo buyer-poor; return 1; }
    pinv_move "$1" "$2" coin "$dtot" >/dev/null; pinv_move "$2" "$1" "$3" "$4" >/dev/null; mkdir -p "$TOWN"
    printf 'TRADE|%s|%s|%s|%s|%s|%s\n' "${PET_AI_NOW:-$(date +%s)}" "$(basename "$1")" "$(basename "$2")" "$3" "$4" "$dtot" >> "$TOWN/ledger.txt"; echo ok; }
town_list() { # town_list <kind>: "bid owner door_x door_y cost" per building of that kind (the door is the tile a visitor walks to)
    awk -F'|' -v k="$1" '/^BUILDING/{b=$2; kd=$3; o=$8; gsub(/ /,"",b); gsub(/ /,"",kd); gsub(/ /,"",o); if (kd==k) printf "%s %s %d %d %d\n", b, o, $4+1, $5+2, $9+0}' "$TOWN/town_built.pdl" 2>/dev/null; }
ai_goto() { # ai_goto <x> <y>: one step toward a tile; returns 0 when beside it, 1 while moving, 2 when stuck
    r=$("$HERE/ops/+x/pet_world.+x" npcgo "$SHARED" "$ACTIVE" "$1" "$2"); case "$r" in arrived) return 0;; moved*) echo "shop $1 $2" > "$PET/ai_goal.txt"; return 1;; *) rm -f "$PET/ai_goal.txt"; return 2;; esac; }
ai_shop() { # ai_shop <item>: walk to a store whose owner (not me) has the item, and buy one; returns 0 when it bought
    pr=$(pricev "$1"); [ "$(inv_count coin)" -ge "${pr:-999}" ] || return 1
    sl=$(town_list store | while read -r sb so sx sy sc; do [ "$so" != "$ACTIVE" ] && [ "$(pinv_count "$SHARED/pets/$so" "$1")" -gt 0 ] && echo "$sb $so $sx $sy" && break; done); [ -n "$sl" ] || return 1
    read -r sb so sx sy <<EOF
$sl
EOF
    ai_goto "$sx" "$sy"; g=$?; [ "$g" = 0 ] || return 1
    d=$(deal "$PET" "$SHARED/pets/$so" "$1" 1); if [ "$d" = ok ]; then rm -f "$PET/ai_goal.txt"; ai_log "$ACTIVE" "bought 1 $1 from $so at $sb"; return 0; fi; ai_log "$ACTIVE" "shop refused $d"; return 1; }
ai_sell_wood() { # sell the wood above what the pet keeps to a store owner that has the coins
    ww=$(aiv want_wood); hw=$(inv_count wood); ex=$(( hw - ${ww:-8} )); [ "$ex" -gt 0 ] || return 1; pr=$(pricev wood)
    sl=$(town_list store | while read -r sb so sx sy sc; do [ "$so" != "$ACTIVE" ] && [ "$(pinv_count "$SHARED/pets/$so" coin)" -ge $(( ${pr:-2} * ex )) ] && echo "$sb $so $sx $sy" && break; done); [ -n "$sl" ] || return 1
    read -r sb so sx sy <<EOF
$sl
EOF
    ai_goto "$sx" "$sy"; g=$?; [ "$g" = 0 ] || return 1
    d=$(deal "$SHARED/pets/$so" "$PET" wood "$ex"); if [ "$d" = ok ]; then rm -f "$PET/ai_goal.txt"; ai_log "$ACTIVE" "sold $ex wood to $so"; return 0; fi; ai_log "$ACTIVE" "sale refused $d"; return 1; }
ai_rent() { # a pet with no house of its own pays rent to the owner of the house it rents (or finds one); no coins = evicted
    rf="$TOWN/rent.pdl"; touch "$rf"; now="${PET_AI_NOW:-$(date +%s)}"
    mine=$(town_list house | awk -v o="$ACTIVE" '$2==o{c++} END{print c+0}'); [ "$mine" -gt 0 ] && { grep -v "^RENT | [^|]*| $ACTIVE |" "$rf" > "$rf.tmp"; mv -f "$rf.tmp" "$rf"; return 0; }
    pct=$(sed -n 's/^RENT *| *pct=\([0-9]*\).*/\1/p' "$HERE/economy.pdl" | head -1); ev=$(sed -n 's/^RENT .*every=\([0-9]*\).*/\1/p' "$HERE/economy.pdl" | head -1); pct=${pct:-5}; ev=${ev:-1440}
    row=$(grep "^RENT | [^|]* | $ACTIVE |" "$rf" | head -1)
    if [ -n "$row" ]; then rb=$(printf '%s' "$row" | awk -F'|' '{gsub(/ /,"",$2); print $2}'); due=$(printf '%s' "$row" | awk -F'|' '{gsub(/ /,"",$4); print $4+0}')
        [ "$now" -ge "$due" ] || return 0
        hl=$(town_list house | awk -v b="$rb" '$1==b'); read -r hb ho hx hy hc <<EOF
$hl
EOF
        amt=$(( (hc * pct + 99) / 100 )); if [ -n "$ho" ] && [ "$(inv_count coin)" -ge "$amt" ]; then pinv_move "$PET" "$SHARED/pets/$ho" coin "$amt" >/dev/null; grep -v "^RENT | $rb | $ACTIVE |" "$rf" > "$rf.tmp"; printf 'RENT | %s | %s | %s\n' "$rb" "$ACTIVE" "$((due + ev))" >> "$rf.tmp"; mv -f "$rf.tmp" "$rf"; printf 'RENTPAY|%s|%s|%s|%s|%s\n' "$now" "$ACTIVE" "$ho" "$rb" "$amt" >> "$TOWN/ledger.txt"; ai_log "$ACTIVE" "paid rent $amt to $ho"
        else grep -v "^RENT | $rb | $ACTIVE |" "$rf" > "$rf.tmp"; mv -f "$rf.tmp" "$rf"; ai_log "$ACTIVE" "evicted from $rb (no coins)"; fi
        return 0; fi
    hl=$(town_list house | while read -r hb ho hx hy hc; do [ "$ho" != "$ACTIVE" ] && ! grep -q "^RENT | $hb |" "$rf" && echo "$hb $ho $hc" && break; done); [ -n "$hl" ] || return 0; read -r hb ho hc <<EOF
$hl
EOF
    amt=$(( (hc * pct + 99) / 100 )); [ "$(inv_count coin)" -ge $(( amt * 2 )) ] || return 0
    pinv_move "$PET" "$SHARED/pets/$ho" coin "$amt" >/dev/null; printf 'RENT | %s | %s | %s\n' "$hb" "$ACTIVE" "$((now + ev))" >> "$rf"; printf 'RENTPAY|%s|%s|%s|%s|%s\n' "$now" "$ACTIVE" "$ho" "$hb" "$amt" >> "$TOWN/ledger.txt"; ai_log "$ACTIVE" "rented $hb from $ho for $amt"; }
ai_build() { # the pet builds when it can pay: a house first, then a farm, then a store (a store needs a house first)
    for kd in house farm store; do
        have=$(town_list "$kd" | awk -v o="$ACTIVE" '$2==o{c++} END{print c+0}'); [ "$have" -gt 0 ] && continue
        [ "$kd" = store ] && [ "$(town_list house | awk -v o="$ACTIVE" '$2==o{c++} END{print c+0}')" -eq 0 ] && continue
        row=$(grep "^KIND *| *$kd " "$HERE/buildings.pdl" | head -1); wd=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$5); print $5}'); cn=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$6); print $6}'); w=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$3); print $3}'); h=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$4); print $4}')
        need=$(( cn + $(land_fee "$w" "$h") )); if [ "$(inv_count wood)" -ge "$wd" ] && [ "$(inv_count coin)" -ge "$need" ]; then sh "$0" build_building "$kd" >/dev/null 2>&1; ai_log "$ACTIVE" "built a $kd"; return 0; fi
    done; return 1; }
gen_events() { # build <pet dir>/event_pkg/pages/page_N for every pet event: system events, then one per menu row. The pages are what events-hq opens (event.ir.pdl, event.pal, condition.pdl, cmd_1.sh).
    GENV=$(cat "$(rooms_file)" "$HERE/menu.pdl" "$HERE/ops/pet_event.sh" 2>/dev/null | cksum | cut -d' ' -f1)
    P="$PET/event_pkg/pages"; [ -f "$PET/event_pkg/events_index.txt" ] && [ "$(cat "$PET/event_pkg/.generated" 2>/dev/null)" = "$GENV" ] && return 0
    rm -rf "$PET/event_pkg"; mkdir -p "$P"; : > "$PET/event_pkg/events_index.txt"; k=0
    mkpage() { # mkpage <id> <trigger> <verb> <arg> <note>
        k=$((k + 1)); p="$P/page_$k"; mkdir -p "$p"
        printf 'SECTION      | KEY                | VALUE\n----------------------------------------\nMETA         | piece_id           | pet_%s\nSTATE        | source             | blocks\nNODE         | id=1 type=pet_verb     | verb=%s arg=%s\nNODE         | id=2 type=ret          | \n' "$1" "$3" "$4" > "$p/event.ir.pdl"
        printf 'COND | trigger | %s\n' "$2" > "$p/condition.pdl"
        printf '# %s: %s\nexec cmd_1.sh\n' "$1" "$5" > "$p/event.pal"
        printf '#!/bin/sh\n# %s: %s\ncd "$(dirname "$0")/../../.." || exit 1\nexec sh "%s/ops/pet_event.sh" %s %s\n' "$1" "$5" "$HERE" "$3" "$4" > "$p/cmd_1.sh"; chmod +x "$p/cmd_1.sh"
        printf '%s | %s | %s %s | %s\n' "$k" "$1" "$3" "$4" "$5" >> "$PET/event_pkg/events_index.txt"
    }
    mkpage start on-click start "" "Play: the pet is started (traffic light green)"
    mkpage stop on-click stop "" "Stop: the pet is stopped (traffic light red)"
    mkpage day_tick parallel tick "" "Day tick: needs rise, self care, report card, evolution"
    mkpage chat_touch on-touch touch head "Touched: the pet reacts"
    mkpage view_manage on-click view manage "Manage tab: show all six pets in a row"
    mkpage view_world on-click view world "World button: the trainer (you) in the village"
    mkpage view_room on-click view room "Back to the pet's house (the village door does the same)"
    mkpage world_up on-click world_move up "Trainer walks up"; mkpage world_down on-click world_move down "Trainer walks down"
    mkpage world_left on-click world_move left "Trainer walks left"; mkpage world_right on-click world_move right "Trainer walks right"
    mkpage world_door on-touch view room "Door: stepping into the player's door teleports into the house (view room)"
    while IFS='|' read -r tag room x dest arr auto; do case "$tag" in DOOR*) ;; *) continue;; esac; room=$(echo $room); dest=$(echo $dest); arr=$(echo $arr)
        mkpage "door_${room}_${dest}" on-touch teleport "$dest $arr" "Door: walking into it teleports to $dest (rooms.pdl row)"; done < "$(rooms_file)"
    for pid in $(awk '{print $1}' "$SHARED/party.txt" 2>/dev/null); do mkpage "select_$pid" on-click select "$pid" "Choose pet $pid as the active pet"; done
    while IFS='|' read -r tag id g label verb arg need; do
        case "$tag" in MENU*) ;; *) continue;; esac
        id=$(echo $id); verb=$(echo $verb); arg=$(echo $arg | sed 's/^-$//'); mkpage "$id" on-click "$verb" "$arg" "$(echo $label)"
    done < "$HERE/menu.pdl"
    printf '%s\n' "$GENV" > "$PET/event_pkg/.generated"
}
stage_pins() { # prints "pin=value" lines for the current level + habit traits
    lvl=$(getv rpg_level); lvl=${lvl:-1}
    awk -F'|' -v lvl="$lvl" '/^STAGE/{ l=0; for(i=2;i<=NF;i++){x=$i; gsub(/^ +| +$/,"",x); if (x ~ /^level=/) {sub(/level=/,"",x); l=x+0}}
        if (l<=lvl && l>=best) { best=l; line=$0 } } END { n=split(line, a, "|"); for(i=3;i<=n;i++){x=a[i]; gsub(/^ +| +$/,"",x); print x} }' "$HERE/evolution.pdl"
    awk -F'|' '/^TRAIT/{c="";per=1;pin="";add=0;cap=0; for(i=2;i<=NF;i++){x=$i; gsub(/^ +| +$/,"",x); split(x,kv,"="); if(kv[1]=="counter")c=kv[2]; if(kv[1]=="per")per=kv[2]+0; if(kv[1]=="pin")pin=kv[2]; if(kv[1]=="add")add=kv[2]+0; if(kv[1]=="cap")cap=kv[2]+0}
        print c, per, pin, add, cap}' "$HERE/evolution.pdl" | while read c per pin add cap; do
        cnt=$(getv "$c"); cnt=${cnt:-0}; extra=$(( cnt / per * add )); [ "$extra" -gt "$cap" ] && extra=$cap
        echo "trait_$pin=$extra"; done
}

evolve() {
    need_pet
    pins=$(stage_pins)
    stage=$(awk -F'|' -v lvl="$(getv rpg_level)" '/^STAGE/{n=$2; gsub(/^ +| +$/,"",n); l=0; for(i=3;i<=NF;i++){x=$i; gsub(/^ +| +$/,"",x); if (x ~ /^level=/){sub(/level=/,"",x); l=x+0}} if (l<=(lvl==""?1:lvl) && l>=b){b=l; s=n}} END{print s}' "$HERE/evolution.pdl")
    printf '%s\n' "$pins" > "$PET/pins.txt"
    sig="$stage $(printf '%s' "$pins" | tr '\n' ' ')"
    old=$(cat "$PET/evolve_sig.txt" 2>/dev/null)
    [ "$sig" = "$old" ] && { printf '%s\n' "$stage" > "$PET/stage.txt"; return 0; }
    # build a pet.pdl of KEY rows for pet_gen from the pins (+ trait adds)
    {
        sp=$(getv species); [ -n "$sp" ] && echo "KEY | species | $sp"
        printf '%s\n' "$pins" | while IFS='=' read k v; do
            case "$k" in trait_*) ;; scale|ear_len|ear_w|body_r|body_g|body_b) echo "KEY | $k | $v";; esac; done
    } > "$PET/pins.pdl"
    for t in $(printf '%s\n' "$pins" | sed -n 's/^trait_//p'); do
        pin=${t%%=*}; add=${t#*=}; base=$(sed -n "s/^KEY | $pin | //p" "$PET/pins.pdl" | head -1)
        [ -n "$base" ] && sed -i "s/^KEY | $pin | .*/KEY | $pin | $((base + add))/" "$PET/pins.pdl"
    done
    seed=$(getv seed); [ -x "$GEN" ] && "$GEN" "$PET/art" "${seed:-1}" "$PET/pins.pdl" >/dev/null 2>&1
    printf '%s\n' "$stage" > "$PET/stage.txt"; printf '%s\n' "$sig" > "$PET/evolve_sig.txt"
    printf '%s | evolved | %s\n' "$(date '+%H:%M:%S')" "$stage" >> "$PET/log.txt"
}

nav_rows() { # nav_<i>_label/verb/arg from nav.pdl for the current view ({party} expands to one row per pet)
    vw=$(cat "$SHARED/view.txt" 2>/dev/null); vw=${vw:-room}; [ "$vw" = world ] && [ "$(cat "$SHARED/party_open.txt" 2>/dev/null)" = 1 ] && vw=worldparty; i=0; : > "$SHARED/nav.tmp.$$"
    while IFS='|' read -r tag v label verb arg; do
        case "$tag" in NAV*) ;; *) continue;; esac; v=$(echo $v); [ "$v" = "$vw" ] || continue; label=$(echo $label); verb=$(echo $verb); arg=$(echo $arg)
        if [ "$label" = "{party}" ]; then n=0
            while read -r pid pname _; do n=$((n+1)); mark=""; [ "$pid" = "$ACTIVE" ] && mark="*"; sp=$(sed -n 's/^species=//p' "$SHARED/pets/$pid/variables.txt" 2>/dev/null | head -1)
                printf 'nav_%s_label=%s %s%s\nnav_%s_verb=%s\nnav_%s_arg=%s\n' "$i" "$n" "$pname" "$mark" "$i" "$verb" "$i" "$pid" >> "$SHARED/nav.tmp.$$"; i=$((i+1)); done < "$SHARED/party.txt"
        else printf 'nav_%s_label=%s\nnav_%s_verb=%s\nnav_%s_arg=%s\n' "$i" "$label" "$i" "$verb" "$i" "$arg" >> "$SHARED/nav.tmp.$$"; i=$((i+1)); fi
    done < "$HERE/nav.pdl"
    printf 'n_nav=%s\n' "$i"; cat "$SHARED/nav.tmp.$$"; rm -f "$SHARED/nav.tmp.$$"
}
db_seed() { # add the six starter pets to the RPG Maker DB once: CLASS Pet, ACTOR 13..18, SYSTEM PetParty (member1..6). Additive only; the hero party (SYSTEM Party) is never touched.
    [ -f "$DB/actors.pdl" ] || return 0
    grep -q '^SYSTEM *| name *| PetParty' "$DB/system.pdl" 2>/dev/null && return 0
    cid=13; grep -q '^CLASS *| name *| Pet$' "$DB/classes.pdl" || { printf 'CLASS        | id                 | %s\nCLASS        | name               | Pet\nCLASS        | exp_curve          | Normal\nCLASS        | note               | a creature the trainer raises (pet-trainer)\n' "$cid" >> "$DB/classes.pdl"; }
    id=12; sp=0
    for row in "Pochi|Bunny|a small grey bunny with long ears" "Momo|Bear|a round brown bear" "Kuro|Cat|an orange cat with a long tail" "Gumi|Frog|a green frog with big eyes" "Rin|Mouse|a grey mouse with big round ears" "Dai|Dragon|a small blue dragon with wings"; do
        id=$((id+1)); nm=${row%%|*}; r=${row#*|}; nick=${r%%|*}; prof=${r#*|}
        { printf 'ACTOR        | id                 | %s\nACTOR        | name               | %s\nACTOR        | nickname           | %s\nACTOR        | class              | Pet\n' "$id" "$nm" "$nick"
          printf 'ACTOR        | init_lv            | 1\nACTOR        | max_lv             | 99\nACTOR        | exp                | 0\nACTOR        | exp_to_next        | 0\nACTOR        | skills             | eat sleep wash play trick\n'
          printf 'ACTOR        | profile            | %s\nACTOR        | face               | \nACTOR        | character          | \nACTOR        | battler            | \n' "$prof"
          printf 'ACTOR        | weapon             | \nACTOR        | shield             | \nACTOR        | head               | \nACTOR        | body               | \nACTOR        | accessory          | \n'
          printf 'ACTOR        | mhp                | 30\nACTOR        | mmp                | 6\nACTOR        | atk                | 4\nACTOR        | def                | 4\nACTOR        | mat                | 4\nACTOR        | mdf                | 4\nACTOR        | agi                | 6\nACTOR        | luk                | 5\n'
          printf 'ACTOR        | note               | pet species %s seed %s\n' "$sp" "$((id-2))"; } >> "$DB/actors.pdl"; sp=$((sp+1))
    done
    { printf 'SYSTEM       | id                 | 5\nSYSTEM       | name               | PetParty\n'; i=0; for nm in Pochi Momo Kuro Gumi Rin Dai; do i=$((i+1)); printf 'SYSTEM       | member%s            | %s\n' "$i" "$nm"; done; } >> "$DB/system.pdl"
}
sync_party() { # party.txt = cache of the DB: one line per PetParty member in order: <a<actor id>> <name> <species> <seed>. Pets that have no folder yet are created.
    db_seed; [ -f "$DB/system.pdl" ] || return 0; mkdir -p "$SHARED/pets"
    awk -F'|' 'function tr(s){gsub(/^ +| +$/,"",s); return s}
        FNR==NR { k=tr($2); v=tr($3); if ($1 ~ /^SYSTEM/) { if (k=="name") inpp=(v=="PetParty"); else if (inpp && k ~ /^member[0-9]+$/) { m[++n]=v } } next }
        $1 ~ /^ACTOR/ { k=tr($2); v=tr($3); if (k=="id") cur=v; else if (k=="name") nm[cur]=v; else if (k=="note" && v ~ /^pet /) { split(v,a," "); sp[cur]=a[3]; sd[cur]=a[5] } }
        END { for (i=1;i<=n;i++) for (c in nm) if (nm[c]==m[i] && (c in sp)) { printf "a%s %s %s %s\n", c, m[i], sp[c], sd[c]; break } }' "$DB/system.pdl" "$DB/actors.pdl" > "$SHARED/party.tmp.$$"
    [ -s "$SHARED/party.tmp.$$" ] || { rm -f "$SHARED/party.tmp.$$"; return 0; }
    cmp -s "$SHARED/party.tmp.$$" "$SHARED/party.txt" 2>/dev/null && { rm -f "$SHARED/party.tmp.$$"; } || mv -f "$SHARED/party.tmp.$$" "$SHARED/party.txt"
    while read -r pid pname psp psd; do [ -f "$SHARED/pets/$pid/variables.txt" ] || PET_DIR="$SHARED/pets/$pid" PET_SPECIES="$psp" PET_NAME="$pname" sh "$0" new_pet "${psd:-1}" >/dev/null 2>&1; done < "$SHARED/party.txt"
    [ -s "$SHARED/active.txt" ] || awk 'NR==1{print $1}' "$SHARED/party.txt" > "$SHARED/active.txt"; [ -s "$SHARED/view.txt" ] || echo room > "$SHARED/view.txt"
}
ensure_party() { # first run: seed the DB and build every pet, then the village
    [ -f "$SHARED/world.st" ] && { sync_party; [ -s "$SRV/phones.index" ] || ph_ensure; return 0; }
    sync_party; [ -x "$WORLD" ] && "$WORLD" init "$SHARED" "$SHARED/party.txt" >/dev/null 2>&1; ph_ensure
}
WORLD="${PET_WORLD:-$HERE/ops/+x/pet_world.+x}"
status() {
    need_pet; [ -z "${PET_DIR:-}" ] && sync_party
    h=$(getv hunger); e=$(getv energy); c=$(getv clean); p=$(getv happy)
    state=Fine; [ "${h:-0}" -ge 60 ] && state=Hungry; [ "${e:-100}" -le 25 ] && state=Sleepy; [ "${c:-100}" -le 25 ] && state=Dirty
    anim=idle; [ "$state" = Hungry ] && anim=hungry; [ "$state" = Sleepy ] && anim=sleepy; [ "$state" = Dirty ] && anim=sad
    [ "${p:-50}" -ge 85 ] && [ "$state" = Fine ] && anim=happy
    if [ -f "$PET/expression.txt" ]; then set -- $(cat "$PET/expression.txt"); [ "$(date +%s)" -le "${2:-0}" ] && anim=$1; fi
    frame=$(( $(date +%s) % 8 ))
    {
        [ -z "$(getv name_id)" ] && setv name_id "$(awk -v i="$ACTIVE" '$1==i{print $2}' "$SHARED/party.txt" 2>/dev/null | head -1)"; [ -z "$(getv name_id)" ] && setv name_id Pochi
        printf 'title=Pet house\nname=%s\nstage=%s\nlevel=%s\nexp=%s\nmp=%s\nhunger=%s\nenergy=%s\nclean=%s\nhappy=%s\nstate=%s\n' \
            "$(getv name_id)" "$(cat "$PET/stage.txt" 2>/dev/null)" "$(getv rpg_level)" "$(getv rpg_exp)" "$(getv rpg_mp)" "${h:-0}" "${e:-100}" "${c:-100}" "${p:-50}" "$state"
        printf 'grade=%s\n' "$(sed -n 's/.*max_tier: *//p' "$PET/learning_limits.pdl" 2>/dev/null | head -1)"
        printf 'likes=%s\n' "$(sed -n 's/^pref_\([a-z]*\)=\(.*\)/\1:\2/p' "$W" | tr '\n' ' ')"
        { lv=$(getv rpg_level); i=0; : > "$PET/menu.tmp.$$"; grp=$(cat "$PET/menu_group.txt" 2>/dev/null)
          if [ -z "$grp" ]; then   # level 1: the groups the pet's level allows (in menu.pdl order, once each)
              for g in $(awk -F'|' -v lv="${lv:-1}" '/^MENU/{g=$3; n=$7; gsub(/ /,"",g); gsub(/ /,"",n); if (n+0<=lv+0 && !(g in seen)) {seen[g]=1; print g}}' "$HERE/menu.pdl"); do
                  printf 'menu_%s_label=%s\nmenu_%s_verb=menu_group\nmenu_%s_arg=%s\n' "$i" "$g >" "$i" "$i" "$g" >> "$PET/menu.tmp.$$"; i=$((i+1)); done
          else
              while IFS='|' read -r tag id g label verb arg need; do
                  case "$tag" in MENU*) ;; *) continue;; esac
                  g=$(echo $g); need=$(echo "$need" | tr -d ' '); [ "$g" = "$grp" ] || continue; [ "${need:-1}" -le "${lv:-1}" ] || continue
                  printf 'menu_%s_label=%s\nmenu_%s_verb=fire\nmenu_%s_arg=%s\n' "$i" "$(echo $label)" "$i" "$i" "$(echo $id)" >> "$PET/menu.tmp.$$"; i=$((i+1))
              done < "$HERE/menu.pdl"
              printf 'menu_%s_label=< Back\nmenu_%s_verb=menu_group\nmenu_%s_arg=\n' "$i" "$i" "$i" >> "$PET/menu.tmp.$$"; i=$((i+1))
          fi
          mo=$(cat "$PET/menu_open.txt" 2>/dev/null || echo 0); [ "$(cat "$SHARED/view.txt" 2>/dev/null)" = room ] || [ ! -f "$SHARED/view.txt" ] || mo=0; shown=0; [ "$mo" = 1 ] && shown=$i
          printf 'n_menu=%s\nn_menu_shown=%s\nmenu_visible=%s\nmenu_group=%s\n' "$i" "$shown" "$mo" "$grp"; cat "$PET/menu.tmp.$$"; }
        if running; then printf 'run_cls=ph-green\nrun_label=GO started\nrun_on=1\n'; else printf 'run_cls=ph-red\nrun_label=STOP stopped\nrun_on=0\n'; fi
        printf 'event_n=%s\n' "$(grep -c . "$PET/event_pkg/events_index.txt" 2>/dev/null)"
        printf 'anim=%s\nscene_raw=%s/scene.raw\ncanvas_raw=%s/scene.raw\nview=%s\nactive_id=%s\nactive_dir=%s\n' "$anim" "$SHARED" "$SHARED" "$(cat "$SHARED/view.txt" 2>/dev/null || echo room)" "$ACTIVE" "$PET"
        ia=$(cat "$SHARED/interact_armed.txt" 2>/dev/null); if [ "$ia" = 1 ]; then printf 'interact_armed=1\ninteract_class=interact-active\ninteract_label=on\n'; else printf 'interact_armed=0\ninteract_class=\ninteract_label=off\n'; fi
        printf 'bv_h1=%s/interact_relay.txt\nbv_h2=%s/keyboard/history.txt\n' "$SHARED" "$SHARED"
        [ -f "$SHARED/camera.st" ] && sed 's/^/cam_/' "$SHARED/camera.st"
        { nb=0; while read -r bid bname _; do printf 'bk_%s_label=%s\nbk_%s_arg=%s\nbk_%s_active=%s\n' "$nb" "$bname" "$nb" "$bid" "$nb" "$([ "$bid" = "$ACTIVE" ] && echo active)"; nb=$((nb+1)); done < "$SHARED/party.txt"
          printf 'n_book=%s\nbook_label=book:%s\npage_label=page:%s\n' "$nb" "$(getv name_id)" "$(cat "$SHARED/view.txt" 2>/dev/null || echo room)"; }
        cs=$(sh "$HERE/ops/pet_clock.sh" status 2>/dev/null); printf '%s\n' "$cs" | grep -v "^time_label=\|^game_ms="
        gms=$(printf '%s\n' "$cs" | sed -n 's/^game_ms=//p'); if [ -n "$gms" ]; then bm=$(getv born_ms); if [ -z "$bm" ]; then bm=$(( $(cat "$SHARED/clock_day0.txt" 2>/dev/null || echo 0) * 86400000 )); setv born_ms "$bm"; fi      # age in GAME time: the pet clock's game ms now minus the game ms it was born (pets that predate this were born when the clock started)
            awk -v g="$gms" -v b="$bm" 'BEGIN { s = int((g - b) / 1000); if (s < 0) s = 0; printf "age_label=age %dd %02dh\n", int(s / 86400), int((s % 86400) / 3600) }'; else printf 'age_label=age --\n'; fi
        printf 'stat_line=pow %s mag %s def %s int %s\n' "$(stat_level power)" "$(stat_level magic)" "$(stat_level defense)" "$(stat_level intellect)"
        printf 'rec_label=%s\n' "$([ "$(cat "$SHARED/recording.txt" 2>/dev/null)" = 1 ] && echo 'RECORDING... (press to cancel)' || echo 'Talk (mic)')"
        printf 'phone_number=%s\nn_contacts=%s\n' "$(ph_number "$ACTIVE")" "$(grep -c '^CONTACT' "$PET/contacts.pdl" 2>/dev/null || echo 0)"
        printf 'loc=%s\n' "$(cat "$PET/loc.txt" 2>/dev/null || echo bedroom)"
        nav_rows
        tail -8 "$CHAT" 2>/dev/null | fold -s -w 26 | sed 's/ *$//' | tail -6 | awk '{ printf "chat_%d=%s\n", NR - 1, $0 } END { for (i = NR; i < 6; i++) printf "chat_%d=\n", i }'      # long lines WRAP onto new lines at word boundaries (26 chars fit the chat column); the last 6 wrapped lines show
        printf 'known_words=%s\n' "$(awk -F'|' '/^LEX/{p=$2; gsub(/^ +| +$/,"",p); printf "%s ", p}' "$LEXF" 2>/dev/null)"
        printf 'pantry=%s\n' "$(for it in $(awk -F'|' '/^ITEM/{n=$2;gsub(/^ +| +$/,"",n);print n}' "$HERE/items.pdl"); do printf '%s %s ' "$it" "$(inv_count "$it")"; done)"
        { n=0; sel=0; [ -x "$IOP" ] && "$IOP" project "$PET" "$PET/inv_proj.txt" >/dev/null 2>&1
          [ -f "$PET/inv_proj.txt" ] && { n=$(sed -n 's/^count=//p' "$PET/inv_proj.txt"); sel=$(sed -n 's/^selected=//p' "$PET/inv_proj.txt"); }
          io=$(cat "$PET/inv_open.txt" 2>/dev/null || echo 0); [ "$(cat "$SHARED/view.txt" 2>/dev/null)" = room ] || [ ! -f "$SHARED/view.txt" ] || io=0; ishown=0; [ "$io" = 1 ] && ishown=${n:-0}
          printf 'inv_n=%s\ninv_sel=%s\ninv_shown=%s\ninv_visible=%s\n' "${n:-0}" "${sel:-0}" "$ishown" "$io"
          [ -f "$PET/inv_proj.txt" ] && sed -n 's/^slot_\([0-9]*\)=\(.*\)|\(.*\)$/inv_\1_text=\2 \3/p' "$PET/inv_proj.txt"
          hv=1; [ "$(cat "$SHARED/hotbar_hidden.txt" 2>/dev/null)" = 1 ] && hv=""
          hn=${n:-0}; [ "$hn" -gt 5 ] && hn=5      # the bar shows the first five items (it must fit over the 360 px picture)
          printf 'hb_visible=%s\nhb_n_slots=%s\n' "$hv" "$hn"
          cv=1; [ "$(cat "$SHARED/chat_hidden.txt" 2>/dev/null)" = 1 ] && cv=""; printf 'chat_visible=%s\n' "$cv"
          selname=$(sed -n "s/^slot_${sel:-0}=.*|//p" "$PET/inv_proj.txt" 2>/dev/null | head -1); printf 'hb_title=%s - slot: %s\n' "$(getv name_id)" "${selname:-empty}"
          [ -f "$PET/inv_proj.txt" ] && awk -F'[=|]' -v s="${sel:-0}" '/^slot_/{i=substr($1,6)+0; if (i>4) next; printf "hbs_%d_text=%s\nhbs_%d_cls=%s\n", i, $2, i, (i==s ? "hb-sel" : "")}' "$PET/inv_proj.txt"; }
    } > "$SHARED/ui.tmp.$$"; if cmp -s "$SHARED/ui.tmp.$$" "$SHARED/ui.txt"; then rm -f "$SHARED/ui.tmp.$$"; else mv -f "$SHARED/ui.tmp.$$" "$SHARED/ui.txt"; fi; printf '%s\n' "$PET" > "$SHARED/active_dir.txt"
    cat "$SHARED/ui.txt"
}


# ---- Play Mode (RPG Maker "run / stop"): the pet is either STARTED (running.txt = 1) or STOPPED. While stopped, only the system events below work; everything else is ignored
# (and logged), exactly like Doom's play flag. The window shows it as a traffic light (green = started, red = stopped).
running() { [ "$(cat "$PET/running.txt" 2>/dev/null)" = 1 ]; }
case "$VERB" in
    start|stop|ai_step|buy|sell|help|tip|land_price|build_building|chat_toggle|phones|exchange|call|build_room|map|listen|chat_send|clock_event|time_rate|time_advance|time_reinstall|status|stats|new_pet|save_slot|load_slot|fire|gen_events|new_event|menu_group|menu_toggle|inv_toggle|open_events|teleport|hotbar_toggle|interact|player|party_toggle|view|select|world_move|world_talk|"") ;;
    *) if ! running && [ "${PET_TRAIN:-0}" != 1 ]; then mkdir -p "$PET"; printf '%s | stopped | ignored %s\n' "$(date '+%H:%M:%S')" "$VERB" >> "$PET/log.txt"
           case "$VERB" in chat_input|chat|chat_send) printf '(the pet is stopped - press Play first)\n' >> "$PET/chat.txt"; status >/dev/null 2>&1;; esac; exit 0; fi ;;
esac
case "$VERB" in
    new_pet)
        mkdir -p "$PET"; seed="${ARG:-1}"
        printf 'hunger=30\nenergy=80\nclean=70\nhappy=50\nplay_total=0\nfed_total=0\nseed=%s\nrpg_level=1\nrpg_exp=0\nrpg_mp=6\nrpg_mp_max=6\n' "$seed" > "$V"
        [ -n "${PET_SPECIES:-}" ] && printf 'species=%s\n' "$PET_SPECIES" >> "$V"; [ -n "${PET_NAME:-}" ] && printf 'name_id=%s\n' "$PET_NAME" >> "$V"
        cp "$HERE/weights.default.pdl" "$PET/weights.pdl"; cp "$HERE/lexicon.default.pdl" "$PET/lexicon.pdl"; : > "$PET/chat.txt"; : > "$PET/chat_ledger.txt"; : > "$PET/obs_feedback_log.txt"; : > "$PET/tuning_ledger.txt"
        rm -rf "$PET/inventory" "$PET/used"; mkdir -p "$PET/inventory"; : > "$PET/log.txt"; rm -f "$PET/evolve_sig.txt"
        for it in $(awk -F'|' '/^ITEM/{n=$2;gsub(/^ +| +$/,"",n);print n}' "$HERE/items.pdl"); do st=$(pdlval "$HERE/items.pdl" "$it" start); [ -n "$st" ] && [ "$st" -gt 0 ] && inv_add "$it" "$st"; done
        { gms=$(sh "$HERE/ops/pet_clock.sh" status 2>/dev/null | sed -n 's/^game_ms=//p'); [ -n "$gms" ] && setv born_ms "$gms"; }      # born now, in game time (age_label counts from here)
        echo 0 > "$PET/running.txt"; rm -rf "$PET/event_pkg"; gen_events
        # meta.pdl: the same METHOD rows an entity like asa has, so the pet is a normal house entity (context menu: Events opens events-hq on its event_pkg, exactly asa's row)
        { printf 'SECTION      | KEY                  | VALUE\n----------------------------------------\nMETA         | piece_id           | pet\nSTATE        | kind                 | deskpal\nSTATE        | glyph                | 🐾\n'
          printf 'METHOD       | Events               | sh -c '"'"'exec "$1/&.widgits/events-hq/button.sh" "$0" "$1"'"'"'\n'
          printf 'METHOD       | Play                 | sh -c '"'"'exec sh "$1/@.apps/pet-trainer/ops/pet_event.sh" fire start'"'"'\n'
          printf 'METHOD       | Stop                 | sh -c '"'"'exec sh "$1/@.apps/pet-trainer/ops/pet_event.sh" fire stop'"'"'\n'
          printf 'METHOD       | Open house           | sh -c '"'"'exec sh "$1/@.apps/pet-trainer/button.sh" run'"'"'\n'
          printf 'METHOD       | Close                | CLOSE\nMETHOD       | Cancel               | void\n'; } > "$PET/meta.pdl"
        evolve; status >/dev/null ;;
    feed|give)
        need_pet; item="${ARG:-apple}"
        kind=$(pdlval "$HERE/items.pdl" "$item" kind 2>/dev/null); kind=$(awk -F'|' -v n="$item" '/^ITEM/{g=$2; gsub(/^ +| +$/,"",g); if(g==n){k=$3; gsub(/^ +| +$/,"",k); print k}}' "$HERE/items.pdl")
        [ -z "$kind" ] && exit 0
        have=$(inv_count "$item")
        [ "$have" -le 0 ] && { printf '%s | none left | %s\n' "$(date '+%H:%M:%S')" "$item" >> "$PET/log.txt"; status >/dev/null; exit 0; }
        inv_take_one "$item"
        pref=$(getw "pref_$item"); pref=${pref:-5}
        need=0; case "$kind" in food) need=$(getv hunger);; toy) need=$((100 - $(getv happy)));; soap) need=$((100 - $(getv clean)));; esac
        for k in hunger happy energy clean; do d=$(pdlval "$HERE/items.pdl" "$item" "$k"); [ -n "$d" ] && addv "$k" "$d"; done
        # weighted reaction: the more it likes the item the more it enjoys it (+1 happy per preference point above neutral)
        addv happy $(( pref - 5 ))
        case "$kind" in food) addv fed_total 1; skill eat; subj=feed;; toy) addv play_total 1; skill play; subj=play;; soap) skill wash; subj=wash;; esac
        # valence: good when the item met a real need (>= 40), bad when it was not needed; the pet learns its liking from that
        v=-1; [ "${need:-0}" -ge 40 ] && v=1
        feedback "$v" "$subj"; learn_pref "$item" "$v"
        evolve; status >/dev/null ;;
    sleep) need_pet; e0=$(getv energy); addv energy "$(getw sleep_energy)"; addv hunger 5; skill sleep; v=-1; [ "$e0" -lt 50 ] && v=1; feedback "$v" sleep; evolve; status >/dev/null ;;
    wash)  need_pet; c0=$(getv clean); addv clean "$(getw wash_clean)"; addv happy -2; skill wash; v=-1; [ "$c0" -lt 50 ] && v=1; feedback "$v" wash; evolve; status >/dev/null ;;
    play)  need_pet; e0=$(getv energy); addv happy "$(getw play_happy)"; addv energy -12; addv play_total 1; skill play; v=-1; [ "$e0" -gt 20 ] && v=1; feedback "$v" play; evolve; status >/dev/null ;;
    start) need_pet; ( setsid sh "$HERE/ops/pet_clock.sh" start >/dev/null 2>&1 & ); echo 1 > "$PET/running.txt"; printf '%s | event | start\n' "$(date '+%H:%M:%S')" >> "$PET/log.txt"; status >/dev/null ;;
    stop)  need_pet; echo 0 > "$PET/running.txt"; anyrun=0; for d in "$SHARED"/pets/*/; do [ "$(cat "${d}running.txt" 2>/dev/null)" = 1 ] && anyrun=1; done; [ "$anyrun" = 0 ] && ( setsid sh "$HERE/ops/pet_clock.sh" stop >/dev/null 2>&1 & ); printf '%s | event | stop\n' "$(date '+%H:%M:%S')" >> "$PET/log.txt"; status >/dev/null ;;
    save_slot) need_pet; n="${ARG:-1}"; case "$n" in ''|*[!0-9]*) exit 0;; esac; d="$PET/saves/slot_$n"; rm -rf "$d"; mkdir -p "$d"
        for x in variables.txt weights.pdl lexicon.pdl chat.txt obs_feedback_log.txt tuning_ledger.txt chat_ledger.txt stage.txt running.txt; do [ -f "$PET/$x" ] && cp "$PET/$x" "$d/"; done
        [ -d "$PET/inventory" ] && cp -r "$PET/inventory" "$d/"; [ -d "$PET/used" ] && cp -r "$PET/used" "$d/"
        printf 'SLOT | n | %s\nSLOT | saved_at | %s\nSLOT | level | %s\n' "$n" "$(date '+%Y-%m-%d %H:%M:%S')" "$(getv rpg_level)" > "$d/meta.pdl"; status >/dev/null ;;
    load_slot) need_pet; n="${ARG:-1}"; d="$PET/saves/slot_$n"; [ -f "$d/variables.txt" ] || exit 0
        for x in variables.txt weights.pdl lexicon.pdl chat.txt obs_feedback_log.txt tuning_ledger.txt chat_ledger.txt running.txt; do [ -f "$d/$x" ] && cp "$d/$x" "$PET/$x"; done
        rm -rf "$PET/inventory" "$PET/used"; [ -d "$d/inventory" ] && cp -r "$d/inventory" "$PET/"; [ -d "$d/used" ] && cp -r "$d/used" "$PET/"; mkdir -p "$PET/inventory"
        rm -f "$PET/evolve_sig.txt"; evolve; status >/dev/null ;;
    gen_events) need_pet; gen_events ;;
    open_events) need_pet; sh "$HOUSE/&.widgits/events-hq/button.sh" "$PET" "$HOUSE" >/dev/null 2>&1 & ;;
    new_event) need_pet; gen_events; n=$(ls -d "$PET/event_pkg/pages/page_"* 2>/dev/null | wc -l); n=$((n + 1)); p="$PET/event_pkg/pages/page_$n"; mkdir -p "$p"
        printf 'SECTION      | KEY                | VALUE\n----------------------------------------\nMETA         | piece_id           | pet_page_%s\nSTATE        | source             | blocks\nNODE         | id=1 type=show_text    | text=(new pet event)\nNODE         | id=2 type=ret          | \n' "$n" > "$p/event.ir.pdl"
        printf 'COND | trigger | on-click\n' > "$p/condition.pdl"; printf '# new pet event\nexec cmd_1.sh\n' > "$p/event.pal"; printf '#!/bin/sh\nexit 0\n' > "$p/cmd_1.sh"; chmod +x "$p/cmd_1.sh"
        printf '%s | new_%s | (empty page, edit it in events-hq)\n' "$n" "$n" >> "$PET/event_pkg/events_index.txt" ;;
    fire) need_pet; evn="$ARG"; n=$(awk -F'|' -v i="$evn" '{a=$2; gsub(/ /,"",a); if (a==i) {gsub(/ /,"",$1); print $1; exit}}' "$PET/event_pkg/events_index.txt" 2>/dev/null)
        [ -z "$n" ] && { gen_events; n=$(awk -F'|' -v i="$evn" '{a=$2; gsub(/ /,"",a); if (a==i) {gsub(/ /,"",$1); print $1; exit}}' "$PET/event_pkg/events_index.txt"); }
        [ -n "$n" ] && [ -x "$PET/event_pkg/pages/page_$n/cmd_1.sh" ] && PET_DIR="$PET" sh "$PET/event_pkg/pages/page_$n/cmd_1.sh" >/dev/null 2>&1; status >/dev/null ;;
    interact) need_pet; mkdir -p "$SHARED/keyboard"; : >> "$SHARED/interact_relay.txt"; : >> "$SHARED/keyboard/history.txt"
        if [ "$(cat "$SHARED/interact_armed.txt" 2>/dev/null)" = 1 ]; then echo 0 > "$SHARED/interact_armed.txt"; else echo 1 > "$SHARED/interact_armed.txt"; fi; status >/dev/null ;;
    party_toggle) need_pet; if [ "$(cat "$SHARED/party_open.txt" 2>/dev/null)" = 1 ]; then echo 0 > "$SHARED/party_open.txt"; else echo 1 > "$SHARED/party_open.txt"; fi; status >/dev/null ;;
    player) need_pet; case "$(cat "$SHARED/view.txt" 2>/dev/null)" in world) sh "$0" party_toggle;; manage) ;; *) sh "$0" menu_toggle;; esac ;;   # the Player tab: the pet's menu in the house, the tii-monster list in the village
    view) need_pet; echo 0 > "$SHARED/party_open.txt"; case "$ARG" in room|manage|world) ;; *) exit 0;; esac
        if [ "$ARG" = world ] && [ "$(cat "$SHARED/view.txt" 2>/dev/null)" != world ]; then [ -x "$WORLD" ] && { [ -f "$SHARED/world.st" ] || "$WORLD" init "$SHARED" "$SHARED/party.txt" >/dev/null 2>&1; "$WORLD" exit "$SHARED" >/dev/null 2>&1; }; fi
        echo "$ARG" > "$SHARED/view.txt"; printf '%s | view | %s\n' "$(date '+%H:%M:%S')" "$ARG" >> "$PET/log.txt"
        mkdir -p "$SHARED/keyboard"; : >> "$SHARED/interact_relay.txt"; : >> "$SHARED/keyboard/history.txt"      # the village is played with the keys: Interact mode on there, off everywhere else
        if [ "$ARG" = world ]; then echo 1 > "$SHARED/interact_armed.txt"; else echo 0 > "$SHARED/interact_armed.txt"; fi; status >/dev/null ;;
    select) need_pet; grep -q "^$ARG " "$SHARED/party.txt" 2>/dev/null || exit 0; echo "$ARG" > "$SHARED/active.txt"; echo room > "$SHARED/view.txt"; echo 0 > "$SHARED/party_open.txt"; echo 0 > "$SHARED/interact_armed.txt"
        PET_DIR="" PET_SHARED="$SHARED" sh "$0" status >/dev/null ;;
    world_move) need_pet; [ -x "$WORLD" ] || exit 0; [ -f "$SHARED/world.st" ] || "$WORLD" init "$SHARED" "$SHARED/party.txt" >/dev/null 2>&1
        out=$("$WORLD" walk "$SHARED" "$ARG" "$ACTIVE"); printf '%s | world | %s %s\n' "$(date '+%H:%M:%S')" "$ARG" "$out" >> "$PET/log.txt"
        case "$out" in
            "door home") echo room > "$SHARED/view.txt"; echo 0 > "$SHARED/interact_armed.txt"; echo "world: the door - back in the house" >> "$PET/chat.txt";;
            "door locked") echo "world: that house is locked" >> "$PET/chat.txt";;
            talk*) sh "$0" world_talk "${out#talk }";;
        esac; status >/dev/null ;;
    world_talk) need_pet; who="$ARG"; [ -z "$who" ] || [ "$who" = - ] && who=$("$WORLD" adjacent "$SHARED" "$ACTIVE" | head -1)
        [ -z "$who" ] && { echo "world: nobody is near" >> "$PET/chat.txt"; status >/dev/null; exit 0; }
        nm=$(awk -v i="$who" '$1==i{print $2}' "$SHARED/party.txt"); sp=$(sed -n 's/^species=//p' "$SHARED/pets/$who/variables.txt" 2>/dev/null | head -1)
        case "${sp:-0}" in 0) sn=bunny;; 1) sn=bear;; 2) sn=cat;; 3) sn=frog;; 4) sn=mouse;; *) sn=dragon;; esac
        echo "world: $nm the $sn says hi to ${nm:+your pet}" >> "$PET/chat.txt"; addv talk_total 1; addv happy 3; sh "$0" exchange "$who" >/dev/null 2>&1; status >/dev/null ;;
    teleport) need_pet; dest="$ARG"; ax="${3:-0}"      # the door event: move the pet to another room (or the village = the world page)
        case "$dest" in
            village) echo world > "$SHARED/view.txt"; echo 1 > "$SHARED/interact_armed.txt"; mkdir -p "$SHARED/keyboard"; : >> "$SHARED/interact_relay.txt"; : >> "$SHARED/keyboard/history.txt";;
            *) grep -q "^ROOM *| *$dest " "$(rooms_file)" || exit 0; printf '%s\n' "$dest" > "$PET/loc.txt"; printf '%s\n' "$ax" > "$PET/arrive_x.txt"; printf '%s\n' "$(date +%s%N)" > "$PET/arrive_seq.txt";;
        esac
        printf '%s | teleport | %s\n' "$(date '+%H:%M:%S')" "$dest" >> "$PET/log.txt"; status >/dev/null ;;
    speak) need_pet   # the pet talks on its own: a word it has learned (picked by weight, so praise makes a word more likely) or a line about its needs; the master answers good / bad and that moves the word's weight
        nm=$(getv name_id); sp=$(getv species); sp=${sp:-0}; h=$(getv hunger); e=$(getv energy); c=$(getv clean); p=$(getv happy)
        need=chatter; [ "${p:-50}" -ge 80 ] && need=happy; [ "${c:-100}" -le 25 ] && need=dirty; [ "${e:-100}" -le 25 ] && need=sleepy; [ "${h:-0}" -ge 60 ] && need=hungry
        seed=$(date +%s%N | cut -c8-18); line=""
        if [ $((seed % 100)) -lt 55 ]; then pick=$(awk -F'|' -v sd="$seed" 'BEGIN{srand(sd)} /^LEX/{ph=$2; vb=$3; w=$5; gsub(/^ +| +$/,"",ph); gsub(/^ +| +$/,"",vb); gsub(/ /,"",w); if (w+0>0) {tot+=w; n++; P[n]=ph; V[n]=vb; W[n]=w}} END{if (!n) exit; r=rand()*tot; for(i=1;i<=n;i++){r-=W[i]; if (r<=0) {print P[i] "|" V[i]; exit}} print P[n] "|" V[n]}' "$LEXF")
            if [ -n "$pick" ]; then line=${pick%%|*}; printf '%s\n' "$pick" > "$PET/last_word.txt"; fi; fi
        if [ -z "$line" ]; then line=$(awk -F'|' -v nd="$need" -v sd="$seed" 'BEGIN{srand(sd)} /^SAY/{a=$2; gsub(/ /,"",a); if (a==nd) {t=$3; gsub(/^ +| +$/,"",t); L[++n]=t}} END{if (n) print L[int(rand()*n)+1]}' "$HERE/replies.pdl"); rm -f "$PET/last_word.txt"; fi
        [ -z "$line" ] && exit 0
        printf '%s: %s\n' "$nm" "$line" >> "$CHAT"; addv say_total 1; expr happy 3
        ( setsid sh "$HERE/ops/pet_voice.sh" "$sp" "$line" >/dev/null 2>&1 & ); status >/dev/null ;;
    hum) need_pet; sp=$(getv species); ( setsid sh "$HERE/ops/pet_hum.sh" "${sp:-0}" >/dev/null 2>&1 & ) ;;
    react) need_pet; now=$(date +%s); last=$(getv react_t); [ -n "$last" ] && [ $((now - last)) -lt 5 ] && exit 0; setv react_t "$now"   # shaken / dropped: fall = scared, land = ouch (the window was moved)
        nm=$(getv name_id); sp=$(getv species); case "$ARG" in fall) line="whoa!!"; expr surprised 4;; land) line="oof!"; expr sad 3;; *) exit 0;; esac
        printf '%s: %s\n' "$nm" "$line" >> "$CHAT"; ( setsid sh "$HERE/ops/pet_voice.sh" "${sp:-0}" "$line" >/dev/null 2>&1 & ); status >/dev/null ;;
    chat) do_chat "$ARG"; status >/dev/null ;;
    phones) ph_ensure; status >/dev/null ;;
    exchange) need_pet; other="$ARG"; { [ -n "$other" ] && [ -d "$SHARED/pets/$other" ] && [ "$other" != "$ACTIVE" ]; } || exit 0      # two pets swap numbers: contacts on both sides, a hello text, a graded extracurricular
        ph_ensure; contact_add "$ACTIVE" "$other"; contact_add "$other" "$ACTIVE"; ph_say "$ACTIVE" "$other" "hi! i am $(getv name_id) - this is my number"
        printf '%s: saved %s number!\n' "$(getv name_id)" "$(ph_name "$other")" >> "$CHAT"; feedback +1 contacts; addv happy 3; status >/dev/null ;;
    call) need_pet; other="$ARG"; msg="${3:-hello}"; msg=$(printf '%s' "$msg" | tr -d '|'); ph_ensure; num=$(ph_number "$other")      # a call = a text to a saved contact
        if [ -n "$num" ] && grep -q "| $num |" "$PET/contacts.pdl" 2>/dev/null; then ph_say "$ACTIVE" "$other" "$msg"; printf '%s: texted %s\n' "$(getv name_id)" "$(ph_name "$other")" >> "$CHAT"; else printf '%s: I do not have that number\n' "$(getv name_id)" >> "$CHAT"; fi; status >/dev/null ;;
    land_price) need_pet; kd="${ARG:-house}"; row=$(grep "^KIND *| *$kd " "$HERE/buildings.pdl" | head -1); [ -n "$row" ] || { echo "unknown kind"; exit 0; }
        w=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$3); print $3}'); h=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$4); print $4}'); wd=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$5); print $5}'); cn=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$6); print $6}'); fee=$(land_fee "$w" "$h")
        echo "kind=$kd fee=$fee wood=$wd coins=$cn total_coins=$((cn + fee))" ;;
    build_building) need_pet; kd="${ARG:-house}"; row=$(grep "^KIND *| *$kd " "$HERE/buildings.pdl" | head -1); nm=$(getv name_id)
        [ -n "$row" ] || { printf '%s: I do not know how to build a %s\n' "$nm" "$kd" >> "$CHAT"; exit 0; }
        w=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$3); print $3}'); h=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$4); print $4}'); wd=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$5); print $5}'); cn=$(echo "$row" | awk -F'|' '{gsub(/ /,"",$6); print $6}'); rows=$(echo "$row" | awk -F'|' '{gsub(/^ +| +$/,"",$7); print $7}')
        mkdir -p "$TOWN"; fee=$(land_fee "$w" "$h"); need=$((cn + fee)); have=$(inv_count coin); hw=$(inv_count wood)
        if [ "$hw" -lt "$wd" ]; then printf '%s: I need %s wood for a %s\n' "$nm" "$wd" "$kd" >> "$CHAT"; status >/dev/null; exit 0; fi
        if [ "$have" -lt "$need" ]; then printf '%s: a %s costs %s coins with the land\n' "$nm" "$kd" "$need" >> "$CHAT"; status >/dev/null; exit 0; fi
        if [ -n "${3:-}" ] && [ -n "${4:-}" ]; then sx="$3"; sy="$4"; [ -f "$SHARED/town_all.txt" ] || town_merge
            okp=$(awk -v x="$sx" -v y="$sy" -v w="$w" -v h="$h" '{ line[NR]=$0 } END { ok=1; for (r=0;r<h;r++) for (c=0;c<w;c++) { ch=substr(line[y+r+1],x+c+1,1); if (ch!="." && ch!=",") ok=0 } print ok }' "$SHARED/town_all.txt"); [ "$okp" = 1 ] || { printf '%s: that spot is not free\n' "$nm" >> "$CHAT"; status >/dev/null; exit 0; }
        else site=$(town_site "$w" "$h"); [ -n "$site" ] || { printf '%s: there is no free land left\n' "$nm" >> "$CHAT"; status >/dev/null; exit 0; }; sx=${site% *}; sy=${site#* }; fi
        spend coin "$need"; spend wood "$wd"; bid="b$(( $(grep -c '^BUILDING' "$TOWN/town_built.pdl" 2>/dev/null || echo 0) + 1 ))"
        printf 'BUILDING | %s | %s | %s | %s | %s | %s | %s | %s | %s\n' "$bid" "$kd" "$sx" "$sy" "$w" "$h" "$ACTIVE" "$need" "$rows" >> "$TOWN/town_built.pdl"; town_merge
        printf 'BUILD|%s|%s|%s|%s|%s|coins=%s|wood=%s|fee=%s\n' "$(date +%s)" "$ACTIVE" "$kd" "$sx" "$sy" "$need" "$wd" "$fee" >> "$TOWN/ledger.txt"
        printf '%s: I built a %s!\n' "$nm" "$kd" >> "$CHAT"; feedback +1 build; addv happy 4; status >/dev/null ;;
    help) need_pet; tp=$(printf '%s' "${ARG:-start}" | tr 'A-Z' 'a-z' | tr -c 'a-z' ' ' | awk '{print $1}'); [ -n "$tp" ] || tp=start
        n=$(grep -c "^HELP *| *$tp " "$HERE/help.pdl"); [ "$n" -gt 0 ] || { tp=start; }
        printf 'YOU: help %s\n' "$tp" >> "$CHAT"; awk -F'|' -v t="$tp" '/^HELP/{a=$2; gsub(/ /,"",a); if (a==t) { l=$3; sub(/^ /,"",l); print "? " l }}' "$HERE/help.pdl" >> "$CHAT"; status >/dev/null ;;
    tip) need_pet; n=$(grep -c '^TIP' "$HERE/help.pdl"); [ "$n" -gt 0 ] || exit 0; k=$(( $(date +%s) / 60 % n + 1 )); awk -F'|' -v k="$k" '/^TIP/{i++; if (i==k) { l=$2; sub(/^ /,"",l); print "? " l }}' "$HERE/help.pdl" >> "$CHAT"; status >/dev/null ;;
    ai_step) need_pet; [ "$ACTIVE" != "$(cat "$SHARED/active.txt" 2>/dev/null)" ] || [ "${PET_AI_ANY:-0}" = 1 ] || exit 0      # the pet on screen has its own autonomy
        mkdir -p "$TOWN"; now="${PET_AI_NOW:-$(date +%s)}"; last=$(cat "$PET/ai_last" 2>/dev/null); [ -n "$last" ] || { last=$now; echo "$now" > "$PET/ai_last"; }
        ne=$(aiv needs_every); ne=${ne:-60}; nt=$(( (now - last) / ne )); [ "$nt" -gt 5 ] && nt=5
        if [ "$nt" -gt 0 ]; then echo $((last + nt * ne)) > "$PET/ai_last"; k=0; while [ "$k" -lt "$nt" ]; do addv hunger "$(getw tick_hunger)"; addv energy -"$(getw tick_energy)"; addv clean -"$(getw tick_clean)"; k=$((k + 1)); done; ai_clamp; fi
        hu=$(getv hunger); en=$(getv energy); hthr=$(aiv hunger_thr); ethr=$(aiv energy_thr); ww=$(aiv want_wood)
        ai_rent
        if [ "${hu:-0}" -ge "${hthr:-60}" ]; then
            for f in apple fish cake; do if [ "$(inv_count $f)" -gt 0 ]; then sh "$0" give "$f" >/dev/null; ai_log "$ACTIVE" "ate $f from the bag"; rm -f "$PET/ai_goal.txt"; exit 0; fi; done
            if ai_shop apple; then sh "$0" give apple >/dev/null; ai_log "$ACTIVE" "ate the apple it bought"; exit 0; fi
            [ -f "$PET/ai_goal.txt" ] && grep -q "^shop " "$PET/ai_goal.txt" && [ "$(inv_count coin)" -ge "$(pricev apple)" ] && exit 0      # on its way to a store
            if ai_gather apple; then sh "$0" give apple >/dev/null; ai_log "$ACTIVE" "ate the apple it picked"; fi; exit 0
        fi
        if [ "${en:-100}" -le "${ethr:-25}" ]; then addv energy "$(aiv rest_gain)"; ai_clamp; ai_log "$ACTIVE" "rested"; rm -f "$PET/ai_goal.txt"; exit 0; fi
        if ai_build; then exit 0; fi
        wc=$(econv want_coins); wx=$(econv work_extra); want=${ww:-8}; [ "$(inv_count coin)" -lt "${wc:-30}" ] && want=$(( want + ${wx:-4} ))
        if [ "$(inv_count wood)" -lt "$want" ]; then ai_gather wood; exit 0; fi
        if ai_sell_wood; then exit 0; fi
        rm -f "$PET/ai_goal.txt"; exit 0 ;;
    buy) need_pet; sid="$ARG"; it="${3:-apple}"; n="${4:-1}"; nm=$(getv name_id); [ -d "$SHARED/pets/$sid" ] || { printf '%s: buy from whom?\n' "$nm" >> "$CHAT"; exit 0; }
        d=$(deal "$PET" "$SHARED/pets/$sid" "$it" "$n"); case "$d" in ok) printf '%s: bought %s %s\n' "$nm" "$n" "$it" >> "$CHAT";; buyer-poor) printf '%s: not enough coins\n' "$nm" >> "$CHAT";; seller-short) printf '%s: they do not have enough %s\n' "$nm" "$it" >> "$CHAT";; *) printf '%s: no price for %s\n' "$nm" "$it" >> "$CHAT";; esac; status >/dev/null ;;
    sell) need_pet; bid="$ARG"; it="${3:-wood}"; n="${4:-1}"; nm=$(getv name_id); [ -d "$SHARED/pets/$bid" ] || { printf '%s: sell to whom?\n' "$nm" >> "$CHAT"; exit 0; }
        d=$(deal "$SHARED/pets/$bid" "$PET" "$it" "$n"); case "$d" in ok) printf '%s: sold %s %s\n' "$nm" "$n" "$it" >> "$CHAT";; buyer-poor) printf '%s: they cannot pay\n' "$nm" >> "$CHAT";; seller-short) printf '%s: I do not have enough %s\n' "$nm" "$it" >> "$CHAT";; *) printf '%s: no price for %s\n' "$nm" "$it" >> "$CHAT";; esac; status >/dev/null ;;
    build_room) need_pet; side="${ARG:-right}"; kind="${3:-room}"; RF=$(rooms_file); HF="$HERE/home.pdl"; [ -f "$SHARED/home_all.pdl" ] && HF="$SHARED/home_all.pdl"
        cur=$(cat "$PET/loc.txt" 2>/dev/null || echo bedroom); line=$(grep "^CELL *| *$cur " "$HF" | head -1); cx=$(echo "$line" | awk -F'|' '{gsub(/ /,"",$3); print $3}'); cy=$(echo "$line" | awk -F'|' '{gsub(/ /,"",$4); print $4}')
        [ -n "$cx" ] || { printf '%s: I cannot build here\n' "$(getv name_id)" >> "$CHAT"; exit 0; }
        if [ "$side" = left ]; then nx=$((cx - 1)); slot=30; arr_new=320; slot_new=334; else nx=$((cx + 1)); slot=334; arr_new=50; slot_new=30; fi
        if grep -q "^CELL *| *[a-z0-9_]* *| *$nx *| *$cy " "$HF"; then printf '%s: there is a room there already\n' "$(getv name_id)" >> "$CHAT"; exit 0; fi
        if awk -F'|' -v r="$cur" -v s="$slot" '/^DOOR/{a=$2; x=$3; gsub(/ /,"",a); gsub(/ /,"",x); if (a==r && x+0>s-45 && x+0<s+45) f=1} END{exit !f}' "$RF"; then printf '%s: no free door spot on that side\n' "$(getv name_id)" >> "$CHAT"; exit 0; fi
        n=$(grep -c "^CELL" "$HF"); [ "$n" -ge 12 ] && { printf '%s: the house is full\n' "$(getv name_id)" >> "$CHAT"; exit 0; }
        id="r$((n + 1))"; case "$kind" in garden) sc=2;; *) sc=3;; esac; k=$((n + 1))
        { printf 'ROOM | %s | %s | %s %s\n' "$id" "$sc" "$kind" "$k"; printf 'DOOR | %s | %s | %s | %s | auto=1\n' "$cur" "$slot" "$id" "$arr_new"; printf 'DOOR | %s | %s | %s | %s | auto=1\n' "$id" "$slot_new" "$cur" "$((slot > 100 ? 300 : 60))"; } >> "$SHARED/built_rooms.pdl"
        printf 'CELL | %s | %s | %s | %s\n' "$id" "$nx" "$cy" "$kind" >> "$SHARED/built_home.pdl"; merge_rooms
        printf '%s | build_room | %s %s %s\n' "$(date '+%H:%M:%S')" "$id" "$side" "$kind" >> "$PET/log.txt"; printf '%s: I built a %s to the %s!\n' "$(getv name_id)" "$kind" "$side" >> "$CHAT"; status >/dev/null ;;
    map) cf="$SHARED/camera.st"; if grep -q '^pov=5' "$cf" 2>/dev/null; then sed -i 's/^pov=5/pov=1/' "$cf"; else if [ -f "$cf" ]; then sed -i 's/^pov=.*/pov=5/' "$cf"; else printf 'mode=2d\npov=5\nyaw=0\npitch=0\nheight=0\n' > "$cf"; fi; fi ;;
    climb) need_pet; printf '%s\n' "${ARG:-bed}" > "$PET/want_platform.txt"; printf '%s | climb | %s\n' "$(date '+%H:%M:%S')" "${ARG:-bed}" >> "$PET/log.txt"; status >/dev/null ;;      # the manager reads want_platform.txt (rooms.pdl PLATFORM names)
    go) sh "$0" teleport "${ARG:-living}" 150 ;;
    listen) ( setsid sh "$HERE/ops/pet_listen.sh" >/dev/null 2>&1 & ); sleep 0.3; status >/dev/null ;;      # the ♨ mic: record, offline STT, chat
    chat_send) TA="$HERE/text_area_pet-say.txt"; [ -f "$TA" ] || TA="$HERE/text_area_say.txt"; msg=$(tr '\n' ' ' < "$TA" 2>/dev/null | sed 's/  */ /g;s/^ //;s/ $//'); : > "$HERE/text_area_pet-say.txt"; : > "$HERE/text_area_say.txt"; [ -n "$msg" ] && do_chat "$msg"; status >/dev/null ;;      # the multi-line text_area is saved by the renderer to text_area_<id>.txt in the package dir; Send reads it, clears it (the renderer reloads the empty file) and chats
    chat_input) do_chat "$4"; status >/dev/null ;;      # a layout cli_io appends: <package_dir> <house_root> <typed text>
    teach) do_teach "$ARG" "$3" "$4"; status >/dev/null ;;
    praise) do_judge 2; status >/dev/null ;;
    scold) do_judge -2; status >/dev/null ;;
    touch) do_touch "$ARG"; status >/dev/null ;;
    menu_toggle) need_pet; if [ "$(cat "$PET/menu_open.txt" 2>/dev/null)" = 1 ]; then echo 0 > "$PET/menu_open.txt"; else echo 1 > "$PET/menu_open.txt"; fi; status >/dev/null ;;
    grant) need_pet; inv_add "$ARG" "${3:-1}"; printf '%s | gift | %s x%s\n' "$(date '+%H:%M:%S')" "$ARG" "${3:-1}" >> "$PET/log.txt"; status >/dev/null ;;     # the master gives the pet an item
    menu_group) need_pet; printf '%s\n' "$ARG" > "$PET/menu_group.txt"; status >/dev/null ;;
    chat_toggle) need_pet; if [ "$(cat "$SHARED/chat_hidden.txt" 2>/dev/null)" = 1 ]; then echo 0 > "$SHARED/chat_hidden.txt"; else echo 1 > "$SHARED/chat_hidden.txt"; fi; status >/dev/null ;;
    hotbar_toggle) need_pet; if [ "$(cat "$SHARED/hotbar_hidden.txt" 2>/dev/null)" = 1 ]; then echo 0 > "$SHARED/hotbar_hidden.txt"; else echo 1 > "$SHARED/hotbar_hidden.txt"; fi; status >/dev/null ;;
    inv_toggle) need_pet; if [ "$(cat "$PET/inv_open.txt" 2>/dev/null)" = 1 ]; then echo 0 > "$PET/inv_open.txt"; else echo 1 > "$PET/inv_open.txt"; fi; status >/dev/null ;;
    inv_use) need_pet; [ -x "$IOP" ] && "$IOP" slot "$PET" "${ARG:-0}" >/dev/null 2>&1; "$IOP" project "$PET" "$PET/inv_proj.txt" >/dev/null 2>&1; sh "$0" use_slot >/dev/null; status >/dev/null ;;
    inv_next) need_pet; [ -x "$IOP" ] && "$IOP" slot "$PET" next >/dev/null 2>&1; status >/dev/null ;;
    inv_prev) need_pet; [ -x "$IOP" ] && "$IOP" slot "$PET" prev >/dev/null 2>&1; status >/dev/null ;;
    use_slot) need_pet; sl=$(sed -n 's/^selected=//p' "$PET/inv_proj.txt" 2>/dev/null); it=$(sed -n "s/^slot_${sl:-0}=.*|\\(.*\\)_[0-9]*$/\\1/p" "$PET/inv_proj.txt" 2>/dev/null); [ -n "$it" ] && sh "$0" give "$it" >/dev/null; status >/dev/null ;;
    self_care) need_pet; self_care; status >/dev/null ;;
    tick)  need_pet; sh "$0" need_tick; sh "$0" day_tick; self_care; status >/dev/null ;;      # manual all-in-one (old behaviour); the clock fires the parts below on its schedule (time.pdl)
    clock_event) for d in "$SHARED"/pets/*/; do [ "$(cat "${d}running.txt" 2>/dev/null)" = 1 ] || continue; PET_DIR="${d%/}" sh "$0" "$ARG" >/dev/null 2>&1; done ;;      # the clock daemon (pet_clock_runner.sh) runs an event for every STARTED pet
    need_tick) need_pet; addv hunger "$(getw tick_hunger)"; addv energy -"$(getw tick_energy)"; addv clean -"$(getw tick_clean)"; printf '%s | clock | need_tick\n' "$(date '+%H:%M:%S')" >> "$PET/log.txt" ;;
    meal_call) need_pet; if [ "$(getv hunger)" -ge 40 ]; then printf '%s: it is time to eat!\n' "$(getv name_id)" >> "$CHAT"; ( setsid sh "$HERE/ops/pet_voice.sh" "$(getv species)" "it is time to eat" >/dev/null 2>&1 & ); self_care; fi; printf '%s | clock | meal_call\n' "$(date '+%H:%M:%S')" >> "$PET/log.txt" ;;
    bedtime) need_pet; printf '%s: so sleepy...\n' "$(getv name_id)" >> "$CHAT"; self_care; printf '%s | clock | bedtime\n' "$(date '+%H:%M:%S')" >> "$PET/log.txt" ;;
    wake) need_pet; addv energy 15; printf '%s: good morning!\n' "$(getv name_id)" >> "$CHAT"; printf '%s | clock | wake\n' "$(date '+%H:%M:%S')" >> "$PET/log.txt" ;;
    day_tick) need_pet; if [ -x "$GRADE" ]; then "$GRADE" rest "$PET" "$HERE/skillbook.pdl" >/dev/null 2>&1; "$GRADE" check "$PET" "$HERE/curriculum.pdl" >/dev/null 2>&1; "$GRADE" advance "$PET" "$HERE/curriculum.pdl" auto >/dev/null 2>&1; fi; evolve; printf '%s | clock | day_tick\n' "$(date '+%H:%M:%S')" >> "$PET/log.txt" ;;
    phase_dawn|phase_day|phase_dusk|phase_night) printf 'phase=%s\n' "${VERB#phase_}" > "$SHARED/daylight.txt" ;;      # clock events: the outdoor scenes read this file (ops/pet_scene.c load_phase)
    time_rate) sh "$HERE/ops/pet_clock.sh" rate "$ARG"; status >/dev/null ;;
    time_advance) sh "$HERE/ops/pet_clock.sh" advance "$ARG"; status >/dev/null ;;
    time_reinstall) sh "$HERE/ops/pet_clock.sh" reinstall; status >/dev/null ;;
    evolve) evolve ;;
    status|stats) status ;;
    *) exit 0 ;;
esac
exit 0
