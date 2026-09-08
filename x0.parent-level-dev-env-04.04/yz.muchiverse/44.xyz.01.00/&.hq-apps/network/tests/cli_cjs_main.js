/* CLI-2 CJS end-to-end fixture: require/module/exports, JSON, cycles,
 * nested '../' resolution, and module-local var scoping. */
console.log('cjs-hello');
var math = require('./cli_cjs_math.js');
console.log('math-add=' + math.add(2, 3));
console.log('same-module=' + (require('./cli_cjs_math.js') === math));
var data = require('./cli_cjs_data.json');
console.log('json-ping=' + data.ping);
var sub = require('./cli_cjs_sub/helper.js');
console.log('nested-up=' + sub.n);
var circ = require('./cli_cjs_circ_a.js');
console.log('circ=' + circ.ok);
console.log('lex-true=' + (typeof leaked === 'undefined'));
console.log('filenames=' + (typeof __filename === 'string') + (typeof __dirname === 'string'));