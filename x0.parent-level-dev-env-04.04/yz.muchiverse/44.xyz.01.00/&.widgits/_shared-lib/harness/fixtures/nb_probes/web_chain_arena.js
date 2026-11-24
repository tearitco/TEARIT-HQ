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
 console.log('WEBCHAIN_TESTRESULT '+pass+'/'+total); console.log('WEBCHAIN_TESTDONE');
})();
