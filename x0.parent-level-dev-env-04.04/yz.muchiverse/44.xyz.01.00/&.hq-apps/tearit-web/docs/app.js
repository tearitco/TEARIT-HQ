/* tearit-web app.js: shared by index.html (landing), ranch.html (the game) and chain.html (the explorer). All rules live in chain.js; this file only keeps the chain text in the browser and draws. */
var TA = (function () {
  var KEY = 'tearit_web_chain_v1', CFG = { zeros: 2, initial: 50 };  /* web chain: block reward 50 coins (the C chain's 10500 would make every cost meaningless) */
  function $(i) { return document.getElementById(i); }
  function load() { return localStorage.getItem(KEY) || ''; }
  function save(t) { localStorage.setItem(KEY, t); }
  function ts() { return Math.floor(Date.now() / 1000); }
  function id(s) { return String(s || '').toLowerCase().replace(/[^a-z0-9_-]/g, '').slice(0, 24); }
  function owner() { return localStorage.getItem('tearit_owner') || ''; }
  function setOwner(o) { localStorage.setItem('tearit_owner', id(o)); }
  function curPet() { return localStorage.getItem('tearit_pet') || ''; }
  function setPet(p) { localStorage.setItem('tearit_pet', p); }
  function state() { return TearChain.verify(load(), CFG); }
  /* mine ONE block holding rows, paid to `miner`; the page refuses a block the verifier refuses and says why. Returns { ok, why, tries } */
  function act(miner, rows) {
    var t = load(), m = TearChain.mine(t, miner, rows, ts(), CFG, 3000000); if (!m.ok) return { ok: false, why: 'mining gave up' };
    var n = t + m.line + '\n', v = TearChain.verify(n, CFG); if (!v.ok) return { ok: false, why: v.error };
    save(n); return { ok: true, tries: m.tries };
  }
  function merge(other) { var r = TearChain.merge(load(), other, CFG); if (r.ok && r.winner === 'b') save(r.text); return r; }
  function header(on) {
    var h = $('hdr'); if (!h) return;
    function a(href, label, k) { return '<a href="' + href + '"' + (on === k ? ' class="on"' : '') + '>' + label + '</a>'; }
    h.innerHTML = '<img src="img/logo.png" alt="TEAR-IT"><b>TEAR-IT RANCH</b><nav>' + a('index.html', 'Home', 'home') + a('ranch.html', 'My ranch', 'ranch') + a('market.html', 'Market', 'market') + a('arena.html', 'Arena', 'arena') + a('chain.html', 'The chain', 'chain') + '<a href="https://github.com/tearitco/tearit-web">Source</a></nav>';
  }
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
  function petImg(g) { return 'img/pets/' + g.speciesIx + '.png'; }
  function bar(v, max, cls) { return '<div class="bar ' + (cls || '') + '"><i style="width:' + Math.max(0, Math.min(100, Math.round(v * 100 / max))) + '%"></i></div>'; }
  function download(name, text) { var a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([text], { type: 'text/plain' })); a.download = name; document.body.appendChild(a); a.click(); a.remove(); }
  return { $: $, load: load, save: save, ts: ts, id: id, owner: owner, setOwner: setOwner, curPet: curPet, setPet: setPet, state: state, act: act, merge: merge, header: header, esc: esc, petImg: petImg, bar: bar, download: download, CFG: CFG };
})();
