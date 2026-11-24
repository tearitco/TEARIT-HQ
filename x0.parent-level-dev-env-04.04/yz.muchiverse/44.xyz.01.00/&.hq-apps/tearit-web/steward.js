/* steward.js: the GitHub-side bot of the tearit web chain (run by .github/workflows/steward.yml on a labelled issue).
 * It runs the SAME verifier as the page (docs/chain.js, TearChain.applyRows): every submitted row is tried alone, accepted rows are mined into one block,
 * docs/chain/latest.txt is rewritten. Usage: node steward.js <issue-body-file> <miner-name> ; prints a one-line JSON verdict. Exit 0 always when it ran (a rejection is a verdict, not an error). */
var fs = require('fs'); global.window = global; require('./docs/chain.js');
var body = fs.readFileSync(process.argv[2], 'utf8'), miner = (process.argv[3] || 'steward').replace(/[^a-z0-9_-]/gi, '') || 'steward';
var rows = body.split('\n').map(function (l) { return l.trim(); }).filter(function (l) { return /^(TX|MSG|FAUCET)\|/.test(l); }).slice(0, 20);
var path = 'docs/chain/latest.txt', cur = fs.existsSync(path) ? fs.readFileSync(path, 'utf8') : '';
var r = TearChain.applyRows(cur, miner, rows, Math.floor(Date.now() / 1000), { zeros: 2 });
if (r.mined) fs.writeFileSync(path, r.text);
console.log(JSON.stringify({ mined: r.mined, accepted: r.accepted.length, rejected: r.rejected, height: TearChain.verify(r.text, { zeros: 2 }).height }));
