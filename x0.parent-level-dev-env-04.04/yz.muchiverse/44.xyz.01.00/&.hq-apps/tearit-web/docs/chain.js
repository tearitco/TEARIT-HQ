/*nbjs-script-boundary*/
/* tearit-web chain.js: the browser side of the pal-chain AND the ranch rules (experimental web chain, owner 2026-11-24: "keep it experimental, signing can wait"; pets are wallets, Monster Rancher style).
 * ONE pure ES5 file (no DOM, no modules) so the same bytes run in the page, in a Web Worker and in the house's JS runner (harness case web_chain).
 * Block line (same as 041.pal-chain/chain_miner.c): BLOCK|index|prev_hash|nonce|hash|timestamp|miner|tx_list ; hash = SHA-256 of "index|prev|nonce|tx_list".
 * Rows inside tx_list: TX|from|to|amount|ts|id  (balance checked)  MSG|from|channel|text|ts  (social row)  FAUCET|wallet|amount|ts  (web-chain rule: <=1000, 3600 s apart per wallet).
 * Fork rule (merge): both copies are fully verified; the valid chain with the most cumulative work wins (work per block = 16^zeros); a tie keeps the first argument.
 * NOT here: signatures (later), peer-to-peer, the steward bot. */
(function (root) {
  var K = [0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
  function utf8(s) { var o = [], i, c; for (i = 0; i < s.length; i++) { c = s.charCodeAt(i);
      if (c < 128) o.push(c); else if (c < 2048) o.push(192 | (c >> 6), 128 | (c & 63));
      else if (c >= 0xd800 && c < 0xdc00 && i + 1 < s.length) { c = 0x10000 + ((c & 0x3ff) << 10) + (s.charCodeAt(++i) & 0x3ff); o.push(240 | (c >> 18), 128 | ((c >> 12) & 63), 128 | ((c >> 6) & 63), 128 | (c & 63)); }
      else o.push(224 | (c >> 12), 128 | ((c >> 6) & 63), 128 | (c & 63)); } return o; }
  function sha256(str) {
    var m = utf8(str), l = m.length, i, j, w = [], H = [0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
    m.push(0x80); while (m.length % 64 !== 56) m.push(0);
    var hi = Math.floor(l / 0x20000000), lo = (l << 3) >>> 0;
    m.push((hi >>> 24) & 255, (hi >>> 16) & 255, (hi >>> 8) & 255, hi & 255, (lo >>> 24) & 255, (lo >>> 16) & 255, (lo >>> 8) & 255, lo & 255);
    function rr(x, n) { return (x >>> n) | (x << (32 - n)); }
    for (i = 0; i < m.length; i += 64) {
      for (j = 0; j < 16; j++) w[j] = ((m[i + 4 * j] << 24) | (m[i + 4 * j + 1] << 16) | (m[i + 4 * j + 2] << 8) | m[i + 4 * j + 3]) | 0;
      for (j = 16; j < 64; j++) { var s0 = rr(w[j - 15], 7) ^ rr(w[j - 15], 18) ^ (w[j - 15] >>> 3), s1 = rr(w[j - 2], 17) ^ rr(w[j - 2], 19) ^ (w[j - 2] >>> 10); w[j] = (w[j - 16] + s0 + w[j - 7] + s1) | 0; }
      var a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
      for (j = 0; j < 64; j++) { var S1 = rr(e, 6) ^ rr(e, 11) ^ rr(e, 25), ch = (e & f) ^ (~e & g), t1 = (h + S1 + ch + K[j] + w[j]) | 0, S0 = rr(a, 2) ^ rr(a, 13) ^ rr(a, 22), mj = (a & b) ^ (a & c) ^ (b & c), t2 = (S0 + mj) | 0;
        h = g; g = f; f = e; e = (d + t1) | 0; d = c; c = b; b = a; a = (t1 + t2) | 0; }
      H[0] = (H[0] + a) | 0; H[1] = (H[1] + b) | 0; H[2] = (H[2] + c) | 0; H[3] = (H[3] + d) | 0; H[4] = (H[4] + e) | 0; H[5] = (H[5] + f) | 0; H[6] = (H[6] + g) | 0; H[7] = (H[7] + h) | 0;
    }
    var out = ''; for (i = 0; i < 8; i++) out += ('00000000' + (H[i] >>> 0).toString(16)).slice(-8); return out;
  }
  var ZERO = new Array(65).join('0');
  var DEF = { zeros: 2, initial: 10500, halving: 1000, supply: 21000000, faucetMax: 1000, faucetGap: 3600 };
  function cfgOf(c) { var o = {}, k; for (k in DEF) o[k] = (c && c[k] !== undefined) ? c[k] : DEF[k]; return o; }
  function reward(i, c) { c = cfgOf(c); var e = Math.floor(i / c.halving); return e > 40 ? 0 : Math.floor(c.initial / Math.pow(2, e)); }
  function meets(h, z) { for (var i = 0; i < z; i++) if (h.charAt(i) !== '0') return false; return true; }
  /* ---- THE RANCH: data, not code. Every number of the game lives in this one table (the pet rules below only read it). Placeholder art/names: our own art comes later. ---- */
  var RANCH = {
    species: ['Fire', 'Water', 'Earth', 'Wind', 'Light', 'Dark', 'Lightning', 'Ice', 'Cosmic', 'Celestial'],
    stats: ['LIF', 'POW', 'INT', 'SKI', 'SPD', 'DEF'],
    foods: { bread: { cost: 5, fat: 5, stat: '', n: 0 }, meat: { cost: 20, fat: 15, stat: 'POW', n: 1 }, fruit: { cost: 12, fat: 10, stat: 'INT', n: 1 }, tonic: { cost: 50, fat: 30, stat: 'LIF', n: 2 } },
    drills: { sprint: { stat: 'SPD', cost: 10, gain: 3, fat: 12 }, lift: { stat: 'POW', cost: 10, gain: 3, fat: 12 }, study: { stat: 'INT', cost: 10, gain: 3, fat: 8 }, spar: { stat: 'SKI', cost: 15, gain: 3, fat: 15 }, guard: { stat: 'DEF', cost: 10, gain: 3, fat: 10 }, jog: { stat: 'LIF', cost: 10, gain: 2, fat: 6 } },
    ranks: ['E', 'D', 'C', 'B', 'A', 'S'],  /* ranked AI ladder (Monster Rancher style): you must have beaten the rank below to enter a rank */
    rank: { E: { entry: 10, prize: 60, rival: 90 }, D: { entry: 20, prize: 130, rival: 125 }, C: { entry: 40, prize: 280, rival: 160 }, B: { entry: 80, prize: 600, rival: 200 }, A: { entry: 160, prize: 1300, rival: 245 }, S: { entry: 320, prize: 3000, rival: 295 } },
    items: { dumbbell: { cost: 30, n: 2 }, charm: { cost: 40, n: 20 }, elixir: { cost: 60, n: 0 } },
    facilities: { gym: { base: 100, max: 3 }, barn: { base: 80, max: 3 } },
    restFat: 40, barnRest: 10, tiredHalf: 60, tiredMax: 100, statCap: 99, compFat: 20, roll: 40, ai: ['Gruff', 'Mira', 'Tobo', 'Vexa', 'Bram', 'Lune', 'Oko', 'Pip', 'Rask', 'Sable', 'Tamsin', 'Yuki'], maxPets: 16, maxActive: 6, cols: 12, rows: 8, blocked: [[1, 1], [10, 1]]  /* the ranch grid (hai-style: positions are rows); the gym stands on (1,1) and the barn on (10,1); at most maxActive pets are out at once, the rest are frozen in storage */
  };
  var VERBS = ['TX', 'MSG', 'FAUCET', 'PET', 'FEED', 'TRAIN', 'REST', 'COMPETE', 'UPGRADE', 'FREEZE', 'THAW', 'TRANSFER', 'LIST', 'UNLIST', 'BUY', 'BUYITEM', 'USEITEM', 'GIVEITEM', 'CHALLENGE', 'MOVE'];
  var RLEN = { FEED: 5, TRAIN: 5, REST: 4, COMPETE: 5, UPGRADE: 5, FREEZE: 4, THAW: 4, TRANSFER: 5, LIST: 6, UNLIST: 4, BUY: 5, BUYITEM: 5, USEITEM: 5, GIVEITEM: 6, CHALLENGE: 5, MOVE: 6 };
  function isVerb(v) { for (var i = 0; i < VERBS.length; i++) if (VERBS[i] === v) return true; return false; }
  function okId(s) { return /^[a-z0-9_-]{1,24}$/.test(s); }
  /* genes(seed, name): the same words always hatch the same pet (SHA-256 of "seed|name"; bytes pick species, six stats of 10..30, lifespan 120..180 weeks, look) */
  function genes(seed, name) {
    var h = sha256(String(seed) + '|' + name), b = [], i, st = {};
    for (i = 0; i < 32; i++) b.push(parseInt(h.substr(i * 2, 2), 16));
    for (i = 0; i < 6; i++) st[RANCH.stats[i]] = 10 + b[i] % 21;
    return { species: RANCH.species[b[6] % RANCH.species.length], speciesIx: b[6] % RANCH.species.length, stats: st, life: 120 + b[7] % 61, hue: Math.round(b[8] * 360 / 256), sx: b[12] % RANCH.cols, sy: b[13] % RANCH.rows, eyes: b[9] % 4, horns: b[10] % 3, spots: b[11] % 4, hash: h };
  }
  function power(p) { return p.stats.POW + p.stats.INT + p.stats.SKI + p.stats.SPD + p.stats.DEF; }
  function splitRows(txl) { var out = [], parts = String(txl).split(';'), i, r; for (i = 0; i < parts.length; i++) { r = parts[i].replace(/^\s+|\s+$/g, ''); if (r) out.push(r); } return out; }
  function cleanText(s) { return String(s).replace(/[|;\r\n]/g, ' ').slice(0, 280); }
  function parse(text) { var lines = String(text).split('\n'), out = [], i, p;
    for (i = 0; i < lines.length; i++) { if (!lines[i]) continue; p = lines[i].split('|'); if (p[0] !== 'BLOCK' || p.length < 8) return { error: 'line ' + (i + 1) + ' is not a BLOCK row' };
      out.push({ index: +p[1], prev: p[2], nonce: p[3], hash: p[4], ts: +p[5], miner: p[6], txl: p.slice(7).join('|') }); }
    return { blocks: out }; }
  /* verify(text, cfg) -> { ok, height, work, balances, messages, pets, error, at } ; every rule the C chain enforces on receipt, plus MSG, FAUCET and the RANCH rows. */
  function verify(text, cfg) {
    var c = cfgOf(cfg), P = parse(text), bal = {}, msgs = [], last = {}, pets = {}, minted = 0, work = 0, i, b, rows, j, f, r, why;
    function fail(at, w) { return { ok: false, error: w, at: at, height: i, balances: bal, messages: msgs, pets: pets, work: work }; }
    function spend(name, amt) { if ((bal[name] || 0) < amt) return 'not enough coins in ' + name; bal[name] = (bal[name] || 0) - amt; bal._ranch = (bal._ranch || 0) + amt; return ''; }
    function week(p) { p.age++; if (p.age >= p.life) p.retired = true; }
    function addStat(p, s, n) { p.stats[s] = Math.min(RANCH.statCap, p.stats[s] + n); }
    function active(o) { var k, n = 0; for (k in pets) if (pets[k].owner === o && !pets[k].frozen) n++; return n; }
    function cellFree(x, y, self) { var k, bl = RANCH.blocked; if (x < 0 || y < 0 || x >= RANCH.cols || y >= RANCH.rows) return false; for (k = 0; k < bl.length; k++) if (bl[k][0] === x && bl[k][1] === y) return false; for (k in pets) if (pets[k] !== self && !pets[k].frozen && pets[k].x === x && pets[k].y === y) return false; return true; }
    function place(p) { var i, x, y; for (i = 0; i < RANCH.cols * RANCH.rows; i++) { x = (p.g.sx + i) % RANCH.cols; y = (p.g.sy + Math.floor((p.g.sx + i) / RANCH.cols)) % RANCH.rows; if (cellFree(x, y, p)) { p.x = x; p.y = y; return; } } p.x = 0; p.y = 0; }
    /* one ranch row: returns '' when allowed (and applied) else the reason. f = fields of the row. Row shapes (owner is always the second-last field, ts the last):
     *   PET|name|seed words|owner|ts            FEED|pet|food|owner|ts     TRAIN|pet|drill|owner|ts     REST|pet|owner|ts      COMPETE|pet|rank|owner|ts   UPGRADE|pet|facility|owner|ts
     *   FREEZE|pet|owner|ts  THAW|pet|owner|ts  TRANSFER|pet|newowner|owner|ts
     *   LIST|pet|price|payee pet|owner|ts       UNLIST|pet|owner|ts        BUY|pet|payer pet|buyer owner|ts  (the payer pays the price to the payee, the pet changes owner)
     *   BUYITEM|pet|item|owner|ts   USEITEM|pet|item|owner|ts   GIVEITEM|pet|item|to pet|owner|ts      CHALLENGE|pet|other pet|owner|ts */
    function ranch(f) {
      var v = f[0], p, g, nm = f[1], len = RLEN[v], d, e, why, cost, sc, rv, prize, q, k, cnt;
      if (v === 'PET') {
        if (f.length !== 5) return 'bad PET row'; if (!okId(nm) || nm.charAt(0) === '_') return 'bad pet name'; if (pets[nm]) return 'name already used';
        if (!f[2] || f[2].length > 40) return 'bad seed words'; if (!okId(f[3])) return 'bad owner';
        cnt = 0; for (k in pets) if (pets[k].owner === f[3]) cnt++; if (cnt >= RANCH.maxPets) return 'too many pets for ' + f[3]; if (active(f[3]) >= RANCH.maxActive) return 'six pets are already out: freeze one first';
        g = genes(f[2], nm); pets[nm] = { name: nm, owner: f[3], seed: f[2], species: g.species, g: g, stats: JSON.parse(JSON.stringify(g.stats)), age: 0, life: g.life, fat: 0, frozen: false, retired: false, fac: { gym: 0, barn: 0 }, wins: 0, cups: 0, best: -1, items: {}, buffT: 0, buffF: 0, price: 0, payee: '', pvpW: 0, pvpL: 0, x: 0, y: 0 }; place(pets[nm]); return '';
      }
      p = pets[nm]; if (!p) return 'no such pet'; if (f.length !== len) return 'bad ' + v + ' row';
      if (v === 'BUY') { q = pets[f[2]]; if (!(p.price > 0)) return nm + ' is not for sale'; if (!q) return 'no such paying pet'; if (q.owner !== f[3]) return 'the paying pet is not yours'; if (p.owner === f[3]) return 'you already own ' + nm; if (!p.frozen && active(f[3]) >= RANCH.maxActive) return 'six pets are already out: freeze one first';
        if ((bal[f[2]] || 0) < p.price) return 'not enough coins in ' + f[2]; bal[f[2]] -= p.price; bal[p.payee] = (bal[p.payee] || 0) + p.price; p.owner = f[3]; p.price = 0; p.payee = ''; return ''; }
      if (f[len - 2] !== p.owner) return 'not the owner of ' + nm;
      if (v === 'THAW') { if (!p.frozen) return 'not frozen'; if (active(p.owner) >= RANCH.maxActive) return 'six pets are already out: freeze one first'; p.frozen = false; if (!cellFree(p.x, p.y, p)) place(p); return ''; }
      if (v === 'FREEZE') { if (p.frozen) return 'already frozen'; p.frozen = true; return ''; }
      if (v === 'TRANSFER') { if (!okId(f[2])) return 'bad new owner'; if (!p.frozen && active(f[2]) >= RANCH.maxActive) return f[2] + ' already has six pets out'; p.owner = f[2]; p.price = 0; p.payee = ''; return ''; }
      if (v === 'LIST') { var pr = +f[2]; if (!(pr > 0) || Math.floor(pr) !== pr || pr > 1000000) return 'bad price'; q = pets[f[3]]; if (!q || q.owner !== p.owner) return 'the payee must be one of your pets'; p.price = pr; p.payee = f[3]; return ''; }
      if (v === 'UNLIST') { if (!(p.price > 0)) return 'not listed'; p.price = 0; p.payee = ''; return ''; }
      if (v === 'GIVEITEM') { q = pets[f[3]]; if (!q) return 'no such pet to give to'; if (!RANCH.items[f[2]]) return 'unknown item'; if (!(p.items[f[2]] > 0)) return nm + ' has no ' + f[2]; p.items[f[2]]--; q.items[f[2]] = (q.items[f[2]] || 0) + 1; return ''; }
      if (p.frozen) return nm + ' is frozen'; if (p.retired) return nm + ' has retired';
      if (v === 'MOVE') { var mx = +f[2], my = +f[3]; if (Math.floor(mx) !== mx || Math.floor(my) !== my) return 'bad cell'; if (!cellFree(mx, my, p)) return 'that cell is not free'; p.x = mx; p.y = my; return ''; }
      if (v === 'FEED') { d = RANCH.foods[f[2]]; if (!d) return 'unknown food'; why = spend(nm, d.cost); if (why) return why; p.fat = Math.max(0, p.fat - d.fat); if (d.stat) addStat(p, d.stat, d.n); week(p); return ''; }
      if (v === 'REST') { p.fat = Math.max(0, p.fat - RANCH.restFat - p.fac.barn * RANCH.barnRest); week(p); return ''; }
      if (v === 'TRAIN') { d = RANCH.drills[f[2]]; if (!d) return 'unknown drill'; if (p.fat >= RANCH.tiredMax) return nm + ' is too tired'; why = spend(nm, d.cost); if (why) return why;
        var gain = p.fat >= RANCH.tiredHalf ? Math.floor(d.gain / 2) : d.gain + p.fac.gym + (p.buffT ? RANCH.items.dumbbell.n : 0); p.buffT = 0; addStat(p, d.stat, gain); p.fat = Math.min(RANCH.tiredMax, p.fat + d.fat); week(p); return ''; }
      if (v === 'UPGRADE') { var fcl = RANCH.facilities[f[2]]; if (!fcl) return 'unknown facility'; if (p.fac[f[2]] >= fcl.max) return 'already at the top level'; cost = fcl.base * (p.fac[f[2]] + 1); why = spend(nm, cost); if (why) return why; p.fac[f[2]]++; return ''; }
      if (v === 'BUYITEM') { d = RANCH.items[f[2]]; if (!d) return 'unknown item'; why = spend(nm, d.cost); if (why) return why; p.items[f[2]] = (p.items[f[2]] || 0) + 1; return ''; }
      if (v === 'USEITEM') { d = RANCH.items[f[2]]; if (!d) return 'unknown item'; if (!(p.items[f[2]] > 0)) return nm + ' has no ' + f[2]; p.items[f[2]]--; if (f[2] === 'dumbbell') p.buffT = 1; else if (f[2] === 'charm') p.buffF = 1; else if (f[2] === 'elixir') p.fat = 0; return ''; }
      if (v === 'COMPETE') { var ri = RANCH.ranks.indexOf(f[2]); if (ri < 0) return 'unknown rank'; e = RANCH.rank[f[2]]; if (p.fat >= RANCH.tiredMax) return nm + ' is too tired'; if (p.best < ri - 1) return nm + ' must beat rank ' + RANCH.ranks[ri - 1] + ' first'; why = spend(nm, e.entry); if (why) return why;
        rv = parseInt(sha256(p.seed + '|' + nm + '|' + f[2] + '|' + p.age).substr(0, 2), 16) % RANCH.roll; sc = power(p) - Math.floor(p.fat / 2) + rv + (p.buffF ? RANCH.items.charm.n : 0); p.buffF = 0; p.cups++;
        if (sc >= e.rival) { prize = Math.min(e.prize, bal._ranch || 0); bal._ranch -= prize; bal[nm] = (bal[nm] || 0) + prize; p.wins++; if (ri > p.best) p.best = ri; }
        p.fat = Math.min(RANCH.tiredMax, p.fat + RANCH.compFat); week(p); return ''; }
      if (v === 'CHALLENGE') { q = pets[f[2]]; if (!q) return 'no such opponent'; if (q === p) return 'a pet cannot challenge itself'; if (q.frozen || q.retired) return f[2] + ' cannot fight now'; if (p.fat >= RANCH.tiredMax) return nm + ' is too tired';
        var ra = parseInt(sha256(p.seed + '|' + nm + '|' + f[2] + '|' + p.age).substr(0, 2), 16) % RANCH.roll, rb = parseInt(sha256(q.seed + '|' + f[2] + '|' + nm + '|' + q.age).substr(0, 2), 16) % RANCH.roll;
        if (power(p) - Math.floor(p.fat / 2) + ra >= power(q) - Math.floor(q.fat / 2) + rb) { p.pvpW++; q.pvpL++; } else { p.pvpL++; q.pvpW++; } p.fat = Math.min(RANCH.tiredMax, p.fat + RANCH.compFat); week(p); return ''; }
      return 'unknown row';
    }
    if (P.error) return { ok: false, error: P.error, at: 0, height: 0, balances: bal, messages: msgs, pets: pets, work: 0 };
    for (i = 0; i < P.blocks.length; i++) { b = P.blocks[i];
      if (b.index !== i) return fail(i, 'index ' + b.index + ' expected ' + i);
      if (b.prev !== (i === 0 ? ZERO : P.blocks[i - 1].hash)) return fail(i, 'prev_hash does not match the tip');
      if (sha256(i + '|' + b.prev + '|' + b.nonce + '|' + b.txl) !== b.hash) return fail(i, 'hash does not recompute');
      if (!meets(b.hash, c.zeros)) return fail(i, 'difficulty not met');
      rows = splitRows(b.txl);
      for (j = 0; j < rows.length; j++) { f = rows[j].split('|');
        if (!isVerb(f[0])) return fail(i, 'unknown row ' + f[0]);
        if (f[0] === 'TX') { var amt = +f[3]; if (!(amt > 0) || Math.floor(amt) !== amt) return fail(i, 'bad amount'); if ((bal[f[1]] || 0) < amt) return fail(i, 'insufficient balance for ' + f[1]); bal[f[1]] -= amt; bal[f[2]] = (bal[f[2]] || 0) + amt; }
        else if (f[0] === 'FAUCET') { var fa = +f[2], ft = +f[3]; if (!(fa > 0) || fa > c.faucetMax) return fail(i, 'faucet amount over the cap'); if (last[f[1]] !== undefined && ft - last[f[1]] < c.faucetGap) return fail(i, 'faucet cooldown'); last[f[1]] = ft; bal[f[1]] = (bal[f[1]] || 0) + fa; }
        else if (f[0] === 'MSG') { if (f.length !== 5) return fail(i, 'bad MSG row'); msgs.push({ block: i, from: f[1], channel: f[2], text: f[3], ts: +f[4] }); }
        else { why = ranch(f); if (why) return fail(i, why); } }
      r = reward(i, c); if (minted + r > c.supply) r = c.supply - minted; minted += r; bal[b.miner] = (bal[b.miner] || 0) + r; work += Math.pow(16, c.zeros); }
    return { ok: true, height: P.blocks.length, work: work, balances: bal, messages: msgs, pets: pets, tip: P.blocks.length ? P.blocks[P.blocks.length - 1].hash : ZERO }; }
  function lineOf(b) { return 'BLOCK|' + b.index + '|' + b.prev + '|' + b.nonce + '|' + b.hash + '|' + b.ts + '|' + b.miner + '|' + b.txl; }
  /* mine(text, miner, rows, ts, cfg, maxTries) -> { ok, line, tries } : rows are strings ; ts is passed in so tests are deterministic. */
  function mine(text, miner, rows, ts, cfg, maxTries) { var c = cfgOf(cfg), P = parse(text), n = P.blocks ? P.blocks.length : 0, prev = n ? P.blocks[n - 1].hash : ZERO, txl = rows.join(';'), nonce = 0, h;
    maxTries = maxTries || 5000000; for (; nonce < maxTries; nonce++) { h = sha256(n + '|' + prev + '|' + nonce + '|' + txl); if (meets(h, c.zeros)) return { ok: true, tries: nonce + 1, line: lineOf({ index: n, prev: prev, nonce: nonce, hash: h, ts: ts, miner: miner, txl: txl }) }; }
    return { ok: false, tries: nonce }; }
  /* merge(a, b, cfg): the fork rule. Returns { ok, text, winner:'a'|'b', reason }. A copy that fails verify never wins; both bad -> ok false. */
  function merge(a, b, cfg) { var va = verify(a, cfg), vb = verify(b, cfg);
    if (!va.ok && !vb.ok) return { ok: false, reason: 'neither copy verifies' };
    if (!vb.ok) return { ok: true, text: a, winner: 'a', reason: 'b rejected: ' + vb.error };
    if (!va.ok) return { ok: true, text: b, winner: 'b', reason: 'a rejected: ' + va.error };
    return vb.work > va.work ? { ok: true, text: b, winner: 'b', reason: 'more work' } : { ok: true, text: a, winner: 'a', reason: va.work === vb.work ? 'tie keeps a' : 'more work' }; }
  /* applyRows(text, miner, rows, ts, cfg): the STEWARD rule. Each submitted row is tried ALONE on top of the chain (one bad row never blocks the others); accepted rows are mined into ONE block. */
  function applyRows(text, miner, rows, ts, cfg) {
    var good = [], bad = [], i, cur = text, m, v;
    for (i = 0; i < rows.length; i++) { var r = String(rows[i]).replace(/[\r\n]+/g, ' ').trim();
      if (!isVerb(r.split('|')[0]) || r.length > 400 || r.indexOf(';') >= 0) { bad.push({ row: r, why: 'not an allowed row' }); continue; }
      m = mine(cur, miner, good.concat([r]), ts, cfg, 4000000); if (!m.ok) { bad.push({ row: r, why: 'mining gave up' }); continue; }
      v = verify(cur + m.line + '\n', cfg); if (!v.ok) { bad.push({ row: r, why: v.error }); continue; } good.push(r); }
    if (!good.length) return { text: text, accepted: [], rejected: bad, mined: false };
    m = mine(text, miner, good, ts, cfg, 4000000); return { text: text + m.line + '\n', accepted: good, rejected: bad, mined: true }; }
  function messages(text, channel, cfg) { var v = verify(text, cfg), o = [], i; if (!v.ok) return o; for (i = 0; i < v.messages.length; i++) if (!channel || v.messages[i].channel === channel) o.push(v.messages[i]); return o; }
  root.TearChain = { RLEN: RLEN, sha256: sha256, verify: verify, mine: mine, merge: merge, applyRows: applyRows, messages: messages, reward: reward, cleanText: cleanText, genes: genes, power: power, okId: okId, RANCH: RANCH, VERBS: VERBS, ZERO: ZERO, DEF: DEF };
})(typeof window !== 'undefined' ? window : (typeof self !== 'undefined' ? self : this));
