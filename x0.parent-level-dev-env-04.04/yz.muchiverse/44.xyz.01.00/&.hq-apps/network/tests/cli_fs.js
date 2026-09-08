/* CLI-3 fixture: fs-lite read/write/append/exists/mkdir via require('fs').
 * The tmpdir comes in as process.argv[2] from the test driver. */
var fs = require('fs');
var dir = process.argv[2];
var p = dir + '/cli-fs-tmp.txt';
fs.writeFileSync(p, 'beta');
fs.appendFileSync(p, '-gamma');
console.log('fs-rw=' + fs.readFileSync(p, 'utf8'));
console.log('fs-ex=true=' + fs.existsSync(p));
console.log('fs-miss=false=' + fs.existsSync(dir + '/no-such-cli-fs'));
fs.mkdirSync(dir + '/cli-fs-sub/nested/deep');
console.log('fs-mkdir=true=' + fs.existsSync(dir + '/cli-fs-sub/nested/deep'));