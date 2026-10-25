cd '/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00'; export BOARD_CTL_SIZE=1296x789
C="$PWD/&.widgits/board-viewer/tools/+x/board_ctl.+x"; A="$PWD/@.apps/rpg-space"; B="$A/ops/+x/rpg_space.+x"; V="$A/rpg-space-pchq/pieces/ship_01/state.txt"
D=/tmp/claude-1000/final3; rm -rf $D; mkdir -p $D/snapshots; O=$D/snapshots; ok=1
ST="trainer.txt:x,trainer.txt:y,flag_helm.txt,helm_mode.txt,flag_ask.txt,direct.txt"
cam(){ "$C" camera "$A" iso-south >/dev/null; sleep 7; "$C" camera "$A" iso-south >/dev/null; sleep 5; }
SK=$(sed -n 's/^bv_session=//p' $A/board/state/ui.txt)
snap(){ for t in 1 2 3 4; do cam; if "$C" snap "$A" "$O/$1" --state $ST >/tmp/claude-1000/snap.out 2>&1; then tail -1 /tmp/claude-1000/snap.out | cut -c1-80; return; fi; echo 126 >> $SK/pieces/apps/player_app/interact_relay.txt; sleep 4; done; tail -1 /tmp/claude-1000/snap.out | cut -c1-80; echo "FAIL frame $1 never changed"; ok=0; }
shipxy(){ echo "$(sed -n 's/^pos_x=//p' $V) $(sed -n 's/^pos_y=//p' $V)"; }
[ "$(cat $A/state/flag_helm.txt)" = 1 ] && { "$B" helm off >/dev/null; sleep 40; }; echo 0 > $A/state/flag_ask.txt; echo "0 0" > $A/state/direct.txt
[ "$(cat $A/state/flag_int.txt)" = 0 ] && { "$B" int >/dev/null; sleep 15; }; "$C" preflight "$A" | tail -1
"$B" setpos bridge 7 5 >/dev/null; sleep 4; for t in 1 2 3; do "$C" key "$A" Up 3000 >/dev/null; sleep 2; [ "$(sed -n 's/^y=//p' $A/state/trainer.txt)" = 4 ] && break; "$B" setpos bridge 7 5 >/dev/null; sleep 4; done; "$C" expect "$A" trainer.txt:y 4 8 || ok=0
snap 01_parked_at_console.png
"$C" key "$A" Return 2000 >/dev/null; "$C" expect "$A" flag_ask.txt 1 10 || ok=0; sleep 4; snap 02_popup_three_choices.png
rm -f $A/state/helm_mode.txt; for t in 1 2 3; do "$C" click "$A" 648 447 >/dev/null; sleep 14; [ "$(cat $A/state/flag_helm.txt)" = 1 ] && break; done
"$C" expect "$A" flag_ask.txt 0 5 || ok=0; "$C" expect "$A" flag_helm.txt 1 10 || ok=0; "$C" expect "$A" helm_mode.txt direct 2 || ok=0; sleep 40; snap 03_direct_on_picture_unchanged.png
n=3; for i in 1 2 3 4 5 6; do
  "$C" key "$A" Right 1000 >/dev/null; sleep 3; "$C" expect "$A" direct.txt "$i 0" 6 || { "$C" key "$A" Right 1000 >/dev/null; sleep 3; "$C" expect "$A" direct.txt "$i 0" 6 || ok=0; }
  [ "$(shipxy)" = "$i 0" ] && echo "ok   ship_01 world pos = $i 0" || { echo "FAIL ship_01 pos '$(shipxy)'"; ok=0; }
  [ $((i%2)) = 0 ] && { n=$((n+1)); snap $(printf %02d $n)_ship_at_$i-0.png; }
done
for i in 1 2 3; do "$C" key "$A" Down 1000 >/dev/null; sleep 3; "$C" expect "$A" direct.txt "6 $i" 6 || ok=0; done; n=$((n+1)); snap $(printf %02d $n)_ship_at_6-3.png
for i in $(seq 10); do "$B" helm move 1 0 >/dev/null; done; sleep 2; "$B" helm move 1 0 | cut -c1-70; "$C" expect "$A" direct.txt "16 3" 4 || ok=0; n=$((n+1)); snap $(printf %02d $n)_edge_of_map_refused.png
"$C" key "$A" Escape 1000 >/dev/null; "$C" expect "$A" flag_helm.txt 0 20 || ok=0; sleep 40; [ "$(cat $A/state/flag_int.txt)" = 0 ] && { "$B" int >/dev/null; sleep 15; }; "$B" setpos bridge 7 4 >/dev/null; sleep 6; n=$((n+1)); snap $(printf %02d $n)_released_parked_again.png
echo "ALL CHECKS OK=$ok"
