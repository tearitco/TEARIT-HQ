/* SABOTAGE probe (generated: this wrapper + web_chain_arena.js verbatim). verify() always says ok; the arena case must then see WEBCHAIN_FAIL. */
(function(){var o=TearChain.verify;TearChain.verify=function(t,c){var r=o(t,c);return {ok:true,height:r.height,work:r.work,balances:r.balances,messages:r.messages,pets:r.pets,arenas:r.arenas};};})();
/* web_chain_arena probe (2026-11-24): the arena rules of tearit-web chain.js (split from web_chain.js because the case console capture echoes the probe source and has a size cap) */
(function(){ var pass=0,total=0,T=TearChain,rcfg={zeros:2};
 function check(n,got,want){ total++; if(got===want){pass++;console.log('WEBCHAIN_OK '+n);} else console.log('WEBCHAIN_FAIL '+n+' got=['+got+'] want=['+want+']'); }
 /* ranch part 4: the arena (1v1, 2v2, free for all) with pets and default fighters */
 var r4='';function r4add(miner,rows,ts){var m=T.mine(r4,miner,rows,ts,rcfg),n=r4+m.line+'\n',v=T.verify(n,rcfg);if(v.ok)r4=n;return v;}
 function sumb(v){var k,t=0;for(k in v.balances)t+=v.balances[k];return t;}
 ['a1','a2','a3','a4'].forEach(function(n,i){r4add('_ranch',['PET|'+n+'|arena words '+i+'|zed|'+(i+1)],i+1);r4add('_ranch',['FAUCET|'+n+'|1000|9000'],9000);});
 r4add('_ranch',['ARENA|duel|1v1|zed|10'],10); check('arena-an-arena-name-cannot-be-reused',r4add('_ranch',['ARENA|duel|2v2|zed|11'],11).error,'arena name already used');
 check('arena-unknown-mode-refused',r4add('_ranch',['ARENA|x|3v3|zed|12'],12).error,'unknown mode');
 var jv=r4add('_ranch',['JOIN|duel|a1|zed|13'],13); check('arena-joining-pays-the-entry-into-the-pot',jv.ok+' coins'+jv.balances.a1+' pot'+jv.arenas.duel.pot,'true coins980 pot20');
 check('arena-a-pet-cannot-join-twice',r4add('_ranch',['JOIN|duel|a1|zed|14'],14).error,'a1 is already in an arena');
 check('arena-fight-needs-the-full-roster',r4add('_ranch',['FIGHT|duel|zed|15'],15).error,'the arena needs 2 fighters');
 check('arena-a-stranger-cannot-start-the-fight',r4add('_ranch',['FIGHT|duel|bob|16'],16).error,'you are not in that arena');
 r4add('_ranch',['LEAVE|duel|a1|zed|17'],17); var lv=T.verify(r4,rcfg); check('arena-leaving-refunds-the-entry',lv.balances.a1+' '+lv.arenas.duel.pot,'1000 0');
 r4add('_ranch',['JOIN|duel|a1|zed|18'],18); r4add('_ranch',['JOIN|duel|a2|zed|19'],19); var s0=sumb(T.verify(r4,rcfg)); var fv=r4add('_ranch',['FIGHT|duel|zed|20'],20);
 check('arena-a-1v1-fight-pays-the-pot-minus-the-house-rake-and-conserves-coins',fv.ok+' '+(sumb(fv)-s0)+' winners'+fv.arenas.duel.winners.length+' closed'+!fv.arenas.duel.open,'true 10540 winners1 closedtrue');
 check('arena-a-finished-arena-is-closed',r4add('_ranch',['JOIN|duel|a3|zed|21'],21).error,'that arena is closed');
 r4add('_ranch',['ARENA|team|2v2|zed|30'],30); r4add('_ranch',['JOIN|team|a3|zed|31'],31); var fl=r4add('_ranch',['FILL|team|zed|32'],32);
 check('arena-fill-adds-default-fighters-up-to-the-mode',fl.arenas.team.members.map(function(m){return m.name}).join(','),'a3,ai-1,ai-2,ai-3');
 var tf=r4add('_ranch',['FIGHT|team|zed|33'],33); check('arena-a-2v2-team-fight-resolves-with-two-winners',tf.ok+' '+tf.arenas.team.winners.length,'true 2');
 r4add('_ranch',['ARENA|brawl|ffa|zed|40'],40); r4add('_ranch',['JOIN|brawl|a4|zed|41'],41); r4add('_ranch',['JOIN|brawl|a1|zed|42'],42);
 check('arena-free-for-all-needs-three-fighters',r4add('_ranch',['FIGHT|brawl|zed|43'],43).error,'the arena needs at least 3 fighters');
 r4add('_ranch',['FILL|brawl|zed|44'],44); var ff=r4add('_ranch',['FIGHT|brawl|zed|45'],45); check('arena-free-for-all-pays-three-places',ff.ok+' '+ff.arenas.brawl.winners.length+' scores'+ff.arenas.brawl.scores.length,'true 3 scores3');
 check('arena-a-fight-tires-the-real-pets-and-ages-them',ff.pets.a4.fat>=20&&ff.pets.a4.age>=1?'yes':'no','yes');
 /* wallet options per pet (rows WOPT): cap, reserve, sale */
 var vb = T.verify(r4, rcfg), a4b = vb.balances.a4;
 r4add('_ranch',['WOPT|a1|cap|10|zed|60'],60); check('wopt-cap-refuses-a-bigger-spend',r4add('_ranch',['FEED|a1|meat|zed|61'],61).error,'cap: a1 may spend at most 10 coins at a time');
 check('wopt-a-spend-under-the-cap-still-works',String(r4add('_ranch',['FEED|a1|bread|zed|62'],62).ok),'true');
 r4add('_ranch',['WOPT|a4|reserve|'+(a4b-10)+'|zed|63'],63); check('wopt-reserve-refuses-a-spend-that-eats-it',r4add('_ranch',['FEED|a4|meat|zed|64'],64).error,'reserve: a4 must keep '+(a4b-10)+' coins');
 check('wopt-a-spend-that-leaves-the-reserve-works',String(r4add('_ranch',['FEED|a4|bread|zed|65'],65).ok),'true');
 r4add('_ranch',['LIST|a3|50|a3|zed|66'],66); var lv3=r4add('_ranch',['WOPT|a3|sale|0|zed|67'],67); check('wopt-sale-lock-clears-the-listing',String(lv3.ok)+' price'+lv3.pets.a3.price,'true price0');
 check('wopt-sale-lock-refuses-a-new-listing',r4add('_ranch',['LIST|a3|50|a3|zed|68'],68).error,'a3 is locked from sale');
 check('wopt-unknown-option-refused',r4add('_ranch',['WOPT|a2|speed|5|zed|69'],69).error,'unknown option');
 check('wopt-bad-value-refused',r4add('_ranch',['WOPT|a2|cap|-1|zed|70'],70).error,'bad option value');
 check('wopt-sale-must-be-0-or-1',r4add('_ranch',['WOPT|a2|sale|2|zed|71'],71).error,'bad option value');
 check('wopt-only-the-owner-sets-an-option',r4add('_ranch',['WOPT|a2|cap|5|bob|72'],72).error,'not the owner of a2');
 r4add('_ranch',['WOPT|a2|cap|10|zed|73'],73); r4add('_ranch',['ARENA|capcup|1v1|zed|74'],74); check('wopt-the-arena-entry-obeys-the-cap',r4add('_ranch',['JOIN|capcup|a2|zed|75'],75).error,'cap: a2 may spend at most 10 coins at a time');
 r4add('_ranch',['WOPT|a2|cap|0|zed|76'],76); check('wopt-cap-0-lifts-the-cap',String(r4add('_ranch',['JOIN|capcup|a2|zed|77'],77).ok),'true');
 console.log('WEBCHAIN_TESTRESULT '+pass+'/'+total); console.log('WEBCHAIN_TESTDONE');
})();
