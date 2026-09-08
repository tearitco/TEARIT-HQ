require("./cli_esm_named.js");
var ns = require("./cli_esm_default.js");
console.log("esm-from-cjs A=" + require("./cli_esm_named.js").A + " esmDefault=" + ns.default(2) + " marker=esm-cjs-ok");