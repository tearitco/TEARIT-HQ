#!/bin/sh
# pet_event.sh <verb> [arg] - the pet house verbs. State lives in PET_DIR (default ../state next to this app); every number comes from the .pdl files here.
# Verbs: new_pet [seed] | feed [item] | give <item> | sleep | wash | play | tick | evolve | status | stats. Each care verb ends with: grade EXP via
# &.widgits/concept-bank/ops/entity_grade, an evolve check, and a refresh of ui.txt for the layout window. Exit 0 on bad input (an empty verb never harms).
HERE="$(cd "$(dirname "$0")/.." && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
PET="${PET_DIR:-$HERE/state}"
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
need_pet() { [ -f "$V" ] || sh "$0" new_pet 1 >/dev/null; }

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
    food=$(awk 'NR>0 && $2>0 && ($1=="apple"||$1=="fish"||$1=="cake"){print $1}' "$PET/pantry.txt" | while read it; do echo "$(getw pref_$it) $it"; done | sort -rn | head -1 | cut -d' ' -f2)
    sc=$(( h * $(getw w_feed) )); [ -n "$food" ] && [ "$sc" -gt "$bs" ] && { bs=$sc; best=feed; item=$food; }
    sc=$(( (100 - e) * $(getw w_sleep) )); [ "$sc" -gt "$bs" ] && { bs=$sc; best=sleep; item=""; }
    sc=$(( (100 - c) * $(getw w_wash) )); have=$(awk '$1=="soap"{print $2}' "$PET/pantry.txt"); [ "${have:-0}" -gt 0 ] && [ "$sc" -gt "$bs" ] && { bs=$sc; best=wash; item=soap; }
    sc=$(( (100 - p) * $(getw w_play) )); [ "$e" -gt 20 ] && [ "$sc" -gt "$bs" ] && { bs=$sc; best=play; item=""; }
    [ -z "$best" ] && return 0
    printf '%s | auto | %s %s (score %s)\n' "$(date '+%H:%M:%S')" "$best" "$item" "$bs" >> "$PET/log.txt"
    if [ "$best" = wash ]; then sh "$0" give soap >/dev/null; elif [ "$best" = feed ]; then sh "$0" give "$item" >/dev/null; else sh "$0" "$best" >/dev/null; fi
    # learn the priority from how it went: the last feedback row's valence moves that action's weight by +-1 (joint_tune, bounded, ledgered)
    v=$(tail -1 "$PET/obs_feedback_log.txt" | sed -n 's/.*valence=\([-+0-9]*\).*/\1/p'); case "$best" in feed) wk=w_feed;; sleep) wk=w_sleep;; wash) wk=w_wash;; play) wk=w_play;; esac
    [ -x "$JT" ] && "$JT" apply "$HERE/joints.pdl" "$W" "$PET/tuning_ledger.txt" "$wk" "${v:-1}" pet "self care $best" >/dev/null 2>&1
    return 0
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
    printf 'YOU: %s\n' "$text" >> "$CHAT"
    hit=$(lex_best "$text")
    if [ -z "$hit" ]; then printf '%s\n' "$text" > "$PET/last_unknown.txt"; reply unknown; expr surprised 5; return 0; fi
    vb=${hit%%|*}; rest=${hit#*|}; it=${rest%%|*}; rest=${rest#*|}; ph=${rest%%|*}
    printf '%s|%s\n' "$ph" "$vb" > "$PET/last_word.txt"
    case "$vb" in
        hello) reply hello; expr wave 6 ;;
        feed) sh "$0" give "${it:-apple}" >/dev/null; reply feed ;;
        sleep|wash|play) sh "$0" "$vb" >/dev/null; reply "$vb" ;;
        touch) sh "$0" touch >/dev/null ;;
    esac
}
do_teach() { # teach "<phrase>" <verb> [item]: the master teaches a word (starts weak, weight 3); teaching it again strengthens it
    need_pet; ph=$(printf '%s' "$1" | tr 'A-Z' 'a-z' | tr -c 'a-z0-9 \n' ' ' | tr -s ' ' | sed 's/^ //;s/ $//'); vb="$2"; it="${3:-}"
    case "$vb" in hello|feed|sleep|wash|play|touch) ;; *) return 0;; esac
    [ -z "$ph" ] && return 0
    lex_set "$ph" "$vb" "$it" 1 3; printf 'YOU: (teaches "%s" = %s)\n' "$ph" "$vb" >> "$CHAT"; say "oh! $ph"; expr happy 5
}
do_judge() { # praise / scold: moves the weight of the last word the pet acted on, +-2, and its mood
    need_pet; [ -f "$PET/last_word.txt" ] || return 0; d="$1"; ph=$(cut -d'|' -f1 "$PET/last_word.txt"); vb=$(cut -d'|' -f2 "$PET/last_word.txt")
    lex_set "$ph" "$vb" "" "$d" 3; feedback "$( [ "$d" -gt 0 ] && echo +1 || echo -1 )" "chat"
    if [ "$d" -gt 0 ]; then addv happy 5; expr happy 6; say "^_^"; else addv happy -4; expr sad 6; say "T_T"; fi
}
do_touch() { # touched: head = pleased, belly = giggle; many touches in a row annoy it (valence -1)
    need_pet; part="${1:-head}"; now=$(date +%s); last=$(getv touch_t); n=$(getv touch_n); n=${n:-0}
    if [ -n "$last" ] && [ $((now - last)) -le 10 ]; then n=$((n + 1)); else n=1; fi
    setv touch_t "$now"; setv touch_n "$n"
    if [ "$n" -gt 5 ]; then addv happy -3; feedback -1 touch; expr sad 5; say "stop it!"; else addv happy 3; [ "$part" = belly ] && addv happy 1; feedback +1 touch; expr happy 5; reply touch; fi
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

status() {
    need_pet
    h=$(getv hunger); e=$(getv energy); c=$(getv clean); p=$(getv happy)
    state=Fine; [ "${h:-0}" -ge 60 ] && state=Hungry; [ "${e:-100}" -le 25 ] && state=Sleepy; [ "${c:-100}" -le 25 ] && state=Dirty
    anim=idle; [ "$state" = Hungry ] && anim=hungry; [ "$state" = Sleepy ] && anim=sleepy; [ "$state" = Dirty ] && anim=sad
    [ "${p:-50}" -ge 85 ] && [ "$state" = Fine ] && anim=happy
    if [ -f "$PET/expression.txt" ]; then set -- $(cat "$PET/expression.txt"); [ "$(date +%s)" -le "${2:-0}" ] && anim=$1; fi
    frame=$(( $(date +%s) % 8 ))
    {
        [ -z "$(getv name_id)" ] && setv name_id Pochi
        printf 'title=Pet house\nname=%s\nstage=%s\nlevel=%s\nexp=%s\nmp=%s\nhunger=%s\nenergy=%s\nclean=%s\nhappy=%s\nstate=%s\n' \
            "$(getv name_id)" "$(cat "$PET/stage.txt" 2>/dev/null)" "$(getv rpg_level)" "$(getv rpg_exp)" "$(getv rpg_mp)" "${h:-0}" "${e:-100}" "${c:-100}" "${p:-50}" "$state"
        printf 'pet_sprite=%s/art/sprites_csv/%s_%02d\n' "$PET" "$anim" "$frame"
        printf 'grade=%s\n' "$(sed -n 's/.*max_tier: *//p' "$PET/learning_limits.pdl" 2>/dev/null | head -1)"
        printf 'likes=%s\n' "$(sed -n 's/^pref_\([a-z]*\)=\(.*\)/\1:\2/p' "$W" | tr '\n' ' ')"
        printf 'anim=%s\n' "$anim"
        n=0; tail -4 "$CHAT" 2>/dev/null | while IFS= read -r line; do printf 'chat_%s=%s\n' "$n" "$line"; n=$((n+1)); done
        printf 'known_words=%s\n' "$(awk -F'|' '/^LEX/{p=$2; gsub(/^ +| +$/,"",p); printf "%s ", p}' "$LEXF" 2>/dev/null)"
        printf 'pantry=%s\n' "$(tr '\n' ' ' < "$PET/pantry.txt" 2>/dev/null)"
    } > "$PET/ui.txt"
    cat "$PET/ui.txt"
}

case "$VERB" in
    new_pet)
        mkdir -p "$PET"; seed="${ARG:-1}"
        printf 'hunger=30\nenergy=80\nclean=70\nhappy=50\nplay_total=0\nfed_total=0\nseed=%s\nrpg_level=1\nrpg_exp=0\nrpg_mp=6\nrpg_mp_max=6\n' "$seed" > "$V"
        cp "$HERE/weights.default.pdl" "$PET/weights.pdl"; cp "$HERE/lexicon.default.pdl" "$PET/lexicon.pdl"; : > "$PET/chat.txt"; : > "$PET/chat_ledger.txt"; : > "$PET/obs_feedback_log.txt"; : > "$PET/tuning_ledger.txt"
        printf 'apple 3\nfish 1\nball 1\nsoap 2\n' > "$PET/pantry.txt"; : > "$PET/log.txt"; rm -f "$PET/evolve_sig.txt"
        evolve; status >/dev/null ;;
    feed|give)
        need_pet; item="${ARG:-apple}"
        kind=$(pdlval "$HERE/items.pdl" "$item" kind 2>/dev/null); kind=$(awk -F'|' -v n="$item" '/^ITEM/{g=$2; gsub(/^ +| +$/,"",g); if(g==n){k=$3; gsub(/^ +| +$/,"",k); print k}}' "$HERE/items.pdl")
        [ -z "$kind" ] && exit 0
        have=$(awk -v n="$item" '$1==n{print $2}' "$PET/pantry.txt" 2>/dev/null); have=${have:-0}
        [ "$have" -le 0 ] && { printf '%s | none left | %s\n' "$(date '+%H:%M:%S')" "$item" >> "$PET/log.txt"; status >/dev/null; exit 0; }
        awk -v n="$item" '$1==n{$2=$2-1} {print}' "$PET/pantry.txt" > "$PET/pantry.tmp" && mv -f "$PET/pantry.tmp" "$PET/pantry.txt"
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
    chat) do_chat "$ARG"; status >/dev/null ;;
    teach) do_teach "$ARG" "$3" "$4"; status >/dev/null ;;
    praise) do_judge 2; status >/dev/null ;;
    scold) do_judge -2; status >/dev/null ;;
    touch) do_touch "$ARG"; status >/dev/null ;;
    self_care) need_pet; self_care; status >/dev/null ;;
    tick)  need_pet; addv hunger "$(getw tick_hunger)"; addv energy -"$(getw tick_energy)"; addv clean -"$(getw tick_clean)"
           if [ -x "$GRADE" ]; then "$GRADE" rest "$PET" "$HERE/skillbook.pdl" >/dev/null 2>&1; "$GRADE" check "$PET" "$HERE/curriculum.pdl" >/dev/null 2>&1; "$GRADE" advance "$PET" "$HERE/curriculum.pdl" auto >/dev/null 2>&1; fi
           self_care
           evolve; status >/dev/null ;;
    evolve) evolve ;;
    status|stats) status ;;
    *) exit 0 ;;
esac
exit 0
