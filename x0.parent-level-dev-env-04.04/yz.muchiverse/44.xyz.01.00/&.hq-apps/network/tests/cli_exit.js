// cli_exit.js — CLI-1 node-mode: process.exit(code) controls the exit status.
console.log("before-exit");
process.exit(3);
console.log("after-exit");   // must never run