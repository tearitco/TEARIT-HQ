/* CLI-2 fixture: module.exports object + a module-local var (must not leak
 * to the global scope of the requiring script). */
var leaked = 'nope';
module.exports = { add: function (a, b) { return a + b; } };