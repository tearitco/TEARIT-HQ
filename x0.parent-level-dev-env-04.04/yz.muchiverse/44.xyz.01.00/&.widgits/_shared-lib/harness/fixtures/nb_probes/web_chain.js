/* web_chain probe (2026-11-24). C_CHAIN is REAL output of the house C ops (chain_new web --difficulty 2, chain_create_wallet alice/bob, chain_miner alice x2, chain_send alice bob 1500, chain_miner bob x1; C balances alice 19500 bob 12000). Regenerate by repeating those commands. */
var C_CHAIN='BLOCK|0|0000000000000000000000000000000000000000000000000000000000000000|117|003001479e1df42bf2fa3c9a69a65cb5ecdd9f85abbd2a8e08b5e1918a0345a6|1795510016|alice|\nBLOCK|1|003001479e1df42bf2fa3c9a69a65cb5ecdd9f85abbd2a8e08b5e1918a0345a6|113|001feb39d00f5c84717ba80054b3ea60f55c714c61bfc4e8fd9db3ed0a8f1018|1795510016|alice|\nBLOCK|2|001feb39d00f5c84717ba80054b3ea60f55c714c61bfc4e8fd9db3ed0a8f1018|324|0090fbaa3ae01fa2727c59819471600515919932aa25582af8548b0b09bdeeab|1795510016|bob|TX|alice|bob|1500|1795510016|alice-1795510016-289383\n';
(function(){ var pass=0,total=0,T=TearChain,Z=T.ZERO;
 function check(n,got,want){ total++; if(got===want){pass++;console.log('WEBCHAIN_OK '+n);} else console.log('WEBCHAIN_FAIL '+n+' got=['+got+'] want=['+want+']'); }
 var cfg={zeros:2};
 check('sha256-abc',T.sha256('abc'),'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
 check('sha256-empty',T.sha256(''),'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');
 check('sha256-two-blocks-448bits',T.sha256('abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq'),'248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1');
 check('c-block0-hash-recomputed',T.sha256('0|'+Z+'|117|'),'003001479e1df42bf2fa3c9a69a65cb5ecdd9f85abbd2a8e08b5e1918a0345a6');
 var v=T.verify(C_CHAIN,cfg);
 check('c-chain-verifies',String(v.ok)+' '+(v.error||''),'true ');
 check('c-chain-height',String(v.height),'3');
 check('c-balance-alice-matches-chain_balance',String(v.balances.alice),'19500');
 check('c-balance-bob-matches-chain_balance',String(v.balances.bob),'12000');
 var ls=C_CHAIN.split('\n');
 var tam=ls[0]+'\n'+ls[1]+'\n'+ls[2].replace('|1500|','|9500|')+'\n';
 var vt=T.verify(tam,cfg); check('tampered-amount-rejected',vt.ok+' '+vt.error+' at '+vt.at,'false hash does not recompute at 2');
 var brk=ls[0]+'\n'+ls[2]+'\n'; var vb=T.verify(brk,cfg); check('broken-link-rejected',vb.ok+' '+vb.at,'false 1');
 var vd=T.verify(C_CHAIN,{zeros:5}); check('wrong-difficulty-rejected',vd.ok+' '+vd.error,'false difficulty not met');
 var m0=T.mine('', 'carol', [], 100, cfg); check('mine-genesis',String(m0.ok),'true');
 var ch=m0.line+'\n'; var m1=T.mine(ch,'carol',['MSG|carol|general|hello from the page|101'],101,cfg); ch+=m1.line+'\n';
 var vc=T.verify(ch,cfg); check('js-mined-verifies',vc.ok+' '+(vc.error||'')+' h'+vc.height+' carol'+vc.balances.carol,'true  h2 carol21000');
 check('message-visible',JSON.stringify(T.messages(ch,'general',cfg).map(function(x){return x.text;})),'["hello from the page"]');
 check('message-other-channel-empty',String(T.messages(ch,'forum',cfg).length),'0');
 check('cleanText-strips-separators',T.cleanText('a|b;c\nd'),'a b c d');
 var mo=T.mine(ch,'carol',['TX|carol|dave|99999999|102|x'],102,cfg); var vo=T.verify(ch+mo.line+'\n',cfg); check('overspend-rejected',vo.ok+' '+vo.error,'false insufficient balance for carol');
 var ok=T.mine(ch,'carol',['TX|carol|dave|500|102|x'],102,cfg); var vk=T.verify(ch+ok.line+'\n',cfg); check('spend-accepted',vk.ok+' dave'+vk.balances.dave+' carol'+vk.balances.carol,'true dave500 carol31000');
 var f1=T.mine(ch,'carol',['FAUCET|erin|1000|200'],200,cfg); var f1v=T.verify(ch+f1.line+'\n',cfg); check('faucet-claim-ok',f1v.ok+' erin'+f1v.balances.erin,'true erin1000');
 var f2=T.mine(ch,'carol',['FAUCET|erin|1001|200'],200,cfg); check('faucet-over-cap-rejected',T.verify(ch+f2.line+'\n',cfg).error,'faucet amount over the cap');
 var f3=T.mine(ch+f1.line+'\n','carol',['FAUCET|erin|1000|300'],300,cfg); check('faucet-cooldown-rejected',T.verify(ch+f1.line+'\n'+f3.line+'\n',cfg).error,'faucet cooldown');
 var f4=T.mine(ch+f1.line+'\n','carol',['FAUCET|erin|1000|5000'],5000,cfg); check('faucet-after-cooldown-ok',String(T.verify(ch+f1.line+'\n'+f4.line+'\n',cfg).ok),'true');
 var longer=ch+ok.line+'\n';
 var mg=T.merge(ch,longer,cfg); check('merge-more-work-wins',mg.winner+' '+mg.reason,'b more work');
 var mg2=T.merge(longer,ch,cfg); check('merge-more-work-wins-reversed',mg2.winner,'a');
 var mg3=T.merge(ch,ch,cfg); check('merge-tie-keeps-first',mg3.winner+' '+mg3.reason,'a tie keeps a');
 var forged=ls[0]+'\n'+ls[1]+'\n'+ls[2].replace('|1500|','|9500|')+'\n'+ls[2]+'\n'; var mg4=T.merge(C_CHAIN,forged,cfg); check('merge-forged-longer-loses',mg4.winner,'a');
 check('merge-both-bad-refused',String(T.merge(tam,brk,cfg).ok),'false');
 check('reward-halves',T.reward(0)+','+T.reward(999)+','+T.reward(1000)+','+T.reward(2000),'10500,10500,5250,2625');
 var ar=T.applyRows(ch,'steward',['TX|carol|dave|100|103|a','TX|carol|dave|99999999|103|b','rm -rf /','MSG|carol|forum|second|104'],103,cfg);
 check('steward-accepts-good-rows-in-one-block',ar.mined+' '+ar.accepted.length+' h'+T.verify(ar.text,cfg).height,'true 2 h3');
 check('steward-rejects-overspend',ar.rejected.map(function(x){return x.why;}).join(' / '),'insufficient balance for carol / not an allowed row');
 check('steward-nothing-good-mines-nothing',String(T.applyRows(ch,'steward',['nonsense'],103,cfg).mined),'false');
 var rc='',rcfg={zeros:2};
 function radd(miner,rows,ts){var m=T.mine(rc,miner,rows,ts,rcfg),n=rc+m.line+'\n',v=T.verify(n,rcfg);if(v.ok)rc=n;return v;}
 var gg=T.genes('blue dragon dreams','rex');
 check('ranch-genes-same-words-same-pet',gg.species+' '+JSON.stringify(gg.stats)+' '+gg.life,'Cosmic {"LIF":29,"POW":14,"INT":30,"SKI":19,"SPD":13,"DEF":20} 130');
 check('ranch-genes-other-name-other-pet',String(T.genes('blue dragon dreams','rexa').hash!==gg.hash),'true');
 var rv=radd('_ranch',['PET|rex|blue dragon dreams|alice|1'],1);
 check('ranch-pet-born-and-pool-funded',rv.ok+' '+rv.pets.rex.species+' pool'+rv.balances._ranch,'true Cosmic pool10500');
 check('ranch-duplicate-name-refused',radd('_ranch',['PET|rex|other words|bob|2'],2).error,'name already used');
 check('ranch-train-without-coins-refused',radd('_ranch',['TRAIN|rex|sprint|alice|3'],3).error,'not enough coins in rex');
 radd('_ranch',['FAUCET|rex|1000|5000'],5000);
 check('ranch-train-by-a-stranger-refused',radd('_ranch',['TRAIN|rex|sprint|bob|6'],6).error,'not the owner of rex');
 rv=radd('_ranch',['TRAIN|rex|sprint|alice|7'],7);
 check('ranch-train-pays-and-raises-the-stat',rv.ok+' SPD'+rv.pets.rex.stats.SPD+' coins'+rv.balances.rex+' tired'+rv.pets.rex.fat,'true SPD16 coins990 tired12');
 rv=radd('_ranch',['UPGRADE|rex|gym|alice|8'],8); check('ranch-gym-costs-100-and-adds-a-point',rv.ok+' lvl'+rv.pets.rex.fac.gym+' coins'+rv.balances.rex,'true lvl1 coins890');
 rv=radd('_ranch',['TRAIN|rex|lift|alice|9'],9); check('ranch-gym-level-raises-the-next-gain',rv.pets.rex.stats.POW+'','18');
 radd('_ranch',['FREEZE|rex|alice|10'],10); check('ranch-frozen-pet-cannot-act',radd('_ranch',['FEED|rex|bread|alice|11'],11).error,'rex is frozen');
 radd('_ranch',['THAW|rex|alice|12'],12);
 var before=T.verify(rc,rcfg).balances.rex; rv=radd('_ranch',['COMPETE|rex|E|alice|13'],13); var won=rv.pets.rex.wins;
 check('ranch-cup-result-is-paid-from-the-pool',rv.ok+' cups'+rv.pets.rex.cups+' bal'+rv.balances.rex,'true cups1 bal'+(before-10+(won?60:0)));
 for(var ti=0;ti<14;ti++){ var tr=radd('_ranch',['TRAIN|rex|jog|alice|'+(20+ti)],20+ti); if(!tr.ok)break; }
 check('ranch-a-tired-pet-cannot-enter-a-cup',radd('_ranch',['COMPETE|rex|E|alice|40'],40).error,'rex is too tired');
 radd('_ranch',['TRANSFER|rex|bob|alice|41'],41); check('ranch-transfer-moves-the-owner',radd('_ranch',['REST|rex|alice|42'],42).error,'not the owner of rex');
 check('ranch-new-owner-can-act',String(radd('_ranch',['REST|rex|bob|43'],43).ok),'true');
var hk=T.mine('','x',['HACK|x'],1,rcfg); check('ranch-unknown-row-refused',T.verify(hk.line+'\n',rcfg).error,'unknown row HACK');
 var arr=T.applyRows('','steward',['PET|zed|zed words|carol|1','PET|zed|again|carol|1','HACK|x'],5,rcfg); check('ranch-steward-takes-the-pet-row-only',arr.accepted.length+' '+arr.rejected.length,'1 2');
 /* ranch part 2: items, the ranked ladder gate, the pet market, pet-vs-pet */
 var r2='',m2=0; function r2add(miner,rows,ts){var m=T.mine(r2,miner,rows,ts,rcfg),n=r2+m.line+'\n',v=T.verify(n,rcfg);if(v.ok)r2=n;return v;}
 r2add('_ranch',['PET|ivy|ivy words|dan|1'],1); r2add('_ranch',['PET|kai|kai words|erin|2'],2); r2add('_ranch',['FAUCET|ivy|1000|9000'],9000); r2add('_ranch',['FAUCET|kai|1000|9000'],9000);
 var iv=r2add('_ranch',['BUYITEM|ivy|dumbbell|dan|10'],10); check('ranch-item-bought-with-the-pets-coins',iv.ok+' coins'+iv.balances.ivy+' have'+iv.pets.ivy.items.dumbbell,'true coins970 have1');
 check('ranch-using-an-item-you-lack-is-refused',r2add('_ranch',['USEITEM|ivy|charm|dan|11'],11).error,'ivy has no charm');
 r2add('_ranch',['USEITEM|ivy|dumbbell|dan|12'],12); iv=r2add('_ranch',['TRAIN|ivy|lift|dan|13'],13); var pw0=T.genes('ivy words','ivy').stats.POW; check('ranch-dumbbell-adds-two-to-the-next-drill-only',(iv.pets.ivy.stats.POW-pw0)+'','5');
 iv=r2add('_ranch',['TRAIN|ivy|lift|dan|14'],14); check('ranch-the-dumbbell-buff-was-used-up',(iv.pets.ivy.stats.POW-pw0)+'','8');
 r2add('_ranch',['BUYITEM|kai|elixir|erin|15'],15); iv=r2add('_ranch',['GIVEITEM|kai|elixir|ivy|erin|16'],16); check('ranch-an-item-can-be-given-to-another-pet',iv.ok+' kai'+(iv.pets.kai.items.elixir||0)+' ivy'+iv.pets.ivy.items.elixir,'true kai0 ivy1');
 check('ranch-ranked-gate-you-must-beat-the-rank-below',r2add('_ranch',['COMPETE|kai|D|erin|17'],17).error,'kai must beat rank E first');
 check('ranch-unknown-rank-refused',r2add('_ranch',['COMPETE|kai|Z|erin|18'],18).error,'unknown rank');
 r2add('_ranch',['LIST|ivy|100|ivy|dan|20'],20); check('ranch-buying-your-own-pet-is-refused',r2add('_ranch',['BUY|ivy|ivy|dan|21'],21).error,'you already own ivy');
 check('ranch-buying-with-a-pet-that-is-not-yours-is-refused',r2add('_ranch',['BUY|ivy|kai|dan|22'],22).error,'the paying pet is not yours');
 var b0=T.verify(r2,rcfg).balances; iv=r2add('_ranch',['BUY|ivy|kai|erin|23'],23); check('ranch-a-sale-moves-the-owner-and-the-coins',iv.ok+' owner'+iv.pets.ivy.owner+' kai'+iv.balances.kai+' ivy'+iv.balances.ivy,'true ownererin kai'+(b0.kai-100)+' ivy'+(b0.ivy+100));
 check('ranch-a-sold-pet-is-no-longer-listed',r2add('_ranch',['BUY|ivy|kai|dan|24'],24).error,'ivy is not for sale');
 iv=r2add('_ranch',['CHALLENGE|ivy|kai|erin|25'],25); check('ranch-a-challenge-is-recorded-for-both-pets',iv.ok+' '+(iv.pets.ivy.pvpW+iv.pets.ivy.pvpL+iv.pets.kai.pvpW+iv.pets.kai.pvpL),'true 2');
 check('ranch-a-pet-cannot-challenge-itself',r2add('_ranch',['CHALLENGE|ivy|ivy|erin|26'],26).error,'a pet cannot challenge itself');
 /* ranch part 3: the grid (positions are rows) and the limit of six pets out at once */
 var r3='';function r3add(miner,rows,ts){var m=T.mine(r3,miner,rows,ts,rcfg),n=r3+m.line+'\n',v=T.verify(n,rcfg);if(v.ok)r3=n;return v;}
 var six=true;for(var pi=1;pi<=6;pi++){six=six&&r3add('_ranch',['PET|p'+pi+'|grid words '+pi+'|zoe|'+pi],pi).ok;} var g3=T.verify(r3,rcfg);
 check('grid-six-pets-fit-on-the-ranch',six+' '+Object.keys(g3.pets).length,'true 6');
 check('grid-a-seventh-pet-is-refused-until-one-is-frozen',r3add('_ranch',['PET|p7|grid words 7|zoe|7'],7).error,'six pets are already out: freeze one first');
 var cells={},inside=true,k3;for(k3 in g3.pets){var q3=g3.pets[k3];cells[q3.x+','+q3.y]=1;inside=inside&&q3.x>=0&&q3.x<T.RANCH.cols&&q3.y>=0&&q3.y<T.RANCH.rows;}
 check('grid-every-pet-spawns-inside-the-grid',String(inside),'true'); check('grid-two-pets-never-share-a-cell',String(Object.keys(cells).length),'6');
 var fx=-1,fy=-1,tx,ty;for(ty=0;ty<T.RANCH.rows&&fx<0;ty++)for(tx=0;tx<T.RANCH.cols&&fx<0;tx++){if(!cells[tx+','+ty]&&!(tx===1&&ty===1)&&!(tx===10&&ty===1)){fx=tx;fy=ty;}}
 var mv=r3add('_ranch',['MOVE|p1|'+fx+'|'+fy+'|zoe|10'],10); check('grid-a-move-is-a-row-and-the-pet-stands-there',mv.ok+' '+mv.pets.p1.x+','+mv.pets.p1.y,'true '+fx+','+fy);
 check('grid-moving-onto-another-pet-is-refused',r3add('_ranch',['MOVE|p2|'+g3.pets.p3.x+'|'+g3.pets.p3.y+'|zoe|11'],11).error,'that cell is not free');
 check('grid-moving-onto-the-gym-is-refused',r3add('_ranch',['MOVE|p2|1|1|zoe|12'],12).error,'that cell is not free');
 check('grid-moving-off-the-grid-is-refused',r3add('_ranch',['MOVE|p2|12|0|zoe|13'],13).error,'that cell is not free');
 r3add('_ranch',['FREEZE|p1|zoe|14'],14); check('grid-freezing-one-makes-room-for-a-seventh',String(r3add('_ranch',['PET|p7|grid words 7|zoe|15'],15).ok),'true');
 check('grid-thawing-into-a-full-ranch-is-refused',r3add('_ranch',['THAW|p1|zoe|16'],16).error,'six pets are already out: freeze one first');
 check('grid-a-frozen-pet-no-longer-blocks-its-cell',String(r3add('_ranch',['MOVE|p2|'+fx+'|'+fy+'|zoe|17'],17).ok),'true');
 console.log('WEBCHAIN_TESTRESULT '+pass+'/'+total); console.log('WEBCHAIN_TESTDONE');
})();
