/* CLI-2 fixture: nested require with '../' resolution up one level. */
var math = require('../cli_cjs_math.js');
module.exports = { n: math.add(40, 2) };