/* CLI-2 fixture: require of a nonexistent module must exit 1 with a
 * "Cannot find module" error on stderr. */
require('./cli_nope.js');