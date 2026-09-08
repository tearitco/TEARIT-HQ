/* CLI-2 fixture: circular require — A requires B, B requires A (partial
 * exports served on re-entry, standard CJS). */
exports.round = 'a-early';
var b = require('./cli_cjs_circ_b.js');
module.exports.ok = (b.seeA === 'a-early');