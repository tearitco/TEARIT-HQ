/*nbjs-script-boundary*/
/* tearit-web chain.js: the browser side of the pal-chain (experimental web chain, owner 2026-11-24: "keep it experimental, signing can wait").
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
  function splitRows(txl) { var out = [], re = /(TX|MSG|FAUCET)\|/g, idx = [], m; while ((m = re.exec(txl))) idx.push(m.index);
    for (var i = 0; i < idx.length; i++) out.push(txl.slice(idx[i], i + 1 < idx.length ? idx[i + 1] : txl.length).replace(/[;,\s]+$/, '')); return out; }
  function cleanText(s) { return String(s).replace(/[|;\r\n]/g, ' ').slice(0, 280); }
  function parse(text) { var lines = String(text).split('\n'), out = [], i, p;
    for (i = 0; i < lines.length; i++) { if (!lines[i]) continue; p = lines[i].split('|'); if (p[0] !== 'BLOCK' || p.length < 8) return { error: 'line ' + (i + 1) + ' is not a BLOCK row' };
      out.push({ index: +p[1], prev: p[2], nonce: p[3], hash: p[4], ts: +p[5], miner: p[6], txl: p.slice(7).join('|') }); }
    return { blocks: out }; }
  /* verify(text, cfg) -> { ok, height, work, balances, messages, error, at } ; every rule the C chain enforces on receipt, plus MSG and FAUCET. */
  function verify(text, cfg) {
    var c = cfgOf(cfg), P = parse(text), bal = {}, msgs = [], last = {}, minted = 0, work = 0, i, b, rows, j, f, r;
    function fail(at, why) { return { ok: false, error: why, at: at, height: i, balances: bal, messages: msgs, work: work }; }
    if (P.error) return { ok: false, error: P.error, at: 0, height: 0, balances: bal, messages: msgs, work: 0 };
    for (i = 0; i < P.blocks.length; i++) { b = P.blocks[i];
      if (b.index !== i) return fail(i, 'index ' + b.index + ' expected ' + i);
      if (b.prev !== (i === 0 ? ZERO : P.blocks[i - 1].hash)) return fail(i, 'prev_hash does not match the tip');
      if (sha256(i + '|' + b.prev + '|' + b.nonce + '|' + b.txl) !== b.hash) return fail(i, 'hash does not recompute');
      if (!meets(b.hash, c.zeros)) return fail(i, 'difficulty not met');
      rows = splitRows(b.txl);
      for (j = 0; j < rows.length; j++) { f = rows[j].split('|');
        if (f[0] === 'TX') { var amt = +f[3]; if (!(amt > 0) || Math.floor(amt) !== amt) return fail(i, 'bad amount'); if ((bal[f[1]] || 0) < amt) return fail(i, 'insufficient balance for ' + f[1]); bal[f[1]] -= amt; bal[f[2]] = (bal[f[2]] || 0) + amt; }
        else if (f[0] === 'FAUCET') { var fa = +f[2], ft = +f[3]; if (!(fa > 0) || fa > c.faucetMax) return fail(i, 'faucet amount over the cap'); if (last[f[1]] !== undefined && ft - last[f[1]] < c.faucetGap) return fail(i, 'faucet cooldown'); last[f[1]] = ft; bal[f[1]] = (bal[f[1]] || 0) + fa; }
        else if (f[0] === 'MSG') { if (f.length !== 5) return fail(i, 'bad MSG row'); msgs.push({ block: i, from: f[1], channel: f[2], text: f[3], ts: +f[4] }); } }
      r = reward(i, c); if (minted + r > c.supply) r = c.supply - minted; minted += r; bal[b.miner] = (bal[b.miner] || 0) + r; work += Math.pow(16, c.zeros); }
    return { ok: true, height: P.blocks.length, work: work, balances: bal, messages: msgs, tip: P.blocks.length ? P.blocks[P.blocks.length - 1].hash : ZERO }; }
  function lineOf(b) { return 'BLOCK|' + b.index + '|' + b.prev + '|' + b.nonce + '|' + b.hash + '|' + b.ts + '|' + b.miner + '|' + b.txl; }
  /* mine(text, miner, rows, ts, cfg, maxTries) -> { ok, line, tries } : rows are strings (TX|..., MSG|..., FAUCET|...) ; ts is passed in so tests are deterministic. */
  function mine(text, miner, rows, ts, cfg, maxTries) { var c = cfgOf(cfg), P = parse(text), n = P.blocks ? P.blocks.length : 0, prev = n ? P.blocks[n - 1].hash : ZERO, txl = rows.join(';'), nonce = 0, h;
    maxTries = maxTries || 5000000; for (; nonce < maxTries; nonce++) { h = sha256(n + '|' + prev + '|' + nonce + '|' + txl); if (meets(h, c.zeros)) return { ok: true, tries: nonce + 1, line: lineOf({ index: n, prev: prev, nonce: nonce, hash: h, ts: ts, miner: miner, txl: txl }) }; }
    return { ok: false, tries: nonce }; }
  /* merge(a, b, cfg): the fork rule. Returns { ok, text, winner:'a'|'b', reason }. A copy that fails verify never wins; both bad -> ok false. */
  function merge(a, b, cfg) { var va = verify(a, cfg), vb = verify(b, cfg);
    if (!va.ok && !vb.ok) return { ok: false, reason: 'neither copy verifies' };
    if (!vb.ok) return { ok: true, text: a, winner: 'a', reason: 'b rejected: ' + vb.error };
    if (!va.ok) return { ok: true, text: b, winner: 'b', reason: 'a rejected: ' + va.error };
    return vb.work > va.work ? { ok: true, text: b, winner: 'b', reason: 'more work' } : { ok: true, text: a, winner: 'a', reason: va.work === vb.work ? 'tie keeps a' : 'more work' }; }
  /* applyRows(text, miner, rows, ts, cfg): the STEWARD rule. Each submitted row is tried ALONE on top of the chain (so one bad row never blocks the others);
   * accepted rows are mined into ONE block. Returns { text, accepted:[rows], rejected:[{row,why}], mined:bool }. A row must be TX|, MSG| or FAUCET| and <= 400 chars. */
  function applyRows(text, miner, rows, ts, cfg) {
    var good = [], bad = [], i, cur = text, m, v;
    for (i = 0; i < rows.length; i++) { var r = String(rows[i]).replace(/[\r\n]+/g, ' ').trim();
      if (!/^(TX|MSG|FAUCET)\|/.test(r) || r.length > 400 || r.indexOf(';') >= 0) { bad.push({ row: r, why: 'not an allowed row' }); continue; }
      m = mine(cur, miner, good.concat([r]), ts, cfg, 4000000); if (!m.ok) { bad.push({ row: r, why: 'mining gave up' }); continue; }
      v = verify(cur + m.line + '\n', cfg); if (!v.ok) { bad.push({ row: r, why: v.error }); continue; } good.push(r); }
    if (!good.length) return { text: text, accepted: [], rejected: bad, mined: false };
    m = mine(text, miner, good, ts, cfg, 4000000); return { text: text + m.line + '\n', accepted: good, rejected: bad, mined: true }; }
  function messages(text, channel, cfg) { var v = verify(text, cfg), o = [], i; if (!v.ok) return o; for (i = 0; i < v.messages.length; i++) if (!channel || v.messages[i].channel === channel) o.push(v.messages[i]); return o; }
  root.TearChain = { sha256: sha256, verify: verify, mine: mine, merge: merge, applyRows: applyRows, messages: messages, reward: reward, cleanText: cleanText, ZERO: ZERO, DEF: DEF };
})(typeof window !== 'undefined' ? window : (typeof self !== 'undefined' ? self : this));
