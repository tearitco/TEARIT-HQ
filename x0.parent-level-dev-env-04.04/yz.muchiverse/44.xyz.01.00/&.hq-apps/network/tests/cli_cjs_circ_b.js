/* CLI-2 fixture: circular require — reads A before A finished evaluating. */
var a = require('./cli_cjs_circ_a.js');
module.exports.seeA = a.round;